module;
#include "PCH.hpp"

module HashLib;

namespace HashLib
{
	size_t PropertySize(BCRYPT_ALG_HANDLE algorithm, std::wstring_view property)
	{
		DWORD object = 0;
		DWORD bytesWritten = 0;

		const NTSTATUS status = BCryptGetProperty(
			algorithm,
			property.data(),
			reinterpret_cast<PUCHAR>(&object),
			sizeof(DWORD),
			&bytesWritten,
			0);

		if (status != 0 || object == 0 || bytesWritten != sizeof(DWORD))
		{
			const std::string message =
				"BCryptGetProperty(" + Strings::ToUtf8(property) + ')';
			throw std::exception(message.c_str(), status);
		}

		return object;
	}

	Hash::Hash(BCRYPT_ALG_HANDLE algorithm)
	{
		// Create hash object
		{
			const NTSTATUS status = BCryptCreateHash(
				algorithm,
				&_handle,
				nullptr,
				0,
				nullptr,
				0,
				0);

			if (status != 0)
			{
				throw Exception(L"BCryptCreateHash", status);
			}
		}

		size_t hashSize = PropertySize(algorithm, BCRYPT_HASH_LENGTH);

		_data.resize(hashSize);
	}

	Hash::~Hash()
	{
		if (_handle)
		{
			BCryptDestroyHash(_handle);
			_handle = nullptr;
		}
	}

	void Hash::Update(std::span<uint8_t> data)
	{
		if (_handle == nullptr)
		{
			return;
		}

		const NTSTATUS status = BCryptHashData(
			_handle,
			data.data(),
			static_cast<ULONG>(data.size_bytes()),
			0);

		if (status != 0)
		{
			throw Exception(L"BCryptHashData", status);
		}
	}

	void Hash::Finish()
	{
		if (_handle == nullptr)
		{
			return;
		}

		const NTSTATUS status = BCryptFinishHash(
			_handle,
			_data.data(),
			static_cast<ULONG>(_data.size()),
			0);

		if (status != 0)
		{
			throw Exception(L"BCryptFinishHash", status);
		}
	}

	std::wstring Hash::ToString() const
	{
		static constexpr wchar_t HexMap[] = L"0123456789abcdef";

		if (_data.empty())
		{
			throw Exception(L"Hash data is empty", STATUS_INVALID_PARAMETER);
		}

		std::wstring result(_data.size() * 2, L'\0');

		auto it = result.begin();

		for (uint8_t byte : _data)
		{
			*it++ = HexMap[byte >> 4];
			*it++ = HexMap[byte & 0xF];
		}

		return result;
	}


	Calculator::Calculator(std::wstring_view algorithmName)
	{
		const NTSTATUS status = BCryptOpenAlgorithmProvider(
			&_algorithmHandle,
			algorithmName.data(),
			nullptr,
			BCRYPT_HASH_REUSABLE_FLAG);

		if (status != 0)
		{
			throw Exception(L"BCryptOpenAlgorithmProvider", status);
		}
	}

	Calculator::~Calculator()
	{
		if (_algorithmHandle)
		{
			BCryptCloseAlgorithmProvider(_algorithmHandle, 0);
			_algorithmHandle = nullptr;
		}
	}

	std::wstring Calculator::CalculateChecksum(std::span<uint8_t> data)
	{
		if (_algorithmHandle == nullptr)
		{
			return {};
		}

		Hash hash(_algorithmHandle);

		hash.Update(data);
		hash.Finish();

		return hash.ToString();
	}

	std::wstring Calculator::CalculateChecksum(std::wstring_view data)
	{
		std::vector<uint8_t> ba = Strings::ToUtf8ByteArray(data);
		return CalculateChecksum(ba);
	}

	std::wstring Calculator::CalculateChecksumFromFile(const std::filesystem::path & path)
	{
		std::basic_ifstream<uint8_t> file(path, std::ios::in | std::ios::binary);

		if (!file)
		{
			return {};
		}

		Hash hash(_algorithmHandle);

		file.exceptions(std::istream::failbit | std::istream::badbit);

		std::vector<uint8_t> buffer(0x400);
		uint64_t bytesLeft = std::filesystem::file_size(path);

		_ASSERT(bytesLeft <= std::numeric_limits<size_t>::max());

		while (bytesLeft && file)
		{
			if (buffer.size() > bytesLeft)
			{
				buffer.resize(static_cast<size_t>(bytesLeft));
			}

			file.read(buffer.data(), buffer.size());
			bytesLeft -= buffer.size();

			hash.Update(buffer);
		}

		hash.Finish();

		return hash.ToString();
	}

	std::map<std::filesystem::path, std::wstring> Calculator::CalculateChecksumFromFolder(const std::filesystem::path & path)
	{
		std::map<std::filesystem::path, std::wstring> result;

		const std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;

		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path, options))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			result.emplace(entry.path(), CalculateChecksumFromFile(entry.path()));
		}

		return result;
	}
}