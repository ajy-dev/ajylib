add_executable(echo_server)

target_sources(
	echo_server
	PRIVATE
		main.cpp
		echo_server.cpp
)

target_link_libraries(
	echo_server
	PRIVATE
		ajylib
		ajylib_build_options
)