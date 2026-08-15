// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Integration tests against a real PAPER trading account.
//
// These are skipped unless APCA_API_KEY_ID and APCA_API_SECRET_KEY are set, so the default
// `ctest` run needs no credentials and no network. They deliberately only ever talk to
// paper-api.alpaca.markets — nothing here should be pointed at a live account.
//
//   set APCA_API_KEY_ID=...
//   set APCA_API_SECRET_KEY=...
//   alpaca_tests.exe --gtest_filter=*Integration*

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <string>
#include <thread>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <slick/logger.hpp>
#include <slick/net/logging.hpp>

#include <alpaca/error.hpp>
#include <alpaca/trading_client.hpp>
#include <alpaca/trading_client_awaitable.hpp>

namespace alpaca::tests {

namespace {

bool credentials_available() {
    return !credentials::from_env().empty();
}

/// Why the suite cannot run, or an empty string when it can.
///
/// Having credentials in the environment is not the same as having credentials that work
/// against *paper*. Live keys (`AK...`) authenticate fine on unscoped endpoints like
/// /v2/clock but are rejected with 401 on every account-scoped one, which would otherwise
/// surface as a dozen unrelated-looking failures. Probe once and report the real reason.
std::string paper_access_blocker() {
    if (!credentials_available()) {
        return "APCA_API_KEY_ID / APCA_API_SECRET_KEY not set";
    }

    trading_client probe;
    probe.set_retry_policy({1, 0, 0});
    try {
        probe.get_account();
        return {};
    }
    catch (const api_error &e) {
        if (e.is_unauthorized()) {
            return std::format(
                "credentials in the environment are not valid for paper trading "
                "(HTTP {}: {}). Paper keys start with 'PK'; live keys start with 'AK' and are "
                "rejected by paper-api.alpaca.markets. Set paper keys to run these tests.",
                e.http_status, e.message);
        }
        return std::format("paper account unreachable (HTTP {}: {})", e.http_status, e.message);
    }
    catch (const std::exception &e) {
        return std::format("paper account unreachable: {}", e.what());
    }
}

/// A client-order-id unique to this run, so repeated runs never collide.
std::string unique_client_order_id(std::string_view prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::format("alpaca-cpp-test-{}-{}", prefix, now);
}

/// Drives an awaitable to completion on a private io_context and returns its result.
template <typename Awaitable>
auto run_awaitable(Awaitable &&awaitable) {
    asio::io_context io;
    auto future = asio::co_spawn(io, std::forward<Awaitable>(awaitable), asio::use_future);
    io.run();
    return future.get();
}

}   // namespace

class TradingIntegration : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        auto &logger = slick::logger::Logger::instance();
        logger.clear_sinks();
        logger.add_console_sink();
        logger.init(1048576, 16777216);
        slick::net::set_log_handler(
            [&logger](slick::net::LogLevel level, const char *format_text, std::format_args args) {
                logger.log(static_cast<slick::logger::LogLevel>(level), format_text, args);
            });

        blocker_ = paper_access_blocker();
    }

    void SetUp() override {
        if (!blocker_.empty()) {
            GTEST_SKIP() << "skipping paper integration tests: " << blocker_;
        }
    }

    void TearDown() override {
        // Never leave a working order behind, even when an assertion aborted the test.
        if (!open_order_id_.empty()) {
            try {
                client_.cancel_order(open_order_id_);
            }
            catch (const std::exception &e) {
                ADD_FAILURE() << "failed to clean up order " << open_order_id_ << ": " << e.what();
            }
            open_order_id_.clear();
        }
    }

    /// Default construction is paper + environment credentials — never live money.
    trading_client client_;
    std::string open_order_id_;
    static inline std::string blocker_;
};

// ---------------------------------------------------------------------------
// Read-only endpoints
// ---------------------------------------------------------------------------

// Not a fixture test: this is the safety-critical default and must be verified on every
// run, including runs with no credentials configured at all.
TEST(TradingClientDefaults, TargetsPaperTradingAndNeverLive) {
    const trading_client client{credentials("k", "s")};
    EXPECT_EQ(client.base_url(), "https://paper-api.alpaca.markets");
    EXPECT_EQ(client.env(), environment::paper);

    const trading_client_awaitable awaitable{credentials("k", "s")};
    EXPECT_EQ(awaitable.base_url(), "https://paper-api.alpaca.markets");

    // Live must be an explicit, deliberate act.
    const trading_client live{credentials("k", "s"), environment::live};
    EXPECT_EQ(live.base_url(), "https://api.alpaca.markets");
}

TEST_F(TradingIntegration, GetAccount) {
    const auto account = client_.get_account();

    EXPECT_FALSE(account.id.empty());
    EXPECT_FALSE(account.account_number.empty());
    EXPECT_EQ(account.currency, "USD");
    EXPECT_NE(account.status, account_status::unknown);
    EXPECT_GT(account.created_at, 0ull);
}

TEST_F(TradingIntegration, GetClock) {
    const auto clock = client_.get_clock();

    EXPECT_GT(clock.timestamp, 0ull);
    EXPECT_GT(clock.next_open, 0ull);
    EXPECT_GT(clock.next_close, 0ull);
}

