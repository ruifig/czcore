cmake_minimum_required(VERSION 4.0)

set(ALL_FILES
	"CoreUnitTests.cpp"
	"CoreUnitTestsPch.h"
	"FNVHashTests.cpp"
	"SharedPtrTests.cpp"
	"StringUtilsTests.cpp"
	"VSOVEctorTests.cpp"
)

if (WIN32)
	list(APPEND ALL_FILES
		"Win32EventTests.cpp"
	)
endif()

add_executable(czcore_UnitTests ${ALL_FILES})

source_group(
	TREE "${CMAKE_CURRENT_LIST_DIR}"
	PREFIX ""
	FILES ${ALL_FILES}
)

set_target_properties(czcore_UnitTests PROPERTIES CXX_STANDARD 26)
target_compile_options(czcore_UnitTests PRIVATE /Wall)
if(MSVC)
	target_compile_options(czcore_UnitTests PRIVATE
		/wd4868 # warning C4868: compiler may not enforce left-to-right evaluation order in braced initializer list
	)
endif()

target_link_libraries(czcore_UnitTests PUBLIC
	czcore
	Catch2
)

target_precompile_headers(czcore_UnitTests PRIVATE "CoreUnitTestsPch.h")

