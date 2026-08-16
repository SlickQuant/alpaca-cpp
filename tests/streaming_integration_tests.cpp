// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Integration tests against the live websocket endpoints.
//
// The market data group runs against Alpaca's synthetic `v2/test` feed, which streams
// FAKEPACA data around the clock and needs no data subscription — so these tests do not
// depend on the market being open, which a test against IEX would. Either paper or live
// keys work: the data host is not account-scoped.
//
// The account (trade updates) group needs credentials that are valid for paper trading
// and is skipped otherwise, exactly like the paper REST integration group.

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <format>
#include <mutex>
#include <string>

#include <alpaca/error.hpp>
#include <alpaca/streaming/crypto_data_stream.hpp>
#include <alpaca/streaming/news_stream.hpp>
#include <alpaca/streaming/stock_data_stream.hpp>
#include <alpaca/streaming/trade_update_stream.hpp>
#include <alpaca/trading_client.hpp>

namespace alpaca::tests {

namespace {

using namespace std::chrono_literals;

/// A one-shot latch a stream handler can trip from the service thread.
///
/// Handlers run on the websocket thread, so every assertion about "did a message
/// arrive" has to hand the result back to the test thread. Wrapping that once keeps the
/// tests readable and stops each of them growing its own mutex.
class latch {
public:
    void trip() {
        {
            std::lock_guard lock(mutex_);
            tripped_ = true;
        }
        cv_.notify_all();
    }

    bool wait(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return tripped_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool tripped_ = false;
};

/// Streams are slower to prove out than REST calls: a connect plus an authentication
/// round trip, then however long until the feed happens to publish.
constexpr auto connect_timeout = 15s;
constexpr auto message_timeout = 30s;

}   // namespace

class StreamIntegration : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (credentials::from_env(environment::paper).empty()) {
            blocker_ = "APCA_PAPER_API_KEY_ID / APCA_PAPER_API_SECRET_KEY "
                       "(or APCA_API_KEY_ID / APCA_API_SECRET_KEY) not set";
        }
    }

    void SetUp() override {
        if (!blocker_.empty()) {
            GTEST_SKIP() << "skipping streaming integration tests: " << blocker_;
        }
    }

    static inline std::string blocker_;
};

TEST_F(StreamIntegration, ConnectsAndAuthenticatesAgainstTheTestFeed) {
    stock_data_stream stream({}, stock_data_stream::test_feed_url());

    ASSERT_TRUE(stream.connect(connect_timeout))
        << "could not authenticate against " << stream.url();
    EXPECT_TRUE(stream.is_ready());
    EXPECT_EQ(stream.state(), detail::stream_state::ready);

    stream.disconnect();
    EXPECT_FALSE(stream.is_ready());
}

TEST_F(StreamIntegration, BadCredentialsAreReportedRatherThanHanging) {
    stock_data_stream stream(credentials("PKNOTREAL", "nope"),
                             stock_data_stream::test_feed_url());

    latch got_error;
    stream.on_error([&](const stream_error &) { got_error.trip(); });

    // Reconnecting forever on credentials that cannot work would just hammer Alpaca.
    stream.set_reconnect_policy({false, 0, 0, 0});

    // Rejected credentials must fail connect() as soon as the error frame lands, not
    // after the timeout expires — the handshake cannot recover, so waiting it out is
    // pure dead time for the caller.
    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(stream.connect(connect_timeout));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(got_error.wait(connect_timeout)) << "no error frame delivered";
    EXPECT_FALSE(stream.is_ready());
    EXPECT_LT(elapsed, connect_timeout / 2)
        << "connect() waited out its timeout instead of failing on the error frame";
}

TEST_F(StreamIntegration, ReceivesTradesFromTheTestFeed) {
    stock_data_stream stream({}, stock_data_stream::test_feed_url());

    latch got_trade;
    std::mutex mutex;
    std::string seen_symbol;
    double seen_price = 0.;

    stream.on_trade([&](const std::string &symbol, const trade &t) {
        {
            std::lock_guard lock(mutex);
            seen_symbol = symbol;
            seen_price = t.price;
        }
        got_trade.trip();
    });

    stream.subscribe_trades({"FAKEPACA"});
    ASSERT_TRUE(stream.connect(connect_timeout));
    ASSERT_TRUE(got_trade.wait(message_timeout)) << "no trade arrived on the test feed";

    std::lock_guard lock(mutex);
    EXPECT_EQ(seen_symbol, "FAKEPACA");
    EXPECT_GT(seen_price, 0.);
}

