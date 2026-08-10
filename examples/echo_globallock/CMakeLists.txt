add_executable(echo_globallock)

target_sources(
	echo_globallock
	PRIVATE
		main.cpp
		echo_globallock_server.cpp
		send_worker_pool.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_globallock
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_globallock
	PRIVATE
		ajylib
		ajylib_build_options
)
