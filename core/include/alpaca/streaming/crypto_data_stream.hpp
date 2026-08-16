// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <alpaca/common.hpp>
#include <alpaca/data/bar.hpp>
#include <alpaca/data/orderbook.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/environment.hpp>
#include <alpaca/streaming/data_stream_base.hpp>

namespace alpaca {

/// Real-time crypto trades, quotes, bars and order books.
///
/// Crypto needs no data subscription, and the venue is a path segment rather than a feed
/// parameter — the same split as `data_client`'s crypto methods. Order books arrive as
/// full snapshots, with `orderbook::reset` marking the first book after a (re)connect.
///
/// @code
///   alpaca::crypto_data_stream stream;
///   stream.on_orderbook([](const std::string &symbol, const alpaca::orderbook &book) {
///       std::cout << symbol << ' ' << book.best_bid() << " / " << book.best_ask() << '\n';
///   });
///   stream.subscribe_orderbooks({"BTC/USD"});
///   stream.connect();
/// @endcode
class crypto_data_stream : public detail::data_stream_base {
public:
    explicit crypto_data_stream(credentials creds = {},
                                crypto_location loc = crypto_location::us,
                                environment env = environment::paper);

    ~crypto_data_stream() override { shutdown(); }

    void on_trade(std::function<void(const std::string &, const trade &)> h) {
        on_trade_ = std::move(h);
    }
    void on_quote(std::function<void(const std::string &, const quote &)> h) {
        on_quote_ = std::move(h);
    }
    void on_bar(std::function<void(const std::string &, const bar &)> h) {
        on_bar_ = std::move(h);
    }
    void on_updated_bar(std::function<void(const std::string &, const bar &)> h) {
        on_updated_bar_ = std::move(h);
    }
    void on_daily_bar(std::function<void(const std::string &, const bar &)> h) {
        on_daily_bar_ = std::move(h);
    }
    void on_orderbook(std::function<void(const std::string &, const orderbook &)> h) {
        on_orderbook_ = std::move(h);
    }

    void subscribe_trades(const std::vector<std::string> &symbols) { subscribe("trades", symbols); }
    void subscribe_quotes(const std::vector<std::string> &symbols) { subscribe("quotes", symbols); }
    void subscribe_bars(const std::vector<std::string> &symbols) { subscribe("bars", symbols); }
    void subscribe_updated_bars(const std::vector<std::string> &symbols) {
        subscribe("updatedBars", symbols);
    }
    void subscribe_daily_bars(const std::vector<std::string> &symbols) {
        subscribe("dailyBars", symbols);
    }
    void subscribe_orderbooks(const std::vector<std::string> &symbols) {
        subscribe("orderbooks", symbols);
    }

    void unsubscribe_trades(const std::vector<std::string> &symbols) {
        unsubscribe("trades", symbols);
    }
    void unsubscribe_quotes(const std::vector<std::string> &symbols) {
        unsubscribe("quotes", symbols);
    }
    void unsubscribe_bars(const std::vector<std::string> &symbols) {
        unsubscribe("bars", symbols);
    }
    void unsubscribe_updated_bars(const std::vector<std::string> &symbols) {
        unsubscribe("updatedBars", symbols);
    }
    void unsubscribe_daily_bars(const std::vector<std::string> &symbols) {
        unsubscribe("dailyBars", symbols);
    }
    void unsubscribe_orderbooks(const std::vector<std::string> &symbols) {
        unsubscribe("orderbooks", symbols);
    }

    crypto_location location() const noexcept { return location_; }

protected:
    void dispatch(std::string_view type, const json &message) override;

private:
    crypto_location location_;

    std::function<void(const std::string &, const trade &)> on_trade_;
    std::function<void(const std::string &, const quote &)> on_quote_;
    std::function<void(const std::string &, const bar &)> on_bar_;
    std::function<void(const std::string &, const bar &)> on_updated_bar_;
    std::function<void(const std::string &, const bar &)> on_daily_bar_;
    std::function<void(const std::string &, const orderbook &)> on_orderbook_;
};

}   // namespace alpaca
