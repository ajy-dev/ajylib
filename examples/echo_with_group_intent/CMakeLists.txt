add_executable(echo_with_group_intent)

target_sources(
	echo_with_group_intent
	PRIVATE
		main.cpp
		echo_with_group_intent_server.cpp
		auth_group.cpp
		echo_group.cpp
		account_store.cpp
		send_worker_pool.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_with_group_intent
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_with_group_intent
	PRIVATE
		ajylib
		ajylib_build_options
)
