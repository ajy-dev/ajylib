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
)

target_link_libraries(
	ajylib_tests
	PRIVATE
		ajylib
		GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(ajylib_tests)