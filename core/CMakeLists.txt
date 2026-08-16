add_library(alpaca-cpp STATIC
    src/utils.cpp
    src/auth.cpp
    src/rate_limiter.cpp
    src/detail/request_context.cpp
    src/data_client.cpp
    src/data_client_awaitable.cpp
    src/trading_client.cpp
    src/trading_client_awaitable.cpp
    src/streaming/stream_base.cpp
    src/streaming/data_stream_base.cpp
    src/streaming/stock_data_stream.cpp
    src/streaming/crypto_data_stream.cpp
    src/streaming/news_stream.cpp
    src/streaming/trade_update_stream.cpp
)
add_library(slick::alpaca-cpp ALIAS alpaca-cpp)

target_include_directories(alpaca-cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(alpaca-cpp PUBLIC cxx_std_20)

# Must be PUBLIC, not add_definitions(): Boost.Asio comes in through slick-net's public
# headers, so a consumer that compiles without this targets Windows 7 while the library
# was built for Windows 10 — the two then disagree on Asio's internals. PUBLIC propagates
# through the exported target to anything that links us.
if (WIN32)
    target_compile_definitions(alpaca-cpp PUBLIC _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(alpaca-cpp PUBLIC
    slick::net
    nlohmann_json::nlohmann_json
)
target_link_libraries(alpaca-cpp PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

# PRIVATE precompiled headers for faster static library compilation
target_precompile_headers(alpaca-cpp PRIVATE
    <nlohmann/json.hpp>
    <string>
    <vector>
    <memory>
    <optional>
    <coroutine>
)

alpaca_set_target_options(alpaca-cpp)

install(DIRECTORY include/ DESTINATION include)
alpaca_install_package(alpaca-cpp)