TEST_F(TradingIntegration, GetCalendar) {
    calendar_query query;
    query.start = "2024-01-02";
    query.end = "2024-01-10";

    const auto days = client_.get_calendar(query);

    ASSERT_FALSE(days.empty());
    EXPECT_EQ(days.front().date, "2024-01-02");
    EXPECT_FALSE(days.front().open.empty());
    EXPECT_GT(days.front().close_ns, days.front().open_ns);
}

TEST_F(TradingIntegration, GetAccountConfigurations) {
    const auto config = client_.get_account_configurations();
    EXPECT_FALSE(config.max_margin_multiplier.empty());
}

TEST_F(TradingIntegration, GetAsset) {
    const auto asset = client_.get_asset("AAPL");

    EXPECT_EQ(asset.symbol, "AAPL");
    EXPECT_EQ(asset.asset_class, asset_class::us_equity);
    EXPECT_EQ(asset.status, asset_status::active);
    EXPECT_TRUE(asset.tradable);
    EXPECT_FALSE(asset.id.empty());
}

TEST_F(TradingIntegration, ListAssetsWithFilter) {
    asset_query query;
    query.status = asset_status::active;
    query.asset_class = alpaca::asset_class::us_equity;

    const auto assets = client_.list_assets(query);

    ASSERT_FALSE(assets.empty());
    EXPECT_EQ(assets.front().asset_class, asset_class::us_equity);
}

TEST_F(TradingIntegration, ListPositions) {
    // A fresh paper account has no positions; the call must still succeed and return an
    // empty vector rather than raising.
    const auto positions = client_.list_positions();
    for (const auto &p : positions) {
        EXPECT_FALSE(p.symbol.empty());
        EXPECT_NE(p.side, position_side::unknown);
    }
}

TEST_F(TradingIntegration, ListOrders) {
    order_query query;
    query.status = order_status_filter::all;
    query.limit = 10;

    const auto orders = client_.list_orders(query);
    EXPECT_LE(orders.size(), 10u);
}

TEST_F(TradingIntegration, GetPortfolioHistory) {
    portfolio_history_query query;
    query.period = "1M";
    query.timeframe = "1D";

    const auto history = client_.get_portfolio_history(query);
    EXPECT_EQ(history.timestamp.size(), history.equity.size());
}

TEST_F(TradingIntegration, GetActivities) {
    activity_query query;
    query.page_size = 10;

    const auto activities = client_.get_activities(query);
    EXPECT_LE(activities.size(), 10u);
}

// ---------------------------------------------------------------------------
// Error handling
//
// The behaviour this SDK exists to get right: a failure must raise, not return {}.
// ---------------------------------------------------------------------------

TEST_F(TradingIntegration, BadCredentialsRaiseUnauthorizedRatherThanReturningEmpty) {
    trading_client bad_client(credentials("not-a-key", "not-a-secret"), environment::paper);
    // Retrying a 401 is pointless and slow; fail on the first attempt.
    bad_client.set_retry_policy({1, 0, 0});

    try {
        const auto account = bad_client.get_account();
        FAIL() << "expected api_error, got account id '" << account.id << "'";
    }
    catch (const api_error &e) {
        EXPECT_TRUE(e.is_unauthorized()) << "status was " << e.http_status;
        EXPECT_FALSE(e.message.empty());
    }
}

TEST_F(TradingIntegration, UnknownSymbolRaisesNotFound) {
    try {
        client_.get_asset("NOTAREALSYMBOL");
        FAIL() << "expected api_error for an unknown symbol";
    }
    catch (const api_error &e) {
        EXPECT_TRUE(e.is_not_found()) << "status was " << e.http_status;
    }
}

TEST_F(TradingIntegration, UnknownOrderIdRaises) {
    EXPECT_THROW(client_.get_order("00000000-0000-0000-0000-000000000000"), api_error);
}

// ---------------------------------------------------------------------------
// Order lifecycle
//
// Uses a far-from-market limit order so it rests rather than filling, then cancels it.
// ---------------------------------------------------------------------------

