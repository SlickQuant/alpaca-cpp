// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/utils.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace alpaca {

namespace {

constexpr uint64_t nanos_per_second = 1'000'000'000ull;

/// Howard Hinnant's civil-from-days inverse: days since 1970-01-01 from a civil date.
constexpr int64_t days_from_civil(int64_t y, uint32_t m, uint32_t d) noexcept {
    y -= (m <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(y - era * 400);              // [0, 399]
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;    // [0, 365]
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;             // [0, 146096]
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

/// Howard Hinnant's civil_from_days: civil date from days since 1970-01-01.
constexpr void civil_from_days(int64_t z, int64_t &y, uint32_t &m, uint32_t &d) noexcept {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = static_cast<uint32_t>(z - era * 146097);           // [0, 146096]
    const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yr = static_cast<int64_t>(yoe) + era * 400;
    const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);           // [0, 365]
    const uint32_t mp = (5 * doy + 2) / 153;                                // [0, 11]
    d = doy - (153 * mp + 2) / 5 + 1;                                       // [1, 31]
    m = mp + (mp < 10 ? 3 : -9);                                            // [1, 12]
    y = yr + (m <= 2);
}

/// Reads exactly `count` decimal digits starting at `pos`. Returns false on any non-digit.
bool read_digits(std::string_view s, size_t pos, size_t count, uint32_t &out) noexcept {
    if (pos + count > s.size()) {
        return false;
    }
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) {
        const char c = s[pos + i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<uint32_t>(c - '0');
    }
    out = value;
    return true;
}

}   // namespace

namespace {

/// Strips surrounding whitespace from an environment value.
///
/// Credentials pick up a trailing newline remarkably easily — `set /p`, a copied line, a
/// value sourced from a file — and no Alpaca key or secret legitimately contains
/// whitespace at either end. Leaving it in produces a genuinely baffling failure: the
/// REST APIs still work, because the value goes into an HTTP header and the stray byte is
/// trimmed in transit, while the websocket streams reject it, because there the secret is
/// embedded in a JSON string and compared exactly. Trimming here fixes both at the source.
std::string trim(std::string value) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

    const auto begin = std::find_if_not(value.begin(), value.end(), is_space);
    const auto end = std::find_if_not(value.rbegin(),
                                      std::make_reverse_iterator(begin), is_space).base();
    return std::string(begin, end);
}

}   // namespace

std::string get_env(std::string_view name) {
    const std::string key(name);
#ifdef _MSC_VER
    char *buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, key.c_str()) != 0 || buffer == nullptr) {
        return {};
    }
    std::string value(buffer);
    std::free(buffer);
    return trim(std::move(value));
#else
    const char *value = std::getenv(key.c_str());
    return value ? trim(std::string(value)) : std::string{};
#endif
}

uint64_t to_nanoseconds(std::string_view iso) {
    // Expected shape: YYYY-MM-DDTHH:MM:SS[.fraction][Z|+HH:MM|-HH:MM]
    // A bare YYYY-MM-DD is also accepted and treated as UTC midnight.
    if (iso.size() < 10) {
        return 0;
    }

    uint32_t year = 0, month = 0, day = 0;
    if (!read_digits(iso, 0, 4, year) || iso[4] != '-' ||
        !read_digits(iso, 5, 2, month) || iso[7] != '-' ||
        !read_digits(iso, 8, 2, day)) {
        return 0;
    }

    const int64_t days = days_from_civil(year, month, day);
    int64_t seconds = days * 86400;

    if (iso.size() == 10) {
        return static_cast<uint64_t>(seconds) * nanos_per_second;
    }

    // Separator is 'T' in RFC-3339, but a space is tolerated.
    if (iso[10] != 'T' && iso[10] != 't' && iso[10] != ' ') {
        return 0;
    }

    uint32_t hour = 0, minute = 0, second = 0;
    if (!read_digits(iso, 11, 2, hour) || iso.size() < 16 || iso[13] != ':' ||
        !read_digits(iso, 14, 2, minute)) {
        return 0;
    }
    size_t pos = 16;
    if (pos < iso.size() && iso[pos] == ':') {
        if (!read_digits(iso, pos + 1, 2, second)) {
            return 0;
        }
        pos += 3;
    }

    seconds += static_cast<int64_t>(hour) * 3600 + static_cast<int64_t>(minute) * 60 + second;

    // Fractional seconds: keep at most 9 digits, right-pad shorter fractions.
    uint64_t fraction = 0;
    if (pos < iso.size() && iso[pos] == '.') {
        ++pos;
        uint32_t digits = 0;
        while (pos < iso.size() && iso[pos] >= '0' && iso[pos] <= '9') {
            if (digits < 9) {
                fraction = fraction * 10 + static_cast<uint64_t>(iso[pos] - '0');
                ++digits;
            }
            ++pos;
        }
        for (; digits < 9; ++digits) {
            fraction *= 10;
        }
    }

    // Timezone: 'Z' (or absent) means UTC, otherwise subtract the numeric offset.
    if (pos < iso.size() && (iso[pos] == '+' || iso[pos] == '-')) {
        const int64_t sign = (iso[pos] == '-') ? -1 : 1;
        uint32_t off_hour = 0, off_minute = 0;
        if (read_digits(iso, pos + 1, 2, off_hour)) {
            const size_t minute_pos = (pos + 3 < iso.size() && iso[pos + 3] == ':') ? pos + 4 : pos + 3;
            read_digits(iso, minute_pos, 2, off_minute);
            seconds -= sign * (static_cast<int64_t>(off_hour) * 3600 + static_cast<int64_t>(off_minute) * 60);
        }
    }

    if (seconds < 0) {
        return 0;
    }
    return static_cast<uint64_t>(seconds) * nanos_per_second + fraction;
}

