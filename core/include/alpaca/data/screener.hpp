// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// One entry of `GET /v1beta1/screener/stocks/most-actives`.
struct most_active {
    std::string symbol;
    uint64_t volume = 0;
    uint64_t trade_count = 0;
};

inline void from_json(const json &j, most_active &m) {
    VARIABLE_FROM_JSON(j, m, symbol);
    INT_FROM_JSON(j, m, volume);
    INT_FROM_JSON(j, m, trade_count);
}

struct most_actives {
    /// Named `items` rather than `most_actives` so it does not shadow the injected class name.
    std::vector<most_active> items;
    uint64_t last_updated = 0;      ///< nanoseconds since the Unix epoch
};

inline void from_json(const json &j, most_actives &m) {
    if (j.contains("most_actives") && j["most_actives"].is_array()) {
        m.items = j["most_actives"].get<std::vector<most_active>>();
    }
    TIMESTAMP_FROM_JSON(j, m, last_updated);
}

/// One entry of `GET /v1beta1/screener/{market_type}/movers`.
struct mover {
    std::string symbol;
    double price = 0.;
    double change = 0.;
    double percent_change = 0.;
};

inline void from_json(const json &j, mover &m) {
    VARIABLE_FROM_JSON(j, m, symbol);
    DOUBLE_FROM_JSON(j, m, price);
    DOUBLE_FROM_JSON(j, m, change);
    DOUBLE_FROM_JSON(j, m, percent_change);
}

struct movers {
    std::vector<mover> gainers;
    std::vector<mover> losers;
    std::string market_type;    ///< "stocks" or "crypto"
    uint64_t last_updated = 0;
};

inline void from_json(const json &j, movers &m) {
    if (j.contains("gainers") && j["gainers"].is_array()) {
        m.gainers = j["gainers"].get<std::vector<mover>>();
    }
    if (j.contains("losers") && j["losers"].is_array()) {
        m.losers = j["losers"].get<std::vector<mover>>();
    }
    VARIABLE_FROM_JSON(j, m, market_type);
    TIMESTAMP_FROM_JSON(j, m, last_updated);
}

}   // namespace alpaca
