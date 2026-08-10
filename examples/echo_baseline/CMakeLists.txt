add_executable(echo_baseline)

target_sources(
	echo_baseline
	PRIVATE
		main.cpp
		echo_baseline_server.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_baseline
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_baseline
	PRIVATE
		ajylib
		ajylib_build_options
)
