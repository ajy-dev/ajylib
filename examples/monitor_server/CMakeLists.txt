add_executable(monitor_server)

target_sources(
	monitor_server
	PRIVATE
	main.cpp
	monitor_server.cpp
	local_sampler.cpp
	db_writer.cpp
)

target_include_directories(
	monitor_server
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/..
	"C:/Program Files/MySQL/MySQL Server 8.0/include"
)

# MySQL C client: link the import library for libmysql.dll (dynamic), which
# keeps this target on the project's static CRT (/MT) since the dll carries its
# own CRT behind the dll boundary. ws2_32 and Secur32 are required by the
# MySQL 8.0 C API on Windows.
target_link_libraries(
	monitor_server
	PRIVATE
	ajylib
	ajylib_build_options
	"C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.lib"
	ws2_32
	Secur32
)

set_source_files_properties(
	monitor_server.cpp
	local_sampler.cpp
	db_writer.cpp
	PROPERTIES
	COMPILE_OPTIONS
	$<$<CXX_COMPILER_ID:MSVC>:/wd4458>
)

# Copy libmysql.dll next to the exe after each build. The OpenSSL dlls it
# depends on (libssl / libcrypto, in the MySQL bin folder) are resolved via
# PATH, so only libmysql.dll needs copying here.
add_custom_command(
	TARGET monitor_server POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"
	"$<TARGET_FILE_DIR:monitor_server>"
)