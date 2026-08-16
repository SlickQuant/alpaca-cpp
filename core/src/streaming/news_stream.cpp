// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/news_stream.hpp>

#include <format>

namespace alpaca {

news_stream::news_stream(credentials creds, environment env)
    : detail::data_stream_base(std::format("{}/v1beta1/news", data_stream_root(env)),
                               credentials::resolve(std::move(creds), env))
{}

void news_stream::dispatch(std::string_view type, const json &message) {
    if (type != "n" || !on_news_) {
        return;
    }
    news_article article;
    from_json(message, article);
    on_news_(article);
}

}   // namespace alpaca
