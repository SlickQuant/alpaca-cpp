// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Integration tests against the live option stream.
//
// These run on the `indicative` feed, which needs no OPRA subscription, and assert only
// on the control protocol — the handshake, the subscription echo and an error frame. All
// of that is answered immediately at any hour, whereas option trades and quotes only flow
// while the market is open, so nothing here waits for market data. Either paper or live
// keys work: the data host is not account-scoped.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include <alpaca/streaming/option_data_stream.hpp>

#include "latch.hpp"

namespace alpaca::tests {

namespace {

using namespace std::chrono_literals;

constexpr auto connect_timeout = 15s;
constexpr auto message_timeout = 30s;

/// The server echoes a subscription without checking that the contract exists, so this
/// does not have to be kept current as expiries roll off.
const std::string contract = "AAPL261218C00250000";

}   // namespace

class OptionStreamIntegration : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (credentials::from_env(environment::paper).empty()) {
            blocker_ = "APCA_PAPER_API_KEY_ID / APCA_PAPER_API_SECRET_KEY "
                       "(or APCA_API_KEY_ID / APCA_API_SECRET_KEY) not set";
        }
    }

    void SetUp() override {
        if (!blocker_.empty()) {
            GTEST_SKIP() << "skipping option stream integration tests: " << blocker_;
        }
    }

    static inline std::string blocker_;
};

TEST_F(OptionStreamIntegration, ConnectsAndAuthenticatesOverMsgpack) {
    // The whole handshake is MessagePack in both directions, so getting to `ready` proves
    // the server chose that codec for this connection, decoded our binary auth frame, and
    // that we decoded its acknowledgement.
    option_data_stream stream;

    ASSERT_TRUE(stream.connect(connect_timeout))
        << "could not authenticate against " << stream.url();
    EXPECT_TRUE(stream.is_ready());

    stream.disconnect();
    EXPECT_FALSE(stream.is_ready());
}

TEST_F(OptionStreamIntegration, BadCredentialsAreReportedRatherThanHanging) {
    option_data_stream stream(credentials("PKNOTREAL", "nope"));

    latch got_error;
    stream.on_error([&](const stream_error &) { got_error.trip(); });
    stream.set_reconnect_policy({false, 0, 0, 0});

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(stream.connect(connect_timeout));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_TRUE(got_error.wait(connect_timeout)) << "no error frame delivered";
    EXPECT_FALSE(stream.is_ready());
    EXPECT_LT(elapsed, connect_timeout / 2)
        << "connect() waited out its timeout instead of failing on the error frame";
}

TEST_F(OptionStreamIntegration, ServerEchoesTheSubscriptionSet) {
    option_data_stream stream;

    latch echoed;
    std::mutex mutex;
    subscriptions confirmed;

    stream.on_subscription([&](const subscriptions &s) {
        if (s.trades.empty() || s.quotes.empty()) {
            return;
        }
        {
            std::lock_guard lock(mutex);
            confirmed = s;
        }
        echoed.trip();
    });

    // Subscribed before connect(), so this also covers the replay on authentication.
    stream.subscribe_trades({contract});
    stream.subscribe_quotes({contract});
    ASSERT_TRUE(stream.connect(connect_timeout));
    ASSERT_TRUE(echoed.wait(message_timeout));

    std::lock_guard lock(mutex);
    EXPECT_EQ(confirmed.trades, std::vector<std::string>{contract});
    EXPECT_EQ(confirmed.quotes, std::vector<std::string>{contract});
}

TEST_F(OptionStreamIntegration, UnsubscribeIsReflectedInTheEcho) {
    option_data_stream stream;

    latch subscribed;
    latch unsubscribed;
    bool had_quote = false;     // only ever touched on the service thread
    stream.on_subscription([&](const subscriptions &s) {
        const bool has_quote =
            std::find(s.quotes.begin(), s.quotes.end(), contract) != s.quotes.end();
        if (has_quote) {
            had_quote = true;
            subscribed.trip();
        }
        else if (had_quote && !s.trades.empty()) {
            unsubscribed.trip();    // trades still there, quotes gone
        }
    });

    ASSERT_TRUE(stream.connect(connect_timeout));
    stream.subscribe_trades({contract});        // after the handshake, not before
    stream.subscribe_quotes({contract});
    ASSERT_TRUE(subscribed.wait(message_timeout));

    stream.unsubscribe_quotes({contract});
    EXPECT_TRUE(unsubscribed.wait(message_timeout));
}

TEST_F(OptionStreamIntegration, WildcardQuotesAreRefusedWithError413) {
    option_data_stream stream;

    latch refused;
    std::mutex mutex;
    stream_error error;
    stream.on_error([&](const stream_error &e) {
        {
            std::lock_guard lock(mutex);
            error = e;
        }
        refused.trip();
    });

    ASSERT_TRUE(stream.connect(connect_timeout));
    stream.subscribe_quotes({"*"});
    ASSERT_TRUE(refused.wait(message_timeout));

    std::lock_guard lock(mutex);
    EXPECT_EQ(error.code, 413);
    // An in-band error after the handshake is not a reason to drop the session.
    EXPECT_TRUE(stream.is_ready());
}

}   // namespace alpaca::tests
