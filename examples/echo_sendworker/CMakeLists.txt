add_executable(echo_sendworker)

target_sources(
	echo_sendworker
	PRIVATE
		main.cpp
		echo_sendworker_server.cpp
		auth_group.cpp
		echo_group.cpp
		account_store.cpp
		send_worker_pool.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_sendworker
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_sendworker
	PRIVATE
		ajylib
		ajylib_build_options
)
