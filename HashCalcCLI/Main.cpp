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

	bool IsSupportedAlgorithm(std::wstring_view algorithm)
	{
		return std::find(
			SupportedAlgorithms.cbegin(),
			SupportedAlgorithms.cend(),
			algorithm) != SupportedAlgorithms.cend();
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

	if (argc == 1 || argc > 3)
	{
		PrintUsage(argv[0]);
		return ERROR_BAD_ARGUMENTS;
	}

	std::wstring selectedAlgorithm = BCRYPT_SHA256_ALGORITHM;

	if (argc == 3)
	{
		if (IsSupportedAlgorithm(argv[2]))
		{
			selectedAlgorithm = argv[2];
		}
		else
		{
			PrintUsage(argv[0]);
			return ERROR_BAD_ARGUMENTS;
		}
	}

	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	try
	{
		HashLib::Calculator hashCalc(selectedAlgorithm);

		if (IsReadableFile(argv[1]))
		{
			std::wcout << hashCalc.CalculateChecksumFromFile(argv[1], StopSource.get_token());
		}
		else if (IsReadableFolder(argv[1]))
		{
			for (const auto& [path, hash] : hashCalc.CalculateChecksumFromFolder(argv[1], StopSource.get_token()))
			{
				std::wcout << path << L":\t" << hash << std::endl;
			}
		}
		else
		{
			std::wcout << hashCalc.CalculateChecksum(std::wstring(argv[1]));
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