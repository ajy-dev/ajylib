cmake_minimum_required(VERSION 4.2...4.3)
project(
	ajylib
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
	CACHE{CMAKE_CONFIGURATION_TYPES}
	TYPE STRING
	HELP "Supported build configuration"
	VALUE
	Debug
	NoOptRelease
	Release
	FORCE
)

if(MSVC)
	set(CMAKE_CXX_FLAGS_NOOPTRELEASE "/Od /Ob1 /Zi")
else()
	set(CMAKE_CXX_FLAGS_NOOPTRELEASE "-O0 -g")
endif()

add_subdirectory(library)