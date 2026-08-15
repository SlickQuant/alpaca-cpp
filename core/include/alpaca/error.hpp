// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace alpaca {

/// Thrown whenever the Alpaca API answers with a non-2xx status.
///
/// The SDK deliberately throws rather than returning an empty value: for a trading API,
/// "you have no open orders" and "your credentials were rejected" must never be
/// indistinguishable at the call site.
struct api_error : std::runtime_error {
    /// HTTP status returned by Alpaca (401, 403, 404, 422, 429, ...).
    uint32_t http_status = 0;
    /// Alpaca's numeric error code from the response body (e.g. 40010001), 0 when absent.
    int32_t code = 0;
    /// Alpaca's human readable message from the response body.
    std::string message;
    /// Raw response body, kept for diagnostics when the body was not the expected shape.
    std::string body;

    api_error(uint32_t status, int32_t error_code, std::string error_message, std::string raw_body)
        : std::runtime_error(build_what(status, error_code, error_message))
        , http_status(status)
        , code(error_code)
        , message(std::move(error_message))
        , body(std::move(raw_body))
    {}

    /// True for statuses that are worth retrying: 429 and 5xx.
    bool is_retryable() const noexcept {
        return http_status == 429 || (http_status >= 500 && http_status < 600);
    }

    bool is_rate_limited() const noexcept { return http_status == 429; }
    bool is_unauthorized() const noexcept { return http_status == 401 || http_status == 403; }
    bool is_not_found() const noexcept { return http_status == 404; }

private:
    static std::string build_what(uint32_t status, int32_t error_code, std::string_view error_message) {
        std::string what = "alpaca api error " + std::to_string(status);
        if (error_code != 0) {
            what += " (code ";
            what += std::to_string(error_code);
            what += ")";
        }
        if (!error_message.empty()) {
            what += ": ";
            what += error_message;
        }
        return what;
    }
};

/// Thrown when a response body could not be parsed as the expected JSON shape.
struct parse_error : std::runtime_error {
    std::string body;

    parse_error(std::string_view what_arg, std::string raw_body)
        : std::runtime_error(std::string(what_arg))
        , body(std::move(raw_body))
    {}
};

/// Thrown when required credentials are missing or a client is misconfigured.
struct config_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}   // namespace alpaca
