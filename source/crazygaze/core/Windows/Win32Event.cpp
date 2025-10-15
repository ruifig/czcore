#include "Win32Event.h"

using namespace std::literals::chrono_literals;

namespace cz
{

Win32Event::Win32Event(bool manualReset, bool initialState)
{
	m_handle = CreateEvent(NULL, manualReset, initialState, NULL);
	if (m_handle == Invalid::value)
	{
		CZ_LOG(Main, Error, "{}", getWin32Error("CreateEvent"));
	}
}


void Win32Event::signal()
{
	if (m_handle == Invalid::value)
	{
		return;
	}

	if (!SetEvent(m_handle))
	{
		CZ_LOG(Main, Error, "{}", getWin32Error("CloseHandle"));
	}
}

void Win32Event::reset()
{
	if (m_handle == Invalid::value)
	{
		return;
	}

	if (!ResetEvent(m_handle))
	{
		CZ_LOG(Main, Error, "{}", getWin32Error("CloseHandle"));
	}
}

}


