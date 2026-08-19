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

/// What one contract delivers on exercise. Non-standard contracts (post corporate action)
/// can have several deliverables.
struct option_deliverable {
    std::string type;               ///< "cash" or "equity"
    std::string symbol;
    std::string asset_id;
    std::string settlement_type;    ///< "T+0" ... "T+5"
    std::string settlement_method;  ///< "BTOB", "CADF", "CAFX", "CCC"
    double amount = 0.;
    double allocation_percentage = 0.;
    bool delayed_settlement = false;
};

inline void from_json(const json &j, option_deliverable &d) {
    VARIABLE_FROM_JSON(j, d, type);
    VARIABLE_FROM_JSON(j, d, symbol);
    VARIABLE_FROM_JSON(j, d, asset_id);
    VARIABLE_FROM_JSON(j, d, settlement_type);
    VARIABLE_FROM_JSON(j, d, settlement_method);
    DOUBLE_FROM_JSON(j, d, amount);
    DOUBLE_FROM_JSON(j, d, allocation_percentage);
    BOOL_FROM_JSON(j, d, delayed_settlement);
}

/// An option contract. `GET /v2/options/contracts[/{symbol_or_id}]`.
struct option_contract {
    std::string id;
    std::string symbol;             ///< OCC symbol, e.g. AAPL240628C00200000
    std::string name;
    alpaca::asset_status status = alpaca::asset_status::unknown;
    bool tradable = false;

    std::string expiration_date;    ///< YYYY-MM-DD
    uint64_t expiration_date_ns = 0;
    std::string root_symbol;
    std::string underlying_symbol;
    std::string underlying_asset_id;

    alpaca::contract_type type = alpaca::contract_type::unknown;
    alpaca::contract_style style = alpaca::contract_style::unknown;
    double strike_price = 0.;
    double multiplier = 0.;
    double size = 0.;

    std::optional<double> open_interest;
    std::string open_interest_date;  ///< YYYY-MM-DD
    std::optional<double> close_price;
    std::string close_price_date;    ///< YYYY-MM-DD
    std::string ppind;

    std::vector<option_deliverable> deliverables;
};

inline void from_json(const json &j, option_contract &c) {
    VARIABLE_FROM_JSON(j, c, id);
    VARIABLE_FROM_JSON(j, c, symbol);
    VARIABLE_FROM_JSON(j, c, name);
    ENUM_FROM_JSON_WITH(j, c, status, to_asset_status);
    BOOL_FROM_JSON(j, c, tradable);

    VARIABLE_FROM_JSON(j, c, expiration_date);
    c.expiration_date_ns = date_to_nanoseconds(c.expiration_date);
    VARIABLE_FROM_JSON(j, c, root_symbol);
    VARIABLE_FROM_JSON(j, c, underlying_symbol);
    VARIABLE_FROM_JSON(j, c, underlying_asset_id);

    ENUM_FROM_JSON_WITH(j, c, type, to_contract_type);
    ENUM_FROM_JSON_WITH(j, c, style, to_contract_style);
    DOUBLE_FROM_JSON(j, c, strike_price);
    DOUBLE_FROM_JSON(j, c, multiplier);
    DOUBLE_FROM_JSON(j, c, size);

    OPTIONAL_DOUBLE_FROM_JSON(j, c, open_interest);
    VARIABLE_FROM_JSON(j, c, open_interest_date);
    OPTIONAL_DOUBLE_FROM_JSON(j, c, close_price);
    VARIABLE_FROM_JSON(j, c, close_price_date);
    STRING_FROM_JSON(j, c, ppind);      // arrives as a bare boolean

    if (j.contains("deliverables") && j["deliverables"].is_array()) {
        c.deliverables = j["deliverables"].get<std::vector<option_deliverable>>();
    }
}

/// Query parameters for `GET /v2/options/contracts`.
struct option_contract_query {
    std::vector<std::string> underlying_symbols;
    std::optional<std::string> show_deliverables;
    std::optional<alpaca::asset_status> status;
    std::optional<std::string> expiration_date;         ///< YYYY-MM-DD
    std::optional<std::string> expiration_date_gte;
    std::optional<std::string> expiration_date_lte;
    std::optional<std::string> root_symbol;
    std::optional<alpaca::contract_type> type;
    std::optional<alpaca::contract_style> style;
    std::optional<double> strike_price_gte;
    std::optional<double> strike_price_lte;
    std::optional<uint32_t> limit;                      ///< max 10000
    std::optional<uint32_t> ppind;
    std::optional<std::string> page_token;

    query_builder build() const {
        query_builder q;
        q.add("underlying_symbols", underlying_symbols);
        q.add("show_deliverables", show_deliverables);
        q.add("status", status);
        q.add("expiration_date", expiration_date);
        q.add("expiration_date_gte", expiration_date_gte);
        q.add("expiration_date_lte", expiration_date_lte);
        q.add("root_symbol", root_symbol);
        q.add("type", type);
        q.add("style", style);
        q.add("strike_price_gte", strike_price_gte);
        q.add("strike_price_lte", strike_price_lte);
        q.add("limit", limit);
        q.add("ppind", ppind);
        q.add("page_token", page_token);
        return q;
    }
};

/// One page of `GET /v2/options/contracts`.
struct option_contract_page {
    std::vector<option_contract> contracts;
    std::string next_page_token;
};

inline void from_json(const json &j, option_contract_page &p) {
    if (j.contains("option_contracts") && j["option_contracts"].is_array()) {
        p.contracts = j["option_contracts"].get<std::vector<option_contract>>();
    }
    if (j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        p.next_page_token = j["next_page_token"].get<std::string>();
    }
}

}   // namespace alpaca
