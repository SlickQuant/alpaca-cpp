// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/common.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// A tradable instrument. `GET /v2/assets` and `GET /v2/assets/{symbol_or_asset_id}`.
struct asset {
    std::string id;
    std::string symbol;
    std::string name;
    alpaca::asset_class asset_class = alpaca::asset_class::unknown;
    alpaca::asset_exchange exchange = alpaca::asset_exchange::unknown;
    alpaca::asset_status status = alpaca::asset_status::unknown;
    bool tradable = false;
    bool marginable = false;
    bool shortable = false;
    bool easy_to_borrow = false;
    bool fractionable = false;
    double maintenance_margin_requirement = 0.;
    double margin_requirement_long = 0.;
    double margin_requirement_short = 0.;
    /// Free-form flags such as "ptp_no_exception", "ipo", "options_enabled".
    std::vector<std::string> attributes;
};

inline void from_json(const json &j, asset &a) {
    VARIABLE_FROM_JSON(j, a, id);
    VARIABLE_FROM_JSON(j, a, symbol);
    VARIABLE_FROM_JSON(j, a, name);
    // The assets endpoint spells the field "class"; orders and positions spell it "asset_class".
    ENUM_FROM_JSON_KEY(j, a, asset_class, "class", to_asset_class);
    ENUM_FROM_JSON_KEY(j, a, asset_class, "asset_class", to_asset_class);
    ENUM_FROM_JSON_WITH(j, a, exchange, to_asset_exchange);
    ENUM_FROM_JSON_WITH(j, a, status, to_asset_status);
    BOOL_FROM_JSON(j, a, tradable);
    BOOL_FROM_JSON(j, a, marginable);
    BOOL_FROM_JSON(j, a, shortable);
    BOOL_FROM_JSON(j, a, easy_to_borrow);
    BOOL_FROM_JSON(j, a, fractionable);
    DOUBLE_FROM_JSON(j, a, maintenance_margin_requirement);
    DOUBLE_FROM_JSON(j, a, margin_requirement_long);
    DOUBLE_FROM_JSON(j, a, margin_requirement_short);
    VARIABLE_FROM_JSON(j, a, attributes);
}

/// Query parameters for `GET /v2/assets`.
struct asset_query {
    std::optional<alpaca::asset_status> status;
    std::optional<alpaca::asset_class> asset_class;
    std::optional<alpaca::asset_exchange> exchange;
    /// Filters to assets carrying every listed attribute.
    std::vector<std::string> attributes;

    query_builder build() const {
        query_builder q;
        q.add("status", status);
        q.add("asset_class", asset_class);
        q.add("exchange", exchange);
        q.add("attributes", attributes);
        return q;
    }
};

}   // namespace alpaca
