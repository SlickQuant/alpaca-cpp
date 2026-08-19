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
#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// One entry from `GET /v2/account/activities`.
///
/// Alpaca returns two different payload shapes on the same endpoint: trade activities
/// (`FILL`) carry execution details, everything else is a non-trade activity carrying a
/// cash amount. Rather than force callers to switch on a variant, both field sets live on
/// one struct and `is_trade_activity()` says which half is populated — which is also how
/// the payloads arrive when a query spans both kinds.
struct account_activity {
    alpaca::activity_type activity_type = alpaca::activity_type::unknown;
    std::string id;

    // --- trade activities (activity_type == fill) --------------------------
    std::string symbol;
    std::string order_id;
    alpaca::order_side side = alpaca::order_side::unknown;
    /// "fill" or "partial_fill".
    std::string type;
    alpaca::order_status order_status = alpaca::order_status::unknown;
    double price = 0.;
    double qty = 0.;
    double cum_qty = 0.;
    double leaves_qty = 0.;
    uint64_t transaction_time = 0;      ///< nanoseconds since the Unix epoch

    // --- non-trade activities ----------------------------------------------
    std::string date;                   ///< YYYY-MM-DD
    std::string description;
    std::string status;
    double net_amount = 0.;
    double per_share_amount = 0.;

    bool is_trade_activity() const noexcept {
        return activity_type == alpaca::activity_type::fill;
    }
};

inline void from_json(const json &j, account_activity &a) {
    ENUM_FROM_JSON_WITH(j, a, activity_type, to_activity_type);
    VARIABLE_FROM_JSON(j, a, id);

    VARIABLE_FROM_JSON(j, a, symbol);
    VARIABLE_FROM_JSON(j, a, order_id);
    ENUM_FROM_JSON_WITH(j, a, side, to_order_side);
    VARIABLE_FROM_JSON(j, a, type);
    ENUM_FROM_JSON_WITH(j, a, order_status, to_order_status);
    DOUBLE_FROM_JSON(j, a, price);
    DOUBLE_FROM_JSON(j, a, qty);
    DOUBLE_FROM_JSON(j, a, cum_qty);
    DOUBLE_FROM_JSON(j, a, leaves_qty);
    TIMESTAMP_FROM_JSON(j, a, transaction_time);

    VARIABLE_FROM_JSON(j, a, date);
    VARIABLE_FROM_JSON(j, a, description);
    VARIABLE_FROM_JSON(j, a, status);
    DOUBLE_FROM_JSON(j, a, net_amount);
    DOUBLE_FROM_JSON(j, a, per_share_amount);
}

/// Query parameters for `GET /v2/account/activities[/{activity_type}]`.
///
/// `page_token` here is an activity id, not the market-data style opaque token, so
/// paging is driven by the caller rather than `request_context::paginate`.
struct activity_query {
    std::vector<std::string> activity_types;
    std::optional<std::string> date;            ///< YYYY-MM-DD; mutually exclusive with after/until
    std::optional<std::string> after;           ///< RFC-3339
    std::optional<std::string> until;           ///< RFC-3339
    std::optional<alpaca::activity_direction> direction;
    std::optional<uint32_t> page_size;          ///< max 100
    std::optional<std::string> page_token;      ///< id of the last activity of the previous page
    std::optional<std::string> category;        ///< "trade_activity" or "non_trade_activity"

    query_builder build() const {
        query_builder q;
        q.add("activity_types", activity_types);
        q.add("date", date);
        q.add("after", after);
        q.add("until", until);
        q.add("direction", direction);
        q.add("page_size", page_size);
        q.add("page_token", page_token);
        q.add("category", category);
        return q;
    }
};

}   // namespace alpaca
