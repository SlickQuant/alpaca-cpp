// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include <alpaca/data/bar.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/utils.hpp>

namespace alpaca {

using json = nlohmann::json;

/// Everything currently known about a symbol in one response.
/// `GET /v2/stocks/snapshots` and the crypto equivalent.
struct snapshot {
    alpaca::trade latest_trade;     ///< `latestTrade`
    alpaca::quote latest_quote;     ///< `latestQuote`
    alpaca::bar minute_bar;         ///< `minuteBar`
    alpaca::bar daily_bar;          ///< `dailyBar`
    alpaca::bar prev_daily_bar;     ///< `prevDailyBar`
};

inline void from_json(const json &j, snapshot &s) {
    STRUCT_FROM_JSON_KEY(j, s, latest_trade, "latestTrade");
    STRUCT_FROM_JSON_KEY(j, s, latest_quote, "latestQuote");
    STRUCT_FROM_JSON_KEY(j, s, minute_bar, "minuteBar");
    STRUCT_FROM_JSON_KEY(j, s, daily_bar, "dailyBar");
    STRUCT_FROM_JSON_KEY(j, s, prev_daily_bar, "prevDailyBar");
}

/// Black-Scholes sensitivities returned alongside an option snapshot.
struct option_greeks {
    double delta = 0.;
    double gamma = 0.;
    double theta = 0.;
    double vega = 0.;
    double rho = 0.;
};

inline void from_json(const json &j, option_greeks &g) {
    DOUBLE_FROM_JSON(j, g, delta);
    DOUBLE_FROM_JSON(j, g, gamma);
    DOUBLE_FROM_JSON(j, g, theta);
    DOUBLE_FROM_JSON(j, g, vega);
    DOUBLE_FROM_JSON(j, g, rho);
}

/// A stock snapshot plus the option-only greeks and implied volatility.
///
/// Both are `std::optional` because Alpaca omits them when it cannot compute them — for an
/// illiquid contract with no valid quote, for instance — and a defaulted delta of 0 would
/// be indistinguishable from a genuinely delta-neutral position.
struct option_snapshot {
    alpaca::trade latest_trade;
    alpaca::quote latest_quote;
    alpaca::bar minute_bar;
    alpaca::bar daily_bar;
    alpaca::bar prev_daily_bar;
    std::optional<option_greeks> greeks;
    std::optional<double> implied_volatility;   ///< `impliedVolatility`
};

inline void from_json(const json &j, option_snapshot &s) {
    STRUCT_FROM_JSON_KEY(j, s, latest_trade, "latestTrade");
    STRUCT_FROM_JSON_KEY(j, s, latest_quote, "latestQuote");
    STRUCT_FROM_JSON_KEY(j, s, minute_bar, "minuteBar");
    STRUCT_FROM_JSON_KEY(j, s, daily_bar, "dailyBar");
    STRUCT_FROM_JSON_KEY(j, s, prev_daily_bar, "prevDailyBar");

    if (j.contains("greeks") && !j["greeks"].is_null()) {
        option_greeks g;
        from_json(j["greeks"], g);
        s.greeks = g;
    }
    if (j.contains("impliedVolatility") && !j["impliedVolatility"].is_null()) {
        s.implied_volatility = double_from_json(j, "impliedVolatility");
    }
}

using snapshots_by_symbol = symbol_map<snapshot>;
using option_snapshots_by_symbol = symbol_map<option_snapshot>;

/// One page of an option chain. The chain can run to thousands of contracts, so the
/// paginated form is exposed alongside the fetch-everything convenience method.
struct option_chain_page {
    option_snapshots_by_symbol snapshots;
    std::string next_page_token;
};

inline void from_json(const json &j, option_chain_page &p) {
    if (j.contains("snapshots") && j["snapshots"].is_object()) {
        p.snapshots = j["snapshots"].get<option_snapshots_by_symbol>();
    }
    if (j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        p.next_page_token = j["next_page_token"].get<std::string>();
    }
}

}   // namespace alpaca
