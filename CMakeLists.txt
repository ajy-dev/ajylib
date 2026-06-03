cmake_minimum_required(VERSION 3.23...4.3)

project(
	ajylib
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
	ajylib
	STATIC
)

target_sources(
	ajylib
	PRIVATE
		# source files here
	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS include
		FILES
			# header files here
)

target_compile_features(
	ajylib
	PUBLIC
		cxx_std_20
)

set_target_properties(
	ajylib
	PROPERTIES
		CXX_EXTENSIONS OFF
)

option(
	AJYLIB_BUILD_TESTS
	"Build tests"
	${PROJECT_IS_TOP_LEVEL}
)
option(
	AJYLIB_BUILD_EXAMPLES
	"Build examples"
	${PROJECT_IS_TOP_LEVEL}
)

if(AJYLIB_BUILD_TESTS)
	add_subdirectory(tests)
endif()

if(AJYLIB_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()