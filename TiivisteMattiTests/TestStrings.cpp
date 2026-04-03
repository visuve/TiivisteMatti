#include "PCH.hpp"
import TiivisteMattiLib;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(StringConversionTests)
{
public:
		
	TEST_METHOD(ToNarrow)
	{
		{
			const std::string result = TiivisteMattiLib::Strings::ToNarrow(L"ABC");
			Assert::AreEqual(result.size(), size_t(3));
			Assert::AreEqual(result[0], 'A');
			Assert::AreEqual(result[1], 'B');
			Assert::AreEqual(result[2], 'C');
		}
		{
			const std::vector<uint8_t> result = TiivisteMattiLib::Strings::ToByteArray(L"ABC");
			Assert::AreEqual(result.size(), size_t(3));
			Assert::AreEqual(result[0], uint8_t(0x41));
			Assert::AreEqual(result[1], uint8_t(0x42));
			Assert::AreEqual(result[2], uint8_t(0x43));
		}
	}

	TEST_METHOD(ToWide)
	{
		const std::wstring result = TiivisteMattiLib::Strings::ToWide("ABC");
		Assert::AreEqual(result.size(), size_t(3));
		Assert::AreEqual(result[0], L'A');
		Assert::AreEqual(result[1], L'B');
		Assert::AreEqual(result[2], L'C');
	}


	TEST_METHOD(ConvertEmptyStrings)
	{
		Assert::AreEqual(size_t(0), TiivisteMattiLib::Strings::ToNarrow(L"").size());
		Assert::AreEqual(size_t(0), TiivisteMattiLib::Strings::ToWide("").size());
		Assert::AreEqual(size_t(0), TiivisteMattiLib::Strings::ToByteArray(L"").size());
		Assert::AreEqual(size_t(0), TiivisteMattiLib::Strings::Split(L"").size());
	}

	TEST_METHOD(Join)
	{
		std::vector<std::wstring> elements = { L"hydrogen", L"helium", L"lithium", L"beryllium" };
		Assert::AreEqual(
			TiivisteMattiLib::Strings::Join(elements).c_str(), L"hydrogen, helium, lithium & beryllium");
	}

	TEST_METHOD(Split)
	{
		std::wstring input = L"hydrogen,helium,lithium,beryllium";
		std::vector<std::wstring> expected = { L"hydrogen", L"helium", L"lithium", L"beryllium" };

		auto result = TiivisteMattiLib::Strings::Split(input);

		Assert::AreEqual(expected.size(), result.size());

		for (size_t i = 0; i < expected.size(); ++i)
		{
			Assert::AreEqual(expected[i].c_str(), result[i].c_str());
		}
	}

	TEST_METHOD(SplitPaths)
	{
		std::wstring input = L"C:\\file1.txt,D:\\folder\\file2.exe";
		auto result = TiivisteMattiLib::Strings::SplitPaths(input);

		Assert::AreEqual(size_t(2), result.size());
		Assert::AreEqual(L"C:\\file1.txt", result[0].c_str());
		Assert::AreEqual(L"D:\\folder\\file2.exe", result[1].c_str());
	}

	TEST_METHOD(Bytes)
	{
		auto result = TiivisteMattiLib::Strings::ToByteArray(L"\xD83D\xDE18", CP_UTF8);
		Assert::AreEqual(result.size(), size_t(4));
		Assert::AreEqual(result[0], uint8_t(0xF0));
		Assert::AreEqual(result[1], uint8_t(0x9F));
		Assert::AreEqual(result[2], uint8_t(0x98));
		Assert::AreEqual(result[3], uint8_t(0x98));
	}
};