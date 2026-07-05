include(FetchContent)

set(INSTALL_GTEST OFF)

FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)

add_executable(ajylib_tests)

target_sources(
	ajylib_tests
	PRIVATE
		# test source files here
		container/ring_buffer_test.cpp
		container/serialization_buffer_test.cpp
		container/lockfree/stack_test.cpp
		container/lockfree/queue_test.cpp
		memory/lockfree/memory_pool_test.cpp
		memory/threadlocal/memory_pool_test.cpp
		io/stdio/output_device_test.cpp
		utility/logger_test.cpp
		network/protocol/net_packet_buffer_test.cpp
		network/protocol/obfuscator/obfuscator_test.cpp
		$<$<PLATFORM_ID:Windows>:io/windows/output_device_test.cpp>
)

target_link_libraries(
	ajylib_tests
	PRIVATE
		ajylib
		GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(ajylib_tests)