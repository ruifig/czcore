cmake_minimum_required(VERSION 4.0)

project(czcore)

include(FetchContent)
include(CMake/utils.cmake)

# By default, tests are only enabled if we are the top level project
option(CZCORE_ENABLE_TESTS "Generate czcore unit tests targets" ${PROJECT_IS_TOP_LEVEL})

FetchContent_Declare(utfcpp
	GIT_REPOSITORY https://github.com/nemtrif/utfcpp
	GIT_TAG f9319195dfddf369f68f18e7c0039b3f351797fd # v4.0.8
)
FetchContent_MakeAvailable(utfcpp)

add_subdirectory(source)

if (CZCORE_ENABLE_TESTS)

	FetchContent_Declare(Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG b3fb4b9feafcd8d91c5cb510a4775143fdbef02f # Release v3.11.0
	)

	FetchContent_MakeAvailable(Catch2)

	add_subdirectory(./UnitTests)
endif()

