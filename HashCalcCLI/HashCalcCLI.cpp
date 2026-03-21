#include "PCH.hpp"

import HashLib;

namespace HashCalcCLI
{
	bool CheckPath(const std::filesystem::path& path, std::filesystem::file_type expectedType)
	{
		try
		{
			const std::filesystem::file_status status = std::filesystem::status(path);

			if (status.type() != expectedType)
			{
				return false;
			}

			const std::filesystem::perms permissions = status.permissions();

			const std::filesystem::perms expectedPermissions =
				std::filesystem::perms::owner_read |
				std::filesystem::perms::group_read |
				std::filesystem::perms::others_read;

			return (permissions & expectedPermissions) != std::filesystem::perms::none;
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << '\n';
			return false;
		}
	}

	bool IsReadableFile(const std::filesystem::path& path)
	{
		return CheckPath(path, std::filesystem::file_type::regular);
	}

	bool IsReadableFolder(const std::filesystem::path& path)
	{
		return CheckPath(path, std::filesystem::file_type::directory);
	}

	const std::array<std::wstring, 7> SupportedAlgorithms =
	{
		BCRYPT_MD2_ALGORITHM,
		BCRYPT_MD4_ALGORITHM,
		BCRYPT_MD5_ALGORITHM,
		BCRYPT_SHA1_ALGORITHM,
		BCRYPT_SHA256_ALGORITHM,
		BCRYPT_SHA384_ALGORITHM,
		BCRYPT_SHA512_ALGORITHM
	};

	bool ContainsUnsupportedAlgorithms(const std::vector<std::wstring>& algorithms)
	{
		return std::ranges::any_of(algorithms, [](const auto& algo)
		{
			return std::ranges::find(SupportedAlgorithms, algo) == std::ranges::end(SupportedAlgorithms);
		});
	}

	void PrintUsage(const std::filesystem::path& exePath)
	{
		std::wcerr << L"HashCalc v0.1" << std::endl;
		std::wcerr << L"Invalid arguments. Usage:" << std::endl;
		std::wcerr << exePath << " <string>" << std::endl;
		std::wcerr << exePath << " X:\\Path\\To\\File" << std::endl;
		std::wcerr << exePath << " <string> <algorithm>" << std::endl;
		std::wcerr << exePath << " X:\\Path\\To\\File <algorithm>" << std::endl;
		std::wcerr << L"Currently supported algorithms: " <<
			HashLib::Strings::Join(SupportedAlgorithms) << std::endl;
	}

	std::stop_source StopSource;

	BOOL WINAPI ConsoleHandler(DWORD ctrlType)
	{
		if (ctrlType == CTRL_CLOSE_EVENT)
		{
			StopSource.request_stop();
			return TRUE;
		}

		return FALSE;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	using namespace HashCalcCLI;

	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	if (argc == 1 || argc > 3)
	{
		PrintUsage(argv[0]);
		return ERROR_BAD_ARGUMENTS;
	}

	std::vector<std::wstring> selectedAlgorithms = { BCRYPT_SHA256_ALGORITHM };

	if (argc == 3)
	{
		selectedAlgorithms = HashLib::Strings::Split(argv[2]);

		if (ContainsUnsupportedAlgorithms(selectedAlgorithms))
		{
			PrintUsage(argv[0]);
			return ERROR_BAD_ARGUMENTS;
		}
	}

	try
	{
		HashLib::Calculator hashCalc(selectedAlgorithms);

		if (IsReadableFile(argv[1]) || IsReadableFolder(argv[1]))
		{
			std::mutex printMutex;
			std::promise<void> completionPromise;

			HashLib::AsyncCallbacks callbacks;

			callbacks.OnProgress = nullptr;

			callbacks.OnComplete = [&printMutex](const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& hashes)
			{
				std::lock_guard lock(printMutex);

				for (const auto& [algo, hash] : hashes)
				{
					std::wcout << path << L'\t' << algo << L'\t' << hash << std::endl;
				}
			};

			callbacks.OnError = [&printMutex](const std::filesystem::path& path, const std::wstring& error)
			{
				std::lock_guard lock(printMutex);
				std::wcerr << path << L"\tError: " << error << std::endl;
			};

			callbacks.OnFinished = [&completionPromise]()
			{
				completionPromise.set_value();
			};

			std::vector<std::filesystem::path> paths = HashLib::Strings::SplitPaths(argv[1]);
			std::jthread worker = hashCalc.CalculateChecksumsAsync(std::move(paths), std::move(callbacks));

			std::stop_callback stopLink(StopSource.get_token(), [&worker]()
			{
				worker.request_stop();
			});

			completionPromise.get_future().wait();
		}
		else
		{
			for (const auto& [algo, hash] : hashCalc.CalculateChecksums(argv[1]))
			{
				std::wcout << algo << L'\t' << hash << std::endl;
			}
		}
	}
	catch (const HashLib::Exception& e)
	{
		std::cerr << "An exception occurred: " << e.what() << std::endl;
		return e.Code;
	}
	catch (const std::ios::failure& e)
	{
		std::cerr << "An I/O exception occurred: " << e.what() << std::endl;
		return ERROR_IO_DEVICE;
	}

	return 0;
}