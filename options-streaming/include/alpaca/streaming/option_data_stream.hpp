// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <alpaca/common.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/environment.hpp>
#include <alpaca/streaming/data_stream_base.hpp>

namespace alpaca {

/// Real-time option trades and quotes.
///
/// The option feeds are the one place Alpaca does not offer JSON: they are served in
/// MessagePack only. That is the whole reason this stream ships in its own library,
/// `alpaca-options-streaming-cpp` — the base SDK never has to depend on msgpack. The
/// codec is an implementation detail and no msgpack type appears in this header.
///
/// Everything else matches the JSON streams. Trades and quotes arrive as the same
/// `alpaca::trade` and `alpaca::quote` that `data_client`'s option methods return, with
/// the contract symbol passed alongside, and the connection, reconnect and subscription
/// behaviour is `detail::data_stream_base`'s. Handlers must be registered before
/// `connect()` and must not block; see `detail::stream_base`.
///
/// The `trade` and `quote` passed to a handler are reused from one message to the next,
/// which is what keeps the message path free of allocations. Copy what has to outlive the
/// callback.
///
/// `indicative` is the feed available without an OPRA subscription.
///
/// @code
///   alpaca::option_data_stream stream({}, alpaca::option_feed::indicative);
///
///   stream.on_quote([](const std::string &symbol, const alpaca::quote &q) {
///       std::cout << symbol << ' ' << q.bid_price << " / " << q.ask_price << '\n';
///   });
///
///   stream.subscribe_quotes({"AAPL240315C00172500"});
///   stream.connect();
/// @endcode
class option_data_stream : public detail::data_stream_base {
public:
    explicit option_data_stream(credentials creds = {},
                                option_feed feed = option_feed::indicative,
                                environment env = environment::paper);

    ~option_data_stream() override;

    using symbol_trade_handler = std::function<void(const std::string &, const trade &)>;
    using symbol_quote_handler = std::function<void(const std::string &, const quote &)>;

    void on_trade(symbol_trade_handler h) { on_trade_ = std::move(h); }
    void on_quote(symbol_quote_handler h) { on_quote_ = std::move(h); }

    /// `{"*"}` subscribes to every contract on the feed.
    void subscribe_trades(const std::vector<std::string> &symbols) { subscribe("trades", symbols); }
    /// Quotes have to name their contracts: Alpaca rejects `{"*"}` here with error 413.
    void subscribe_quotes(const std::vector<std::string> &symbols) { subscribe("quotes", symbols); }

    void unsubscribe_trades(const std::vector<std::string> &symbols) {
        unsubscribe("trades", symbols);
    }
    void unsubscribe_quotes(const std::vector<std::string> &symbols) {
        unsubscribe("quotes", symbols);
    }

    option_feed feed() const noexcept { return feed_; }

protected:
    void handle_frame(const char *data, std::size_t len) override;
    std::string encode(const json &frame) const override;
    std::string auth_frame() const override;
    void dispatch(std::string_view type, const json &message) override;

private:
    struct decoder;     ///< keeps msgpack out of this header

    option_feed feed_;
    std::unique_ptr<decoder> decoder_;

    symbol_trade_handler on_trade_;
    symbol_quote_handler on_quote_;
};

}   // namespace alpaca
