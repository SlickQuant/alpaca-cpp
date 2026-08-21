# alpaca-cpp

[![CI](https://github.com/SlickQuant/alpaca-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/SlickQuant/alpaca-cpp/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Static Library](https://img.shields.io/badge/library-static-brightgreen.svg)](#building)
[![Lock-free](https://img.shields.io/badge/concurrency-lock--free-orange.svg)](#rate-limiting)

A modern C++20 SDK for [alpaca.markets](https://alpaca.markets) — Trading API, Market Data API,
Broker API, and every streaming interface.

> **Status: Phase 3 of 6.** The Trading API (v2), the Market Data API and the JSON
> websocket streams are complete. The Broker library and the options-streaming library
> are being added in subsequent phases — see [Roadmap](#roadmap).

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
- [slick-net](https://github.com/SlickQuant/slick-net) 3.1.0, nlohmann/json, OpenSSL
- GTest and [slick-logger](https://github.com/SlickQuant/slick-logger) 1.1.0+ for the test suite only

All of these are public vcpkg ports, so one `vcpkg install` covers them — slick-net pulls
Boost and the remaining `slick-*` packages in transitively:

```bash
vcpkg install nlohmann-json openssl slick-net slick-logger
```

Each is looked up with `find_package(... CONFIG)` first. slick-net, slick-logger and GTest
fall back to `FetchContent` when not found, so the build still works without vcpkg — it
just clones and builds them from source instead.

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

The client defaults to **paper trading**. Trading with real money is always an explicit act:

```cpp
#include <alpaca/trading_client.hpp>

alpaca::trading_client paper;                                  // paper + env credentials
alpaca::trading_client live({}, alpaca::environment::live);    // live + env credentials
alpaca::trading_client explicit_creds({key, secret}, alpaca::environment::live);
```

Credentials left empty are resolved from the environment for the target environment:

| Environment | Variables read |
|---|---|
| `paper` | `APCA_PAPER_API_KEY_ID` / `APCA_PAPER_API_SECRET_KEY`, falling back to the unprefixed pair |
| `live`, `sandbox` | `APCA_API_KEY_ID` / `APCA_API_SECRET_KEY` |

> Paper API keys begin with `PK`, live keys with `AK`, and they are **not** interchangeable —
> a live key is rejected by `paper-api.alpaca.markets` with HTTP 401 (`40110000`). The two
> variable pairs let both live in the environment at once; a paper key is never used for a
> live client, or vice versa. If you only have one account, export just the unprefixed pair
> and paper falls back to it.

An explicitly passed `credentials` always wins over the environment. OAuth bearer tokens
work anywhere credentials do:

```cpp
alpaca::trading_client client(alpaca::credentials::from_oauth_token(token));
```

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

### Market data

`data_client` talks to `data.alpaca.markets`, which is not account-scoped — paper and live
keys both work. `iex` is the only feed available without a data subscription.

```cpp
#include <alpaca/data_client.hpp>

alpaca::data_client data;

alpaca::bar_query query;
query.symbols   = {"AAPL", "MSFT"};
query.timeframe = alpaca::timeframes::one_day;
query.start     = "2024-01-02";
query.end       = "2024-02-01";
query.feed      = alpaca::data_feed::iex;

// Follows next_page_token to completion and merges every page, per symbol.
auto bars = data.get_stock_bars(query);
for (const auto &b : bars["AAPL"]) {
    std::cout << alpaca::to_rfc3339(b.timestamp) << ' ' << b.close << '\n';
}

auto quote = data.get_latest_stock_quote("AAPL", alpaca::data_feed::iex);
auto snap  = data.get_stock_snapshot("AAPL", alpaca::data_feed::iex);
```

Crypto needs no subscription, and the venue is a path segment rather than a feed parameter:

```cpp
auto books = data.get_latest_crypto_orderbooks({"BTC/USD"});
std::cout << books["BTC/USD"].best_bid() << " / " << books["BTC/USD"].best_ask() << '\n';
```

Options snapshots carry greeks and implied volatility, both `std::optional` because Alpaca
omits them for contracts it cannot price:

```cpp
alpaca::option_chain_query chain;
chain.type = alpaca::contract_type::call;
chain.expiration_date = "2024-06-28";

for (const auto &[symbol, snap] : data.get_option_chain("AAPL", chain)) {
    if (snap.greeks) { std::cout << symbol << " delta " << snap.greeks->delta << '\n'; }
}
```

#### Paging large windows

Every historical method has a `_page` twin returning one page plus `next_page_token`. Use
the plain method for a bounded window, and the `_page` variant when the window is
open-ended and fetching everything could be unbounded:

```cpp
auto page = data.get_stock_bars_page(query);
while (!page.next_page_token.empty()) {
    query.page_token = page.next_page_token;
    page = data.get_stock_bars_page(query);
}
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

`data_client_awaitable` does the same for the Market Data API.

### Streaming

Four websocket streams: `stock_data_stream`, `crypto_data_stream`, `news_stream` and
`trade_update_stream`. Messages are delivered by **callback, on the websocket service
thread**, with no intermediate queue.

```cpp
#include <alpaca/streaming/stock_data_stream.hpp>

alpaca::stock_data_stream stream({}, alpaca::data_feed::iex);

stream.on_trade([](const std::string &symbol, const alpaca::trade &t) {
    std::cout << symbol << ' ' << t.price << " x " << t.size << '\n';
});
stream.on_bar([](const std::string &symbol, const alpaca::bar &b) { /* ... */ });

stream.subscribe_trades({"AAPL", "MSFT"});
stream.subscribe_bars({"AAPL"});

stream.connect();      // blocks until authenticated
```

Two rules follow from delivering on the service thread, and the SDK cannot enforce
either:

- **Register handlers before `connect()`.** They are read without a lock on the message
  path, so attaching one to a live stream is a data race.
- **Do not block inside a handler.** A slow handler stalls the socket and Alpaca will
  eventually drop the connection. Hand slow work to your own thread.

Trades, quotes, bars, order books and news articles arrive as the *same* types the REST
client returns — `alpaca::trade`, `quote`, `bar`, `orderbook`, `news_article` — because
the stream sends the same wire keys. The symbol is passed alongside rather than inside,
since those REST types are keyed by symbol at the map level. Only the payloads with no
REST equivalent (`trading_status`, `luld`, `trade_correction`, `trade_cancel_error`,
`imbalance`) are streaming-specific types.

Dropped connections are re-established, re-authenticated and re-subscribed automatically:

```cpp
stream.set_reconnect_policy({true, 500, 30000, 0});   // on, 0.5s→30s backoff, forever
stream.on_disconnected([] { std::cerr << "reconnecting\n"; });
stream.on_error([](const alpaca::stream_error &e) {   // an Alpaca {"T":"error"} frame
    std::cerr << e.code << ": " << e.message << '\n';
});
```

The account stream carries order lifecycle events, with the same `alpaca::order` the
REST API returns embedded in each one:

```cpp
#include <alpaca/streaming/trade_update_stream.hpp>

alpaca::trade_update_stream updates;      // paper by default

updates.on_trade_update([](const alpaca::trade_update &u) {
    std::cout << alpaca::to_string(u.event) << ' ' << u.order.symbol;
    if (u.price) { std::cout << " @ " << *u.price; }   // only set on executions
    std::cout << '\n';
});

updates.connect();
```

Alpaca's synthetic `test` feed streams `FAKEPACA` around the clock, which is the only way
to exercise a stream outside market hours:

```cpp
alpaca::stock_data_stream s({}, alpaca::stock_data_stream::test_feed_url());
s.subscribe_trades({"FAKEPACA"});
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

### Market Data API

| Group | Methods |
|---|---|
| Stocks — historical | `get_stock_bars`, `get_stock_trades`, `get_stock_quotes`, `get_stock_auctions` (each with a `_page` twin) |
| Stocks — latest | `get_latest_stock_bars`, `get_latest_stock_trades`, `get_latest_stock_quotes`, `get_stock_snapshots`, plus single-symbol wrappers |
| Stocks — reference | `get_stock_condition_codes`, `get_stock_exchange_codes` |
| Crypto | `get_crypto_bars`, `get_crypto_trades`, `get_crypto_quotes`, `get_latest_crypto_bars`/`_trades`/`_quotes`/`_orderbooks`, `get_crypto_snapshots` |
| Options | `get_option_bars`, `get_option_trades`, `get_latest_option_trades`, `get_latest_option_quotes`, `get_option_snapshots`, `get_option_chain`, `get_option_condition_codes`, `get_option_exchange_codes` |
| News | `get_news`, `get_news_page` |
| Screener | `get_most_actives`, `get_movers` |
| Corporate actions | `get_corporate_actions`, `get_corporate_actions_page` |
| Forex | `get_forex_rates`, `get_latest_forex_rates` |
| Fixed income | `get_latest_fixed_income_prices`, `get_latest_fixed_income_quotes` |
| Logos | `get_logo` (raw PNG bytes) |

Available on both `data_client` and `data_client_awaitable`. Timestamps are nanoseconds,
symbol-keyed responses come back as `std::unordered_map`, and pagination merges per symbol
rather than overwriting.

### Streams

| Stream | Channels |
|---|---|
| `stock_data_stream` | `trades`, `quotes`, `bars`, `updatedBars`, `dailyBars`, `statuses`, `lulds`, `imbalances`, plus corrections and cancel/error prints |
| `crypto_data_stream` | `trades`, `quotes`, `bars`, `updatedBars`, `dailyBars`, `orderbooks` |
| `news_stream` | `news` |
| `trade_update_stream` | account order lifecycle events |

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 1 | Foundation + Trading API v2 | ✅ complete |
| 2 | Market Data API (stocks, crypto, options REST, news, screener, corporate actions, forex) | ✅ complete |
| 3 | JSON streaming — trade updates, stocks, crypto, news | ✅ complete |
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
- **Integration tests** — skipped unless credentials are configured. The trading group needs
  **paper** credentials (`APCA_PAPER_API_KEY_ID` / `APCA_PAPER_API_SECRET_KEY`, or the
  unprefixed pair if that holds paper keys) and only ever talks to paper trading; orders are
  placed far from the market and cancelled in teardown. The market data group is read-only,
  pinned to the IEX feed, and runs with either paper or live keys since
  `data.alpaca.markets` is not account-scoped. The streaming group connects to the
  synthetic `v2/test` feed so it does not depend on the market being open. Each group
  probes once at start-up and skips as a whole with the reason if it cannot reach the
  API, rather than failing test by test.

```bash
./build/tests/alpaca_tests --gtest_filter=-*Integration*   # offline only
```

## Examples

Built with `-DBUILD_ALPACA_EXAMPLES=ON` into `build/examples/`.

| Example | What it does |
|---|---|
| `trading_overview` | Read-only: account, clock, positions, open orders. Places no orders. |
| `market_data_overview` | Read-only: stock snapshot, daily bars, a crypto order book, recent news. Free-tier endpoints only. |
| `stream_market_data` | Live trades, quotes and bars over websockets. Defaults to the synthetic test feed so it works outside market hours; pass symbols to use IEX. |
| `place_and_cancel_order` | Full order lifecycle on paper — submit a resting limit order, read it back, replace, cancel. Refuses to run against a live account. |

## License

MIT — see [LICENSE](LICENSE).

## Contributing

Contributions are welcome. Please open a pull request.
