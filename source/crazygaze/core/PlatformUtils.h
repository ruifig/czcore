#pragma once

#include "Common.h"

namespace cz
{

namespace fs = std::filesystem;

#if CZ_WINDOWS
/**
 * Returns an utf8 string with the specified win32 error
 * @param err Error code to convert to string. If not specified, it will use GetLastError() to get it
 * @param funcName Name of the Win32 function that failed. This information is added to the string. Can be nulled.
 */
std::string getWin32Error(DWORD err = ERROR_SUCCESS, const char* funcname = nullptr);
inline std::string getWin32Error(const char* funcname)
{
	return getWin32Error(ERROR_SUCCESS, funcname);
}
#endif

/**
 * Gets the process's directory.
 */
fs::path getProcessPath();

} // namespace cz


