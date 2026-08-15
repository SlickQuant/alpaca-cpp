// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/trading/asset.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// A named list of assets. `GET`/`POST /v2/watchlists`.
///
/// `assets` is only populated on the single-watchlist endpoints; the list endpoint
/// returns watchlists without their contents.
struct watchlist {
    std::string id;
    std::string account_id;
    std::string name;
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
    std::vector<asset> assets;
};

inline void from_json(const json &j, watchlist &w) {
    VARIABLE_FROM_JSON(j, w, id);
    VARIABLE_FROM_JSON(j, w, account_id);
    VARIABLE_FROM_JSON(j, w, name);
    TIMESTAMP_FROM_JSON(j, w, created_at);
    TIMESTAMP_FROM_JSON(j, w, updated_at);
    if (j.contains("assets") && j["assets"].is_array()) {
        w.assets = j["assets"].get<std::vector<asset>>();
    }
}

/// Body for creating or replacing a watchlist.
struct watchlist_request {
    std::string name;
    std::vector<std::string> symbols;

    json to_json() const {
        return json{{"name", name}, {"symbols", symbols}};
    }
};

}   // namespace alpaca
