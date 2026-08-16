// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for stream construction and subscription bookkeeping.
//
// These build stream objects but never call connect(), so no socket is ever opened and
// no credentials are needed. Subscribing while disconnected is a supported and in fact
// the usual pattern — the set is recorded and replayed once the stream authenticates —
// which is exactly what makes the bookkeeping testable without a network.

#include <gtest/gtest.h>

#include <string>

#include <alpaca/streaming/crypto_data_stream.hpp>
#include <alpaca/streaming/news_stream.hpp>
#include <alpaca/streaming/stock_data_stream.hpp>
#include <alpaca/streaming/trade_update_stream.hpp>

namespace alpaca::tests {

namespace {

credentials dummy() { return credentials("PKTEST", "secret"); }

}   // namespace

// ---------------------------------------------------------------------------
// URLs
// ---------------------------------------------------------------------------

TEST(StreamUrls, StockFeedsCarryTheirOwnApiVersion) {
    // sip/iex/delayed_sip are v2; boats and overnight are v1beta1. Getting this wrong
    // yields a 404 on connect rather than anything diagnosable.
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::iex).url(),
              "wss://stream.data.alpaca.markets/v2/iex");
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::sip).url(),
              "wss://stream.data.alpaca.markets/v2/sip");
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::delayed_sip).url(),
              "wss://stream.data.alpaca.markets/v2/delayed_sip");
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::boats).url(),
              "wss://stream.data.alpaca.markets/v1beta1/boats");
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::overnight).url(),
              "wss://stream.data.alpaca.markets/v1beta1/overnight");
}

TEST(StreamUrls, SandboxUsesTheSandboxHost) {
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::iex, environment::sandbox).url(),
              "wss://stream.data.sandbox.alpaca.markets/v2/iex");
    EXPECT_EQ(news_stream(dummy(), environment::sandbox).url(),
              "wss://stream.data.sandbox.alpaca.markets/v1beta1/news");
}

TEST(StreamUrls, PaperAndLiveShareTheMarketDataHost) {
    // Market data is not account-scoped, so only sandbox differs.
    EXPECT_EQ(stock_data_stream(dummy(), data_feed::iex, environment::paper).url(),
              stock_data_stream(dummy(), data_feed::iex, environment::live).url());
}

TEST(StreamUrls, CryptoVenueIsAPathSegment) {
    EXPECT_EQ(crypto_data_stream(dummy(), crypto_location::us).url(),
              "wss://stream.data.alpaca.markets/v1beta3/crypto/us");
    EXPECT_EQ(crypto_data_stream(dummy(), crypto_location::us_1).url(),
              "wss://stream.data.alpaca.markets/v1beta3/crypto/us-1");
    EXPECT_EQ(crypto_data_stream(dummy(), crypto_location::eu_1).url(),
              "wss://stream.data.alpaca.markets/v1beta3/crypto/eu-1");
}

TEST(StreamUrls, NewsStream) {
    EXPECT_EQ(news_stream(dummy()).url(), "wss://stream.data.alpaca.markets/v1beta1/news");
}

TEST(StreamUrls, TradeUpdatesFollowTheTradingHostNotTheDataHost) {
    // The account stream lives on the trading host, so unlike market data it *does*
    // differ between paper and live.
    EXPECT_EQ(trade_update_stream(dummy(), environment::paper).url(),
              "wss://paper-api.alpaca.markets/stream");
    EXPECT_EQ(trade_update_stream(dummy(), environment::live).url(),
              "wss://api.alpaca.markets/stream");
}

TEST(StreamUrls, TestFeedIsReachableWithoutSpellingOutTheUrl) {
    EXPECT_EQ(stock_data_stream::test_feed_url(), "wss://stream.data.alpaca.markets/v2/test");
    EXPECT_EQ(stock_data_stream(dummy(), stock_data_stream::test_feed_url()).url(),
              "wss://stream.data.alpaca.markets/v2/test");
}

// ---------------------------------------------------------------------------
// Subscription bookkeeping
// ---------------------------------------------------------------------------

TEST(StreamSubscriptions, SubscribingBeforeConnectRecordsTheSet) {
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({"AAPL", "MSFT"});
    stream.subscribe_quotes({"AAPL"});

    const auto channels = stream.pending_channels();
    ASSERT_TRUE(channels.contains("trades"));
    EXPECT_EQ(channels.at("trades").size(), 2u);
    EXPECT_TRUE(channels.at("trades").contains("AAPL"));
    EXPECT_TRUE(channels.at("trades").contains("MSFT"));
    ASSERT_TRUE(channels.contains("quotes"));
    EXPECT_EQ(channels.at("quotes").size(), 1u);
}

