
set(MODULE_FILES
	"crazygaze/core/Algorithm.ixx"
	"crazygaze/core/AsyncCommandQueue.cpp"
	"crazygaze/core/AsyncCommandQueue.ixx"
	"crazygaze/core/CommandLine.cpp"
	"crazygaze/core/CommandLine.ixx"
	"crazygaze/core/Common.ixx"
	"crazygaze/core/czcore.ixx"
	"crazygaze/core/File.cpp"
	"crazygaze/core/File.ixx"
	"crazygaze/core/FixedHeapArray.ixx"
	"crazygaze/core/FNVHash.ixx"
	"crazygaze/core/Handles.ixx"
	"crazygaze/core/IniFile.cpp"
	"crazygaze/core/IniFile.ixx"
	"crazygaze/core/LinkedList.ixx"
	"crazygaze/core/Logging.cpp"
	"crazygaze/core/Logging.ixx"
	"crazygaze/core/LogOutputs.cpp"
	"crazygaze/core/LogOutputs.ixx"
	"crazygaze/core/Math.cpp"
	"crazygaze/core/Math.ixx"
	"crazygaze/core/Misc.cpp"
	"crazygaze/core/Misc.ixx"
	"crazygaze/core/PlatformUtils.cpp"
	"crazygaze/core/PlatformUtils.ixx"
	"crazygaze/core/PolyChunkVector.ixx"
	"crazygaze/core/ScopeGuard.ixx"
	"crazygaze/core/SharedPtr.ixx"
	"crazygaze/core/SharedQueue.ixx"
	"crazygaze/core/Singleton.ixx"
	"crazygaze/core/StringUtils.cpp"
	"crazygaze/core/StringUtils.ixx"
	"crazygaze/core/TaggedPtr.ixx"
	"crazygaze/core/Threading.cpp"
	"crazygaze/core/Threading.ixx"
	"crazygaze/core/VSOVector.ixx"
)

if(WIN32)
	list(APPEND MODULE_FILES
		"crazygaze/core/Windows/Win32Event.cpp"
		"crazygaze/core/Windows/Win32Event.ixx"
	)
endif()

set(OTHER_FILES
	"crazygaze/core/czcore.natvis"
	"crazygaze/core/czcorePch.h"
	"crazygaze/core/Common_Macros.h"
	"crazygaze/core/Logging_Macros.h"
	"crazygaze/core/Misc_Macros.h"
	"crazygaze/core/ScopeGuard_Macros.h"
)

set(ALL_FILES
	${MODULE_FILES}
	${OTHER_FILES}
)


add_library(czcore STATIC)

target_sources(czcore PUBLIC
	${OTHER_FILES}
)

target_sources(czcore PUBLIC
	FILE_SET czcoremodules TYPE CXX_MODULES
	FILES ${MODULE_FILES}
)

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
target_precompile_headers(czcore PUBLIC "crazygaze/core/czcorePch.h")

#
# Set some preprocessor macros depending on the build configurations
#
czAddConfigMacro(czcore "Debug"          "CZ_DEBUG"       "1")
czAddConfigMacro(czcore "Development"    "CZ_DEVELOPMENT" "1")
czAddConfigMacro(czcore "Release"        "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "MinSizeRel"     "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "RelWithDebInfo" "CZ_RELEASE"     "1")

