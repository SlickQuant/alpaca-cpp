// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace alpaca::detail {

using json = nlohmann::json;

/// Extracts an array-valued response into a vector, tolerating a null or absent body.
/// A 204 or an unexpected shape yields an empty vector — a genuine failure has already
/// been turned into an `api_error` by `request_context`, so this never hides an error.
template <typename T>
std::vector<T> to_vector(const json &j) {
    if (!j.is_array()) {
        return {};
    }
    return j.get<std::vector<T>>();
}

/// Extracts an object-valued response into a model.
template <typename T>
T to_model(const json &j) {
    T value{};
    if (j.is_object()) {
        from_json(j, value);
    }
    return value;
}

/// Appends every element of `page[key]` onto `out`, when present and array-shaped.
/// Shared by the paginating list_* methods on both the sync and awaitable clients.
template <typename T>
void append_page(const json &page, std::string_view key, std::vector<T> &out) {
    if (!page.is_object() || !page.contains(key) || !page[key].is_array()) {
        return;
    }
    auto batch = page[key].template get<std::vector<T>>();
    out.insert(out.end(),
               std::make_move_iterator(batch.begin()),
               std::make_move_iterator(batch.end()));
}

/// Merges one page of a symbol-keyed Market Data response into an accumulating map.
///
/// The historical endpoints answer `{"bars": {"AAPL": [...], "MSFT": [...]}}` and page
/// across symbols, so a symbol can appear on several consecutive pages. Concatenating per
/// symbol — rather than assigning — is what keeps a multi-symbol history intact; Alpaca
/// sorts by symbol then timestamp, so appending preserves the requested ordering.
template <typename T>
void merge_symbol_page(const json &page, std::string_view key,
                       std::unordered_map<std::string, std::vector<T>> &out) {
    if (!page.is_object() || !page.contains(key) || !page[key].is_object()) {
        return;
    }
    for (const auto &[symbol, entries] : page[key].items()) {
        if (!entries.is_array()) {
            continue;
        }
        auto batch = entries.template get<std::vector<T>>();
        auto &target = out[symbol];
        target.insert(target.end(),
                      std::make_move_iterator(batch.begin()),
                      std::make_move_iterator(batch.end()));
    }
}

/// Extracts a symbol-keyed map of single values, as returned by the `latest*` endpoints.
template <typename T>
std::unordered_map<std::string, T> to_symbol_map(const json &j, std::string_view key) {
    if (!j.is_object() || !j.contains(key) || !j[key].is_object()) {
        return {};
    }
    return j[key].template get<std::unordered_map<std::string, T>>();
}

}   // namespace alpaca::detail
