
set(MODULE_FILES
	"crazygaze/core/Algorithm.ixx"
	"crazygaze/core/AsyncCommandQueue.cpp"
	"crazygaze/core/AsyncCommandQueue.ixx"
	"crazygaze/core/Common.ixx"
	"crazygaze/core/czcore.ixx"
	"crazygaze/core/Logging.cpp"
	"crazygaze/core/Logging.ixx"
	"crazygaze/core/LogOutputs.cpp"
	"crazygaze/core/LogOutputs.ixx"
	"crazygaze/core/Math.cpp"
	"crazygaze/core/Math.ixx"
	"crazygaze/core/SharedQueue.ixx"
	"crazygaze/core/Singleton.ixx"
	"crazygaze/core/StringUtils.cpp"
	"crazygaze/core/StringUtils.ixx"
	"crazygaze/core/Threading.cpp"
	"crazygaze/core/Threading.ixx"
)

set(OTHER_FILES
	"crazygaze/core/czcore.natvis"
	"crazygaze/core/czcorePch.h"
	"crazygaze/core/Common_Macros.h"
	"crazygaze/core/Logging_Macros.h"
)

set(ALL_FILES
	${MODULE_FILES}
	${OTHER_FILES}
)

if (0)
	if(WIN32)
		list(APPEND ALL_FILES
			"crazygaze/core/Windows/Win32Event.cpp"
			"crazygaze/core/Windows/Win32Event.h"
		)
	endif()
endif()


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

