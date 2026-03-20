#include "PCH.hpp"

import HashLib;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(HashCalcTests)
{
public:
	TEST_METHOD(StringChecksumSame)
	{
		Assert::AreEqual(L"a9046c73e00331af68917d3804f70655", HashLib::Calculator(BCRYPT_MD2_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"866437cb7a794bce2b727acc0362ee27", HashLib::Calculator(BCRYPT_MD4_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"5d41402abc4b2a76b9719d911017c592", HashLib::Calculator(BCRYPT_MD5_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d", HashLib::Calculator(BCRYPT_SHA1_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824", HashLib::Calculator(BCRYPT_SHA256_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"59e1748777448c69de6b800d7a33bbfb9ff1b463e44354c3553bcdb9c666fa90125a3c79f90397bdf5f6a13de828684f", HashLib::Calculator(BCRYPT_SHA384_ALGORITHM).CalculateChecksum(L"hello").c_str());
		Assert::AreEqual(L"9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043", HashLib::Calculator(BCRYPT_SHA512_ALGORITHM).CalculateChecksum(L"hello").c_str());
	}

	TEST_METHOD(StringChecksumAlternating)
	{
		Assert::AreEqual(L"80a4442c57b9ae254e262ba19c1c832c", HashLib::Calculator(BCRYPT_MD2_ALGORITHM).CalculateChecksum(L"hydrogen").c_str());
		Assert::AreEqual(L"61bd12ec5d9d13a2956d8e0a33be5bdd", HashLib::Calculator(BCRYPT_MD4_ALGORITHM).CalculateChecksum(L"helium").c_str());
		Assert::AreEqual(L"42cacb235328b80cd4548840e57f89b6", HashLib::Calculator(BCRYPT_MD5_ALGORITHM).CalculateChecksum(L"lithium").c_str());
		Assert::AreEqual(L"bada19a17b86234ca21377e07317132fadfb853f", HashLib::Calculator(BCRYPT_SHA1_ALGORITHM).CalculateChecksum(L"beryllium").c_str());
		Assert::AreEqual(L"25b049a5984206dd6fe6eca9c02b468677615a568855010013d4960873c86527", HashLib::Calculator(BCRYPT_SHA256_ALGORITHM).CalculateChecksum(L"boron").c_str());
		Assert::AreEqual(L"c0df5e67ee6a34d2cee9f6c80d45c8e136ff363404027db863cc301acd4bf01aedfe8ea408dc3654bab4689db94f4585", HashLib::Calculator(BCRYPT_SHA384_ALGORITHM).CalculateChecksum(L"carbon").c_str());
		Assert::AreEqual(L"44e75d435bf4e8c41552a61bc9df08baede971a4470ec79e99408f8a4644e393dc56f7d7a1001a075e763e8773a368ed5a960323f3ff050d0e7f1d4cd237549c", HashLib::Calculator(BCRYPT_SHA512_ALGORITHM).CalculateChecksum(L"nitrogen").c_str());
	}

	TEST_METHOD(FileChecksum)
	{
		const std::filesystem::path path(L"..\\..\\..\\HashCalc.props");
		std::stop_source source;

		Assert::AreEqual(L"b745f90ecd2ef998b3f1bb200dd38f9aeb9957a0f316225dabcd4246ff0b5de8", HashLib::Calculator(BCRYPT_SHA256_ALGORITHM).CalculateChecksumFromFile(path, source.get_token()).c_str());
		Assert::AreEqual(L"d5bae55d40c9897b2795623197e33a2822a2aba4", HashLib::Calculator(BCRYPT_SHA1_ALGORITHM).CalculateChecksumFromFile(path, source.get_token()).c_str());
		Assert::AreEqual(L"7e63fbb9cbeef24e72be090819bc3aaab8aa3faf8c5a771312d465f1c8a407e6efa5bef7835d3329250459fdef37ecb25622a17c4cfd70480c0f61a578f3df61", HashLib::Calculator(BCRYPT_SHA512_ALGORITHM).CalculateChecksumFromFile(path, source.get_token()).c_str());
	}
};