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
				"BCryptGetProperty(" + Strings::ToNarrow(property) + ')';
			throw std::exception(message.c_str(), status);
		}

		return object;
	}

	Hash::Hash(BCRYPT_ALG_HANDLE algorithm)
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

	Hash::Hash(Hash&& other) :
		_handle(std::exchange(other._handle, nullptr)),
		_data(std::move(other._data))
	{
	}

	Hash& Hash::operator = (Hash&& other)
	{
		if (this != &other)
		{
			if (_handle)
			{
				BCryptDestroyHash(_handle);
			}

			_handle = std::exchange(other._handle, nullptr);
			_data = std::move(other._data);
		}

		return *this;
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


	Calculator::Calculator(const std::vector<std::wstring>& algorithms)
	{
		for (const std::wstring& algorithm : algorithms)
		{
			BCRYPT_ALG_HANDLE handle = nullptr;

			const NTSTATUS status = BCryptOpenAlgorithmProvider(
				&handle,
				algorithm.data(),
				nullptr,
				BCRYPT_HASH_REUSABLE_FLAG);

			if (status != 0)
			{
				throw Exception(L"BCryptOpenAlgorithmProvider", status);
			}

			_providers.emplace_back(algorithm, handle);
		}
	}

	Calculator::~Calculator()
	{
		for (const auto& [_, handle] : _providers)
		{
			if (handle)
			{
				BCryptCloseAlgorithmProvider(handle, 0);
			}
		}

		_providers.clear();
	}

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksums(std::span<uint8_t> data)
	{
		std::map<std::wstring, std::wstring> results;
		
		for (const auto& [name, handle] : _providers)
		{
			Hash hash(handle);
			hash.Update(data);
			hash.Finish();
			results.emplace(name, hash.ToString());
		}

		return results;
	}

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksums(std::wstring_view data)
	{
		std::vector<uint8_t> ba = Strings::ToByteArray(data, CP_UTF8);
		return CalculateChecksums(ba);
	}

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksumsFromFile(const std::filesystem::path& path, std::stop_token stopToken)
	{
		std::map<std::wstring, std::wstring> results;

		std::basic_ifstream<uint8_t> file(path, std::ios::in | std::ios::binary);

		if (!file)
		{
			return results;
		}

		std::vector<std::pair<std::wstring, Hash>> hashes;

		for (const auto& [name, handle] : _providers)
		{
			hashes.emplace_back(name, Hash(handle));
		}

		file.exceptions(std::istream::failbit | std::istream::badbit);

		std::vector<uint8_t> buffer(0x100000);
		uint64_t bytesLeft = std::filesystem::file_size(path);

		_ASSERT(bytesLeft <= std::numeric_limits<size_t>::max());

		while (bytesLeft && file)
		{
			if (stopToken.stop_requested())
			{
				throw std::runtime_error("Cancelled");
			}

			if (buffer.size() > bytesLeft)
			{
				buffer.resize(static_cast<size_t>(bytesLeft));
			}

			file.read(buffer.data(), buffer.size());
			bytesLeft -= buffer.size();

			for (auto& [name, hash] : hashes)
			{
				hash.Update(buffer);
			}
		}

		for (auto& [name, hash] : hashes)
		{
			hash.Finish();
			results.emplace(name, hash.ToString());
		}

		return results;
	}

	std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> Calculator::CalculateChecksumsFromFolder(const std::filesystem::path& path, std::stop_token stopToken)
	{
		std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> results;

		const std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;

		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path, options))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			results.emplace(entry.path(), CalculateChecksumsFromFile(entry.path(), stopToken));
		}

		return results;
	}
}