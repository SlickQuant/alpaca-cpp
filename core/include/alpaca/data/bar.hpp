// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// An OHLCV aggregate.
///
/// The Market Data API uses single-letter keys and sends numbers as JSON numbers (unlike
/// the Trading API, which sends them as strings). The field names here are spelled out;
/// the wire keys are in the parser.
struct bar {
    uint64_t timestamp = 0;     ///< `t` — nanoseconds since the Unix epoch
    double open = 0.;           ///< `o`
    double high = 0.;           ///< `h`
    double low = 0.;            ///< `l`
    double close = 0.;          ///< `c`
    double volume = 0.;         ///< `v`
    double vwap = 0.;           ///< `vw` — volume-weighted average price
    uint64_t trade_count = 0;   ///< `n`
};

inline void from_json(const json &j, bar &b) {
    TIMESTAMP_FROM_JSON_KEY(j, b, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, b, open, "o");
    DOUBLE_FROM_JSON_KEY(j, b, high, "h");
    DOUBLE_FROM_JSON_KEY(j, b, low, "l");
    DOUBLE_FROM_JSON_KEY(j, b, close, "c");
    DOUBLE_FROM_JSON_KEY(j, b, volume, "v");
    DOUBLE_FROM_JSON_KEY(j, b, vwap, "vw");
    INT_FROM_JSON_KEY(j, b, trade_count, "n");
}

/// Historical responses are keyed by symbol: `{"bars": {"AAPL": [...], "MSFT": [...]}}`.
template <typename T>
using symbol_map = std::unordered_map<std::string, T>;

using bars_by_symbol = symbol_map<std::vector<bar>>;
using bar_by_symbol = symbol_map<bar>;

}   // namespace alpaca
