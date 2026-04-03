module;
#include "PCH.hpp"

module TiivisteMattiLib;

namespace TiivisteMattiLib
{
	Handle::Handle(HANDLE handle) :
		_handle(handle)
	{
	}

	Handle::~Handle()
	{
		Close();
	}

	Handle& Handle::operator = (HANDLE handle)
	{
		if (_handle != handle)
		{
			Close();
			_handle = handle;
		}

		return *this;
	}

	void Handle::Close()
	{
		if (_handle != nullptr && _handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(_handle);
			_handle = INVALID_HANDLE_VALUE;
		}
	}

	bool Handle::IsValid() const
	{
		return _handle != nullptr && _handle != INVALID_HANDLE_VALUE;
	}

	Handle::operator HANDLE() const
	{
		return _handle;
	}
}