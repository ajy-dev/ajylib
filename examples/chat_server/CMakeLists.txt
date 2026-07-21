# --- single-thread build ---
add_executable(chat_server_single)

target_sources(
	chat_server_single
	PRIVATE
		single_thread/main.cpp
		single_thread/chat_server.cpp
		monitor_reporter.cpp
)

target_include_directories(
	chat_server_single
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(
	chat_server_single
	PRIVATE
		ajylib
		ajylib_build_options
)

set_source_files_properties(
	single_thread/chat_server.cpp
	PROPERTIES
	COMPILE_OPTIONS
		$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
)

# --- multi-thread build ---
add_executable(chat_server_multi)

target_sources(
	chat_server_multi
	PRIVATE
		multi_thread/main.cpp
		multi_thread/chat_server.cpp
		monitor_reporter.cpp
)

target_include_directories(
	chat_server_multi
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(
	chat_server_multi
	PRIVATE
		CHAT_SERVER_MULTI_THREAD
)

target_link_libraries(
	chat_server_multi
	PRIVATE
		ajylib
		ajylib_build_options
)

set_source_files_properties(
	multi_thread/chat_server.cpp
	PROPERTIES
	COMPILE_OPTIONS
		$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
)