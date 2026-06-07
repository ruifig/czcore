#include "PlatformUtils.h"
#include "ScopeGuard.h"
#include "StringUtils.h"
#include "Logging.h"

namespace cz
{

#if CZ_WINDOWS
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

fs::path getProcessExe()
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
			return fs::path(buf.c_str());
		}

		if (res != size) // Call failed
		{
			CZ_LOG(Main, Fatal, "{}", getWin32Error("GetModuleFileNameW"));
		}

		// Try again with a larger buffer;
		size *=2;
	}

#else

	#error Unknown/unsupported platform
	return "";
#endif
}

fs::path getProcessPath()
{
	return getProcessExe().parent_path();
}


#if CZ_WINDOWS
//
// Copied from https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformation
//
typedef BOOL(WINAPI * LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
uint32_t getNumPhysicalCores()
{

// Helper function to count set bits in the processor mask.
	auto CountSetBits = [](ULONG_PTR bitMask) -> DWORD
	{
		DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
		DWORD bitSetCount = 0;
		ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
		DWORD i;

		for (i = 0; i <= LSHIFT; ++i)
		{
			bitSetCount += ((bitMask & bitTest) ? 1 : 0);
			bitTest /= 2;
		}

		return bitSetCount;
	};

	LPFN_GLPI glpi;
	BOOL done = FALSE;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
	DWORD returnLength = 0;
	DWORD logicalProcessorCount = 0;
	DWORD numaNodeCount = 0;
	DWORD processorCoreCount = 0;
	DWORD processorL1CacheCount = 0;
	DWORD processorL2CacheCount = 0;
	DWORD processorL3CacheCount = 0;
	DWORD processorPackageCount = 0;
	DWORD byteOffset = 0;
	PCACHE_DESCRIPTOR Cache;

	#pragma warning(push)
	#pragma warning(disable: 4191)
	glpi = (LPFN_GLPI)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");
	#pragma warning(pop)

	//#define dolog printf
	#define dolog

	if (NULL == glpi)
	{
		dolog("\nGetLogicalProcessorInformation is not supported.\n");
		return (1);
	}

	while (!done)
	{
		DWORD rc = static_cast<DWORD>(glpi(buffer, &returnLength));

		if (FALSE == rc)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					free(buffer);

				buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

				if (NULL == buffer)
				{
					dolog("\nError: Allocation failure\n");
					return (2);
				}
			}
			else
			{
				dolog("\nError %ld\n", GetLastError());
				return (3);
			}
		}
		else
		{
			done = TRUE;
		}
	}

	ptr = buffer;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		switch (ptr->Relationship)
		{
		case RelationNumaNode:
			// Non-NUMA systems report a single record of this type.
			numaNodeCount++;
			break;

		case RelationProcessorCore:
			processorCoreCount++;

			// A hyperthreaded core supplies more than one logical processor.
			logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
			break;

		case RelationCache:
			// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache.
			Cache = &ptr->Cache;
			if (Cache->Level == 1)
			{
				processorL1CacheCount++;
			}
			else if (Cache->Level == 2)
			{
				processorL2CacheCount++;
			}
			else if (Cache->Level == 3)
			{
				processorL3CacheCount++;
			}
			break;

		case RelationProcessorPackage:
			// Logical processors share a physical package.
			processorPackageCount++;
			break;

		case RelationNumaNodeEx:
		case RelationProcessorDie:
		case RelationProcessorModule:
		case RelationGroup:
		case RelationAll:
			dolog("\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
			break;
		}

		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}

	dolog("\nGetLogicalProcessorInformation results:\n");
	dolog("Number of NUMA nodes: %ld\n", numaNodeCount);
	dolog("Number of physical processor packages: %ld\n", processorPackageCount);
	dolog("Number of processor cores: %ld\n", processorCoreCount);
	dolog("Number of logical processors: %ld\n", logicalProcessorCount);
	dolog("Number of processor L1/L2/L3 caches: %ld/%ld/%ld\n", processorL1CacheCount, processorL2CacheCount,
		processorL3CacheCount);

	free(buffer);

	return processorCoreCount;
}

#else
uint32_t getNumPhysicalCores()
{
	return std::thread::hardware_concurrency();
}
#endif


} // namespace cz

