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

namespace alpaca {

using json = nlohmann::json;

/// A top-of-book bid/ask pair. Shared by stocks, crypto and options; options send a single
/// condition string where stocks send an array, and both are accepted.
struct quote {
    uint64_t timestamp = 0;                 ///< `t` — nanoseconds since the Unix epoch
    double bid_price = 0.;                  ///< `bp`
    double bid_size = 0.;                   ///< `bs`
    double ask_price = 0.;                  ///< `ap`
    double ask_size = 0.;                   ///< `as`
    std::string bid_exchange;               ///< `bx`
    std::string ask_exchange;               ///< `ax`
    std::string tape;                       ///< `z` — A, B or C (stocks)
    std::vector<std::string> conditions;    ///< `c`

    double spread() const noexcept { return ask_price - bid_price; }
    double mid_price() const noexcept { return (bid_price + ask_price) * 0.5; }
};

inline void from_json(const json &j, quote &q) {
    TIMESTAMP_FROM_JSON_KEY(j, q, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, q, bid_price, "bp");
    DOUBLE_FROM_JSON_KEY(j, q, bid_size, "bs");
    DOUBLE_FROM_JSON_KEY(j, q, ask_price, "ap");
    DOUBLE_FROM_JSON_KEY(j, q, ask_size, "as");
    VARIABLE_FROM_JSON_KEY(j, q, bid_exchange, "bx");
    VARIABLE_FROM_JSON_KEY(j, q, ask_exchange, "ax");
    VARIABLE_FROM_JSON_KEY(j, q, tape, "z");

    if (j.contains("c") && !j["c"].is_null()) {
        const auto &conditions = j["c"];
        if (conditions.is_array()) {
            q.conditions = conditions.get<std::vector<std::string>>();
        }
        else if (conditions.is_string()) {
            q.conditions = {conditions.get<std::string>()};
        }
    }
}

using quotes_by_symbol = symbol_map<std::vector<quote>>;
using quote_by_symbol = symbol_map<quote>;

}   // namespace alpaca
