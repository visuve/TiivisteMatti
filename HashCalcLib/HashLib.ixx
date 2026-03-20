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

		Hash(const Hash&) = delete;
		Hash& operator=(const Hash&) = delete;

		Hash(Hash&& other);
		Hash& operator = (Hash&& other);

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
		Calculator(const std::vector<std::wstring>& algorithms);
		~Calculator();

		std::map<std::wstring, std::wstring> CalculateChecksums(std::span<uint8_t> data);
		std::map<std::wstring, std::wstring> CalculateChecksums(std::wstring_view data);
		std::map<std::wstring, std::wstring> CalculateChecksumsFromFile(const std::filesystem::path& path, std::stop_token stopToken);
		std::map<std::filesystem::path, std::map<std::wstring, std::wstring>> CalculateChecksumsFromFolder(const std::filesystem::path& path, std::stop_token stopToken);

	private:
		std::vector<std::pair<std::wstring, BCRYPT_ALG_HANDLE>> _providers;
	};
}

export namespace HashLib::Strings
{
	std::string ToNarrow(std::wstring_view data, uint32_t codepage = CP_ACP);
	std::wstring ToWide(std::string_view data, uint32_t codepage = CP_ACP);
	std::vector<uint8_t> ToByteArray(std::wstring_view data, uint32_t codepage = CP_ACP);

	template<std::ranges::input_range Range, typename C>
	std::basic_string<C> Join(
		const Range& range,
		std::basic_string_view<C> separator,
		std::basic_string_view<C> lastSeparator)
	{
		std::basic_string<C> result;

		auto n = std::ranges::distance(range);
		decltype(n) i = 0;

		for (const auto& elem : range)
		{
			result += elem;

			if (i + 2 < n)
			{
				result += separator;
			}
			else if (i + 1 < n)
			{
				result += lastSeparator;
			}

			++i;
		}

		return result;
	}

	template<typename C>
	std::vector<std::basic_string<C>> Split(std::basic_string_view<C> text, C separator)
	{
		std::vector<std::basic_string<C>> result;

		for (auto&& part : text | std::views::split(separator))
		{
			result.emplace_back(part.begin(), part.end());
		}

		return result;
	}

	template<typename T>
	std::wstring Join(const T& array)
	{
		return Join<T, wchar_t>(array, L", ", L" & ");
	}

	std::vector<std::wstring> Split(std::wstring_view text, char separator = ',')
	{
		return Split<wchar_t>(text, separator);
	}
}