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

#define ALPACA_CORPORATE_ACTION_TYPE(X, N)                      \
    X(N, reverse_split,          "reverse_split")               \
    X(N, forward_split,          "forward_split")               \
    X(N, unit_split,             "unit_split")                  \
    X(N, cash_dividend,          "cash_dividend")               \
    X(N, stock_dividend,         "stock_dividend")              \
    X(N, spin_off,               "spin_off")                    \
    X(N, cash_merger,            "cash_merger")                 \
    X(N, stock_merger,           "stock_merger")                \
    X(N, stock_and_cash_merger,  "stock_and_cash_merger")       \
    X(N, redemption,             "redemption")                  \
    X(N, name_change,            "name_change")                 \
    X(N, worthless_removal,      "worthless_removal")           \
    X(N, rights_distribution,    "rights_distribution")         \
    X(N, partial_call,           "partial_call")                \
    X(N, reorganization,         "reorganization")
ALPACA_DEFINE_ENUM(corporate_action_type, ALPACA_CORPORATE_ACTION_TYPE)

/// A corporate action. `GET /v1/corporate-actions`.
///
/// Alpaca groups the response into fifteen arrays by type, each with a partly overlapping
/// field set. Rather than fifteen structs the caller has to switch between, this is one
/// inclusive struct with `type` as the discriminator — the same approach `account_activity`
/// takes for trade vs non-trade activities. Fields that do not apply to a given type stay
/// at their defaults, and the type-specific ones are documented below.
struct corporate_action {
    corporate_action_type type = corporate_action_type::unknown;
    std::string id;
    std::string symbol;
    std::string cusip;

    // Dates, as YYYY-MM-DD strings plus a nanosecond form for arithmetic.
    std::string process_date;
    std::string ex_date;
    std::string record_date;
    std::string payable_date;
    std::string effective_date;
    uint64_t process_date_ns = 0;
    uint64_t ex_date_ns = 0;
    uint64_t record_date_ns = 0;
    uint64_t payable_date_ns = 0;
    uint64_t effective_date_ns = 0;

    /// Splits and stock dividends: the conversion ratio.
    std::optional<double> rate;
    std::optional<double> old_rate;
    std::optional<double> new_rate;

    /// Cash dividends, cash mergers, redemptions and partial calls.
    std::optional<double> cash_rate;
    /// Cash dividends. Alpaca sends these as JSON booleans, not as the "true"/"false"
    /// strings most of this endpoint's other scalars use.
    bool special = false;
    bool foreign = false;
    /// Splits, mergers, name changes and spin-offs.
    std::string old_symbol;
    std::string new_symbol;
    std::string source_symbol;
    std::string new_cusip;
    std::string old_cusip;
    std::string source_cusip;
    std::string target_symbol;
    std::string target_cusip;
    std::string initiating_symbol;
    std::string initiating_cusip;
};

inline void from_json(const json &j, corporate_action &c) {
    VARIABLE_FROM_JSON(j, c, id);
    VARIABLE_FROM_JSON(j, c, symbol);
    VARIABLE_FROM_JSON(j, c, cusip);

    VARIABLE_FROM_JSON(j, c, process_date);
    VARIABLE_FROM_JSON(j, c, ex_date);
    VARIABLE_FROM_JSON(j, c, record_date);
    VARIABLE_FROM_JSON(j, c, payable_date);
    VARIABLE_FROM_JSON(j, c, effective_date);
    c.process_date_ns = date_to_nanoseconds(c.process_date);
    c.ex_date_ns = date_to_nanoseconds(c.ex_date);
    c.record_date_ns = date_to_nanoseconds(c.record_date);
    c.payable_date_ns = date_to_nanoseconds(c.payable_date);
    c.effective_date_ns = date_to_nanoseconds(c.effective_date);

    OPTIONAL_DOUBLE_FROM_JSON(j, c, rate);
    OPTIONAL_DOUBLE_FROM_JSON(j, c, old_rate);
    OPTIONAL_DOUBLE_FROM_JSON(j, c, new_rate);
    OPTIONAL_DOUBLE_FROM_JSON(j, c, cash_rate);

    // BOOL_FROM_JSON, not VARIABLE_FROM_JSON: these arrive as real booleans, and reading
    // them as strings threw inside the macro, which logged and left both flags silently
    // unset on every cash dividend.
    BOOL_FROM_JSON(j, c, special);
    BOOL_FROM_JSON(j, c, foreign);
    VARIABLE_FROM_JSON(j, c, old_symbol);
    VARIABLE_FROM_JSON(j, c, new_symbol);
    VARIABLE_FROM_JSON(j, c, source_symbol);
    VARIABLE_FROM_JSON(j, c, new_cusip);
    VARIABLE_FROM_JSON(j, c, old_cusip);
    VARIABLE_FROM_JSON(j, c, source_cusip);
    VARIABLE_FROM_JSON(j, c, target_symbol);
    VARIABLE_FROM_JSON(j, c, target_cusip);
    VARIABLE_FROM_JSON(j, c, initiating_symbol);
    VARIABLE_FROM_JSON(j, c, initiating_cusip);
}

/// Maps Alpaca's plural response keys (`cash_dividends`) onto the singular enum
/// (`cash_dividend`). Every group name is the type name with a plain "s" appended, so
/// stripping one trailing character covers all fifteen.
inline corporate_action_type corporate_action_type_from_group(std::string_view group) noexcept {
    if (group.ends_with("s")) {
        return to_corporate_action_type(group.substr(0, group.size() - 1));
    }
    return to_corporate_action_type(group);
}

/// One page of `GET /v1/corporate-actions`, flattened across the per-type groups.
struct corporate_action_page {
    std::vector<corporate_action> actions;
    std::string next_page_token;
};

/// Flattens the `corporate_actions` object of grouped arrays into one typed vector.
inline void append_corporate_actions(const json &j, std::vector<corporate_action> &out) {
    if (!j.is_object() || !j.contains("corporate_actions") || !j["corporate_actions"].is_object()) {
        return;
    }
    for (const auto &[group, entries] : j["corporate_actions"].items()) {
        if (!entries.is_array()) {
            continue;
        }
        const auto type = corporate_action_type_from_group(group);
        for (const auto &entry : entries) {
            corporate_action action;
            from_json(entry, action);
            action.type = type;
            out.push_back(std::move(action));
        }
    }
}

inline void from_json(const json &j, corporate_action_page &p) {
    append_corporate_actions(j, p.actions);
    if (j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        p.next_page_token = j["next_page_token"].get<std::string>();
    }
}

/// Query parameters for `GET /v1/corporate-actions`.
struct corporate_action_query {
    std::vector<std::string> symbols;
    std::vector<std::string> cusips;
    std::vector<std::string> types;
    std::vector<std::string> ids;           ///< mutually exclusive with the other filters
    std::optional<std::string> region;      ///< "us" (default), "non_us" or "all"
    std::optional<std::string> start;       ///< YYYY-MM-DD, inclusive
    std::optional<std::string> end;         ///< YYYY-MM-DD, inclusive
    /// 1-1000, default 100.
    std::optional<uint32_t> limit;
    std::optional<std::string> data_quality; ///< "complete" (default) or "all"
    std::optional<std::string> page_token;
    std::optional<alpaca::sort_direction> sort;

    query_builder build() const {
        query_builder q;
        q.add("symbols", symbols);
        q.add("cusips", cusips);
        q.add("types", types);
        q.add("ids", ids);
        q.add("region", region);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("data_quality", data_quality);
        q.add("page_token", page_token);
        q.add("sort", sort);
        return q;
    }
};

}   // namespace alpaca
