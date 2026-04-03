module;

#include "PCH.hpp"

export module TiivisteMattiLib;

export namespace TiivisteMattiLib
{
	class Exception : public std::exception
	{
	public:
		Exception(const std::wstring& what, const NTSTATUS status);
		const int Code;

	private:
		std::string Format(const std::wstring& what, const NTSTATUS status);
	};

	class Handle
	{
	public:
		Handle() = default;
		explicit Handle(HANDLE handle);
		~Handle();

		Handle(const Handle&) = delete;
		Handle(Handle&& other) = delete;
		Handle& operator = (const Handle&) = delete;
		Handle& operator = (Handle&&) = delete;

		Handle& operator = (HANDLE handle);

		void Close();
		bool IsValid() const;
		operator HANDLE() const;

	private:
		HANDLE _handle = INVALID_HANDLE_VALUE;
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

		void Update(std::span<const uint8_t> data);
		void Finish();
		std::wstring ToString() const;

	private:
		BCRYPT_HASH_HANDLE _handle = nullptr;
		std::vector<uint8_t> _data;
	};

	using FileProgressCallback = std::function<void(float percent)>;

	struct AsyncCallbacks
	{
		std::function<void()> OnStart;
		std::function<void(const std::filesystem::path&, float)> OnProgress;
		std::function<void(const std::filesystem::path&, const std::map<std::wstring, std::wstring>&)> OnComplete;
		std::function<void(const std::filesystem::path&, const std::wstring&)> OnError;
		std::function<void()> OnFinished;
	};

	class Calculator
	{
	public:
		Calculator(const std::vector<std::wstring>& algorithms);
		~Calculator();

		std::jthread CalculateChecksumsAsync(
			std::vector<std::filesystem::path> paths,
			AsyncCallbacks callbacks) const;

		std::map<std::wstring, std::wstring> CalculateChecksums(std::span<const uint8_t> data) const;
		std::map<std::wstring, std::wstring> CalculateChecksums(std::wstring_view data) const;
		
		std::map<std::wstring, std::wstring> CalculateChecksumsFromFile(
			const std::filesystem::path& path,
			std::stop_token stopToken,
			FileProgressCallback callback = nullptr) const;

	private:
		static void ProcessFileAsync(
			std::stop_token stopToken,
			const Calculator* self,
			const std::filesystem::path& path,
			const AsyncCallbacks& callbacks);

		static void AsyncWorker(
			std::stop_token stopToken,
			const Calculator* self,
			std::vector<std::filesystem::path> paths,
			AsyncCallbacks callbacks);

		std::vector<std::pair<std::wstring, BCRYPT_ALG_HANDLE>> _providers;
	};
}

export namespace TiivisteMattiLib::Strings
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

	template<std::ranges::input_range Range, typename C>
	std::basic_string<C> Join(const Range& range, C character)
	{
		auto joined = std::views::join_with(range, character);
		return std::basic_string<C>(std::from_range, joined);
	}

	template<typename T, typename C>
	std::vector<T> Split(std::basic_string_view<C> text, C separator)
	{
		std::vector<T> result;
		auto splitView = std::views::split(text, separator);

		std::ranges::transform(splitView, std::back_inserter(result), [](const auto& part)
		{
			return T(part.begin(), part.end()); 
		});

		return result;
	}

	template<typename T>
	std::wstring Join(const T& array)
	{
		return Join<T, wchar_t>(array, L", ", L" & ");
	}

	std::vector<std::wstring> Split(std::wstring_view text, wchar_t separator = L',')
	{
		return Split<std::wstring, wchar_t>(text, separator);
	}

	std::vector<std::filesystem::path> SplitPaths(std::wstring_view text, wchar_t separator = L',')
	{
		return Split<std::filesystem::path, wchar_t>(text, separator);
	}
}