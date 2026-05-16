include(FetchContent)

FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)

add_executable(
	ajylib_test
	placeholder_test.cpp
	container/ring_buffer_test.cpp
)

target_link_libraries(
	ajylib_test
	PRIVATE
		ajylib
		GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(ajylib_test)