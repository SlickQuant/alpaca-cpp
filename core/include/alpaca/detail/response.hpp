// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <iterator>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace alpaca::detail {

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

}   // namespace alpaca::detail
