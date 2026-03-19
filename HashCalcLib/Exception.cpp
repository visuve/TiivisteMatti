module;
#include "PCH.hpp"

module HashLib;

namespace HashLib
{
	Exception::Exception(const std::wstring& what, const NTSTATUS status) :
		std::exception(Format(what, status).c_str()),
		Code(status)
	{
	}

	std::string Exception::Format(const std::wstring& what, const NTSTATUS status)
	{
		std::array<wchar_t, 0x400> buffer;
		const DWORD size = FormatMessageW(
			FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
			GetModuleHandle(L"ntdll"),
			status,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			buffer.data(),
			static_cast<DWORD>(buffer.size()),
			nullptr);

		bool failure = status != 0;
		std::wstring message = failure ? what + L" failed." : L"";

		if (size > 2)
		{
			if (failure)
			{
				message += L" Description: ";
			}

			message += std::wstring(buffer.data(), size - 2); // Trim excess /r/n
		}


		return Strings::ToUtf8(message);
	}
}