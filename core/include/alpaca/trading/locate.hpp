// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// A hard-to-borrow share locate. `GET`/`POST /v1/locates`.
struct locate {
    std::string id;
    std::string symbol;
    std::string status;             ///< "active", "expired", "rejected"
    std::string rejection_reason;
    int64_t requested_qty = 0;
    std::optional<int64_t> located_qty;
    bool all_or_none = false;
    std::optional<double> limit_price;   ///< max acceptable fee per share
    std::optional<double> located_price; ///< actual fee per share, USD
    std::optional<double> total_fee;     ///< total locate fee, USD
    uint64_t created_at = 0;
    uint64_t expires_at = 0;
};

inline void from_json(const json &j, locate &l) {
    VARIABLE_FROM_JSON(j, l, id);
    VARIABLE_FROM_JSON(j, l, symbol);
    VARIABLE_FROM_JSON(j, l, status);
    VARIABLE_FROM_JSON(j, l, rejection_reason);
    INT_FROM_JSON(j, l, requested_qty);
    if (j.contains("located_qty") && !j["located_qty"].is_null()) {
        l.located_qty = int_from_json(j, "located_qty");
    }
    BOOL_FROM_JSON(j, l, all_or_none);
    OPTIONAL_DOUBLE_FROM_JSON(j, l, limit_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, l, located_price);
    OPTIONAL_DOUBLE_FROM_JSON(j, l, total_fee);
    TIMESTAMP_FROM_JSON(j, l, created_at);
    TIMESTAMP_FROM_JSON(j, l, expires_at);
}

/// An indicative borrow quote. `GET /v1/locates/quotes`.
struct locate_quote {
    std::string symbol;
    double price = 0.;          ///< fee per share, USD
    int64_t available_qty = 0;
    uint64_t as_of = 0;
};

inline void from_json(const json &j, locate_quote &q) {
    VARIABLE_FROM_JSON(j, q, symbol);
    DOUBLE_FROM_JSON(j, q, price);
    INT_FROM_JSON(j, q, available_qty);
    TIMESTAMP_FROM_JSON(j, q, as_of);
}

/// Query parameters for `GET /v1/locates`.
struct locate_query {
    std::optional<std::string> symbol;
    std::optional<std::string> status;      ///< "active", "expired", "rejected"
    std::optional<std::string> start;       ///< YYYY-MM-DD, inclusive
    std::optional<std::string> end;         ///< YYYY-MM-DD, exclusive
    std::optional<uint32_t> limit;          ///< default 1000, max 10000
    std::optional<std::string> page_token;

    query_builder build() const {
        query_builder q;
        q.add("symbol", symbol);
        q.add("status", status);
        q.add("start", start);
        q.add("end", end);
        q.add("limit", limit);
        q.add("page_token", page_token);
        return q;
    }
};

/// Body for `POST /v1/locates`.
struct locate_request {
    std::string symbol;
    int64_t requested_qty = 0;
    std::optional<bool> all_or_none;
    std::optional<double> limit_price;

    json to_json() const {
        json j{{"symbol", symbol}, {"requested_qty", requested_qty}};
        if (all_or_none)  j["all_or_none"] = *all_or_none;
        if (limit_price)  j["limit_price"] = alpaca::to_string(*limit_price);
        return j;
    }
};

/// One page of `GET /v1/locates`.
struct locate_page {
    std::vector<locate> locates;
    std::string next_page_token;
};

inline void from_json(const json &j, locate_page &p) {
    if (j.contains("locates") && j["locates"].is_array()) {
        p.locates = j["locates"].get<std::vector<locate>>();
    }
    if (j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        p.next_page_token = j["next_page_token"].get<std::string>();
    }
}

}   // namespace alpaca
