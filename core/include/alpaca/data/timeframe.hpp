// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>

namespace alpaca {

enum class timeframe_unit : uint8_t {
    minute,
    hour,
    day,
    week,
    month,
};

inline std::string_view to_string_view(timeframe_unit unit) noexcept {
    switch (unit) {
    case timeframe_unit::minute: return "Min";
    case timeframe_unit::hour:   return "Hour";
    case timeframe_unit::day:    return "Day";
    case timeframe_unit::week:   return "Week";
    case timeframe_unit::month:  return "Month";
    }
    return "Day";
}

/// Bar aggregation interval, serialised as Alpaca's `1Min` / `5Hour` / `1Day` form.
///
/// Alpaca constrains the amount per unit: 1-59 for minutes, 1-23 for hours, and 1 for
/// day, week and month. `valid()` reports whether an interval satisfies that, so callers
/// can check before paying for a round trip that would come back 422.
struct timeframe {
    uint32_t amount = 1;
    timeframe_unit unit = timeframe_unit::day;

    constexpr timeframe() = default;
    constexpr timeframe(uint32_t n, timeframe_unit u) noexcept : amount(n), unit(u) {}

    std::string to_string() const {
        return std::to_string(amount) + std::string(to_string_view(unit));
    }

    bool valid() const noexcept {
        switch (unit) {
        case timeframe_unit::minute: return amount >= 1 && amount <= 59;
        case timeframe_unit::hour:   return amount >= 1 && amount <= 23;
        case timeframe_unit::day:
        case timeframe_unit::week:
        case timeframe_unit::month:  return amount == 1;
        }
        return false;
    }

    friend bool operator==(const timeframe &, const timeframe &) = default;

    static constexpr timeframe minutes(uint32_t n) noexcept { return {n, timeframe_unit::minute}; }
    static constexpr timeframe hours(uint32_t n) noexcept { return {n, timeframe_unit::hour}; }
};

inline std::string to_string(const timeframe &tf) { return tf.to_string(); }

/// The intervals Alpaca documents; any other valid combination works via `timeframe`.
namespace timeframes {
inline constexpr timeframe one_minute{1, timeframe_unit::minute};
inline constexpr timeframe five_minutes{5, timeframe_unit::minute};
inline constexpr timeframe fifteen_minutes{15, timeframe_unit::minute};
inline constexpr timeframe thirty_minutes{30, timeframe_unit::minute};
inline constexpr timeframe one_hour{1, timeframe_unit::hour};
inline constexpr timeframe four_hours{4, timeframe_unit::hour};
inline constexpr timeframe one_day{1, timeframe_unit::day};
inline constexpr timeframe one_week{1, timeframe_unit::week};
inline constexpr timeframe one_month{1, timeframe_unit::month};
}   // namespace timeframes

}   // namespace alpaca
