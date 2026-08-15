// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

/// One trading session. `GET /v2/calendar`.
///
/// Alpaca reports `open`/`close` as bare `HH:MM` strings in US/Eastern, so both the raw
/// string and a nanoseconds-since-midnight form are kept — the string is what round-trips
/// back into a request, the integer is what arithmetic wants.
struct calendar_day {
    std::string date;               ///< YYYY-MM-DD
    std::string open;               ///< HH:MM, US/Eastern
    std::string close;              ///< HH:MM, US/Eastern
    std::string settlement_date;    ///< YYYY-MM-DD
    uint64_t date_ns = 0;           ///< `date` as nanoseconds since the Unix epoch (UTC midnight)
    uint64_t open_ns = 0;           ///< `open` as nanoseconds since midnight
    uint64_t close_ns = 0;          ///< `close` as nanoseconds since midnight
};

inline void from_json(const json &j, calendar_day &c) {
    VARIABLE_FROM_JSON(j, c, date);
    VARIABLE_FROM_JSON(j, c, open);
    VARIABLE_FROM_JSON(j, c, close);
    VARIABLE_FROM_JSON(j, c, settlement_date);
    c.date_ns = date_to_nanoseconds(c.date);
    c.open_ns = time_of_day_to_nanoseconds(c.open);
    c.close_ns = time_of_day_to_nanoseconds(c.close);
}

/// Query parameters for `GET /v2/calendar`.
struct calendar_query {
    std::optional<std::string> start;       ///< YYYY-MM-DD
    std::optional<std::string> end;         ///< YYYY-MM-DD
    std::optional<std::string> date_type;   ///< "TRADING" or "SETTLEMENT"

    query_builder build() const {
        query_builder q;
        q.add("start", start);
        q.add("end", end);
        q.add("date_type", date_type);
        return q;
    }
};

/// Current market state. `GET /v2/clock`.
struct market_clock {
    uint64_t timestamp = 0;     ///< nanoseconds since the Unix epoch
    uint64_t next_open = 0;
    uint64_t next_close = 0;
    bool is_open = false;
};

inline void from_json(const json &j, market_clock &c) {
    TIMESTAMP_FROM_JSON(j, c, timestamp);
    TIMESTAMP_FROM_JSON(j, c, next_open);
    TIMESTAMP_FROM_JSON(j, c, next_close);
    BOOL_FROM_JSON(j, c, is_open);
}

}   // namespace alpaca
