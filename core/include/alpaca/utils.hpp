// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <slick/net/logging.hpp>

namespace alpaca {
    
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

/// Reads an environment variable, returning an empty string when unset.
std::string get_env(std::string_view name);

// ---------------------------------------------------------------------------
// Timestamps
//
// Alpaca emits RFC-3339 timestamps with a variable number of sub-second digits
// (none, milli, micro or nano) and either a `Z` suffix or a numeric UTC offset.
// Everything is normalised to nanoseconds since the Unix epoch, which is what
// the models store. Parsing is hand-rolled and allocation-free: these run on
// every websocket message, so a stringstream-based parser is not acceptable.
// ---------------------------------------------------------------------------

/// Parses an RFC-3339 timestamp to nanoseconds since the Unix epoch.
/// Returns 0 for an empty or unparseable input.
uint64_t to_nanoseconds(std::string_view iso);

/// Parses an RFC-3339 timestamp to microseconds since the Unix epoch.
uint64_t to_microseconds(std::string_view iso);

/// Parses an RFC-3339 timestamp to milliseconds since the Unix epoch.
uint64_t to_milliseconds(std::string_view iso);

/// Parses a bare `YYYY-MM-DD` calendar date to nanoseconds since the Unix epoch (UTC midnight).
uint64_t date_to_nanoseconds(std::string_view yyyy_mm_dd);

/// Parses a bare `HH:MM` clock time to nanoseconds since midnight.
uint64_t time_of_day_to_nanoseconds(std::string_view hh_mm);

/// Formats nanoseconds since the Unix epoch as `YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ`.
std::string to_rfc3339(uint64_t nanoseconds);

/// Formats nanoseconds since the Unix epoch as a bare `YYYY-MM-DD` date.
std::string to_date_string(uint64_t nanoseconds);

// ---------------------------------------------------------------------------
// JSON field extraction
//
// The Trading API returns numeric fields as JSON *strings* ("qty": "1.5") while
// the Market Data API returns them as JSON *numbers* ("p": 178.26). Every helper
// below accepts either form so the same macros work across both APIs.
// ---------------------------------------------------------------------------

inline uint64_t nanoseconds_from_json(const json &j, std::string_view field) {
    const auto &v = j.at(field);
    return v.is_null() ? 0 : to_nanoseconds(v.get<std::string_view>());
}

inline uint64_t microseconds_from_json(const json &j, std::string_view field) {
    const auto &v = j.at(field);
    return v.is_null() ? 0 : to_microseconds(v.get<std::string_view>());
}

inline uint64_t milliseconds_from_json(const json &j, std::string_view field) {
    const auto &v = j.at(field);
    return v.is_null() ? 0 : to_milliseconds(v.get<std::string_view>());
}

inline double double_from_json(const json &j, std::string_view field) {
    try {
        const auto &v = j.at(field);
        if (v.is_null()) {
            return 0.;
        }
        if (v.is_number()) {
            return v.get<double>();
        }
        auto s = v.get<std::string_view>();
        if (s.empty()) {
            return 0.;
        }
        return std::stod(std::string(s));
    }
    catch (const std::exception &e) {
        LOG_ERROR("double_from_json exception: {}. field: {} {}", e.what(), field, j.dump());
    }
    return 0.;
}

inline int64_t int_from_json(const json &j, std::string_view field) {
    try {
        const auto &v = j.at(field);
        if (v.is_null()) {
            return 0;
        }
        if (v.is_number()) {
            return v.get<int64_t>();
        }
        auto s = v.get<std::string_view>();
        if (s.empty()) {
            return 0;
        }
        return std::stoll(std::string(s));
    }
    catch (const std::exception &e) {
        LOG_ERROR("int_from_json exception: {}. field: {} {}", e.what(), field, j.dump());
    }
    return 0;
}

inline bool bool_from_json(const json &j, std::string_view field) {
    try {
        const auto &v = j.at(field);
        if (v.is_null()) {
            return false;
        }
        if (v.is_boolean()) {
            return v.get<bool>();
        }
        if (v.is_string()) {
            return v.get<std::string_view>() == "true";
        }
        if (v.is_number()) {
            return v.get<double>() != 0.;
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR("bool_from_json exception: {}. field: {} {}", e.what(), field, j.dump());
    }
    return false;
}

/// Renders a scalar field as a string whether Alpaca quoted it or not.
///
/// The Trading API is inconsistent about this even within a single response: the account
/// payload sends `"multiplier":"4"` but `"crypto_tier":1` and `"options_approved_level":3`,
/// and option contracts send `"ppind":true`. Reading any of those with a string-typed
/// extractor throws inside the macro, which logs and moves on — leaving the field silently
/// unset rather than failing loudly. Going through here accepts either spelling.
inline std::string string_from_json(const json &j, std::string_view field) {
    const auto &v = j.at(field);
    if (v.is_null()) {
        return {};
    }
    // dump() renders 3 as "3" and true as "true", which is exactly the wire vocabulary
    // the enum tables are written against.
    return v.is_string() ? v.get<std::string>() : v.dump();
}

// ---------------------------------------------------------------------------
// Field extraction macros
//
// Every macro is a no-op when the field is absent, so a model never has to be
// updated in lockstep with Alpaca adding or removing optional fields.
// ---------------------------------------------------------------------------

/// String field that may arrive unquoted. See `string_from_json`.
#define STRING_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) (o).field = alpaca::string_from_json(j, #field)

/// Plain assignment via nlohmann's get_to. Skips null and missing fields.
#define VARIABLE_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) \
        try { (j).at(#field).get_to((o).field); } \
        catch (const std::exception &e) { LOG_ERROR("VARIABLE_FROM_JSON " #field ": {} {}", e.what(), (j).dump()); }

/// Same as VARIABLE_FROM_JSON but reads from a differently named JSON key.
#define VARIABLE_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key) && !(j)[key].is_null()) \
        try { (j).at(key).get_to((o).field); } \
        catch (const std::exception &e) { LOG_ERROR("VARIABLE_FROM_JSON_KEY " key ": {} {}", e.what(), (j).dump()); }

#define DOUBLE_FROM_JSON(j, o, field) \
    if ((j).contains(#field)) (o).field = alpaca::double_from_json(j, #field)

#define DOUBLE_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key)) (o).field = alpaca::double_from_json(j, key)

#define INT_FROM_JSON(j, o, field) \
    if ((j).contains(#field)) (o).field = static_cast<decltype((o).field)>(alpaca::int_from_json(j, #field))

#define INT_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key)) (o).field = static_cast<decltype((o).field)>(alpaca::int_from_json(j, key))

#define BOOL_FROM_JSON(j, o, field) \
    if ((j).contains(#field)) (o).field = alpaca::bool_from_json(j, #field)

#define BOOL_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key)) (o).field = alpaca::bool_from_json(j, key)

#define TIMESTAMP_FROM_JSON(j, o, field) \
    if ((j).contains(#field)) (o).field = alpaca::nanoseconds_from_json(j, #field)

#define TIMESTAMP_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key)) (o).field = alpaca::nanoseconds_from_json(j, key)

/// Nullable numeric field, e.g. `limit_price` which is null on market orders.
#define OPTIONAL_DOUBLE_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) (o).field = alpaca::double_from_json(j, #field)

/// Nullable non-numeric field.
#define OPTIONAL_VARIABLE_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) \
        try { (o).field = (j).at(#field).get<typename std::decay_t<decltype((o).field)>::value_type>(); } \
        catch (const std::exception &e) { LOG_ERROR("OPTIONAL_VARIABLE_FROM_JSON " #field ": {} {}", e.what(), (j).dump()); }

/// Nested object parsed by its own from_json overload.
#define STRUCT_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) from_json((j)[#field], (o).field)

#define STRUCT_FROM_JSON_KEY(j, o, field, key) \
    if ((j).contains(key) && !(j)[key].is_null()) from_json((j)[key], (o).field)

/// Enum field converted through the matching `to_<field>` free function.
#define ENUM_FROM_JSON(j, o, field) \
    if ((j).contains(#field) && !(j)[#field].is_null()) \
        try { (o).field = to_##field((j).at(#field).get<std::string_view>()); } \
        catch (const std::exception &e) { LOG_ERROR("ENUM_FROM_JSON " #field ": {} {}", e.what(), (j).dump()); }

/// Enum field converted through an explicitly named converter.
#define ENUM_FROM_JSON_WITH(j, o, field, converter) \
    if ((j).contains(#field) && !(j)[#field].is_null()) \
        try { (o).field = converter((j).at(#field).get<std::string_view>()); } \
        catch (const std::exception &e) { LOG_ERROR("ENUM_FROM_JSON_WITH " #field ": {} {}", e.what(), (j).dump()); }

/// Enum field whose wire value may arrive unquoted, e.g. the account's options approval
/// level, whose enumerators are "0".."3" but which Alpaca sends as a bare number.
#define ENUM_FROM_JSON_SCALAR_WITH(j, o, field, converter) \
    if ((j).contains(#field) && !(j)[#field].is_null()) \
        try { (o).field = converter(alpaca::string_from_json(j, #field)); } \
        catch (const std::exception &e) { LOG_ERROR("ENUM_FROM_JSON_SCALAR_WITH " #field ": {} {}", e.what(), (j).dump()); }

/// Enum field whose JSON key differs from the member name, e.g. the assets endpoint
/// spells `asset_class` as `"class"`.
#define ENUM_FROM_JSON_KEY(j, o, field, key, converter) \
    if ((j).contains(key) && !(j)[key].is_null()) \
        try { (o).field = converter((j).at(key).get<std::string_view>()); } \
        catch (const std::exception &e) { LOG_ERROR("ENUM_FROM_JSON_KEY " key ": {} {}", e.what(), (j).dump()); }

// ---------------------------------------------------------------------------
// Floating point helpers
// ---------------------------------------------------------------------------

inline constexpr double epsilon = 1e-9;
inline constexpr double default_norm_factor = 1e8;

/// Trims accumulated binary floating point noise, e.g. 0.1 + 0.2 -> 0.3.
inline double fix_floating_error(double value, double norm_factor = default_norm_factor) {
    auto v = value + (value < 0 ? -epsilon : epsilon);
    return static_cast<double>(static_cast<int64_t>(v * norm_factor)) / norm_factor;
}

/// Number of decimal places needed to represent `value` exactly.
inline uint32_t compute_number_decimals(double value) {
    value = std::abs(fix_floating_error(value));
    uint32_t count = 0;
    while (value - std::floor(value) > epsilon && count < 18) {
        ++count;
        value *= 10;
    }
    return count;
}

/// Formats a double without scientific notation and without trailing zeros.
/// Alpaca rejects `1e-05` style payloads for qty/price fields, so this is used
/// everywhere a number is serialised into a request body or query string.
std::string to_string(double value);

// ---------------------------------------------------------------------------
// query_builder
//
// Replaces the per-struct `operator()` + std::accumulate boilerplate: one place
// owns optional skipping, vector->CSV flattening and percent-encoding. Values
// are kept as key/value pairs rather than a pre-joined string so pagination can
// replace `page_token` in place.
// ---------------------------------------------------------------------------

class query_builder {
public:
    query_builder() = default;

    query_builder& add(std::string_view key, std::string_view value) {
        if (!value.empty()) {
            params_.emplace_back(std::string(key), std::string(value));
        }
        return *this;
    }

    query_builder& add(std::string_view key, const char *value) {
        return value ? add(key, std::string_view(value)) : *this;
    }

    query_builder& add(std::string_view key, const std::string &value) {
        return add(key, std::string_view(value));
    }

    query_builder& add(std::string_view key, bool value) {
        params_.emplace_back(std::string(key), value ? "true" : "false");
        return *this;
    }

    query_builder& add(std::string_view key, double value) {
        params_.emplace_back(std::string(key), alpaca::to_string(value));
        return *this;
    }

    template <typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    query_builder& add(std::string_view key, T value) {
        params_.emplace_back(std::string(key), std::to_string(value));
        return *this;
    }

    template <typename T>
        requires std::is_enum_v<T>
    query_builder& add(std::string_view key, T value) {
        return add(key, to_string(value));
    }

    /// Optionals are skipped entirely when unset.
    template <typename T>
    query_builder& add(std::string_view key, const std::optional<T> &value) {
        return value.has_value() ? add(key, value.value()) : *this;
    }

    /// Flattens to a comma separated list, e.g. `symbols=AAPL,MSFT`.
    query_builder& add(std::string_view key, const std::vector<std::string> &values) {
        if (values.empty()) {
            return *this;
        }
        std::string csv;
        for (const auto &v : values) {
            if (!csv.empty()) {
                csv.push_back(',');
            }
            csv += v;
        }
        params_.emplace_back(std::string(key), std::move(csv));
        return *this;
    }

    /// Adds an RFC-3339 formatted timestamp; 0 is treated as unset.
    query_builder& add_timestamp(std::string_view key, uint64_t nanoseconds) {
        if (nanoseconds != 0) {
            params_.emplace_back(std::string(key), to_rfc3339(nanoseconds));
        }
        return *this;
    }

    /// Inserts or replaces a key. Used by pagination to swap `page_token`.
    query_builder& set(std::string_view key, std::string_view value) {
        for (auto &p : params_) {
            if (p.first == key) {
                p.second = std::string(value);
                return *this;
            }
        }
        return add(key, value);
    }

    void remove(std::string_view key) {
        std::erase_if(params_, [key](const auto &p) { return p.first == key; });
    }

    bool empty() const noexcept { return params_.empty(); }

    /// Returns "" when empty, otherwise "?k=v&k2=v2" with values percent-encoded.
    std::string str() const;

private:
    std::vector<std::pair<std::string, std::string>> params_;
};

/// Percent-encodes a string for use in a query string or path segment.
std::string url_encode(std::string_view value);

}   // namespace alpaca
