#include "PCH.hpp"

import TiivisteMattiLib;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(HashTests)
{
public:
	TEST_METHOD(StringChecksumSame)
	{
		TiivisteMattiLib::Calculator calculator({ BCRYPT_MD2_ALGORITHM, BCRYPT_MD4_ALGORITHM, BCRYPT_MD5_ALGORITHM, BCRYPT_SHA1_ALGORITHM, BCRYPT_SHA256_ALGORITHM, BCRYPT_SHA384_ALGORITHM, BCRYPT_SHA512_ALGORITHM });

		auto result = calculator.CalculateChecksums(L"hello");

		Assert::AreEqual(L"a9046c73e00331af68917d3804f70655", result[BCRYPT_MD2_ALGORITHM].c_str());
		Assert::AreEqual(L"866437cb7a794bce2b727acc0362ee27", result[BCRYPT_MD4_ALGORITHM].c_str());
		Assert::AreEqual(L"5d41402abc4b2a76b9719d911017c592", result[BCRYPT_MD5_ALGORITHM].c_str());
		Assert::AreEqual(L"aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d", result[BCRYPT_SHA1_ALGORITHM].c_str());
		Assert::AreEqual(L"2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824", result[BCRYPT_SHA256_ALGORITHM].c_str());
		Assert::AreEqual(L"59e1748777448c69de6b800d7a33bbfb9ff1b463e44354c3553bcdb9c666fa90125a3c79f90397bdf5f6a13de828684f", result[BCRYPT_SHA384_ALGORITHM].c_str());
		Assert::AreEqual(L"9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043", result[BCRYPT_SHA512_ALGORITHM].c_str());
	}

	TEST_METHOD(StringChecksumAlternating)
	{
		TiivisteMattiLib::Calculator calculator({ BCRYPT_MD2_ALGORITHM, BCRYPT_MD4_ALGORITHM, BCRYPT_MD5_ALGORITHM, BCRYPT_SHA1_ALGORITHM, BCRYPT_SHA256_ALGORITHM, BCRYPT_SHA384_ALGORITHM, BCRYPT_SHA512_ALGORITHM });

		Assert::AreEqual(L"80a4442c57b9ae254e262ba19c1c832c", calculator.CalculateChecksums(L"hydrogen")[BCRYPT_MD2_ALGORITHM].c_str());
		Assert::AreEqual(L"61bd12ec5d9d13a2956d8e0a33be5bdd", calculator.CalculateChecksums(L"helium")[BCRYPT_MD4_ALGORITHM].c_str());
		Assert::AreEqual(L"42cacb235328b80cd4548840e57f89b6", calculator.CalculateChecksums(L"lithium")[BCRYPT_MD5_ALGORITHM].c_str());
		Assert::AreEqual(L"bada19a17b86234ca21377e07317132fadfb853f", calculator.CalculateChecksums(L"beryllium")[BCRYPT_SHA1_ALGORITHM].c_str());
		Assert::AreEqual(L"25b049a5984206dd6fe6eca9c02b468677615a568855010013d4960873c86527", calculator.CalculateChecksums(L"boron")[BCRYPT_SHA256_ALGORITHM].c_str());
		Assert::AreEqual(L"c0df5e67ee6a34d2cee9f6c80d45c8e136ff363404027db863cc301acd4bf01aedfe8ea408dc3654bab4689db94f4585", calculator.CalculateChecksums(L"carbon")[BCRYPT_SHA384_ALGORITHM].c_str());
		Assert::AreEqual(L"44e75d435bf4e8c41552a61bc9df08baede971a4470ec79e99408f8a4644e393dc56f7d7a1001a075e763e8773a368ed5a960323f3ff050d0e7f1d4cd237549c", calculator.CalculateChecksums(L"nitrogen")[BCRYPT_SHA512_ALGORITHM].c_str());
	}

	TEST_METHOD(InvalidAlgorithm)
	{
		auto createInvalidCalculator = []
		{
			TiivisteMattiLib::Calculator calculator({ L"INVALID_ALGO" });
		};

		Assert::ExpectException<TiivisteMattiLib::Exception>(createInvalidCalculator);
	}

	TEST_METHOD(FileChecksum)
	{
		const std::filesystem::path path(L"..\\..\\..\\TiivisteMatti.props");
		std::stop_source source;

		TiivisteMattiLib::Calculator calculator({ BCRYPT_SHA1_ALGORITHM, BCRYPT_SHA256_ALGORITHM, BCRYPT_SHA512_ALGORITHM });
		auto result = calculator.CalculateChecksumsFromFile(path, source.get_token());

		Assert::AreEqual(L"b745f90ecd2ef998b3f1bb200dd38f9aeb9957a0f316225dabcd4246ff0b5de8", result[BCRYPT_SHA256_ALGORITHM].c_str());
		Assert::AreEqual(L"d5bae55d40c9897b2795623197e33a2822a2aba4", result[BCRYPT_SHA1_ALGORITHM].c_str());
		Assert::AreEqual(L"7e63fbb9cbeef24e72be090819bc3aaab8aa3faf8c5a771312d465f1c8a407e6efa5bef7835d3329250459fdef37ecb25622a17c4cfd70480c0f61a578f3df61", result[BCRYPT_SHA512_ALGORITHM].c_str());
	}

	TEST_METHOD(CalculateChecksumsAsyncSuccess)
	{
		TiivisteMattiLib::Calculator calculator({ BCRYPT_SHA256_ALGORITHM });
		std::promise<void> done;
		bool completeFired = false;

		TiivisteMattiLib::AsyncCallbacks callbacks;

		callbacks.OnComplete = [&](const std::filesystem::path&, const std::map<std::wstring, std::wstring>& hashes)
		{
			completeFired = true;
			Assert::IsTrue(hashes.contains(BCRYPT_SHA256_ALGORITHM));
			Assert::AreEqual(L"b745f90ecd2ef998b3f1bb200dd38f9aeb9957a0f316225dabcd4246ff0b5de8", hashes.at(BCRYPT_SHA256_ALGORITHM).c_str());
		};

		callbacks.OnFinished = [&]()
		{
			done.set_value();
		};

		std::vector<std::filesystem::path> paths = { L"..\\..\\..\\TiivisteMatti.props" };
		auto worker = calculator.CalculateChecksumsAsync(paths, callbacks);

		done.get_future().wait();

		Assert::IsTrue(completeFired);
	}

	TEST_METHOD(CalculateChecksumsAsyncCancellation)
	{
		TiivisteMattiLib::Calculator calculator({ BCRYPT_SHA256_ALGORITHM });
		std::promise<void> done;
		bool completeFired = false;

		TiivisteMattiLib::AsyncCallbacks callbacks;
		callbacks.OnComplete = [&](const auto&, const auto&)
		{ 
				completeFired = true;
		};

		callbacks.OnFinished = [&]() { done.set_value(); };

		std::vector<std::filesystem::path> paths = { L"..\\..\\..\\TiivisteMatti.props" };
		auto worker = calculator.CalculateChecksumsAsync(paths, callbacks);

		worker.request_stop();
		done.get_future().wait();

		Assert::IsFalse(completeFired);
	}

	TEST_METHOD(CalculateChecksumsAsyncFileNotFound)
	{
		TiivisteMattiLib::Calculator calculator({ BCRYPT_MD5_ALGORITHM });
		std::promise<void> done;
		bool errorFired = false;

		TiivisteMattiLib::AsyncCallbacks callbacks;
		
		callbacks.OnError = [&](const auto&, const auto&)
		{ 
			errorFired = true;
		};

		callbacks.OnFinished = [&]()
		{ 
			done.set_value();
		};

		std::vector<std::filesystem::path> paths = { L"C:\\does_not_exist_12345.txt" };
		auto worker = calculator.CalculateChecksumsAsync(paths, callbacks);

		done.get_future().wait();

		Assert::IsTrue(errorFired);
	}
};