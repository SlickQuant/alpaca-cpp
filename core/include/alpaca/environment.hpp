// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace alpaca {

/// Which Alpaca deployment a client talks to.
enum class environment : uint8_t {
    paper,      ///< paper-api.alpaca.markets — simulated trading, real market data
    live,       ///< api.alpaca.markets — real money
    sandbox,    ///< *.sandbox.alpaca.markets — Broker API sandbox and sandbox data streams
};

inline std::string to_string(environment env) {
    switch (env) {
    case environment::paper:   return "paper";
    case environment::live:    return "live";
    case environment::sandbox: return "sandbox";
    }
    return "paper";
}

namespace urls {

// REST
inline constexpr std::string_view live_trading    = "https://api.alpaca.markets";
inline constexpr std::string_view paper_trading   = "https://paper-api.alpaca.markets";
inline constexpr std::string_view market_data     = "https://data.alpaca.markets";
inline constexpr std::string_view sandbox_data    = "https://data.sandbox.alpaca.markets";
inline constexpr std::string_view broker_live     = "https://broker-api.alpaca.markets";
inline constexpr std::string_view broker_sandbox  = "https://broker-api.sandbox.alpaca.markets";
inline constexpr std::string_view oauth_token     = "https://authx.alpaca.markets/v1/oauth2/token";

// Streams
inline constexpr std::string_view live_trade_stream   = "wss://api.alpaca.markets/stream";
inline constexpr std::string_view paper_trade_stream  = "wss://paper-api.alpaca.markets/stream";
inline constexpr std::string_view data_stream         = "wss://stream.data.alpaca.markets";
inline constexpr std::string_view sandbox_data_stream = "wss://stream.data.sandbox.alpaca.markets";

}   // namespace urls

/// Trading API base URL for an environment. `sandbox` has no trading endpoint of its own,
/// so it maps to paper.
inline std::string_view trading_base_url(environment env) {
    return env == environment::live ? urls::live_trading : urls::paper_trading;
}

/// Market Data API base URL for an environment.
inline std::string_view data_base_url(environment env) {
    return env == environment::sandbox ? urls::sandbox_data : urls::market_data;
}

/// Broker API base URL for an environment. Only `live` hits production.
inline std::string_view broker_base_url(environment env) {
    return env == environment::live ? urls::broker_live : urls::broker_sandbox;
}

/// Trade-updates websocket URL for an environment.
inline std::string_view trade_stream_url(environment env) {
    return env == environment::live ? urls::live_trade_stream : urls::paper_trade_stream;
}

/// Root of the market data websocket URLs for an environment; feed paths are appended to it.
inline std::string_view data_stream_root(environment env) {
    return env == environment::sandbox ? urls::sandbox_data_stream : urls::data_stream;
}

}   // namespace alpaca
