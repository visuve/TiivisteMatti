#include "PCH.hpp"


import HashLib;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(ExceptionTests)
{
public:

	TEST_METHOD(Message)
	{
		{
			const HashLib::Exception ex(L"X", 0xC0E7000B);
			Assert::AreEqual(
				"X failed. Description: There were not enough physical disks to complete the requested operation.", ex.what());
			Assert::AreEqual(int(0xC0E7000B), ex.Code);
		}
		Assert::AreEqual("STATUS_SUCCESS", HashLib::Exception(L"SNAFU", 0).what());
	}
};
