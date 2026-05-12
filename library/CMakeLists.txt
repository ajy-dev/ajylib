add_library(
	ajylib
	STATIC
)

target_sources(
	ajylib
	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
			${CMAKE_CURRENT_SOURCE_DIR}/include
		FILES
)

if(MSVC)
	target_compile_options(
		ajylib
		PRIVATE
			/W4
			/sdl-
			/permissive-
			/source-charset:utf-8
			/execution-charset:utf-8
	)
else()
	target_compile_options(
		ajylib
		PRIVATE
			-Wall
			-Wextra
			-finput-charset=UTF-8
			-fexec-charset=UTF-8
	)
endif()