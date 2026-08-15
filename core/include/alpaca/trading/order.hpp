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

using json = nlohmann::json;

namespace alpaca {

/// An order as returned by the Trading API. `GET`/`POST /v2/orders`.
///
/// Prices and quantities arrive as JSON strings and are normalised to doubles. Fields that
/// Alpaca genuinely leaves null (a market order has no limit price) are `std::optional`
/// rather than defaulted to zero, so "no limit price" and "a limit price of 0" stay distinct.
struct order {
    std::string id;
    std::string client_order_id;
    std::string symbol;
    std::string asset_id;
    alpaca::asset_class asset_class = alpaca::asset_class::unknown;

    alpaca::order_side side = alpaca::order_side::unknown;
    alpaca::order_type type = alpaca::order_type::unknown;
    alpaca::order_class order_class = alpaca::order_class::unknown;
    alpaca::time_in_force time_in_force = alpaca::time_in_force::unknown;
    alpaca::order_status status = alpaca::order_status::unknown;
    alpaca::position_intent position_intent = alpaca::position_intent::unknown;

    std::optional<double> qty;
    std::optional<double> notional;
    double filled_qty = 0.;
    std::optional<double> filled_avg_price;
    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail_price;
    std::optional<double> trail_percent;
    std::optional<double> hwm;          ///< high-water mark for trailing stops
    std::optional<double> ratio_qty;    ///< set on multi-leg (mleg) legs

    bool extended_hours = false;

    // All timestamps are nanoseconds since the Unix epoch; 0 means the event has not happened.
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
    uint64_t submitted_at = 0;
    uint64_t filled_at = 0;
    uint64_t expired_at = 0;
    uint64_t canceled_at = 0;
    uint64_t failed_at = 0;
    uint64_t replaced_at = 0;
    uint64_t expires_at = 0;

    std::string replaced_by;
    std::string replaces;
    std::string subtag;
    std::string source;

    /// Child orders for bracket/oto/oco, or the individual legs of an mleg order.
    std::vector<order> legs;

    bool is_open() const noexcept {
        switch (status) {
        case alpaca::order_status::filled:
        case alpaca::order_status::canceled:
        case alpaca::order_status::expired:
        case alpaca::order_status::rejected:
        case alpaca::order_status::replaced:
        case alpaca::order_status::done_for_day:
            return false;
        default:
            return true;
        }
    }

    bool is_filled() const noexcept { return status == alpaca::order_status::filled; }
};

void from_json(const json &j, order &o);

inline void from_json(const json &j, order &o) {
    VARIABLE_FROM_JSON(j, o, id);
    VARIABLE_FROM_JSON(j, o, client_order_id);
    VARIABLE_FROM_JSON(j, o, symbol);
    VARIABLE_FROM_JSON(j, o, asset_id);
    ENUM_FROM_JSON_KEY(j, o, asset_class, "asset_class", to_asset_class);

    ENUM_FROM_JSON_WITH(j, o, side, to_order_side);
    ENUM_FROM_JSON_WITH(j, o, type, to_order_type);
    ENUM_FROM_JSON_WITH(j, o, order_class, to_order_class);
    ENUM_FROM_JSON_WITH(j, o, time_in_force, to_time_in_force);
    ENUM_FROM_JSON_WITH(j, o, status, to_order_status);
    ENUM_FROM_JSON_WITH(j, o, position_intent, to_position_intent);

    OPTIONAL_DOUBLE_FROM_JSON(j, o, qty);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, notional);
    DOUBLE_FROM_JSON(j, o, filled_qty);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, filled_avg_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, limit_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, stop_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, trail_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, trail_percent);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, hwm);
    OPTIONAL_DOUBLE_FROM_JSON(j, o, ratio_qty);

    BOOL_FROM_JSON(j, o, extended_hours);

    TIMESTAMP_FROM_JSON(j, o, created_at);
    TIMESTAMP_FROM_JSON(j, o, updated_at);
    TIMESTAMP_FROM_JSON(j, o, submitted_at);
    TIMESTAMP_FROM_JSON(j, o, filled_at);
    TIMESTAMP_FROM_JSON(j, o, expired_at);
    TIMESTAMP_FROM_JSON(j, o, canceled_at);
    TIMESTAMP_FROM_JSON(j, o, failed_at);
    TIMESTAMP_FROM_JSON(j, o, replaced_at);
    TIMESTAMP_FROM_JSON(j, o, expires_at);

    VARIABLE_FROM_JSON(j, o, replaced_by);
    VARIABLE_FROM_JSON(j, o, replaces);
    VARIABLE_FROM_JSON(j, o, subtag);
    VARIABLE_FROM_JSON(j, o, source);

    if (j.contains("legs") && j["legs"].is_array()) {
        o.legs.clear();
        o.legs.reserve(j["legs"].size());
        for (const auto &leg_json : j["legs"]) {
            order leg;
            from_json(leg_json, leg);
            o.legs.push_back(std::move(leg));
        }
    }
}

