// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/detail/request_context.hpp>
#include <alpaca/error.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <slick/net/http.hpp>
#include <slick/net/logging.hpp>

using Http = slick::net::Http;

namespace alpaca::detail {

namespace {

/// Trims the trailing slash so joining a path never produces a double slash.
std::string normalize_base_url(std::string_view url) {
    std::string out(url);
    while (!out.empty() && out.back() == '/') {
        out.pop_back();
    }
    return out;
}

bool is_blank(std::string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; });
}

/// Alpaca signals a retryable condition with 429 or any 5xx.
bool is_retryable_status(uint32_t status) noexcept {
    return status == 429 || (status >= 500 && status < 600);
}

uint32_t next_backoff_ms(const retry_policy &policy, uint32_t attempt) noexcept {
    uint64_t backoff = policy.initial_backoff_ms;
    for (uint32_t i = 1; i < attempt; ++i) {
        backoff *= 2;
        if (backoff >= policy.max_backoff_ms) {
            return policy.max_backoff_ms;
        }
    }
    return static_cast<uint32_t>(std::min<uint64_t>(backoff, policy.max_backoff_ms));
}

}   // namespace

json parse_body(std::string_view body) {
    // 204 No Content and some DELETE responses come back with an empty body.
    if (body.empty() || is_blank(body)) {
        return json();
    }
    try {
        return json::parse(body);
    }
    catch (const std::exception &e) {
        throw parse_error(e.what(), std::string(body));
    }
}

void throw_api_error(uint32_t status, std::string_view reason, std::string body) {
    int32_t code = 0;
    std::string message;

    // Alpaca error bodies are {"code": 40010001, "message": "..."}; some endpoints send
    // only a message, and a few send plain text. Fall back gracefully through all three.
    try {
        if (!body.empty() && !is_blank(body)) {
            const auto j = json::parse(body);
            if (j.is_object()) {
                if (j.contains("code") && j["code"].is_number()) {
                    code = j["code"].get<int32_t>();
                }
                if (j.contains("message") && j["message"].is_string()) {
                    message = j["message"].get<std::string>();
                }
                else if (j.contains("error") && j["error"].is_string()) {
                    message = j["error"].get<std::string>();
                }
            }
        }
    }
    catch (const std::exception &) {
        // Body was not JSON; the raw text is preserved on the exception.
    }

    if (message.empty()) {
        message = reason.empty() ? body : std::string(reason);
    }

    throw api_error(status, code, std::move(message), std::move(body));
}

request_context::request_context(std::string base_url,
                                 credentials creds,
                                 auth_scheme scheme,
                                 uint32_t requests_per_minute)
    : base_url_(normalize_base_url(base_url))
    , credentials_(std::move(creds))
    , scheme_(scheme)
    , limiter_(requests_per_minute)
{}

void request_context::set_base_url(std::string_view url) {
    base_url_ = normalize_base_url(url);
}

void request_context::set_credentials(credentials creds) {
    credentials_ = std::move(creds);
}

std::string request_context::build_url(std::string_view path, const query_builder &query) const {
    std::string url;
    url.reserve(base_url_.size() + path.size() + 32);
    url += base_url_;
    if (!path.empty() && path.front() != '/') {
        url.push_back('/');
    }
    url += path;
    url += query.str();
    return url;
}

header_list request_context::build_headers(bool with_body) const {
    header_list headers = (scheme_ == auth_scheme::basic)
        ? credentials_.basic_auth_headers()
        : credentials_.trading_headers();

    headers.emplace_back("Accept", "application/json");
    if (with_body) {
        headers.emplace_back("Content-Type", "application/json");
    }
    return headers;
}

json request_context::execute(verb v, std::string_view path,
                              const query_builder &query, const json *body) const {
    const std::string url = build_url(path, query);
    const std::string payload = body ? body->dump() : std::string{};

    uint32_t attempt = 0;
    for (;;) {
        ++attempt;
        limiter_.acquire();

        Http::Response response;
        switch (v) {
        case verb::get:   response = Http::get(url, build_headers(false)); break;
        case verb::post:  response = Http::post(url, payload, build_headers(true)); break;
        case verb::put:   response = Http::put(url, payload, build_headers(true)); break;
        case verb::patch: response = Http::patch(url, payload, build_headers(true)); break;
        case verb::del:   response = Http::del(url, payload, build_headers(body != nullptr)); break;
        }

        if (response.is_ok()) {
            return parse_body(response.result_text);
        }

        if (response.result_code == 429) {
            // slick-net does not surface response headers, so Retry-After cannot be read.
            // Back off for the computed interval instead and let the limiter hold the line.
            limiter_.notify_rate_limited(1);
        }

        if (is_retryable_status(response.result_code) && attempt < retry_.max_attempts) {
            const uint32_t backoff = next_backoff_ms(retry_, attempt);
            LOG_WARN("alpaca request {} failed with {} (attempt {}/{}), retrying in {}ms",
                     url, response.result_code, attempt, retry_.max_attempts, backoff);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
            continue;
        }

        LOG_ERROR("alpaca request {} failed with {}: {}", url, response.result_code, response.result_text);
        throw_api_error(response.result_code, response.reason, std::move(response.result_text));
    }
}

