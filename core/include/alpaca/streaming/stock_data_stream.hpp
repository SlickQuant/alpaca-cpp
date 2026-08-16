// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <alpaca/common.hpp>
#include <alpaca/data/bar.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/environment.hpp>
#include <alpaca/streaming/data_stream_base.hpp>

namespace alpaca {

/// Real-time stock trades, quotes, bars and the auction-adjacent feeds.
///
/// The stream sends the same single-letter payloads as the REST Market Data API, so
/// trades, quotes and bars are delivered as `alpaca::trade`, `alpaca::quote` and
/// `alpaca::bar` — the same types `data_client` returns. The symbol arrives alongside
/// rather than inside, because those REST types are keyed by symbol at the map level.
///
/// `iex` is the only feed available without a data subscription. Handlers must be
/// registered before `connect()` and must not block; see `detail::stream_base`.
///
/// @code
///   alpaca::stock_data_stream stream({}, alpaca::data_feed::iex);
///
///   stream.on_trade([](const std::string &symbol, const alpaca::trade &t) {
///       std::cout << symbol << ' ' << t.price << " x " << t.size << '\n';
///   });
///
///   stream.subscribe_trades({"AAPL", "MSFT"});
///   stream.connect();
/// @endcode
class stock_data_stream : public detail::data_stream_base {
public:
    explicit stock_data_stream(credentials creds = {},
                               data_feed feed = data_feed::iex,
                               environment env = environment::paper);

    /// Points the stream at an explicit websocket URL. The reason this exists is
    /// Alpaca's `wss://stream.data.alpaca.markets/v2/test` feed, which streams synthetic
    /// FAKEPACA data around the clock — the only way to exercise a stream end to end
    /// outside market hours. Use `test_feed_url()` rather than spelling it out.
    stock_data_stream(credentials creds, std::string url);

    /// URL of the synthetic test feed for an environment.
    static std::string test_feed_url(environment env = environment::paper);

    ~stock_data_stream() override { shutdown(); }

    using symbol_trade_handler = std::function<void(const std::string &, const trade &)>;
    using symbol_quote_handler = std::function<void(const std::string &, const quote &)>;
    using symbol_bar_handler   = std::function<void(const std::string &, const bar &)>;

    void on_trade(symbol_trade_handler h) { on_trade_ = std::move(h); }
    void on_quote(symbol_quote_handler h) { on_quote_ = std::move(h); }
    void on_bar(symbol_bar_handler h) { on_bar_ = std::move(h); }
    void on_updated_bar(symbol_bar_handler h) { on_updated_bar_ = std::move(h); }
    void on_daily_bar(symbol_bar_handler h) { on_daily_bar_ = std::move(h); }

    void on_trading_status(std::function<void(const std::string &, const trading_status &)> h) {
        on_status_ = std::move(h);
    }
    void on_luld(std::function<void(const std::string &, const luld &)> h) {
        on_luld_ = std::move(h);
    }
    void on_correction(std::function<void(const std::string &, const trade_correction &)> h) {
        on_correction_ = std::move(h);
    }
    void on_cancel_error(std::function<void(const std::string &, const trade_cancel_error &)> h) {
        on_cancel_error_ = std::move(h);
    }
    void on_imbalance(std::function<void(const std::string &, const imbalance &)> h) {
        on_imbalance_ = std::move(h);
    }

    /// `{"*"}` subscribes to every symbol on the feed.
    void subscribe_trades(const std::vector<std::string> &symbols) { subscribe("trades", symbols); }
    void subscribe_quotes(const std::vector<std::string> &symbols) { subscribe("quotes", symbols); }
    void subscribe_bars(const std::vector<std::string> &symbols) { subscribe("bars", symbols); }
    void subscribe_updated_bars(const std::vector<std::string> &symbols) {
        subscribe("updatedBars", symbols);
    }
    void subscribe_daily_bars(const std::vector<std::string> &symbols) {
        subscribe("dailyBars", symbols);
    }
    void subscribe_statuses(const std::vector<std::string> &symbols) {
        subscribe("statuses", symbols);
    }
    void subscribe_lulds(const std::vector<std::string> &symbols) { subscribe("lulds", symbols); }
    void subscribe_imbalances(const std::vector<std::string> &symbols) {
        subscribe("imbalances", symbols);
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
    void unsubscribe_statuses(const std::vector<std::string> &symbols) {
        unsubscribe("statuses", symbols);
    }
    void unsubscribe_lulds(const std::vector<std::string> &symbols) {
        unsubscribe("lulds", symbols);
    }
    void unsubscribe_imbalances(const std::vector<std::string> &symbols) {
        unsubscribe("imbalances", symbols);
    }

    data_feed feed() const noexcept { return feed_; }

protected:
    void dispatch(std::string_view type, const json &message) override;

private:
    data_feed feed_;

    symbol_trade_handler on_trade_;
    symbol_quote_handler on_quote_;
    symbol_bar_handler on_bar_;
    symbol_bar_handler on_updated_bar_;
    symbol_bar_handler on_daily_bar_;
    std::function<void(const std::string &, const trading_status &)> on_status_;
    std::function<void(const std::string &, const luld &)> on_luld_;
    std::function<void(const std::string &, const trade_correction &)> on_correction_;
    std::function<void(const std::string &, const trade_cancel_error &)> on_cancel_error_;
    std::function<void(const std::string &, const imbalance &)> on_imbalance_;
};

}   // namespace alpaca
