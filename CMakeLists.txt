cmake_minimum_required(VERSION 4.2...4.3)

project(
	ajylib
	VERSION 0.1.0
	LANGUAGES CXX
)

if (PROJECT_IS_TOP_LEVEL)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_EXTENSIONS OFF)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	set(
		CACHE{CMAKE_CONFIGURATION_TYPES}
		TYPE STRING
		HELP "Available build configurations"
		FORCE
		VALUE
			Debug
			NoOptRelease
			Release
	)
	set(
		CACHE{CMAKE_BUILD_TYPE}
		TYPE STRING
		HELP "Default build configurations"
		VALUE
			NoOptRelease
	)
	set(
		CACHE{CMAKE_CXX_FLAGS_DEBUG}
		TYPE STRING
		FORCE
		VALUE
			""
	)
	set(
		CACHE{CMAKE_CXX_FLAGS_RELEASE}
		TYPE STRING
		FORCE
		VALUE
			""
	)
endif()

add_library(
	ajylib
	STATIC
)

target_sources(
	ajylib
	PRIVATE
		# source files here
		source/container/ring_buffer.cpp
		source/container/serialization_buffer.cpp
		source/io/stdio/output_device.cpp
		source/utility/logger.cpp
		$<$<PLATFORM_ID:Windows>:source/io/windows/output_device.cpp>
	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS include
		FILES
			# header files here
			include/ajy/container/ring_buffer.hpp
			include/ajy/container/serialization_buffer.hpp
			include/ajy/container/lockfree/stack.hpp
			include/ajy/container/lockfree/queue.hpp
			include/ajy/memory/concepts.hpp
			include/ajy/memory/shared_types.hpp
			include/ajy/memory/lockfree/memory_pool.hpp
			include/ajy/memory/threadlocal/memory_pool.hpp
			include/ajy/io/output_device.hpp
			include/ajy/io/stdio/output_device.hpp
			include/ajy/utility/logger.hpp
			$<$<PLATFORM_ID:Windows>:include/ajy/io/windows/output_device.hpp>
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
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_compile_options(
	ajylib
	PRIVATE
		# MSVC Common
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<CXX_COMPILER_ID:MSVC>:/permissive->
		$<$<CXX_COMPILER_ID:MSVC>:/sdl->
		$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>
		$<$<CXX_COMPILER_ID:MSVC>:/execution-charset:utf-8>
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
		$<$<CXX_COMPILER_ID:MSVC>:/Zi>
		$<$<CXX_COMPILER_ID:MSVC>:/MP>
		$<$<CXX_COMPILER_ID:MSVC>:/EHsc>
		$<$<CXX_COMPILER_ID:MSVC>:/fp:precise>

		# MSVC Debug
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od>
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Ob0>
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/RTC1>

		# MSVC Release
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Ob2>

		# MSVC NoOptRelease + Fallback
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>>:/Od>
		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>>:/Ob1>

		# Clang/GCC common
		$<$<CXX_COMPILER_ID:Clang,GNU>:-Wall>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-Wextra>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-Wpedantic>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-finput-charset=UTF-8>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-fexec-charset=UTF-8>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-g>
		$<$<CXX_COMPILER_ID:Clang,GNU>:-fexceptions>
		$<$<CXX_COMPILER_ID:Clang>:-ffp-model=precise>

		# Clang/GCC Debug
		$<$<AND:$<CXX_COMPILER_ID:Clang,GNU>,$<CONFIG:Debug>>:-O0>
		$<$<AND:$<CXX_COMPILER_ID:Clang,GNU>,$<CONFIG:Debug>>:-fno-inline>

		# Clang/GCC Release
		$<$<AND:$<CXX_COMPILER_ID:Clang,GNU>,$<CONFIG:Release>>:-O2>

		# Clang/GCC NoOptRelease + Fallback
		$<$<AND:$<CXX_COMPILER_ID:Clang,GNU>,$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>>:-O0>
		$<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>>:-finline-hint-functions>
)

target_compile_definitions(
	ajylib
	PRIVATE
		$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
		$<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

set_source_files_properties(
	source/container/ring_buffer.cpp
	source/io/windows/output_device.cpp
	PROPERTIES
	COMPILE_OPTIONS
	$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
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
	enable_testing()
	add_subdirectory(tests)
endif()

if(AJYLIB_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()