# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-08-18

Initial release: a C++20 SDK for the Alpaca Trading API v2, the Market Data API, and every
JSON websocket stream. Every client comes in blocking and coroutine form
(`trading_client` / `trading_client_awaitable`) with identical method sets and semantics.

### Added

**Trading API v2**

- Account, account configurations, portfolio history, account activities, orders, positions,
  assets, option contracts, option exercise, watchlists, market calendar and clock, crypto
  funding, and short locates.
- Market, limit, stop, stop-limit and trailing-stop orders in `simple` / `bracket` / `oco` /
  `oto` / `mleg` classes, plus notional orders and factory functions for the common shapes.
  Callers work in doubles; the SDK handles Alpaca's string-typed numerics and never emits
  scientific notation.

**Market Data API**

- Stock bars/trades/quotes/auctions/snapshots and their `latest*` forms, condition and
  exchange maps, crypto bars/trades/quotes/snapshots/order books, options bars/trades/latest
  quotes/snapshots and the option chain, news, most-actives and movers screeners, corporate
  actions, forex rates, fixed income, and logos.
- `alpaca::timeframe` with the documented intervals and a `valid()` check against Alpaca's
  per-unit limits.
- Historical methods follow `next_page_token` to completion and merge pages **per symbol**;
  a `_page` twin returns a single page for open-ended windows.
- Corporate actions are flattened from Alpaca's fifteen per-type arrays into one vector
  tagged with a `corporate_action_type`.

**Streaming**

- `stock_data_stream`, `crypto_data_stream`, `news_stream` and `trade_update_stream`.
- Messages are delivered by callback on the websocket thread with no intermediate queue, so
  the message path takes no lock and allocates nothing beyond the parse. The tradeoff is a
  contract the SDK cannot enforce: **handlers must be registered before `connect()`, and must
  not block.**
- Trades, quotes, bars, order books and news arrive as the same REST models, so streamed data
  can be handed to code written against `trading_client`. Stream-only types
  (`trading_status`, `luld`, `trade_correction`, `trade_cancel_error`, `imbalance`) are new.
- Dropped connections reconnect with capped exponential backoff, then re-authenticate and
  re-subscribe from the recorded subscription set. Subscribing before `connect()` is
  supported and is the usual pattern.
- In-band `{"T":"error"}` frames reach `on_error` rather than raising; one arriving during the
  handshake fails `connect()` immediately, so rejected credentials return in milliseconds.
- `stock_data_stream::test_feed_url()` reaches the synthetic `v2/test` feed, the only way to
  exercise a stream outside market hours.

**Core**

- `alpaca::credentials` covering all three auth schemes (`APCA-API-*` headers, Broker HTTP
  Basic, OAuth bearer). `from_env(environment)` resolves paper and live separately —
  `environment::paper` prefers `APCA_PAPER_API_*` and falls back to the unprefixed pair — so
  both key sets can sit in the environment without either leaking into the other. Paper and
  live keys are not interchangeable.
- `detail::request_context` — the single path every REST call takes, owning URL joining, auth,
  rate limiting, retry with backoff, status checking, JSON parsing, error mapping and
  pagination, in both blocking and coroutine form.
- `alpaca::rate_limiter` — lock-free GCRA token bucket (compare-exchange on one atomic word,
  no mutex), defaulting to 200 requests/minute and honouring server-side 429s.
- `api_error`, `parse_error` and `config_error`. Non-2xx responses raise rather than returning
  an empty value, so "no results" and "request failed" are never ambiguous.
- Allocation-free RFC-3339 parsing and formatting, normalising every timestamp to nanoseconds
  since the Unix epoch.
- Enums generated from X-macro lists, mapping unrecognised wire values to `unknown` rather
  than throwing.
- Nullable numerics — limit prices, option greeks, streamed fill quantities — are
  `std::optional`, keeping "absent" distinct from a genuine zero.

**Build and packaging**

- The repo is structured for three independently installable libraries, each with its own
  export set and CMake package config so each can ship as a separate vcpkg port. **0.1.0 ships
  `alpaca-cpp` (core) only**; `alpaca-broker-cpp` and `alpaca-options-streaming-cpp` are
  wired into the build but not yet implemented.
- `BUILD_ALPACA_CORE` / `_BROKER` / `_OPTIONS_STREAMING` / `_TESTS` / `_EXAMPLES` options.
  With `BUILD_ALPACA_CORE=OFF` the satellites resolve core via `find_package`.
- msgpack stays confined to the options-streaming library, so it is not a dependency of the
  base SDK.

**Tests and examples**

- 213 offline unit tests needing no credentials and no network, plus 49 integration tests:
  read-only market data on the IEX feed, streaming pinned to the test feed, and paper-only
  trading, each skipped as a group with a diagnostic when credentials cannot reach them.
- Four examples: `trading_overview`, `place_and_cancel_order`, `market_data_overview` and
  `stream_market_data`.

[0.1.0]: https://github.com/SlickQuant/alpaca-cpp/releases/tag/v0.1.0
