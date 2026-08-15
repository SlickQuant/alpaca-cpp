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

/// Trading account balances and permissions. `GET /v2/account`.
///
/// Every monetary field arrives as a JSON string; `double_from_json` normalises them.
struct account {
    std::string id;
    std::string account_number;
    std::string currency;
    alpaca::account_status status = alpaca::account_status::unknown;
    alpaca::account_status crypto_status = alpaca::account_status::unknown;
    alpaca::options_approved_level options_approved_level = alpaca::options_approved_level::unknown;
    alpaca::options_approved_level options_trading_level = alpaca::options_approved_level::unknown;

    // Buying power
    double buying_power = 0.;
    double regt_buying_power = 0.;
    double daytrading_buying_power = 0.;
    double effective_buying_power = 0.;
    double non_marginable_buying_power = 0.;
    double options_buying_power = 0.;
    double bod_dtbp = 0.;

    // Cash and transfers
    double cash = 0.;
    double accrued_fees = 0.;
    double pending_transfer_in = 0.;
    double pending_transfer_out = 0.;
    double pending_reg_taf_fees = 0.;

    // Equity and market values
    double portfolio_value = 0.;
    double equity = 0.;
    double last_equity = 0.;
    double long_market_value = 0.;
    double short_market_value = 0.;
    double position_market_value = 0.;

    // Margin
    double initial_margin = 0.;
    double maintenance_margin = 0.;
    double last_maintenance_margin = 0.;
    double sma = 0.;
    double multiplier = 0.;
    int32_t daytrade_count = 0;
    int32_t intraday_adjustments = 0;

    // Flags
    bool pattern_day_trader = false;
    bool trading_blocked = false;
    bool transfers_blocked = false;
    bool account_blocked = false;
    bool trade_suspended_by_user = false;
    bool shorting_enabled = false;

    std::string crypto_tier;
    uint64_t created_at = 0;    ///< nanoseconds since the Unix epoch
    uint64_t balance_asof = 0;  ///< nanoseconds since the Unix epoch
};

inline void from_json(const json &j, account &a) {
    VARIABLE_FROM_JSON(j, a, id);
    VARIABLE_FROM_JSON(j, a, account_number);
    VARIABLE_FROM_JSON(j, a, currency);
    ENUM_FROM_JSON_WITH(j, a, status, to_account_status);
    ENUM_FROM_JSON_WITH(j, a, crypto_status, to_account_status);
    ENUM_FROM_JSON_WITH(j, a, options_approved_level, to_options_approved_level);
    ENUM_FROM_JSON_WITH(j, a, options_trading_level, to_options_approved_level);

    DOUBLE_FROM_JSON(j, a, buying_power);
    DOUBLE_FROM_JSON(j, a, regt_buying_power);
    DOUBLE_FROM_JSON(j, a, daytrading_buying_power);
    DOUBLE_FROM_JSON(j, a, effective_buying_power);
    DOUBLE_FROM_JSON(j, a, non_marginable_buying_power);
    DOUBLE_FROM_JSON(j, a, options_buying_power);
    DOUBLE_FROM_JSON(j, a, bod_dtbp);

    DOUBLE_FROM_JSON(j, a, cash);
    DOUBLE_FROM_JSON(j, a, accrued_fees);
    DOUBLE_FROM_JSON(j, a, pending_transfer_in);
    DOUBLE_FROM_JSON(j, a, pending_transfer_out);
    DOUBLE_FROM_JSON(j, a, pending_reg_taf_fees);

    DOUBLE_FROM_JSON(j, a, portfolio_value);
    DOUBLE_FROM_JSON(j, a, equity);
    DOUBLE_FROM_JSON(j, a, last_equity);
    DOUBLE_FROM_JSON(j, a, long_market_value);
    DOUBLE_FROM_JSON(j, a, short_market_value);
    DOUBLE_FROM_JSON(j, a, position_market_value);

    DOUBLE_FROM_JSON(j, a, initial_margin);
    DOUBLE_FROM_JSON(j, a, maintenance_margin);
    DOUBLE_FROM_JSON(j, a, last_maintenance_margin);
    DOUBLE_FROM_JSON(j, a, sma);
    DOUBLE_FROM_JSON(j, a, multiplier);
    INT_FROM_JSON(j, a, daytrade_count);
    INT_FROM_JSON(j, a, intraday_adjustments);

    BOOL_FROM_JSON(j, a, pattern_day_trader);
    BOOL_FROM_JSON(j, a, trading_blocked);
    BOOL_FROM_JSON(j, a, transfers_blocked);
    BOOL_FROM_JSON(j, a, account_blocked);
    BOOL_FROM_JSON(j, a, trade_suspended_by_user);
    BOOL_FROM_JSON(j, a, shorting_enabled);

    VARIABLE_FROM_JSON(j, a, crypto_tier);
    TIMESTAMP_FROM_JSON(j, a, created_at);
    TIMESTAMP_FROM_JSON(j, a, balance_asof);
}

/// Mutable account settings. `GET`/`PATCH /v2/account/configurations`.
struct account_configurations {
    alpaca::dtbp_check dtbp_check = alpaca::dtbp_check::unknown;
    alpaca::trade_confirm_email trade_confirm_email = alpaca::trade_confirm_email::unknown;
    bool no_shorting = false;
    bool suspend_trade = false;
    bool fractional_trading = false;
    bool pdt_check_enabled = false;
    bool ptp_no_exception_entry = false;
    std::string max_margin_multiplier;
    std::string max_options_trading_level;
    std::string pdt_check;
};

