#include "PCH.hpp"
#include "../Version.h"

import TiivisteMattiLib;

namespace TML = TiivisteMattiLib;

namespace TiivisteMatti
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
		std::wcerr << L"Tiiviste-Matti v" << TML::Strings::ToWide(TIIVISTEMATTI_VERSION) << std::endl;
		std::wcerr << L"A command-line utility for calculating file checksums using Windows CNG API." << std::endl;
		std::wcerr << L"Git commit hash: " << TML::Strings::ToWide(TIIVISTEMATTI_COMMIT_HASH) << std::endl;
		std::wcerr << L"\nUsage:" << std::endl;
		std::wcerr << exePath << " <string>" << std::endl;
		std::wcerr << exePath << " X:\\Path\\To\\FileOrFolder" << std::endl;
		std::wcerr << exePath << " <string> <algorithm>" << std::endl;
		std::wcerr << exePath << " X:\\Path\\To\\FileOrFolder <algorithm>" << std::endl;
		std::wcerr << L"Currently supported algorithms: " << TML::Strings::Join(SupportedAlgorithms) << std::endl;
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
	using namespace TiivisteMatti;

	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	if (argc == 1 || argc > 3)
	{
		PrintUsage(argv[0]);
		return ERROR_BAD_ARGUMENTS;
	}

	std::vector<std::wstring> selectedAlgorithms = 
	{ 
		BCRYPT_MD5_ALGORITHM,
		BCRYPT_SHA1_ALGORITHM,
		BCRYPT_SHA256_ALGORITHM
	};

	if (argc == 3)
	{
		selectedAlgorithms = TML::Strings::Split(argv[2]);

		if (ContainsUnsupportedAlgorithms(selectedAlgorithms))
		{
			PrintUsage(argv[0]);
			return ERROR_BAD_ARGUMENTS;
		}
	}

	try
	{
		TML::Calculator hashCalc(selectedAlgorithms);

		if (IsReadableFile(argv[1]) || IsReadableFolder(argv[1]))
		{
			std::wcout << L"Path," << TML::Strings::Join(selectedAlgorithms, L',') << std::endl;

			std::mutex printMutex;
			std::promise<void> completionPromise;

			TML::AsyncCallbacks callbacks;

			callbacks.OnProgress = nullptr;

			callbacks.OnComplete = [&printMutex, selectedAlgorithms](const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& hashes)
			{
				std::lock_guard lock(printMutex);

				std::wcout << path;

				for (const auto& algo : selectedAlgorithms)
				{
					std::wcout << L',';

					auto it = hashes.find(algo);

					if (it != hashes.end())
					{
						std::wcout << it->second;
					}
					else
					{
						std::wcout << L"Unknown";
					}
				}

				std::wcout << std::endl;
			};

			callbacks.OnError = [&printMutex, selectedAlgorithms](const std::filesystem::path& path, const std::wstring& error)
			{
				std::lock_guard lock(printMutex);

				std::wcout << path;

				for (size_t i = 0; i < selectedAlgorithms.size(); ++i)
				{
					std::wcout << L',' << error;
				}

				std::wcout << std::endl;
			};

			callbacks.OnFinished = [&completionPromise]()
			{
				completionPromise.set_value();
			};

			std::vector<std::filesystem::path> paths = TML::Strings::SplitPaths(argv[1]);
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
				std::wcout << algo << L':' << hash << std::endl;
			}
		}
	}
	catch (const TiivisteMattiLib::Exception& e)
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