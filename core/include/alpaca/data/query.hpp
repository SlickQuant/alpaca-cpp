// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <alpaca/common.hpp>
#include <alpaca/data/timeframe.hpp>
#include <alpaca/utils.hpp>

namespace alpaca {

/// Query parameters shared by every historical Market Data endpoint.
///
/// Bars, trades, quotes and auctions all take the same symbols/window/paging/feed set, so
/// it is declared once here and the endpoint-specific queries add only their own fields.
struct history_query {
    /// Required by the multi-symbol endpoints; ignored by the single-symbol variants,
    /// which take the symbol in the path.
    std::vector<std::string> symbols;
    /// RFC-3339 or YYYY-MM-DD. Defaults to the beginning of the current day.
    std::optional<std::string> start;
    /// RFC-3339 or YYYY-MM-DD. Defaults to now, or 15 minutes ago without a subscription.
    std::optional<std::string> end;
    /// 1-10000, default 1000.
    std::optional<uint32_t> limit;
    std::optional<std::string> page_token;
    std::optional<alpaca::sort_direction> sort;
    /// `iex` is the only feed available without a data subscription.
    std::optional<alpaca::data_feed> feed;
    /// ISO 4217, default USD.
    std::optional<std::string> currency;
    /// YYYY-MM-DD symbol-mapping date.
    std::optional<std::string> asof;

    void add_to(query_builder &q) const {
        q.add("symbols", symbols);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("page_token", page_token);
        q.add("sort", sort);
        q.add("feed", feed);
        q.add("currency", currency);
        q.add("asof", asof);
    }

    query_builder build() const {
        query_builder q;
        add_to(q);
        return q;
    }
};

/// `history_query` plus the bar-specific interval and corporate-action adjustment.
struct bar_query : history_query {
    alpaca::timeframe timeframe = timeframes::one_day;
    std::optional<alpaca::adjustment> adjustment;

    query_builder build() const {
        query_builder q;
        add_to(q);
        q.add("timeframe", timeframe.to_string());
        q.add("adjustment", adjustment);
        return q;
    }
};

/// Query for the `latest*` endpoints, which take no time window.
struct latest_query {
    std::vector<std::string> symbols;
    std::optional<alpaca::data_feed> feed;
    std::optional<std::string> currency;

    query_builder build() const {
        query_builder q;
        q.add("symbols", symbols);
        q.add("feed", feed);
        q.add("currency", currency);
        return q;
    }
};

/// Options historical queries take an `opra`/`indicative` feed rather than a stock feed.
struct option_history_query {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<uint32_t> limit;
    std::optional<std::string> page_token;
    std::optional<alpaca::sort_direction> sort;

    void add_to(query_builder &q) const {
        q.add("symbols", symbols);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("page_token", page_token);
        q.add("sort", sort);
    }

    query_builder build() const {
        query_builder q;
        add_to(q);
        return q;
    }
};

struct option_bar_query : option_history_query {
    alpaca::timeframe timeframe = timeframes::one_day;

    query_builder build() const {
        query_builder q;
        add_to(q);
        q.add("timeframe", timeframe.to_string());
        return q;
    }
};

struct option_latest_query {
    std::vector<std::string> symbols;
    std::optional<alpaca::option_feed> feed;

    query_builder build() const {
        query_builder q;
        q.add("symbols", symbols);
        q.add("feed", feed);
        return q;
    }
};

/// Query for `GET /v1beta1/options/snapshots/{underlying_symbol}` (the option chain).
struct option_chain_query {
    std::optional<alpaca::option_feed> feed;
    /// 1-1000, default 100.
    std::optional<uint32_t> limit;
    std::optional<std::string> updated_since;
    std::optional<std::string> page_token;
    std::optional<alpaca::contract_type> type;
    std::optional<double> strike_price_gte;
    std::optional<double> strike_price_lte;
    std::optional<std::string> expiration_date;
    std::optional<std::string> expiration_date_gte;
    std::optional<std::string> expiration_date_lte;
    std::optional<std::string> root_symbol;

    query_builder build() const {
        query_builder q;
        q.add("feed", feed);
        q.add("limit", limit);
        q.add("updated_since", updated_since);
        q.add("page_token", page_token);
        q.add("type", type);
        q.add("strike_price_gte", strike_price_gte);
        q.add("strike_price_lte", strike_price_lte);
        q.add("expiration_date", expiration_date);
        q.add("expiration_date_gte", expiration_date_gte);
        q.add("expiration_date_lte", expiration_date_lte);
        q.add("root_symbol", root_symbol);
        return q;
    }
};

/// Crypto historical queries have no feed or currency; the venue is in the path.
struct crypto_history_query {
    std::vector<std::string> symbols;
    std::optional<std::string> start;
    std::optional<std::string> end;
    std::optional<uint32_t> limit;
    std::optional<std::string> page_token;
    std::optional<alpaca::sort_direction> sort;

    void add_to(query_builder &q) const {
        q.add("symbols", symbols);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("page_token", page_token);
        q.add("sort", sort);
    }

    query_builder build() const {
        query_builder q;
        add_to(q);
        return q;
    }
};

struct crypto_bar_query : crypto_history_query {
    alpaca::timeframe timeframe = timeframes::one_day;

    query_builder build() const {
        query_builder q;
        add_to(q);
        q.add("timeframe", timeframe.to_string());
        return q;
    }
};

}   // namespace alpaca
