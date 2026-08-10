add_executable(echo_with_group)

target_sources(
	echo_with_group
	PRIVATE
		main.cpp
		echo_with_group_server.cpp
		auth_group.cpp
		echo_group.cpp
		account_store.cpp
		monitor_reporter.cpp
)

target_include_directories(
	echo_with_group
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
	echo_with_group
	PRIVATE
		ajylib
		ajylib_build_options
)
