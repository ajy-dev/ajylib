add_executable(echo_sendintent)

target_sources(
	echo_sendintent
	PRIVATE
		main.cpp
		echo_sendintent_server.cpp
		auth_group.cpp
		echo_group.cpp
		account_store.cpp
		send_worker_pool.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_sendintent
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_sendintent
	PRIVATE
		ajylib
		ajylib_build_options
)
