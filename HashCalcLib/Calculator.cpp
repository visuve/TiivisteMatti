module;
#include "PCH.hpp"

module HashLib;

namespace HashLib
{
	class Handle
	{
	public:
		Handle() = default;

		explicit Handle(HANDLE handle) : 
			_handle(handle) 
		{
		}

		~Handle()
		{
			Close();
		}

		Handle(const Handle&) = delete;
		Handle(Handle&& other) = delete;
		Handle& operator = (const Handle&) = delete;
		Handle& operator = (Handle&&) = delete;

		Handle& operator = (HANDLE handle)
		{
			if (_handle != handle)
			{
				Close();
				_handle = handle;
			}

			return *this;
		}

		void Close()
		{
			if (_handle != nullptr && _handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(_handle);
				_handle = INVALID_HANDLE_VALUE;
			}
		}

		bool IsValid() const
		{
			return _handle != nullptr && _handle != INVALID_HANDLE_VALUE;
		}

		operator HANDLE() const
		{ 
			return _handle; 
		}

	private:
		HANDLE _handle = INVALID_HANDLE_VALUE;
	};

	class MemoryMappedFile
	{
	public:
		MemoryMappedFile(const std::filesystem::path& path) :
			_file(CreateFileW(
				path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr))
		{
			if (!_file.IsValid())
			{
				throw std::runtime_error("Failed to open file: " + path.string());
			}

			LARGE_INTEGER size;
			if (GetFileSizeEx(_file, &size))
			{
				_size = size.QuadPart;
			}

			if (!_size)
			{
				return;
			}

#ifndef _WIN64
			constexpr uint64_t MaxX86MapSize = 1024ULL * 1024ULL * 1024ULL; // 1 GB
			if (_size > MaxX86MapSize)
			{
				throw std::runtime_error("File size exceeds safe contiguous mapping limits for 32-bit architecture.");
			}
#endif

			_mapping = CreateFileMappingW(_file, nullptr, PAGE_READONLY, 0, 0, nullptr);

			if (!_mapping)
			{
				throw std::runtime_error("CreateFileMappingW");
			}

			_view = MapViewOfFile(_mapping, FILE_MAP_READ, 0, 0, 0);

			if (!_view)
			{
				throw std::runtime_error("MapViewOfFile");
			}
		}

		~MemoryMappedFile()
		{
			if (_view)
			{
				UnmapViewOfFile(_view);
			}
		}

		MemoryMappedFile(const MemoryMappedFile&) = delete;
		MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

		explicit operator bool() const
		{
			return _file.IsValid() && (_mapping.IsValid() || !_size);
		}

		uint64_t Size() const
		{
			return _size;
		}

		std::span<const uint8_t> Chunk(uint64_t offset, size_t maxSize) const
		{
			if (!_view || offset >= _size)
			{
				return {};
			}

			size_t bytesAvailable = static_cast<size_t>(std::min<uint64_t>(maxSize, _size - offset));
			const uint8_t* data = static_cast<uint8_t*>(_view);

			return { data + offset, bytesAvailable };
		}

	private:
		Handle _file;
		Handle _mapping;
		void* _view = nullptr;
		uint64_t _size = 0;
	};

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
			throw Exception(std::format(L"BCryptGetProperty({})", property), status);
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

	void Hash::Update(std::span<const uint8_t> data)
	{
		if (_handle == nullptr)
		{
			return;
		}

		const NTSTATUS status = BCryptHashData(
			_handle,
			const_cast<PUCHAR>(data.data()),
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
	}

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksums(std::span<const uint8_t> data) const
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

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksums(std::wstring_view data) const
	{
		std::vector<uint8_t> ba = Strings::ToByteArray(data, CP_UTF8);
		return CalculateChecksums(ba);
	}

	std::map<std::wstring, std::wstring> Calculator::CalculateChecksumsFromFile(
		const std::filesystem::path& path,
		std::stop_token stopToken,
		ProgressCallback callback) const
	{
		std::map<std::wstring, std::wstring> results;

		MemoryMappedFile file(path);

		if (!file)
		{
			return results;
		}

		std::vector<std::pair<std::wstring, Hash>> hashes;

		for (const auto& [name, handle] : _providers)
		{
			hashes.emplace_back(name, Hash(handle));
		}

		constexpr size_t viewSize = 0x100000; // 1 MB chunks
		const uint64_t totalBytes = file.Size();
		uint64_t offset = 0;

		// 10000 represents 0.01% increments
		const uint64_t updateThreshold = std::max<uint64_t>(1, totalBytes / 10000);
		uint64_t nextUpdate = updateThreshold;

		if (callback)
		{
			callback(0.00f);
		}

		while (totalBytes && offset < totalBytes)
		{
			if (stopToken.stop_requested())
			{
				throw std::runtime_error("Cancelled");
			}

			std::span<const uint8_t> data = file.Chunk(offset, viewSize);

			for (auto& [name, hash] : hashes)
			{
				hash.Update(data);
			}

			offset += data.size_bytes();

			if (callback && offset >= nextUpdate)
			{
				callback(static_cast<float>(offset) / static_cast<float>(totalBytes) * 100.0f);
				nextUpdate = offset + updateThreshold;
			}
		}

		for (auto& [name, hash] : hashes)
		{
			hash.Finish();
			results.emplace(name, hash.ToString());
		}

		if (callback)
		{
			callback(100.0f);
		}

		return results;
	}

	std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> Calculator::CalculateChecksumsFromFolder(
		const std::filesystem::path& path,
		std::stop_token stopToken,
		ProgressCallback callback) const
	{
		std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> results;

		const std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;

		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path, options))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			results.emplace(entry.path(), CalculateChecksumsFromFile(entry.path(), stopToken, callback));
		}

		return results;
	}
}