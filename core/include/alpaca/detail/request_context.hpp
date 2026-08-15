// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <nlohmann/json.hpp>

#include <alpaca/auth.hpp>
#include <alpaca/rate_limiter.hpp>
#include <alpaca/utils.hpp>

namespace asio = boost::asio;
using json = nlohmann::json;

namespace alpaca::detail {

/// Which authentication scheme a context applies to its requests.
enum class auth_scheme : uint8_t {
    api_key,    ///< APCA-API-KEY-ID / APCA-API-SECRET-KEY (Trading, Market Data)
    basic,      ///< HTTP Basic (Broker API)
};

/// Retry policy applied to transient failures (429 and 5xx).
struct retry_policy {
    uint32_t max_attempts = 3;          ///< total attempts including the first; 1 disables retrying
    uint32_t initial_backoff_ms = 250;  ///< doubled after each failed attempt
    uint32_t max_backoff_ms = 4000;
};

/// The single place every REST call in the SDK passes through.
///
/// It owns base URL joining, authentication headers, client-side rate limiting, retry
/// with exponential backoff, status checking, JSON parsing, and mapping Alpaca's error
/// bodies onto `api_error`. Endpoint methods on the clients are therefore one line each,
/// and the broker library reuses this class rather than reimplementing any of it.
///
/// Copyable: each client owns its own context, and copies share nothing mutable except a
/// freshly initialised rate limiter.
class request_context {
public:
    request_context(std::string base_url,
                    credentials creds,
                    auth_scheme scheme = auth_scheme::api_key,
                    uint32_t requests_per_minute = 200);

    // --- configuration -----------------------------------------------------

    std::string_view base_url() const noexcept { return base_url_; }
    void set_base_url(std::string_view url);

    const credentials& creds() const noexcept { return credentials_; }
    void set_credentials(credentials creds);

    rate_limiter& limiter() noexcept { return limiter_; }
    const rate_limiter& limiter() const noexcept { return limiter_; }

    const retry_policy& retries() const noexcept { return retry_; }
    void set_retry_policy(const retry_policy &policy) noexcept { retry_ = policy; }

    // --- synchronous -------------------------------------------------------

    json get(std::string_view path, const query_builder &query = {}) const;
    json post(std::string_view path, const json &body) const;
    json put(std::string_view path, const json &body) const;
    json patch(std::string_view path, const json &body) const;
    json del(std::string_view path, const json &body = json::object()) const;

    // --- coroutine ---------------------------------------------------------

    asio::awaitable<json> async_get(std::string_view path, query_builder query = {}) const;
    asio::awaitable<json> async_post(std::string_view path, json body) const;
    asio::awaitable<json> async_put(std::string_view path, json body) const;
    asio::awaitable<json> async_patch(std::string_view path, json body) const;
    asio::awaitable<json> async_del(std::string_view path, json body = json::object()) const;

    // --- pagination --------------------------------------------------------

    /// Repeatedly GETs `path`, following `next_page_token` until it is null or absent,
    /// invoking `on_page` once per page. The page shape differs per endpoint (an array for
    /// some, an object keyed by symbol for others), so merging is left to the caller.
    void paginate(std::string_view path,
                  query_builder query,
                  const std::function<void(const json &)> &on_page) const;

    asio::awaitable<void> async_paginate(std::string_view path,
                                         query_builder query,
                                         std::function<void(const json &)> on_page) const;

private:
    enum class verb : uint8_t { get, post, put, patch, del };

    /// Builds the absolute URL for a path plus query string.
    std::string build_url(std::string_view path, const query_builder &query) const;

    /// Headers for one request, including Content-Type for bodied verbs.
    header_list build_headers(bool with_body) const;

    /// Executes one attempt and returns the parsed body, throwing `api_error` on failure.
    json execute(verb v, std::string_view path, const query_builder &query, const json *body) const;

    asio::awaitable<json> async_execute(verb v, std::string path, query_builder query,
                                        std::shared_ptr<const json> body) const;

    std::string base_url_;
    credentials credentials_;
    auth_scheme scheme_;
    retry_policy retry_;
    mutable rate_limiter limiter_;
};

/// Parses an Alpaca response body into an `api_error` and throws it.
/// Exposed so the broker library maps errors identically.
[[noreturn]] void throw_api_error(uint32_t status, std::string_view reason, std::string body);

/// Parses a response body, tolerating the empty body returned by 204 responses.
json parse_body(std::string_view body);

}   // namespace alpaca::detail
