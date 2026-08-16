find_package(GTest CONFIG QUIET)

include(FetchContent)

if (GTest_FOUND)
    message(STATUS "GTest found: ${GTest_VERSION}")
else()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    if (WIN32)
        # Prevent googletest from overriding the parent project's compiler/linker settings.
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(googletest)
endif()

find_package(slick-logger 1.1.0 CONFIG QUIET)
if (NOT slick-logger_FOUND)
    message(STATUS "Fetching slick-logger...")
    set(BUILD_SLICK_LOGGER_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SLICK_LOGGER_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SLICK_LOGGER_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        slick-logger
        GIT_REPOSITORY https://github.com/SlickQuant/slick-logger.git
        GIT_TAG v1.1.3
    )
    FetchContent_MakeAvailable(slick-logger)
endif()

include(GoogleTest)

# Each package gets its own test executable so it stays independently runnable, matching
# the per-package build options.
function(alpaca_add_test_target tgt)
    add_executable(${tgt} ${ARGN})
    target_link_libraries(${tgt} PRIVATE slick::net slick::logger GTest::gtest_main)
    alpaca_set_target_options(${tgt})
    set_target_properties(${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    # 30s is not enough when the build tree sits on a network drive: launching the test
    # binary to enumerate its cases intermittently overran it and failed the build with
    # MSB3073, long after every binary had already linked.
    gtest_discover_tests(${tgt} DISCOVERY_TIMEOUT 120)
endfunction()

if (BUILD_ALPACA_CORE)
    alpaca_add_test_target(alpaca_tests
        utils_tests.cpp
        rate_limiter_tests.cpp
        error_tests.cpp
        trading_model_tests.cpp
        order_request_tests.cpp
        trading_integration_tests.cpp
        data_model_tests.cpp
        data_query_tests.cpp
        data_integration_tests.cpp
        streaming_model_tests.cpp
        streaming_stream_tests.cpp
        streaming_integration_tests.cpp
    )
    target_link_libraries(alpaca_tests PRIVATE slick::alpaca-cpp)
endif()
