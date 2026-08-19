// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include <alpaca/common.hpp>
#include <alpaca/trading/order.hpp>
#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// An open position. `GET /v2/positions` and `GET /v2/positions/{symbol_or_asset_id}`.
struct position {
    std::string asset_id;
    std::string symbol;
    alpaca::asset_class asset_class = alpaca::asset_class::unknown;
    alpaca::asset_exchange exchange = alpaca::asset_exchange::unknown;
    alpaca::position_side side = alpaca::position_side::unknown;
    bool asset_marginable = false;

    double qty = 0.;
    double qty_available = 0.;
    double avg_entry_price = 0.;
    double market_value = 0.;
    double cost_basis = 0.;
    double current_price = 0.;
    double lastday_price = 0.;
    double change_today = 0.;

    double unrealized_pl = 0.;
    double unrealized_plpc = 0.;
    double unrealized_intraday_pl = 0.;
    double unrealized_intraday_plpc = 0.;
};

inline void from_json(const json &j, position &p) {
    VARIABLE_FROM_JSON(j, p, asset_id);
    VARIABLE_FROM_JSON(j, p, symbol);
    ENUM_FROM_JSON_KEY(j, p, asset_class, "asset_class", to_asset_class);
    ENUM_FROM_JSON_WITH(j, p, exchange, to_asset_exchange);
    ENUM_FROM_JSON_WITH(j, p, side, to_position_side);
    BOOL_FROM_JSON(j, p, asset_marginable);

    DOUBLE_FROM_JSON(j, p, qty);
    DOUBLE_FROM_JSON(j, p, qty_available);
    DOUBLE_FROM_JSON(j, p, avg_entry_price);
    DOUBLE_FROM_JSON(j, p, market_value);
    DOUBLE_FROM_JSON(j, p, cost_basis);
    DOUBLE_FROM_JSON(j, p, current_price);
    DOUBLE_FROM_JSON(j, p, lastday_price);
    DOUBLE_FROM_JSON(j, p, change_today);

    DOUBLE_FROM_JSON(j, p, unrealized_pl);
    DOUBLE_FROM_JSON(j, p, unrealized_plpc);
    DOUBLE_FROM_JSON(j, p, unrealized_intraday_pl);
    DOUBLE_FROM_JSON(j, p, unrealized_intraday_plpc);
}

/// How much of a position to liquidate. Exactly one of qty / percentage may be set;
/// leaving both unset closes the whole position.
struct close_position_request {
    std::optional<double> qty;
    std::optional<double> percentage;

    query_builder build() const {
        query_builder q;
        if (qty)        q.add("qty", *qty);
        if (percentage) q.add("percentage", *percentage);
        return q;
    }
};

/// One entry of the `DELETE /v2/positions` (close-all) response.
struct close_position_result {
    std::string symbol;
    uint32_t status = 0;    ///< per-position HTTP status
    alpaca::order order;    ///< the liquidating order, when Alpaca accepted the request

    bool is_ok() const noexcept { return status >= 200 && status < 300; }
};

inline void from_json(const json &j, close_position_result &r) {
    VARIABLE_FROM_JSON(j, r, symbol);
    INT_FROM_JSON(j, r, status);
    // Alpaca nests the liquidating order under "body".
    STRUCT_FROM_JSON_KEY(j, r, order, "body");
}

}   // namespace alpaca
