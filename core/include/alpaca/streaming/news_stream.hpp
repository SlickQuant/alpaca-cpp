// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <alpaca/data/news.hpp>
#include <alpaca/environment.hpp>
#include <alpaca/streaming/data_stream_base.hpp>

namespace alpaca {

/// Real-time news articles.
///
/// Articles are delivered as `alpaca::news_article`, the same type `data_client::get_news`
/// returns — the news stream is the one Alpaca feed that uses spelled-out field names on
/// the wire, so the REST model parses it directly. There is no symbol argument here
/// because an article carries its own `symbols` list.
///
/// @code
///   alpaca::news_stream stream;
///   stream.on_news([](const alpaca::news_article &a) {
///       std::cout << '[' << a.source << "] " << a.headline << '\n';
///   });
///   stream.subscribe_news({"*"});          // every symbol
///   stream.connect();
/// @endcode
class news_stream : public detail::data_stream_base {
public:
    explicit news_stream(credentials creds = {}, environment env = environment::paper);

    ~news_stream() override { shutdown(); }

    void on_news(std::function<void(const news_article &)> h) { on_news_ = std::move(h); }

    /// `{"*"}` subscribes to every article rather than a symbol-filtered subset.
    void subscribe_news(const std::vector<std::string> &symbols) { subscribe("news", symbols); }
    void unsubscribe_news(const std::vector<std::string> &symbols) {
        unsubscribe("news", symbols);
    }

protected:
    void dispatch(std::string_view type, const json &message) override;

private:
    std::function<void(const news_article &)> on_news_;
};

}   // namespace alpaca
