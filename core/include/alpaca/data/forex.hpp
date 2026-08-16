// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/common.hpp>
#include <alpaca/data/bar.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// A currency-pair rate. `GET /v1beta1/forex/rates` and `/v1beta1/forex/latest/rates`.
struct forex_rate {
    uint64_t timestamp = 0;     ///< `t` — nanoseconds since the Unix epoch
    double bid_price = 0.;      ///< `bp`
    double mid_price = 0.;      ///< `mp`
    double ask_price = 0.;      ///< `ap`
};

inline void from_json(const json &j, forex_rate &r) {
    TIMESTAMP_FROM_JSON_KEY(j, r, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, r, bid_price, "bp");
    DOUBLE_FROM_JSON_KEY(j, r, mid_price, "mp");
    DOUBLE_FROM_JSON_KEY(j, r, ask_price, "ap");
}

/// Keyed by currency pair, e.g. "USDJPY".
using forex_rates_by_pair = symbol_map<std::vector<forex_rate>>;
using forex_rate_by_pair = symbol_map<forex_rate>;

/// Query parameters for `GET /v1beta1/forex/rates`.
struct forex_query {
    std::vector<std::string> currency_pairs;
    /// "5S", "1Min" (default) or "1D" — a smaller set than the bar timeframes.
    std::optional<std::string> timeframe;
    std::optional<std::string> start;
    std::optional<std::string> end;
    /// 1-10000, default 1000.
    std::optional<uint32_t> limit;
    std::optional<alpaca::sort_direction> sort;
    std::optional<std::string> page_token;

    query_builder build() const {
        query_builder q;
        q.add("currency_pairs", currency_pairs);
        q.add("timeframe", timeframe);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("sort", sort);
        q.add("page_token", page_token);
        return q;
    }
};

}   // namespace alpaca
