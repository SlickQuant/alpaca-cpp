// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/data/bar.hpp>
#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;
    
/// One price level of a crypto order book.
struct orderbook_level {
    double price = 0.;      ///< `p`
    double size = 0.;       ///< `s`
};

inline void from_json(const json &j, orderbook_level &l) {
    DOUBLE_FROM_JSON_KEY(j, l, price, "p");
    DOUBLE_FROM_JSON_KEY(j, l, size, "s");
}

/// A crypto order book snapshot. `GET /v1beta3/crypto/{loc}/latest/orderbooks`.
/// Bids arrive descending by price and asks ascending, i.e. `bids[0]` and `asks[0]` are
/// the top of book.
struct orderbook {
    uint64_t timestamp = 0;                 ///< `t` — nanoseconds since the Unix epoch
    std::vector<orderbook_level> bids;      ///< `b`
    std::vector<orderbook_level> asks;      ///< `a`
    /// `r` — streams only. Marks the full snapshot Alpaca sends after a (re)connect, so
    /// a consumer maintaining its own book knows to discard what it had. The REST
    /// endpoint always answers a complete book, and leaves this false.
    bool reset = false;

    double best_bid() const noexcept { return bids.empty() ? 0. : bids.front().price; }
    double best_ask() const noexcept { return asks.empty() ? 0. : asks.front().price; }
    double spread() const noexcept { return best_ask() - best_bid(); }
};

inline void from_json(const json &j, orderbook &o) {
    TIMESTAMP_FROM_JSON_KEY(j, o, timestamp, "t");
    if (j.contains("b") && j["b"].is_array()) {
        o.bids = j["b"].get<std::vector<orderbook_level>>();
    }
    if (j.contains("a") && j["a"].is_array()) {
        o.asks = j["a"].get<std::vector<orderbook_level>>();
    }
    BOOL_FROM_JSON_KEY(j, o, reset, "r");
}

using orderbooks_by_symbol = symbol_map<orderbook>;

}   // namespace alpaca
