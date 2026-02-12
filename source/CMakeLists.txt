
set(ALL_FILES
	"crazygaze/core/Algorithm.h"
	"crazygaze/core/AsyncCommandQueue.cpp"
	"crazygaze/core/AsyncCommandQueue.h"
	"crazygaze/core/CommandLine.cpp"
	"crazygaze/core/CommandLine.h"
	"crazygaze/core/Common.h"
	"crazygaze/core/CorePch.h"
	"crazygaze/core/CorePreSetup.h"
	"crazygaze/core/File.cpp"
	"crazygaze/core/File.h"
	"crazygaze/core/FNVHash.h"
	"crazygaze/core/IniFile.cpp"
	"crazygaze/core/IniFile.h"
	"crazygaze/core/LinkedList.h"
	"crazygaze/core/Logging.cpp"
	"crazygaze/core/Logging.h"
	"crazygaze/core/LogOutputs.cpp"
	"crazygaze/core/LogOutputs.h"
	"crazygaze/core/Math.cpp"
	"crazygaze/core/Math.h"
	"crazygaze/core/Misc.cpp"
	"crazygaze/core/Misc.h"
	"crazygaze/core/PlatformUtils.cpp"
	"crazygaze/core/PlatformUtils.h"
	"crazygaze/core/PolyChunkVector.h"
	"crazygaze/core/ScopeGuard.h"
	"crazygaze/core/Semaphore.cpp"
	"crazygaze/core/Semaphore.h"
	"crazygaze/core/SharedPtr.h"
	"crazygaze/core/SharedQueue.h"
	"crazygaze/core/Singleton.h"
	"crazygaze/core/StringUtils.cpp"
	"crazygaze/core/StringUtils.h"
	"crazygaze/core/ThreadingUtils.h"
	"crazygaze/core/VSOVector.h"
)

if(WIN32)
	list(APPEND ALL_FILES
		"crazygaze/core/Windows/Win32Event.cpp"
		"crazygaze/core/Windows/Win32Event.h"
	)
endif()


add_library(czcore STATIC ${ALL_FILES})

target_link_libraries(czcore PUBLIC
	utf8cpp
	$<TARGET_NAME_IF_EXISTS:TracyClient>
)

target_include_directories(czcore PUBLIC
	"./"
)

source_group(
	TREE "${CMAKE_CURRENT_LIST_DIR}"
	PREFIX ""
	FILES ${ALL_FILES}
)

set_target_properties(czcore PROPERTIES CXX_STANDARD 26)
target_compile_options(czcore PRIVATE /Wall)
target_precompile_headers(czcore PUBLIC "crazygaze/core/CorePch.h")

#
# Set some preprocessor macros depending on the build configurations
#
czAddConfigMacro(czcore "Debug"          "CZ_DEBUG"       "1")
czAddConfigMacro(czcore "Development"    "CZ_DEVELOPMENT" "1")
czAddConfigMacro(czcore "Release"        "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "MinSizeRel"     "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "RelWithDebInfo" "CZ_RELEASE"     "1")

