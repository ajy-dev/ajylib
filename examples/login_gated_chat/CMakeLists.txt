# --- hiredis ---
include(FetchContent)

FetchContent_Declare(
	hiredis
	GIT_REPOSITORY https://github.com/redis/hiredis.git
	GIT_TAG v1.4.0
	GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF)
set(DISABLE_TESTS ON)
set(ENABLE_SSL OFF)
set(ENABLE_EXAMPLES OFF)

FetchContent_MakeAvailable(hiredis)

# --- MySQL Connector/C install location (adjust per machine) ---
set(MYSQL_DIR "C:/Program Files/MySQL/MySQL Server 8.0")

# ==== chat server (sector-based chat; login-gated via Redis + SS) ====

add_executable(chat_server)

target_sources(
	chat_server
	PRIVATE
		chat_server/main.cpp
		chat_server/chat_server.cpp
		chat_server/login_link.cpp
		chat_server/monitor_reporter.cpp
)

target_include_directories(
	chat_server
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(
	chat_server
	PRIVATE
		ajylib
		ajylib_build_options
		hiredis::hiredis
)

set_source_files_properties(
	chat_server/chat_server.cpp
	chat_server/login_link.cpp
	PROPERTIES
	COMPILE_OPTIONS
		$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
)

# ==== login server (shard-worker pool; DB + Redis + SS) ====

add_executable(login_server)

target_sources(
	login_server
	PRIVATE
		login_server/main.cpp
		login_server/login_server.cpp
		login_server/content_link.cpp
		login_server/monitor_reporter.cpp
)

target_include_directories(
	login_server
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/..
		"${MYSQL_DIR}/include"
)

target_link_libraries(
	login_server
	PRIVATE
		ajylib
		ajylib_build_options
		hiredis::hiredis
		"${MYSQL_DIR}/lib/libmysql.lib"
)

set_source_files_properties(
	login_server/login_server.cpp
	PROPERTIES
	COMPILE_OPTIONS
		$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
)
# Copy libmysql.dll next to login_server.exe (dynamic MySQL C API).
add_custom_command(
	TARGET login_server POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${MYSQL_DIR}/lib/libmysql.dll"
	"$<TARGET_FILE_DIR:login_server>"
)