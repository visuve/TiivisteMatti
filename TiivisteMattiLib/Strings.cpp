module;
#include "PCH.hpp"

module TiivisteMattiLib;

namespace TiivisteMattiLib::Strings
{
	std::string ToNarrow(std::wstring_view input, uint32_t codepage)
	{
		std::string output;

		int required = WideCharToMultiByte(
			codepage,
			0,
			input.data(),
			static_cast<int>(input.length()),
			nullptr,
			0,
			nullptr,
			nullptr);

		if (required > 0)
		{
			output.resize(static_cast<size_t>(required));

			required = WideCharToMultiByte(
				codepage,
				0,
				input.data(),
				static_cast<int>(input.length()),
				output.data(),
				required,
				nullptr,
				nullptr);

			_ASSERT(output.size() == static_cast<size_t>(required));
		}

		return output;
	}

	std::wstring ToWide(std::string_view input, uint32_t codepage)
	{
		std::wstring output;

		int required = MultiByteToWideChar(
			codepage,
			0,
			input.data(),
			static_cast<int>(input.length()),
			nullptr,
			0);

		if (required > 0)
		{
			output.resize(static_cast<size_t>(required));

			required = MultiByteToWideChar(
				codepage,
				0,
				input.data(),
				static_cast<int>(input.length()),
				output.data(),
				required);

			_ASSERT(output.size() == static_cast<size_t>(required));
		}

		return output;
	}

	std::vector<uint8_t> ToByteArray(std::wstring_view data, uint32_t codepage)
	{
		const std::string utf8 = ToNarrow(data, codepage);

		std::vector<uint8_t> bytes(utf8.size());

		std::ranges::transform(utf8, bytes.begin(), [](char c)
		{ 
			return static_cast<uint8_t>(c); 
		});

		return bytes;
	}
}