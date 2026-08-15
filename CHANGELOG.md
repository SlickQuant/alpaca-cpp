# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added — Phase 1: foundation and Trading API v2

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
