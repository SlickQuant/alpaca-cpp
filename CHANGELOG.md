# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-10-17

Adds the option data streams as a second library, `alpaca-options-streaming-cpp`, and moves
the SDK onto slick-net 4.

### Added

**`alpaca-options-streaming-cpp`**

- `option_data_stream` — real-time option trades and quotes on the `indicative` and `opra`
  feeds. Alpaca serves these in MessagePack only, which is why they live in their own
  library: msgpack-cxx is a private, header-only dependency of it and never reaches the base
  SDK or anything that links the library.
- Same interface and behaviour as the JSON streams: `on_trade` / `on_quote` handlers,
  `subscribe_*` / `unsubscribe_*`, reconnect with re-authentication and subscription replay,
  and the same `on_error` / `on_subscription` control handling. Trades and quotes arrive as
  the `alpaca::trade` and `alpaca::quote` the REST option methods return.
- Frames are decoded with a SAX parser straight into the handler arguments, with no
  intermediate document. The decoder's parse state and the `trade`, `quote` and symbol string
  handed to handlers are reused from message to message, so the message path stops
  allocating once warm — **handlers must copy whatever has to outlive the callback.**
- Timestamps accept all three MessagePack timestamp encodings as well as RFC-3339 text;
  unknown keys and nested values are skipped, so fields Alpaca adds later are harmless.
- No `Content-Type: application/msgpack` header is sent on the websocket upgrade, although
  Alpaca's documentation asks for one: slick-net cannot add headers to that request, and the
  option endpoints answer in MessagePack when no `Content-Type` is given. A JSON text frame is
  routed to the JSON path, so if that default ever changed, Alpaca's refusal would surface
  through `on_error` instead of as an undecodable frame.
- `stream_option_data` example.
- 38 offline tests — packed frames, a byte-for-byte hand-encoded frame, and control and data
  frames captured from Alpaca's servers — plus 5 integration tests against the `indicative`
  feed covering the handshake, rejected credentials, the subscription echo, unsubscribe and
  the 413 refusal of wildcard quote subscriptions. None of them need the market to be open.

**Core**

- `data_client::env()` and `data_client_awaitable::env()`, matching `trading_client::env()`.
  The environment was already stored but unreadable, which is also what made Clang warn
  about an unused private field.
- `detail::stream_base` extension points for streams that do not speak JSON: virtual
  `handle_frame`, `encode` and `auth_frame`, and an outbound `frame_type` (text or binary)
  taken by the constructor and reported by `outbound_frame_type()`. The JSON streams keep
  the defaults and behave exactly as before.

### Changed

- **slick-net 4.1.1 is required** (was 3.1.0), both by the build and by the installed
  `alpaca-cppConfig.cmake`. Three of its changes reach applications through this SDK:
  - TLS certificates are verified on every `https://` and `wss://` connection. slick-net
    3.x did not verify them; a TLS-intercepting proxy or a private CA now has to be trusted
    explicitly.
  - slick-net no longer installs process-wide `SIGINT` / `SIGTERM` handlers. An application
    that relied on Ctrl-C shutting down its streams has to handle the signal itself, as the
    examples do.
  - Release archives are no longer compiled with `-march=native` unless slick-net is built
    with `SLICK_NET_ENABLE_NATIVE_ARCH=ON`, so a prebuilt archive runs on any CPU again.
- With slick-net 4.1.1 the vcpkg ports tree has to be reasonably current. One older than
  slick-stream-buffer-multiplexer 2.2.0 fails configure with
  `cannot create ALIAS target "slick::slick-queue" because target "slick::queue" is itself an
  ALIAS`: slick-net fetches the multiplexer and the slick-queue 2.3.0 it needs, then its own
  unversioned `find_package(slick-queue)` loads vcpkg's older slick-queue too. Updating vcpkg
  fixes it; the README gives a flag-only workaround.
- The test suite requires slick-logger 2.3.0 (was 1.1.0).
- `BUILD_ALPACA_OPTIONS_STREAMING` defaults to `ON`, so a default build now needs msgpack-cxx
  (vcpkg `msgpack`, with a `FetchContent` fallback). Set it to `OFF` to build without it.
- CI builds and tests the options-streaming library on every job, and its install-and-consume
  job links the installed `alpaca-options-streaming-cpp` without looking for msgpack. Release
  archives include the library.
- CI and the release workflow install their dependencies through one composite action,
  `.github/actions/install-vcpkg-dependencies`. It moves the runner's preinstalled vcpkg to a
  pinned ports commit and bootstraps the matching tool before installing, because the
  images' own vcpkg was old enough to hit the slick-queue configure failure above. The action
  holds the package list and the binary-cache key too, so every job resolves the same
  versions and bumping the pin starts a fresh cache.

### Fixed

- Linux Clang builds no longer fail to link `slick::net::Websocket`'s constructor. vcpkg
  builds `libslick-net.a` with the system GCC, and GCC and Clang mangle the constructor's
  trailing `requires std::default_initializable<BufferT>` into different symbol names, so a
  Clang consumer of that archive is left with an undefined reference. The CI Clang job now
  builds slick-net from source with the same compiler
  (`-DCMAKE_DISABLE_FIND_PACKAGE_slick-net=ON`); the same flag is the fix for anyone
  building with Clang against a GCC-built slick-net, and it is documented in the README.
- The Linux Release jobs no longer abort with `SIGILL` in every test that constructs a
  stream. slick-net 3.1.0's `CMakeLists.txt` set `CMAKE_CXX_FLAGS_RELEASE` to
  `-O3 -march=native`, so vcpkg's `libslick-net.a` carried whatever instruction set the runner
  that built it happened to have; vcpkg's ABI hash does not cover the host CPU, so the
  workflow's archive cache replayed that build on runners with a narrower one and the
  `Websocket` constructor — the first frame outside this SDK's own objects — executed an
  unsupported instruction. Those jobs build slick-net from source with
  `-DCMAKE_DISABLE_FIND_PACKAGE_slick-net=ON`, and slick-net 4.x, now the minimum, no longer
  enables `-march=native` by default.

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

**Portability**

- The coroutine watchlist methods build their JSON body into a named local rather than
  passing an inline `json{{...}}` argument. An initializer_list temporary whose backing
  array must survive a `co_await` crashes GCC 11 and 13 with an internal compiler error in
  `morph_fn_to_coro`; GCC 14 compiles it. Both affected versions are inside the supported
  range, so the SDK avoids the shape rather than raising its minimum compiler.
- The test build probes whether `_Float128` is usable when `__SIZEOF_FLOAT128__` is
  defined, and defines it to `__float128` when it is not. Clang sets that macro without
  declaring the type, which makes a `std::format` visitor branch in slick-logger ill-formed
  and breaks the test suite under Clang. The probe tests the condition rather than the
  compiler id, so it lapses on its own once either side changes.

**Tests and examples**

- 213 offline unit tests needing no credentials and no network, plus 49 integration tests:
  read-only market data on the IEX feed, streaming pinned to the test feed, and paper-only
  trading, each skipped as a group with a diagnostic when credentials cannot reach them.
- Four examples: `trading_overview`, `place_and_cancel_order`, `market_data_overview` and
  `stream_market_data`.

[Unreleased]: https://github.com/SlickQuant/alpaca-cpp/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/SlickQuant/alpaca-cpp/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/SlickQuant/alpaca-cpp/releases/tag/v0.1.0
