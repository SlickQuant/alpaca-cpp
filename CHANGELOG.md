# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added — separate paper and live credentials

- `credentials::from_env(environment)` resolves the pair matching the target environment:
  `environment::paper` reads `APCA_PAPER_API_KEY_ID` / `APCA_PAPER_API_SECRET_KEY` (plus
  `APCA_PAPER_API_OAUTH_TOKEN`) and falls back to the unprefixed variables when they are
  unset, while live and sandbox always read the unprefixed pair. Paper and live keys are not
  interchangeable — a live key is rejected by `paper-api.alpaca.markets` with HTTP 401 — so
  this lets both sit in the environment at once without either leaking into the other.
- A half-configured paper pair (key set, secret missing) falls back rather than sending a
  request with an empty secret that would fail as a confusing 401.
- `credentials::resolve(creds, env)` fills in empty credentials from the environment and
  leaves anything explicitly supplied untouched.
- 7 further offline tests covering the precedence rules, using a scoped environment-variable
  helper so they assert against a known environment rather than the developer's own.

### Changed

- The client constructors take `credentials creds = {}` instead of eagerly calling
  `credentials::from_env()` in the default argument, and resolve against their environment
  in the constructor body — a default argument cannot see the `env` parameter, so the old
  form always read the live variables even when constructing a paper client. Passing
  credentials explicitly behaves exactly as before.

### Added — Market Data API

- `alpaca::data_client` and `alpaca::data_client_awaitable`, with identical method sets,
  covering stock bars/trades/quotes/auctions/snapshots and their `latest*` forms, condition
  and exchange code maps, crypto bars/trades/quotes/snapshots/order books, options
  bars/trades/latest quotes/snapshots and the option chain, news, the most-actives and
  movers screeners, corporate actions, forex rates, fixed income prices and quotes, and
  logos.
- `alpaca::timeframe` with the documented intervals as constants and a `valid()` check
  against Alpaca's per-unit limits (minutes 1-59, hours 1-23, 1 for day/week/month).
- Shared query bases (`history_query`, `bar_query`, `latest_query` and the crypto/option
  variants) so the symbols/window/paging/feed set is declared once.
- Every historical method follows `next_page_token` to completion and merges pages **per
  symbol**; a `_page` twin returns a single page for open-ended windows where fetching
  everything would be unbounded.
- Models normalise the Market Data API's single-letter wire keys to spelled-out fields, and
  absorb the places where the same key means different things — options send `c` as one
  condition string where stocks send an array, and on the auctions endpoint `c` is the
  closing-auction array entirely.
- Option greeks and implied volatility are `std::optional`: Alpaca omits them for contracts
  it cannot price, and a defaulted delta of 0 would be indistinguishable from a genuinely
  delta-neutral position.
- Corporate actions are flattened from Alpaca's fifteen per-type arrays into one vector
  tagged with a `corporate_action_type`, rather than fifteen parallel collections.
- `request_context::get_raw` / `async_get_raw` for endpoints that do not answer JSON; the
  logos endpoint serves PNG bytes. Status checking, rate limiting and retry are unchanged —
  only the parse step is skipped.
- 42 further offline tests plus a 20-test read-only market-data integration group pinned to
  the IEX feed, which runs with either paper or live keys.
- A `market_data_overview` example covering a stock snapshot, a bounded window of daily
  bars, a crypto order book and recent news — every call on it works on a free account.

### Added — foundation and Trading API v2

**Build system**
- Three independently buildable libraries in one repo: `alpaca-cpp` (core),
  `alpaca-broker-cpp`, and `alpaca-options-streaming-cpp`, each with its own install/export
  set and CMake package config so each can ship as a separate vcpkg port.
- `BUILD_ALPACA_CORE` / `_BROKER` / `_OPTIONS_STREAMING` / `_TESTS` / `_EXAMPLES` options.
  With `BUILD_ALPACA_CORE=OFF` the satellite libraries resolve core through
  `find_package(alpaca-cpp CONFIG REQUIRED)`.
- msgpack is confined to the options-streaming library and linked `PRIVATE`, so it is not a
  dependency of the base SDK nor of that library's consumers.

**Core**
- `alpaca::credentials` covering all three Alpaca authentication schemes: `APCA-API-*`
  headers, Broker HTTP Basic, and OAuth bearer tokens; defaults read from
  `APCA_API_KEY_ID` / `APCA_API_SECRET_KEY` / `APCA_API_OAUTH_TOKEN`.
- `alpaca::api_error`, `parse_error` and `config_error`. Non-2xx responses raise rather than
  returning an empty value, so "no results" and "request failed" are never ambiguous.
- `alpaca::detail::request_context` — the single path every REST call takes, owning URL
  joining, auth headers, rate limiting, retry with exponential backoff, status checking,
  JSON parsing, error mapping, and `next_page_token` pagination, in both blocking and
  coroutine form.
- `alpaca::rate_limiter` — lock-free GCRA token bucket (compare-exchange on one atomic word,
  no mutex), defaulting to Alpaca's 200 requests/minute and honouring server-side 429s.
- `alpaca::query_builder` — optional skipping, vector-to-CSV flattening, percent-encoding,
  and in-place `page_token` replacement for pagination.
- Allocation-free RFC-3339 parsing and formatting normalising every timestamp to nanoseconds
  since the Unix epoch; handles absent/milli/micro/nano fractions, `Z` and numeric UTC
  offsets, and bare dates.
- Enum definitions generated from X-macro lists, each with a wire-format `to_string` and a
  parser that maps unrecognised values to `unknown` rather than throwing.

**Trading API (v2)**
- `alpaca::trading_client` and `alpaca::trading_client_awaitable`, with identical method
  sets and semantics, covering account, account configurations, portfolio history, account
  activities, orders, positions, assets, option contracts, option exercise and
  do-not-exercise, watchlists (by id and by name), market calendar and clock, crypto funding
  wallets/transfers/whitelists, and short locates.
- Order types: market, limit, stop, stop-limit and trailing stop (price or percent), with
  `simple` / `bracket` / `oco` / `oto` / `mleg` order classes, notional orders, and factory
  functions for the common shapes. Callers work in doubles; the SDK handles Alpaca's
  string-typed numeric wire format and never emits scientific notation.
- Nullable numeric fields are modelled as `std::optional`, keeping "no limit price" distinct
  from "a limit price of zero".

**Tests**
- 117 offline unit tests that need no credentials and no network, covering timestamp parsing
  edge cases, query encoding, number formatting, error mapping, base64, concurrent
  rate-limiter accounting, and `from_json` against captured Alpaca payload shapes.
- Paper-only integration tests, skipped as a group with a diagnostic when the configured
  credentials cannot reach a paper account.

### Fixed

- `_WIN32_WINNT` is now a `PUBLIC` compile definition on the exported target rather than a
  build-tree-only `add_definitions()`. Without this, downstream consumers compiled the
  Boost.Asio headers reached through slick-net as Windows 7 while the library itself was
  built for Windows 10.
- `CMAKE_SUPPRESS_REGENERATION` is now scoped to the Visual Studio generator. Under Ninja it
  also removed the rule that regenerates `build.ninja`, so edits to `CMakeLists.txt` were
  silently ignored.
