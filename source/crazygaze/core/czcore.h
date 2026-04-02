#include "Common_Macros.h"
#include "Logging_Macros.h"
#include "Misc_Macros.h"
#include "ScopeGuard_Macros.h"
#include "Profiler.h"

CZ_THIRD_PARTY_INCLUDES_START
	//#include <cstdint>
	#include <assert.h>
	#include <time.h>
CZ_THIRD_PARTY_INCLUDES_END


CZ_THIRD_PARTY_INCLUDES_START
	#include "utf8.h"
	#include "utf8/unchecked.h"
CZ_THIRD_PARTY_INCLUDES_END

#if CZ_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <strsafe.h>
	#include <Psapi.h>

	#ifdef max
		#undef max
	#endif

	#ifdef min	
		#undef min
	#endif
#endif