// ---------------------------------------------------------------------------
// Order placement
// ---------------------------------------------------------------------------

/// Take-profit leg of a bracket or OTO order.
struct take_profit_request {
    double limit_price = 0.;

    json to_json() const {
        return json{{"limit_price", alpaca::to_string(limit_price)}};
    }
};

/// Stop-loss leg of a bracket or OTO order. `limit_price` upgrades it to a stop-limit.
struct stop_loss_request {
    double stop_price = 0.;
    std::optional<double> limit_price;

    json to_json() const {
        json j{{"stop_price", alpaca::to_string(stop_price)}};
        if (limit_price) {
            j["limit_price"] = alpaca::to_string(*limit_price);
        }
        return j;
    }
};

/// One leg of a multi-leg (`mleg`) options order.
struct order_leg_request {
    std::string symbol;
    double ratio_qty = 0.;
    alpaca::order_side side = alpaca::order_side::unknown;
    std::optional<alpaca::position_intent> position_intent;

    json to_json() const {
        json j{
            {"symbol", symbol},
            {"ratio_qty", alpaca::to_string(ratio_qty)},
            {"side", alpaca::to_string(side)},
        };
        if (position_intent) {
            j["position_intent"] = alpaca::to_string(*position_intent);
        }
        return j;
    }
};

/// Request body for `POST /v2/orders`.
///
/// Build one directly for full control, or use the factory functions below for the
/// common shapes. Alpaca requires every numeric field as a string, which `to_json`
/// handles — callers always work with doubles.
struct order_request {
    std::string symbol;
    alpaca::order_side side = alpaca::order_side::unknown;
    alpaca::order_type type = alpaca::order_type::unknown;
    alpaca::time_in_force time_in_force = alpaca::time_in_force::day;

    /// Exactly one of qty / notional must be set. `notional` is market + day only.
    std::optional<double> qty;
    std::optional<double> notional;

    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail_price;
    std::optional<double> trail_percent;

    std::optional<alpaca::order_class> order_class;
    std::optional<bool> extended_hours;
    std::optional<std::string> client_order_id;
    std::optional<alpaca::position_intent> position_intent;

    std::optional<take_profit_request> take_profit;
    std::optional<stop_loss_request> stop_loss;

    /// Up to four legs; required when `order_class` is `mleg`, in which case `symbol`,
    /// `side` and `type` are left unset at the top level.
    std::vector<order_leg_request> legs;

    json to_json() const;

    // --- factories for the common shapes -----------------------------------

    static order_request market(std::string symbol, double qty, alpaca::order_side side,
                                alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::market;
        r.time_in_force = tif;
        return r;
    }

    /// Notional (dollar-denominated) market order. Day TIF only, per Alpaca.
    static order_request market_notional(std::string symbol, double notional, alpaca::order_side side) {
        order_request r;
        r.symbol = std::move(symbol);
        r.notional = notional;
        r.side = side;
        r.type = alpaca::order_type::market;
        r.time_in_force = alpaca::time_in_force::day;
        return r;
    }

    static order_request limit(std::string symbol, double qty, alpaca::order_side side,
                               double limit_price,
                               alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::limit;
        r.time_in_force = tif;
        r.limit_price = limit_price;
        return r;
    }

    static order_request stop(std::string symbol, double qty, alpaca::order_side side,
                              double stop_price,
                              alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::stop;
        r.time_in_force = tif;
        r.stop_price = stop_price;
        return r;
    }

    static order_request stop_limit(std::string symbol, double qty, alpaca::order_side side,
                                    double stop_price, double limit_price,
                                    alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::stop_limit;
        r.time_in_force = tif;
        r.stop_price = stop_price;
        r.limit_price = limit_price;
        return r;
    }

    static order_request trailing_stop_price(std::string symbol, double qty, alpaca::order_side side,
                                             double trail_price,
                                             alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::trailing_stop;
        r.time_in_force = tif;
        r.trail_price = trail_price;
        return r;
    }

    static order_request trailing_stop_percent(std::string symbol, double qty, alpaca::order_side side,
                                               double trail_percent,
                                               alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.symbol = std::move(symbol);
        r.qty = qty;
        r.side = side;
        r.type = alpaca::order_type::trailing_stop;
        r.time_in_force = tif;
        r.trail_percent = trail_percent;
        return r;
    }