json request_context::get(std::string_view path, const query_builder &query) const {
    return execute(verb::get, path, query, nullptr);
}

json request_context::post(std::string_view path, const json &body) const {
    return execute(verb::post, path, {}, &body);
}

json request_context::put(std::string_view path, const json &body) const {
    return execute(verb::put, path, {}, &body);
}

json request_context::patch(std::string_view path, const json &body) const {
    return execute(verb::patch, path, {}, &body);
}

json request_context::del(std::string_view path, const json &body) const {
    return execute(verb::del, path, {}, body.empty() ? nullptr : &body);
}

asio::awaitable<json> request_context::async_execute(verb v, std::string path,
                                                     query_builder query,
                                                     std::shared_ptr<const json> body) const {
    const std::string url = build_url(path, query);
    const std::string payload = body ? body->dump() : std::string{};

    uint32_t attempt = 0;
    for (;;) {
        ++attempt;

        // The limiter can only be waited on without blocking the io_context by polling it
        // through a timer, so honour the reservation asynchronously.
        while (!limiter_.try_acquire()) {
            const uint64_t wait_ns = std::max<uint64_t>(limiter_.wait_time_ns(), 1'000'000ull);
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::nanoseconds(wait_ns));
            co_await timer.async_wait(asio::use_awaitable);
        }

        Http::Response response;
        switch (v) {
        case verb::get:   response = co_await Http::async_get(url, build_headers(false)); break;
        case verb::post:  response = co_await Http::async_post(url, payload, build_headers(true)); break;
        case verb::put:   response = co_await Http::async_put(url, payload, build_headers(true)); break;
        case verb::patch: response = co_await Http::async_patch(url, payload, build_headers(true)); break;
        case verb::del:   response = co_await Http::async_del(url, payload, build_headers(body != nullptr)); break;
        }

        if (response.is_ok()) {
            co_return parse_body(response.result_text);
        }

        if (response.result_code == 429) {
            limiter_.notify_rate_limited(1);
        }

        if (is_retryable_status(response.result_code) && attempt < retry_.max_attempts) {
            const uint32_t backoff = next_backoff_ms(retry_, attempt);
            LOG_WARN("alpaca request {} failed with {} (attempt {}/{}), retrying in {}ms",
                     url, response.result_code, attempt, retry_.max_attempts, backoff);
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(backoff));
            co_await timer.async_wait(asio::use_awaitable);
            continue;
        }

        LOG_ERROR("alpaca request {} failed with {}: {}", url, response.result_code, response.result_text);
        throw_api_error(response.result_code, response.reason, std::move(response.result_text));
    }
}

asio::awaitable<json> request_context::async_get(std::string_view path, query_builder query) const {
    return async_execute(verb::get, std::string(path), std::move(query), nullptr);
}

asio::awaitable<json> request_context::async_post(std::string_view path, json body) const {
    return async_execute(verb::post, std::string(path), {}, std::make_shared<const json>(std::move(body)));
}

asio::awaitable<json> request_context::async_put(std::string_view path, json body) const {
    return async_execute(verb::put, std::string(path), {}, std::make_shared<const json>(std::move(body)));
}

asio::awaitable<json> request_context::async_patch(std::string_view path, json body) const {
    return async_execute(verb::patch, std::string(path), {}, std::make_shared<const json>(std::move(body)));
}

asio::awaitable<json> request_context::async_del(std::string_view path, json body) const {
    auto payload = body.empty() ? nullptr : std::make_shared<const json>(std::move(body));
    return async_execute(verb::del, std::string(path), {}, std::move(payload));
}

void request_context::paginate(std::string_view path,
                               query_builder query,
                               const std::function<void(const json &)> &on_page) const {
    for (;;) {
        const json page = get(path, query);
        on_page(page);

        if (!page.is_object() || !page.contains("next_page_token") || page["next_page_token"].is_null()) {
            return;
        }
        const auto token = page["next_page_token"].get<std::string>();
        if (token.empty()) {
            return;
        }
        query.set("page_token", token);
    }
}

asio::awaitable<void> request_context::async_paginate(std::string_view path,
                                                      query_builder query,
                                                      std::function<void(const json &)> on_page) const {
    const std::string target(path);
    for (;;) {
        const json page = co_await async_get(target, query);
        on_page(page);

        if (!page.is_object() || !page.contains("next_page_token") || page["next_page_token"].is_null()) {
            co_return;
        }
        const auto token = page["next_page_token"].get<std::string>();
        if (token.empty()) {
            co_return;
        }
        query.set("page_token", token);
    }
}

}   // namespace alpaca::detail
