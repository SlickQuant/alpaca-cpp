// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/crypto_data_stream.hpp>

#include <format>

namespace alpaca {

namespace {

std::string crypto_stream_url(crypto_location loc, environment env) {
    return std::format("{}/v1beta3/crypto/{}", data_stream_root(env), to_string_view(loc));
}

}   // namespace

using detail::deliver;

crypto_data_stream::crypto_data_stream(credentials creds, crypto_location loc, environment env)
    : detail::data_stream_base(crypto_stream_url(loc, env),
                               credentials::resolve(std::move(creds), env))
    , location_(loc)
{}

void crypto_data_stream::dispatch(std::string_view type, const json &message) {
    if (type.size() != 1) {
        return;
    }

    switch (type.front()) {
    case 't': deliver<trade>(on_trade_, message); break;
    case 'q': deliver<quote>(on_quote_, message); break;
    case 'b': deliver<bar>(on_bar_, message); break;
    case 'u': deliver<bar>(on_updated_bar_, message); break;
    case 'd': deliver<bar>(on_daily_bar_, message); break;
    case 'o': deliver<orderbook>(on_orderbook_, message); break;
    default: break;
    }
}

}   // namespace alpaca
