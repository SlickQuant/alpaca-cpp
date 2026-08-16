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

/// A single execution.
///
/// One struct covers stocks, crypto and options even though the three differ slightly on
/// the wire: options send a single condition string where stocks send an array, and crypto
/// sends a taker side instead of an exchange. Both spellings are accepted and the fields
/// that do not apply stay empty, which keeps callers off a variant they would have to
/// switch on anyway.
struct trade {
    uint64_t timestamp = 0;                 ///< `t` — nanoseconds since the Unix epoch
    double price = 0.;                      ///< `p`
    double size = 0.;                       ///< `s`
    uint64_t id = 0;                        ///< `i` — trade id (stocks)
    std::string exchange;                   ///< `x`
    std::string tape;                       ///< `z` — A, B or C (stocks)
    std::string update;                     ///< `u` — set when the trade was corrected
    std::string taker_side;                 ///< `tks` — B or S (crypto)
    std::vector<std::string> conditions;    ///< `c`
};

inline void from_json(const json &j, trade &t) {
    TIMESTAMP_FROM_JSON_KEY(j, t, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, t, price, "p");
    DOUBLE_FROM_JSON_KEY(j, t, size, "s");
    INT_FROM_JSON_KEY(j, t, id, "i");
    VARIABLE_FROM_JSON_KEY(j, t, exchange, "x");
    VARIABLE_FROM_JSON_KEY(j, t, tape, "z");
    VARIABLE_FROM_JSON_KEY(j, t, update, "u");
    VARIABLE_FROM_JSON_KEY(j, t, taker_side, "tks");

    // Stocks and crypto send an array of condition codes; options send a single string.
    if (j.contains("c") && !j["c"].is_null()) {
        const auto &conditions = j["c"];
        if (conditions.is_array()) {
            t.conditions = conditions.get<std::vector<std::string>>();
        }
        else if (conditions.is_string()) {
            t.conditions = {conditions.get<std::string>()};
        }
    }
}

using trades_by_symbol = symbol_map<std::vector<trade>>;
using trade_by_symbol = symbol_map<trade>;

}   // namespace alpaca
