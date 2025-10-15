#include "PlatformUtils.h"
#include "ScopeGuard.h"
#include "StringUtils.h"
#include "Logging.h"

namespace cz
{

#if CZ_WINDOWS
// #CMAKE : Test this for when building with UNICODE
std::string getWin32Error(DWORD err, const char* funcname)
{
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	if (err == ERROR_SUCCESS)
		err = GetLastError();

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				   NULL,
				   err,
				   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				   (LPWSTR)&lpMsgBuf,
				   0,
				   NULL);
	CZ_SCOPE_EXIT{ LocalFree(lpMsgBuf); };

	int funcnameLength = funcname ? lstrlen((LPCTSTR)funcname) : 0;

	lpDisplayBuf =
		(LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlenW((LPCWSTR)lpMsgBuf) + funcnameLength + 50) * sizeof(wchar_t));
	if (lpDisplayBuf == NULL)
		return "Win32ErrorMsg failed";
	CZ_SCOPE_EXIT{ LocalFree(lpDisplayBuf); };

	auto wfuncname = funcname ? widen(funcname) : std::wstring(L"");

	StringCchPrintfW((LPWSTR)lpDisplayBuf,
					 LocalSize(lpDisplayBuf) / sizeof(wchar_t),
					 L"%s failed with error %lu: %s",
					 wfuncname.c_str(),
					 err,
					 (LPWSTR)lpMsgBuf);

	std::wstring ret = (LPWSTR)lpDisplayBuf;

	// Remove the \r\n at the end
	while (ret.size() && ret.back() < ' ')
		ret.pop_back();

	return narrow(ret);
}
#endif

namespace
{

std::wstring convertDevicePathToWin32Path(const std::wstring& devicePath)
{
	WCHAR driveStrings[255];
	WCHAR deviceName[255];
	WCHAR win32Path[MAX_PATH];

	// Retrieve a list of all drive strings (e.g., "A:\0B:\0C:\0\0")
	if (GetLogicalDriveStringsW(sizeof(driveStrings) / sizeof(WCHAR), driveStrings))
	{
		WCHAR* drive = driveStrings;
		while (*drive)
		{
			// For each drive, retrieve the device name (e.g., "\Device\HarddiskVolume2")
			WCHAR driveLetter[3] = {drive[0], L':', L'\0'};
			if (QueryDosDeviceW(driveLetter, deviceName, sizeof(deviceName) / sizeof(WCHAR)))
			{
				std::wstring deviceNameStr(deviceName);
				// If the device name is a prefix of the device path, replace it with the drive letter
				if (devicePath.find(deviceNameStr) == 0)
				{
					// Construct the final Win32 path
					swprintf_s(win32Path, MAX_PATH, L"%s%s", driveLetter, devicePath.substr(deviceNameStr.length()).c_str());
					return std::wstring(win32Path);
				}
			}
			// Move to the next drive string
			drive += wcslen(drive) + 1;
		}
	}
	return devicePath;	// Return original path if no mapping was found
}

} // anonymous namespace

fs::path getProcessPath()
{
#if CZ_WINDOWS

	DWORD size = MAX_PATH;

	while(true)
	{
		auto buffer = std::make_unique<wchar_t[]>(size);
		HANDLE processHandle = ::GetCurrentProcess();
		DWORD res = GetProcessImageFileNameW(processHandle, buffer.get(), size);

		if (res > 0 && res < size)
		{
			auto buf = convertDevicePathToWin32Path(buffer.get());
			//return std::filesystem::path(buffer.get()).parent_path();
			return std::filesystem::path(buf.c_str()).parent_path();
		}

		if (res != size) // Call failed
		{
			CZ_LOG(Main, Fatal, "{}", getWin32Error("GetModuleFileNameW"));
			return "";
		}

		// Try again with a larger buffer;
		size *=2;
	}

#else

	#error Unknown/unsupported platform
	return "";
#endif
}


} // namespace cz