TEST_F(StreamIntegration, ReceivesQuotesFromTheTestFeed) {
    stock_data_stream stream({}, stock_data_stream::test_feed_url());

    latch got_quote;
    stream.on_quote([&](const std::string &symbol, const quote &q) {
        if (symbol == "FAKEPACA" && q.ask_price > 0.) {
            got_quote.trip();
        }
    });

    stream.subscribe_quotes({"FAKEPACA"});
    ASSERT_TRUE(stream.connect(connect_timeout));
    EXPECT_TRUE(got_quote.wait(message_timeout));
}

TEST_F(StreamIntegration, ServerEchoesTheSubscriptionSet) {
    stock_data_stream stream({}, stock_data_stream::test_feed_url());

    latch echoed;
    std::mutex mutex;
    subscriptions confirmed;

    stream.on_subscription([&](const subscriptions &s) {
        if (s.trades.empty()) {
            return;     // the initial empty echo, before our subscribe lands
        }
        {
            std::lock_guard lock(mutex);
            confirmed = s;
        }
        echoed.trip();
    });

    stream.subscribe_trades({"FAKEPACA"});
    ASSERT_TRUE(stream.connect(connect_timeout));
    ASSERT_TRUE(echoed.wait(message_timeout));

    std::lock_guard lock(mutex);
    ASSERT_EQ(confirmed.trades.size(), 1u);
    EXPECT_EQ(confirmed.trades[0], "FAKEPACA");
}

TEST_F(StreamIntegration, SubscribingAfterConnectWorks) {
    stock_data_stream stream({}, stock_data_stream::test_feed_url());

    latch got_trade;
    stream.on_trade([&](const std::string &, const trade &) { got_trade.trip(); });

    ASSERT_TRUE(stream.connect(connect_timeout));
    stream.subscribe_trades({"FAKEPACA"});      // after the handshake, not before

    EXPECT_TRUE(got_trade.wait(message_timeout));
}

TEST_F(StreamIntegration, CryptoStreamConnectsWithoutASubscription) {
    crypto_data_stream stream;

    ASSERT_TRUE(stream.connect(connect_timeout)) << "could not authenticate against "
                                                 << stream.url();
    EXPECT_TRUE(stream.is_ready());
}

TEST_F(StreamIntegration, NewsStreamConnects) {
    news_stream stream;

    ASSERT_TRUE(stream.connect(connect_timeout)) << "could not authenticate against "
                                                 << stream.url();
    EXPECT_TRUE(stream.is_ready());
}

// ---------------------------------------------------------------------------
// Account stream — needs credentials that work for paper trading
// ---------------------------------------------------------------------------

class TradeUpdateStreamIntegration : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (credentials::from_env(environment::paper).empty()) {
            blocker_ = "APCA_PAPER_API_KEY_ID / APCA_PAPER_API_SECRET_KEY not set";
            return;
        }

        // Same probe as the REST paper group: a live key reaches the host but is refused
        // on anything account-scoped, and one clear reason beats several odd failures.
        trading_client probe;
        probe.set_retry_policy({1, 0, 0});
        try {
            probe.get_account();
            blocker_.clear();
        }
        catch (const api_error &e) {
            blocker_ = std::format(
                "the configured credentials are not valid for paper trading (HTTP {}: {}). "
                "Paper keys start with 'PK'. Set APCA_PAPER_API_KEY_ID / "
                "APCA_PAPER_API_SECRET_KEY to run these tests.",
                e.http_status, e.message);
        }
        catch (const std::exception &e) {
            blocker_ = std::format("paper account unreachable: {}", e.what());
        }
    }

    void SetUp() override {
        if (!blocker_.empty()) {
            GTEST_SKIP() << "skipping trade update stream tests: " << blocker_;
        }
    }

    static inline std::string blocker_;
};

TEST_F(TradeUpdateStreamIntegration, AuthenticatesAndListens) {
    trade_update_stream stream;     // paper by default

    ASSERT_TRUE(stream.connect(connect_timeout)) << "could not authenticate against "
                                                 << stream.url();
    EXPECT_TRUE(stream.is_ready());

    stream.disconnect();
}

}   // namespace alpaca::tests