TEST(StreamSubscriptions, RepeatedSubscribesDoNotDuplicateSymbols) {
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({"AAPL"});
    stream.subscribe_trades({"AAPL", "MSFT"});
    stream.subscribe_trades({"AAPL"});

    EXPECT_EQ(stream.pending_channels().at("trades").size(), 2u);
}

TEST(StreamSubscriptions, UnsubscribeRemovesOnlyTheNamedSymbols) {
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({"AAPL", "MSFT", "TSLA"});
    stream.unsubscribe_trades({"MSFT"});

    const auto trades = stream.pending_channels().at("trades");
    EXPECT_EQ(trades.size(), 2u);
    EXPECT_FALSE(trades.contains("MSFT"));
    EXPECT_TRUE(trades.contains("AAPL"));
    EXPECT_TRUE(trades.contains("TSLA"));
}

TEST(StreamSubscriptions, UnsubscribingAnUnknownSymbolIsHarmless) {
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({"AAPL"});

    EXPECT_NO_THROW(stream.unsubscribe_trades({"NVDA"}));
    EXPECT_NO_THROW(stream.unsubscribe_quotes({"AAPL"}));
    EXPECT_EQ(stream.pending_channels().at("trades").size(), 1u);
}

TEST(StreamSubscriptions, EmptySymbolListIsANoOp) {
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({});

    EXPECT_TRUE(stream.pending_channels().empty());
}

TEST(StreamSubscriptions, EveryStockChannelIsRecordedUnderItsWireName) {
    // The camelCase channels are the easy ones to get wrong.
    stock_data_stream stream(dummy(), data_feed::iex);
    stream.subscribe_trades({"A"});
    stream.subscribe_quotes({"A"});
    stream.subscribe_bars({"A"});
    stream.subscribe_updated_bars({"A"});
    stream.subscribe_daily_bars({"A"});
    stream.subscribe_statuses({"A"});
    stream.subscribe_lulds({"A"});
    stream.subscribe_imbalances({"A"});

    const auto channels = stream.pending_channels();
    EXPECT_TRUE(channels.contains("trades"));
    EXPECT_TRUE(channels.contains("quotes"));
    EXPECT_TRUE(channels.contains("bars"));
    EXPECT_TRUE(channels.contains("updatedBars"));
    EXPECT_TRUE(channels.contains("dailyBars"));
    EXPECT_TRUE(channels.contains("statuses"));
    EXPECT_TRUE(channels.contains("lulds"));
    EXPECT_TRUE(channels.contains("imbalances"));
}

TEST(StreamSubscriptions, CryptoExposesOrderbooks) {
    crypto_data_stream stream(dummy());
    stream.subscribe_orderbooks({"BTC/USD"});
    stream.subscribe_bars({"ETH/USD"});

    const auto channels = stream.pending_channels();
    ASSERT_TRUE(channels.contains("orderbooks"));
    EXPECT_TRUE(channels.at("orderbooks").contains("BTC/USD"));
    EXPECT_TRUE(channels.contains("bars"));
}

TEST(StreamSubscriptions, NewsUsesAWildcardChannel) {
    news_stream stream(dummy());
    stream.subscribe_news({"*"});

    const auto channels = stream.pending_channels();
    ASSERT_TRUE(channels.contains("news"));
    EXPECT_TRUE(channels.at("news").contains("*"));
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

TEST(StreamState, StartsDisconnected) {
    stock_data_stream stream(dummy(), data_feed::iex);

    EXPECT_EQ(stream.state(), detail::stream_state::disconnected);
    EXPECT_FALSE(stream.is_ready());
}

TEST(StreamState, DisconnectBeforeConnectIsHarmless) {
    stock_data_stream stream(dummy(), data_feed::iex);

    EXPECT_NO_THROW(stream.disconnect());
    EXPECT_EQ(stream.state(), detail::stream_state::disconnected);
}

TEST(StreamState, AuthMessageMatchesTheDocumentedFrame) {
    // Both the data streams and the account stream take this same flat form.
    const auto message = json::parse(dummy().stream_auth_message());

    EXPECT_EQ(message["action"], "auth");
    EXPECT_EQ(message["key"], "PKTEST");
    EXPECT_EQ(message["secret"], "secret");
}

}   // namespace alpaca::tests