uint64_t to_microseconds(std::string_view iso) {
    return to_nanoseconds(iso) / 1'000ull;
}

uint64_t to_milliseconds(std::string_view iso) {
    return to_nanoseconds(iso) / 1'000'000ull;
}

uint64_t date_to_nanoseconds(std::string_view yyyy_mm_dd) {
    if (yyyy_mm_dd.size() < 10) {
        return 0;
    }
    return to_nanoseconds(yyyy_mm_dd.substr(0, 10));
}

uint64_t time_of_day_to_nanoseconds(std::string_view hh_mm) {
    uint32_t hour = 0, minute = 0, second = 0;
    if (!read_digits(hh_mm, 0, 2, hour) || hh_mm.size() < 5 || hh_mm[2] != ':' ||
        !read_digits(hh_mm, 3, 2, minute)) {
        return 0;
    }
    if (hh_mm.size() >= 8 && hh_mm[5] == ':') {
        read_digits(hh_mm, 6, 2, second);
    }
    return (static_cast<uint64_t>(hour) * 3600 + static_cast<uint64_t>(minute) * 60 + second) * nanos_per_second;
}

std::string to_rfc3339(uint64_t nanoseconds) {
    const int64_t total_seconds = static_cast<int64_t>(nanoseconds / nanos_per_second);
    const uint32_t fraction = static_cast<uint32_t>(nanoseconds % nanos_per_second);

    int64_t days = total_seconds / 86400;
    int64_t rem = total_seconds % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }

    int64_t year = 0;
    uint32_t month = 0, day = 0;
    civil_from_days(days, year, month, day);

    char buffer[40];
    const int written = std::snprintf(buffer, sizeof(buffer),
        "%04lld-%02u-%02uT%02u:%02u:%02u.%09uZ",
        static_cast<long long>(year), month, day,
        static_cast<uint32_t>(rem / 3600),
        static_cast<uint32_t>((rem % 3600) / 60),
        static_cast<uint32_t>(rem % 60),
        fraction);
    return written > 0 ? std::string(buffer, static_cast<size_t>(written)) : std::string{};
}

std::string to_date_string(uint64_t nanoseconds) {
    const int64_t total_seconds = static_cast<int64_t>(nanoseconds / nanos_per_second);
    int64_t days = total_seconds / 86400;
    if (total_seconds % 86400 < 0) {
        --days;
    }

    int64_t year = 0;
    uint32_t month = 0, day = 0;
    civil_from_days(days, year, month, day);

    char buffer[16];
    const int written = std::snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02u",
        static_cast<long long>(year), month, day);
    return written > 0 ? std::string(buffer, static_cast<size_t>(written)) : std::string{};
}

std::string to_string(double value) {
    if (std::isnan(value) || std::isinf(value)) {
        return {};
    }

    // Fixed notation only: Alpaca rejects "1e-05" for qty/price fields.
    char buffer[64];
    int written = std::snprintf(buffer, sizeof(buffer), "%.9f", value);
    if (written <= 0) {
        return {};
    }

    std::string s(buffer, static_cast<size_t>(written));
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    return s.empty() ? "0" : s;
}

std::string url_encode(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";

    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        }
        else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

std::string query_builder::str() const {
    if (params_.empty()) {
        return {};
    }

    std::string out;
    out.reserve(params_.size() * 24);
    for (const auto &[key, value] : params_) {
        out.push_back(out.empty() ? '?' : '&');
        out += url_encode(key);
        out.push_back('=');
        out += url_encode(value);
    }
    return out;
}

}   // namespace alpaca
