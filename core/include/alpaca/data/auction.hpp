// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/data/bar.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// One opening or closing auction print.
struct auction_entry {
    uint64_t timestamp = 0;     ///< `t` — nanoseconds since the Unix epoch
    double price = 0.;          ///< `p`
    double size = 0.;           ///< `s`
    std::string exchange;       ///< `x`
    std::string condition;      ///< `c`
};

inline void from_json(const json &j, auction_entry &e) {
    TIMESTAMP_FROM_JSON_KEY(j, e, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, e, price, "p");
    DOUBLE_FROM_JSON_KEY(j, e, size, "s");
    VARIABLE_FROM_JSON_KEY(j, e, exchange, "x");
    VARIABLE_FROM_JSON_KEY(j, e, condition, "c");
}

/// A trading day's auctions. `GET /v2/stocks/auctions` (SIP feed only).
///
/// Note the wire keys collide with other models: here `c` is the *closing auctions array*,
/// not a condition code, and `d` is the trading date.
struct daily_auctions {
    std::string date;                       ///< `d` — YYYY-MM-DD
    uint64_t date_ns = 0;                   ///< `date` as nanoseconds since the Unix epoch
    std::vector<auction_entry> opening;     ///< `o`
    std::vector<auction_entry> closing;     ///< `c`
};

inline void from_json(const json &j, daily_auctions &a) {
    VARIABLE_FROM_JSON_KEY(j, a, date, "d");
    a.date_ns = date_to_nanoseconds(a.date);
    if (j.contains("o") && j["o"].is_array()) {
        a.opening = j["o"].get<std::vector<auction_entry>>();
    }
    if (j.contains("c") && j["c"].is_array()) {
        a.closing = j["c"].get<std::vector<auction_entry>>();
    }
}

using auctions_by_symbol = symbol_map<std::vector<daily_auctions>>;

}   // namespace alpaca
