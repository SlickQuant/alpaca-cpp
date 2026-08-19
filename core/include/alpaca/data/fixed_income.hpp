// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>

#include <nlohmann/json.hpp>

#include <alpaca/data/bar.hpp>
#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// A bond price. `GET /v1beta1/fixed_income/latest/prices`.
struct fixed_income_price {
    uint64_t timestamp = 0;             ///< `t` — nanoseconds since the Unix epoch
    double price = 0.;                  ///< `p` — percentage of par value
    double yield_to_maturity = 0.;      ///< `ytm`
    double yield_to_worst = 0.;         ///< `ytw`
};

inline void from_json(const json &j, fixed_income_price &p) {
    TIMESTAMP_FROM_JSON_KEY(j, p, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, p, price, "p");
    DOUBLE_FROM_JSON_KEY(j, p, yield_to_maturity, "ytm");
    DOUBLE_FROM_JSON_KEY(j, p, yield_to_worst, "ytw");
}

/// A bond quote. `GET /v1beta1/fixed_income/latest/quotes`.
struct fixed_income_quote {
    uint64_t timestamp = 0;                 ///< `t` — nanoseconds since the Unix epoch
    double bid_price = 0.;                  ///< `bp`
    double bid_size = 0.;                   ///< `bs`
    double bid_min_size = 0.;               ///< `bms`
    double bid_yield_to_maturity = 0.;      ///< `bytm`
    double bid_yield_to_worst = 0.;         ///< `bytw`
    double ask_price = 0.;                  ///< `ap`
    double ask_size = 0.;                   ///< `as`
    double ask_min_size = 0.;               ///< `ams`
    double ask_yield_to_maturity = 0.;      ///< `aytm`
    double ask_yield_to_worst = 0.;         ///< `aytw`

    double spread() const noexcept { return ask_price - bid_price; }
};

inline void from_json(const json &j, fixed_income_quote &q) {
    TIMESTAMP_FROM_JSON_KEY(j, q, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, q, bid_price, "bp");
    DOUBLE_FROM_JSON_KEY(j, q, bid_size, "bs");
    DOUBLE_FROM_JSON_KEY(j, q, bid_min_size, "bms");
    DOUBLE_FROM_JSON_KEY(j, q, bid_yield_to_maturity, "bytm");
    DOUBLE_FROM_JSON_KEY(j, q, bid_yield_to_worst, "bytw");
    DOUBLE_FROM_JSON_KEY(j, q, ask_price, "ap");
    DOUBLE_FROM_JSON_KEY(j, q, ask_size, "as");
    DOUBLE_FROM_JSON_KEY(j, q, ask_min_size, "ams");
    DOUBLE_FROM_JSON_KEY(j, q, ask_yield_to_maturity, "aytm");
    DOUBLE_FROM_JSON_KEY(j, q, ask_yield_to_worst, "aytw");
}

/// Fixed income responses are keyed by ISIN rather than by ticker symbol.
using fixed_income_prices_by_isin = symbol_map<fixed_income_price>;
using fixed_income_quotes_by_isin = symbol_map<fixed_income_quote>;

}   // namespace alpaca
