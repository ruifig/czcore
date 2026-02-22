#pragma once

#include "CorePreSetup.h"

#include <assert.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <unordered_map>
#include <set>
#include <format>
#include <fstream>
#include <filesystem>
#include <variant>
#include <queue>
#include <condition_variable>
#include <string_view>
#include <span>
#include <stacktrace>
#include <numeric>

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