    /// Entry order with both a take-profit and a stop-loss child. Bracket orders require
    /// a `gtc` or `day` time in force.
    static order_request bracket(order_request entry, double take_profit_limit,
                                 double stop_loss_stop,
                                 std::optional<double> stop_loss_limit = {}) {
        entry.order_class = alpaca::order_class::bracket;
        entry.take_profit = take_profit_request{take_profit_limit};
        entry.stop_loss = stop_loss_request{stop_loss_stop, stop_loss_limit};
        return entry;
    }

    /// Multi-leg options order. `symbol`, `side` and `type` stay unset; each leg carries its own.
    static order_request multi_leg(std::vector<order_leg_request> legs, double qty,
                                   alpaca::time_in_force tif = alpaca::time_in_force::day) {
        order_request r;
        r.order_class = alpaca::order_class::mleg;
        r.legs = std::move(legs);
        r.qty = qty;
        r.time_in_force = tif;
        return r;
    }
};

inline json order_request::to_json() const {
    json j = json::object();

    // mleg orders carry symbol/side/type per leg instead of at the top level.
    const bool is_mleg = order_class.has_value() && *order_class == alpaca::order_class::mleg;
    if (!is_mleg) {
        j["symbol"] = symbol;
        if (side != alpaca::order_side::unknown) {
            j["side"] = alpaca::to_string(side);
        }
    }
    if (type != alpaca::order_type::unknown) {
        j["type"] = alpaca::to_string(type);
    }
    j["time_in_force"] = alpaca::to_string(time_in_force);

    if (qty)            j["qty"] = alpaca::to_string(*qty);
    if (notional)       j["notional"] = alpaca::to_string(*notional);
    if (limit_price)    j["limit_price"] = alpaca::to_string(*limit_price);
    if (stop_price)     j["stop_price"] = alpaca::to_string(*stop_price);
    if (trail_price)    j["trail_price"] = alpaca::to_string(*trail_price);
    if (trail_percent)  j["trail_percent"] = alpaca::to_string(*trail_percent);

    if (order_class)     j["order_class"] = alpaca::to_string(*order_class);
    if (extended_hours)  j["extended_hours"] = *extended_hours;
    if (client_order_id) j["client_order_id"] = *client_order_id;
    if (position_intent) j["position_intent"] = alpaca::to_string(*position_intent);

    if (take_profit) j["take_profit"] = take_profit->to_json();
    if (stop_loss)   j["stop_loss"] = stop_loss->to_json();

    if (!legs.empty()) {
        json leg_array = json::array();
        for (const auto &leg : legs) {
            leg_array.push_back(leg.to_json());
        }
        j["legs"] = std::move(leg_array);
    }

    return j;
}

/// Request body for `PATCH /v2/orders/{order_id}`. Only set fields are sent.
struct replace_order_request {
    std::optional<double> qty;
    std::optional<double> limit_price;
    std::optional<double> stop_price;
    std::optional<double> trail;
    std::optional<alpaca::time_in_force> time_in_force;
    std::optional<std::string> client_order_id;

    json to_json() const {
        json j = json::object();
        if (qty)             j["qty"] = alpaca::to_string(*qty);
        if (limit_price)     j["limit_price"] = alpaca::to_string(*limit_price);
        if (stop_price)      j["stop_price"] = alpaca::to_string(*stop_price);
        if (trail)           j["trail"] = alpaca::to_string(*trail);
        if (time_in_force)   j["time_in_force"] = alpaca::to_string(*time_in_force);
        if (client_order_id) j["client_order_id"] = *client_order_id;
        return j;
    }
};

/// Query parameters for `GET /v2/orders`.
struct order_query {
    std::optional<alpaca::order_status_filter> status;
    std::optional<uint32_t> limit;              ///< max 500, default 50
    std::optional<std::string> after;           ///< RFC-3339
    std::optional<std::string> until;           ///< RFC-3339
    std::optional<alpaca::sort_direction> direction;
    std::optional<bool> nested;                 ///< roll child legs into the parent order
    std::vector<std::string> symbols;
    std::optional<std::string> side;

    query_builder build() const {
        query_builder q;
        q.add("status", status);
        q.add("limit", limit);
        q.add("after", after);
        q.add("until", until);
        q.add("direction", direction);
        q.add("nested", nested);
        q.add("symbols", symbols);
        q.add("side", side);
        return q;
    }
};

/// One entry of the `DELETE /v2/orders` (cancel-all) response.
struct cancel_order_result {
    std::string id;
    uint32_t status = 0;    ///< per-order HTTP status, 200 when the cancel was accepted

    bool is_ok() const noexcept { return status >= 200 && status < 300; }
};

inline void from_json(const json &j, cancel_order_result &r) {
    VARIABLE_FROM_JSON(j, r, id);
    INT_FROM_JSON(j, r, status);
}

}   // namespace alpaca
