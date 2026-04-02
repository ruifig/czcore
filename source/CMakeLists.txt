
set(MODULE_INTERFACES
	"crazygaze/core/Algorithm.ixx"
	"crazygaze/core/AsyncCommandQueue.ixx"
	"crazygaze/core/CommandLine.ixx"
	"crazygaze/core/Common.ixx"
	"crazygaze/core/czcore.ixx"
	"crazygaze/core/File.ixx"
	"crazygaze/core/FixedHeapArray.ixx"
	"crazygaze/core/FNVHash.ixx"
	"crazygaze/core/Handles.ixx"
	"crazygaze/core/IniFile.ixx"
	"crazygaze/core/LinkedList.ixx"
	"crazygaze/core/Logging.ixx"
	"crazygaze/core/LogOutputs.ixx"
	"crazygaze/core/Math.ixx"
	"crazygaze/core/Misc.ixx"
	"crazygaze/core/PlatformUtils.ixx"
	"crazygaze/core/PolyChunkVector.ixx"
	"crazygaze/core/ScopeGuard.ixx"
	"crazygaze/core/SharedPtr.ixx"
	"crazygaze/core/SharedQueue.ixx"
	"crazygaze/core/Singleton.ixx"
	"crazygaze/core/StringUtils.ixx"
	"crazygaze/core/TaggedPtr.ixx"
	"crazygaze/core/Threading.ixx"
	"crazygaze/core/VSOVector.ixx"
)

set(MODULE_IMPLEMENTATIONS
	"crazygaze/core/AsyncCommandQueue.cpp"
	"crazygaze/core/CommandLine.cpp"
	"crazygaze/core/File.cpp"
	"crazygaze/core/IniFile.cpp"
	"crazygaze/core/Logging.cpp"
	"crazygaze/core/LogOutputs.cpp"
	"crazygaze/core/Math.cpp"
	"crazygaze/core/Misc.cpp"
	"crazygaze/core/PlatformUtils.cpp"
	"crazygaze/core/StringUtils.cpp"
	"crazygaze/core/Threading.cpp"
)

if(WIN32)
	list(APPEND MODULE_INTERFACES
		"crazygaze/core/Windows/Win32Event.ixx"
	)
	list(APPEND MODULE_IMPLEMENTATIONS
		"crazygaze/core/Windows/Win32Event.cpp"
	)
endif()

set(OTHER_FILES
	"crazygaze/core/czcore.natvis"
	"crazygaze/core/czcore.h"
	"crazygaze/core/Common_Macros.h"
	"crazygaze/core/Logging_Macros.h"
	"crazygaze/core/Misc_Macros.h"
	"crazygaze/core/Profiler.h"
	"crazygaze/core/ScopeGuard_Macros.h"
)

set_property(SOURCE "crazygaze/core/czcore.h" PROPERTY LANGUAGE CXX)
set_property(SOURCE "crazygaze/core/czcore.h" PROPERTY HEADER_FILE_ONLY OFF)

set(ALL_FILES
	${MODULE_INTERFACES}
	${MODULE_IMPLEMENTATIONS}
	${OTHER_FILES}
)

add_library(czcore STATIC
	${ALL_FILES}
)

target_sources(czcore PUBLIC
	FILE_SET czcoremodules TYPE CXX_MODULES
	FILES ${MODULE_INTERFACES} ${MODULE_IMPLEMENTATIONS} 
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

#
# Set some preprocessor macros depending on the build configurations
#
czAddConfigMacro(czcore "Debug"          "CZ_DEBUG"       "1")
czAddConfigMacro(czcore "Development"    "CZ_DEVELOPMENT" "1")
czAddConfigMacro(czcore "Release"        "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "MinSizeRel"     "CZ_RELEASE"     "1")
czAddConfigMacro(czcore "RelWithDebInfo" "CZ_RELEASE"     "1")

