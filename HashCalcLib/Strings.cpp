module;
#include "PCH.hpp"

module HashLib;

namespace HashLib::Strings
{
	std::string ToUtf8(std::wstring_view unicode)
	{
		std::string utf8;

		int required = WideCharToMultiByte(
			CP_UTF8,
			0,
			unicode.data(),
			static_cast<int>(unicode.length()),
			nullptr,
			0,
			nullptr,
			nullptr);

		if (required > 0)
		{
			utf8.resize(static_cast<size_t>(required));

			required = WideCharToMultiByte(
				CP_UTF8,
				0,
				unicode.data(),
				static_cast<int>(unicode.length()),
				utf8.data(),
				required,
				nullptr,
				nullptr);

			_ASSERT(utf8.size() == static_cast<size_t>(required));
		}

		return utf8;
	}

	std::vector<uint8_t> ToUtf8ByteArray(std::wstring_view unicode)
	{
		const std::string utf8 = ToUtf8(unicode);
		std::vector<uint8_t> bytes(utf8.size());

		std::ranges::transform(utf8, bytes.begin(), [](char c)
		{ 
			return static_cast<uint8_t>(c); 
		});

		return bytes;
	}
}