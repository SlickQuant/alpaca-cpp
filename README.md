# alpaca-cpp

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Static Library](https://img.shields.io/badge/library-static-brightgreen.svg)](#building)
[![Lock-free](https://img.shields.io/badge/concurrency-lock--free-orange.svg)](#rate-limiting)

A modern C++20 SDK for [alpaca.markets](https://alpaca.markets) — Trading API, Market Data API,
Broker API, and every streaming interface.

> **Status: Phase 1 of 6.** The Trading API (v2) is complete with both synchronous and
> coroutine clients. Market Data, streaming, the Broker library and the options-streaming
> library are being added in subsequent phases — see [Roadmap](#roadmap).

## Three libraries, one repo

The SDK ships as three independently buildable libraries so the base package stays lean and
each can become its own vcpkg port. In particular, **msgpack is never a dependency of the
base SDK** — it is needed only by the options streams, which use a binary-only wire format.

| Target | Contents | Extra dependencies |
|---|---|---|
| `slick::alpaca-cpp` | Trading API v2, Market Data API (including options REST), and all JSON streams | — |
| `slick::alpaca-broker-cpp` | Broker API v1 REST + `/v1/events/*` SSE | `alpaca-cpp` |
| `slick::alpaca-options-streaming-cpp` | `opra` / `indicative` option data streams | `alpaca-cpp`, `msgpack` |

```
alpaca-cpp/
├── core/               → alpaca-cpp
├── broker/             → alpaca-broker-cpp                 (Phase 5)
├── options-streaming/  → alpaca-options-streaming-cpp      (Phase 4)
├── cmake/              → one package config per library
├── tests/  examples/
```

Each library has its own install/export set and package config, so a consumer can depend on
exactly the ones it needs.

## Installation

### Prerequisites

- C++20 compiler (MSVC 2022, GCC 11+, Clang 14+)
- CMake 3.21+
- [slick-net](https://github.com/SlickQuant/slick-net) 3.1.0 — fetched automatically if not installed
- nlohmann/json, OpenSSL
- GTest and [slick-logger](https://github.com/SlickQuant/slick-logger) for the test suite only

### Building

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 16
cmake --install build --prefix /your/prefix
```

On Windows with Ninja, run both commands from a Developer Command Prompt (or chain
`vcvars64.bat`) so `cl.exe` is on the path.

### Build options

Every package is switchable, so a vcpkg port builds exactly one library and resolves the
rest through `find_package`:

| Option | Default | Effect |
|---|---|---|
| `BUILD_ALPACA_CORE` | `ON` | Build `alpaca-cpp` |
| `BUILD_ALPACA_BROKER` | `OFF` | Build `alpaca-broker-cpp` |
| `BUILD_ALPACA_OPTIONS_STREAMING` | `OFF` | Build `alpaca-options-streaming-cpp` |
| `BUILD_ALPACA_TESTS` | top-level | Tests for the enabled packages |
| `BUILD_ALPACA_EXAMPLES` | top-level | Examples for the enabled packages |

Turning `BUILD_ALPACA_CORE=OFF` makes the satellite libraries resolve core via
`find_package(alpaca-cpp CONFIG REQUIRED)` instead of the in-tree target — the exact
configuration their vcpkg ports use.

### Consuming it

```cmake
find_package(alpaca-cpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE slick::alpaca-cpp)
```

## Usage

### Credentials and environments

The client defaults to **paper trading** and reads `APCA_API_KEY_ID` / `APCA_API_SECRET_KEY`
from the environment. Trading with real money is always an explicit act:

```cpp
#include <alpaca/trading_client.hpp>

alpaca::trading_client paper;                                  // paper + env credentials
alpaca::trading_client explicit_creds({key, secret});          // paper, explicit credentials
alpaca::trading_client live({key, secret}, alpaca::environment::live);
```

OAuth bearer tokens work anywhere credentials do:

```cpp
alpaca::trading_client client(alpaca::credentials::from_oauth_token(token));
```

> Paper API keys begin with `PK`, live keys with `AK`. Live keys are rejected by
> `paper-api.alpaca.markets` with HTTP 401 (`40110000`).

### Errors are raised, never swallowed

Every method throws `alpaca::api_error` on a non-2xx response, so an empty result always
means "the account really has none of these" and never "the request failed":

```cpp
try {
    auto positions = client.list_positions();   // empty vector == genuinely no positions
}
catch (const alpaca::api_error &e) {
    if (e.is_unauthorized()) { /* 401/403 */ }
    if (e.is_rate_limited()) { /* 429 */ }
    std::cerr << e.http_status << ' ' << e.code << ' ' << e.message << '\n';
}
```

### Trading

```cpp
auto account = client.get_account();
std::cout << account.equity << ' ' << account.buying_power << '\n';

// Orders are built with doubles; the SDK handles Alpaca's string-typed wire format.
auto order = client.submit_order(
    alpaca::order_request::limit("AAPL", 10, alpaca::order_side::buy, 187.50,
                                 alpaca::time_in_force::gtc));

auto bracket = client.submit_order(alpaca::order_request::bracket(
    alpaca::order_request::limit("AAPL", 10, alpaca::order_side::buy, 187.50,
                                 alpaca::time_in_force::gtc),
    195.00,    // take profit
    180.00));  // stop loss

client.cancel_order(order.id);

auto positions = client.list_positions();
client.close_position("AAPL");
```

Multi-leg options orders carry symbol and side per leg:

```cpp
auto spread = client.submit_order(alpaca::order_request::multi_leg({
    {"AAPL240628C00200000", 1, alpaca::order_side::buy,  alpaca::position_intent::buy_to_open},
    {"AAPL240628C00210000", 1, alpaca::order_side::sell, alpaca::position_intent::sell_to_open},
}, 1));
```

### Coroutine client

`trading_client_awaitable` mirrors every `trading_client` method, returning
`asio::awaitable<T>` with identical semantics — including throwing `api_error`:

```cpp
#include <alpaca/trading_client_awaitable.hpp>

alpaca::trading_client_awaitable client;

asio::awaitable<void> run() {
    auto account   = co_await client.get_account();
    auto positions = co_await client.list_positions();
    auto order     = co_await client.submit_order(
        alpaca::order_request::market("AAPL", 1, alpaca::order_side::buy));
}
```

### Timestamps

Every timestamp is normalised to **nanoseconds since the Unix epoch** (`uint64_t`), parsed
from Alpaca's RFC-3339 strings with an allocation-free parser. `0` means the event has not
happened — an unfilled order has `filled_at == 0`.

```cpp
uint64_t ns = alpaca::to_nanoseconds("2024-01-02T03:04:05.123456789Z");
std::string iso = alpaca::to_rfc3339(ns);
```

Fields Alpaca genuinely leaves null are `std::optional`, so "no limit price" stays distinct
from "a limit price of zero":

```cpp
if (order.limit_price) { use(*order.limit_price); }
```

### Rate limiting

Each client carries a lock-free GCRA token bucket (200 requests/minute by default, matching
the free plan). Admission is a compare-exchange on a single atomic word, so concurrent
callers never serialise on a mutex. A server-side 429 is honoured automatically, and
retryable failures (429, 5xx) are retried with exponential backoff.

```cpp
client.limiter().set_rate(1000);              // paid plan
client.set_retry_policy({.max_attempts = 5}); // or {1, 0, 0} to disable retrying
```

### Logging

The SDK logs through slick-net's hooks. Install a handler with
`slick::net::set_log_handler()`, or bridge into `slick-logger`:

```cpp
#include <slick/logger.hpp>
#include <slick/net/logging.hpp>

auto &logger = slick::logger::Logger::instance();
logger.add_console_sink();
logger.init(1024, 16 * 1024 * 1024);

slick::net::set_log_handler(
    [&logger](slick::net::LogLevel level, const char *fmt, std::format_args args) {
        logger.log(static_cast<slick::logger::LogLevel>(level), fmt, args);
    },
    [] { return static_cast<slick::net::LogLevel>(
             slick::logger::Logger::instance().get_level()); });
```

## Implemented endpoints

### Trading API (v2)

| Group | Methods |
|---|---|
| Account | `get_account`, `get_account_configurations`, `update_account_configurations`, `get_portfolio_history`, `get_activities`, `get_activities_by_type` |
| Orders | `submit_order`, `list_orders`, `get_order`, `get_order_by_client_order_id`, `replace_order`, `cancel_order`, `cancel_all_orders` |
| Positions | `list_positions`, `get_position`, `close_position`, `close_all_positions`, `exercise_option_position`, `do_not_exercise_option_position` |
| Assets | `list_assets`, `get_asset` |
| Option contracts | `list_option_contracts`, `list_option_contracts_page`, `get_option_contract` |
| Watchlists | `list_watchlists`, `create_watchlist`, `get_watchlist`, `get_watchlist_by_name`, `update_watchlist`, `update_watchlist_by_name`, `add_asset_to_watchlist`, `add_asset_to_watchlist_by_name`, `remove_asset_from_watchlist`, `delete_watchlist`, `delete_watchlist_by_name` |
| Calendar | `get_calendar`, `get_clock` |
| Crypto funding | `list_crypto_wallets`, `list_crypto_transfers`, `get_crypto_transfer`, `request_crypto_withdrawal`, `get_crypto_transfer_estimate`, `list_whitelisted_addresses`, `create_whitelisted_address`, `delete_whitelisted_address` |
| Short locates | `list_locates`, `list_locates_page`, `create_locate`, `get_locate`, `get_locate_quotes` |

Order types: market, limit, stop, stop-limit, trailing stop (by price or percent), with
`simple` / `bracket` / `oco` / `oto` / `mleg` order classes and notional (dollar) orders.

All of the above are available on both `trading_client` and `trading_client_awaitable`.

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 1 | Foundation + Trading API v2 | ✅ complete |
| 2 | Market Data API (stocks, crypto, options REST, news, screener, corporate actions, forex) | planned |
| 3 | JSON streaming — trade updates, stocks, crypto, news | planned |
| 4 | `alpaca-options-streaming-cpp` — opra / indicative msgpack streams | planned |
| 5 | `alpaca-broker-cpp` — Broker API v1 + SSE events | planned |
| 6 | Examples, CI matrix, vcpkg ports | planned |

## Testing

```bash
cmake -S . -B build -DBUILD_ALPACA_TESTS=ON
cmake --build build -j 16
./build/tests/alpaca_tests
```

The suite has two tiers:

- **Offline unit tests** — run with no credentials and no network. They cover RFC-3339
  parsing edge cases, query-string encoding, number formatting, error mapping, rate-limiter
  accounting under concurrent threads, and `from_json` against captured Alpaca payloads.
- **Integration tests** — skipped unless `APCA_API_KEY_ID` / `APCA_API_SECRET_KEY` are set,
  and they only ever talk to **paper trading**. Orders are placed far from the market and
  cancelled in teardown. If the configured credentials cannot reach the paper account the
  whole group is skipped once with the reason, rather than failing test by test.

```bash
./build/tests/alpaca_tests --gtest_filter=-*Integration*   # offline only
```

## License

MIT — see [LICENSE](LICENSE).

## Contributing

Contributions are welcome. Please open a pull request.