TEST_F(TradingIntegration, SubmitGetReplaceAndCancelOrder) {
    const auto client_order_id = unique_client_order_id("lifecycle");

    // $1 limit on AAPL will not fill.
    auto request = order_request::limit("AAPL", 1, order_side::buy, 1.00, time_in_force::gtc);
    request.client_order_id = client_order_id;

    const auto submitted = client_.submit_order(request);
    open_order_id_ = submitted.id;

    ASSERT_FALSE(submitted.id.empty());
    EXPECT_EQ(submitted.symbol, "AAPL");
    EXPECT_EQ(submitted.side, order_side::buy);
    EXPECT_EQ(submitted.type, order_type::limit);
    EXPECT_EQ(submitted.time_in_force, time_in_force::gtc);
    EXPECT_EQ(submitted.client_order_id, client_order_id);
    ASSERT_TRUE(submitted.limit_price.has_value());
    EXPECT_DOUBLE_EQ(*submitted.limit_price, 1.00);
    EXPECT_TRUE(submitted.is_open());

    // Fetch by id.
    const auto fetched = client_.get_order(submitted.id);
    EXPECT_EQ(fetched.id, submitted.id);

    // Fetch by client order id.
    const auto by_client_id = client_.get_order_by_client_order_id(client_order_id);
    EXPECT_EQ(by_client_id.id, submitted.id);

    // Replace, which supersedes the original with a new order id.
    replace_order_request replacement;
    replacement.limit_price = 1.50;
    const auto replaced = client_.replace_order(submitted.id, replacement);
    ASSERT_FALSE(replaced.id.empty());
    open_order_id_ = replaced.id;
    ASSERT_TRUE(replaced.limit_price.has_value());
    EXPECT_DOUBLE_EQ(*replaced.limit_price, 1.50);

    // Cancel. Alpaca answers 204 and cancels asynchronously.
    ASSERT_NO_THROW(client_.cancel_order(replaced.id));
    open_order_id_.clear();

    // Poll briefly for the terminal state rather than assuming it is immediate.
    bool reached_terminal_state = false;
    for (int attempt = 0; attempt < 20 && !reached_terminal_state; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        reached_terminal_state = !client_.get_order(replaced.id).is_open();
    }
    EXPECT_TRUE(reached_terminal_state) << "order did not reach a terminal state after cancel";
}

TEST_F(TradingIntegration, RejectedOrderRaisesWithAlpacasMessage) {
    // Quantity 0 is rejected by Alpaca with a 422 and a descriptive message.
    auto request = order_request::limit("AAPL", 0, order_side::buy, 1.00);

    try {
        const auto submitted = client_.submit_order(request);
        open_order_id_ = submitted.id;
        FAIL() << "expected api_error for a zero-quantity order";
    }
    catch (const api_error &e) {
        EXPECT_GE(e.http_status, 400u);
        EXPECT_LT(e.http_status, 500u);
        EXPECT_FALSE(e.message.empty());
    }
}

// ---------------------------------------------------------------------------
// Watchlists
// ---------------------------------------------------------------------------

TEST_F(TradingIntegration, WatchlistLifecycle) {
    const auto name = unique_client_order_id("watchlist");

    const auto created = client_.create_watchlist({name, {"AAPL"}});
    ASSERT_FALSE(created.id.empty());

    try {
        EXPECT_EQ(created.name, name);
        ASSERT_EQ(created.assets.size(), 1u);
        EXPECT_EQ(created.assets[0].symbol, "AAPL");

        const auto with_msft = client_.add_asset_to_watchlist(created.id, "MSFT");
        EXPECT_EQ(with_msft.assets.size(), 2u);

        const auto fetched = client_.get_watchlist(created.id);
        EXPECT_EQ(fetched.id, created.id);

        const auto without_msft = client_.remove_asset_from_watchlist(created.id, "MSFT");
        EXPECT_EQ(without_msft.assets.size(), 1u);
    }
    catch (...) {
        client_.delete_watchlist(created.id);
        throw;
    }

    ASSERT_NO_THROW(client_.delete_watchlist(created.id));
    EXPECT_THROW(client_.get_watchlist(created.id), api_error);
}

// ---------------------------------------------------------------------------
// Option contracts
// ---------------------------------------------------------------------------

TEST_F(TradingIntegration, ListOptionContracts) {
    option_contract_query query;
    query.underlying_symbols = {"AAPL"};
    query.limit = 10;

    const auto page = client_.list_option_contracts_page(query);

    ASSERT_FALSE(page.contracts.empty());
    EXPECT_EQ(page.contracts.front().underlying_symbol, "AAPL");
    EXPECT_GT(page.contracts.front().strike_price, 0.0);
    EXPECT_NE(page.contracts.front().type, contract_type::unknown);
}

// ---------------------------------------------------------------------------
// Coroutine client
//
// Same endpoints, same semantics, awaited instead of blocking.
// ---------------------------------------------------------------------------

class TradingIntegrationAwaitable : public TradingIntegration {};

TEST_F(TradingIntegrationAwaitable, GetAccount) {
    trading_client_awaitable client;

    const auto account = run_awaitable(client.get_account());

    EXPECT_FALSE(account.id.empty());
    EXPECT_EQ(account.currency, "USD");
}

TEST_F(TradingIntegrationAwaitable, GetClockAndAsset) {
    trading_client_awaitable client;

    const auto clock = run_awaitable(client.get_clock());
    EXPECT_GT(clock.timestamp, 0ull);

    const auto asset = run_awaitable(client.get_asset("AAPL"));
    EXPECT_EQ(asset.symbol, "AAPL");
}

TEST_F(TradingIntegrationAwaitable, ErrorsPropagateThroughTheCoroutine) {
    trading_client_awaitable bad_client(credentials("not-a-key", "not-a-secret"), environment::paper);
    bad_client.set_retry_policy({1, 0, 0});

    EXPECT_THROW(run_awaitable(bad_client.get_account()), api_error);
}

}   // namespace alpaca::tests
