module;

#include "PCH.hpp"

export module HashLib;

export namespace HashLib
{
	class Exception : public std::exception
	{
	public:
		Exception(const std::wstring& what, const NTSTATUS status);
		const int Code;

	private:
		std::string Format(const std::wstring& what, const NTSTATUS status);
	};

	class Hash
	{
	public:
		Hash(BCRYPT_ALG_HANDLE algorithm);
		~Hash();

		void Update(std::span<uint8_t> data);
		void Finish();
		std::wstring ToString() const;

	private:
		BCRYPT_HASH_HANDLE _handle = nullptr;
		std::vector<uint8_t> _data;
	};

	class Calculator
	{
	public:
		Calculator(std::wstring_view algorithmName);
		~Calculator();

		std::wstring CalculateChecksum(std::span<uint8_t> data);
		std::wstring CalculateChecksum(std::wstring_view data);
		std::wstring CalculateChecksumFromFile(const std::filesystem::path& path);
		std::map<std::filesystem::path, std::wstring> CalculateChecksumFromFolder(const std::filesystem::path& path);

	private:
		BCRYPT_ALG_HANDLE _algorithmHandle = nullptr;
	};
}

export namespace HashLib::Strings
{
	std::string ToNarrow(std::wstring_view data, uint32_t codepage = CP_ACP);
	std::wstring ToWide(std::string_view data, uint32_t codepage = CP_ACP);
	std::vector<uint8_t> ToByteArray(std::wstring_view data, uint32_t codepage = CP_ACP);

	template<typename T, typename C>
	std::basic_string<C> Join(
		const T& array,
		std::basic_string_view<C> separator,
		std::basic_string_view<C> lastSeparator)
	{
		std::basic_string<C> joined;

		for (size_t i = 0; i < array.size(); ++i)
		{
			joined += array[i];

			if (i + 2 < array.size())
			{
				joined += separator;
				continue;
			}

			if (i + 1 < array.size())
			{
				joined += lastSeparator;
				continue;
			}
		}

		return joined;
	}

	template<typename T>
	std::wstring Join(const T& array)
	{
		return Join<T, wchar_t>(array, L", ", L" & ");
	}
}