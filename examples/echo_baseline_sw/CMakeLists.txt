add_executable(echo_baseline_sw)

target_sources(
	echo_baseline_sw
	PRIVATE
		main.cpp
		echo_baseline_sw_server.cpp
		send_worker_pool.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_baseline_sw
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_baseline_sw
	PRIVATE
		ajylib
		ajylib_build_options
)