inline void from_json(const json &j, account_configurations &c) {
    ENUM_FROM_JSON_WITH(j, c, dtbp_check, to_dtbp_check);
    ENUM_FROM_JSON_WITH(j, c, trade_confirm_email, to_trade_confirm_email);
    BOOL_FROM_JSON(j, c, no_shorting);
    BOOL_FROM_JSON(j, c, suspend_trade);
    BOOL_FROM_JSON(j, c, fractional_trading);
    BOOL_FROM_JSON(j, c, ptp_no_exception_entry);
    VARIABLE_FROM_JSON(j, c, max_margin_multiplier);
    VARIABLE_FROM_JSON(j, c, max_options_trading_level);
    VARIABLE_FROM_JSON(j, c, pdt_check);
    c.pdt_check_enabled = c.pdt_check == "entry" || c.pdt_check == "exit" || c.pdt_check == "both";
}

/// Partial update for `PATCH /v2/account/configurations`. Only set fields are sent.
struct account_configurations_update {
    std::optional<alpaca::dtbp_check> dtbp_check;
    std::optional<alpaca::trade_confirm_email> trade_confirm_email;
    std::optional<bool> no_shorting;
    std::optional<bool> suspend_trade;
    std::optional<bool> fractional_trading;
    std::optional<bool> ptp_no_exception_entry;
    std::optional<std::string> max_margin_multiplier;
    std::optional<std::string> max_options_trading_level;
    std::optional<std::string> pdt_check;

    json to_json() const {
        json j = json::object();
        if (dtbp_check)               j["dtbp_check"] = to_string(*dtbp_check);
        if (trade_confirm_email)      j["trade_confirm_email"] = to_string(*trade_confirm_email);
        if (no_shorting)              j["no_shorting"] = *no_shorting;
        if (suspend_trade)            j["suspend_trade"] = *suspend_trade;
        if (fractional_trading)       j["fractional_trading"] = *fractional_trading;
        if (ptp_no_exception_entry)   j["ptp_no_exception_entry"] = *ptp_no_exception_entry;
        if (max_margin_multiplier)    j["max_margin_multiplier"] = *max_margin_multiplier;
        if (max_options_trading_level) j["max_options_trading_level"] = *max_options_trading_level;
        if (pdt_check)                j["pdt_check"] = *pdt_check;
        return j;
    }
};

/// Equity and P/L time series. `GET /v2/account/portfolio/history`.
///
/// Alpaca returns parallel arrays rather than an array of records; they are kept in that
/// shape because that is what charting code wants, and index i is the same instant in all
/// of them.
struct portfolio_history {
    std::vector<uint64_t> timestamp;    ///< nanoseconds since the Unix epoch
    std::vector<double> equity;
    std::vector<double> profit_loss;
    std::vector<double> profit_loss_pct;
    std::vector<double> cashflow;
    double base_value = 0.;
    uint64_t base_value_asof = 0;
    std::string timeframe;
};

inline void from_json(const json &j, portfolio_history &h) {
    if (j.contains("timestamp") && j["timestamp"].is_array()) {
        h.timestamp.reserve(j["timestamp"].size());
        for (const auto &t : j["timestamp"]) {
            // This endpoint reports epoch *seconds* as numbers, unlike the RFC-3339
            // timestamps everywhere else in the API.
            h.timestamp.push_back(t.is_number() ? t.get<uint64_t>() * 1'000'000'000ull
                                                : to_nanoseconds(t.get<std::string_view>()));
        }
    }

    const auto read_doubles = [&j](const char *key, std::vector<double> &out) {
        if (j.contains(key) && j[key].is_array()) {
            out.reserve(j[key].size());
            for (const auto &v : j[key]) {
                out.push_back(v.is_null() ? 0. : v.get<double>());
            }
        }
    };
    read_doubles("equity", h.equity);
    read_doubles("profit_loss", h.profit_loss);
    read_doubles("profit_loss_pct", h.profit_loss_pct);
    read_doubles("cashflow", h.cashflow);

    DOUBLE_FROM_JSON(j, h, base_value);
    TIMESTAMP_FROM_JSON(j, h, base_value_asof);
    VARIABLE_FROM_JSON(j, h, timeframe);
}

/// Query parameters for `GET /v2/account/portfolio/history`.
struct portfolio_history_query {
    std::optional<std::string> period;          ///< e.g. "1D", "1M", "3M", "1A"
    std::optional<std::string> timeframe;       ///< "1Min", "5Min", "15Min", "1H", "1D"
    std::optional<std::string> intraday_reporting;
    std::optional<std::string> start;           ///< RFC-3339
    std::optional<std::string> end;             ///< RFC-3339
    std::optional<std::string> pnl_reset;
    std::optional<bool> extended_hours;
    std::optional<bool> cashflow_types;

    query_builder build() const {
        query_builder q;
        q.add("period", period);
        q.add("timeframe", timeframe);
        q.add("intraday_reporting", intraday_reporting);
        q.add("start", start);
        q.add("end", end);
        q.add("pnl_reset", pnl_reset);
        q.add("extended_hours", extended_hours);
        q.add("cashflow_types", cashflow_types);
        return q;
    }
};

}   // namespace alpaca
