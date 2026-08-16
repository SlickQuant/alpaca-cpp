// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/stock_data_stream.hpp>

#include <format>

namespace alpaca {

namespace {

/// Feeds are served under different API versions, so the version is a property of the
/// feed rather than a constant in the URL.
std::string_view stream_version(data_feed feed) {
    switch (feed) {
    case data_feed::boats:
    case data_feed::overnight:
        return "v1beta1";
    default:
        return "v2";
    }
}

std::string stock_stream_url(data_feed feed, environment env) {
    return std::format("{}/{}/{}", data_stream_root(env), stream_version(feed),
                       to_string_view(feed));
}

}   // namespace

using detail::deliver;

stock_data_stream::stock_data_stream(credentials creds, data_feed feed, environment env)
    : detail::data_stream_base(stock_stream_url(feed, env),
                               credentials::resolve(std::move(creds), env))
    , feed_(feed)
{}

stock_data_stream::stock_data_stream(credentials creds, std::string url)
    : detail::data_stream_base(std::move(url),
                               credentials::resolve(std::move(creds), environment::paper))
    , feed_(data_feed::unknown)
{}

std::string stock_data_stream::test_feed_url(environment env) {
    return std::format("{}/v2/test", data_stream_root(env));
}

void stock_data_stream::dispatch(std::string_view type, const json &message) {
    // Single-character types, so a switch on the first byte beats a chain of string
    // comparisons on a path that runs for every message on the feed.
    if (type.size() != 1) {
        return;
    }

    switch (type.front()) {
    case 't': deliver<trade>(on_trade_, message); break;
    case 'q': deliver<quote>(on_quote_, message); break;
    case 'b': deliver<bar>(on_bar_, message); break;
    case 'u': deliver<bar>(on_updated_bar_, message); break;
    case 'd': deliver<bar>(on_daily_bar_, message); break;
    case 's': deliver<trading_status>(on_status_, message); break;
    case 'l': deliver<luld>(on_luld_, message); break;
    case 'c': deliver<trade_correction>(on_correction_, message); break;
    case 'x': deliver<trade_cancel_error>(on_cancel_error_, message); break;
    case 'i': deliver<imbalance>(on_imbalance_, message); break;
    default: break;
    }
}

}   // namespace alpaca
