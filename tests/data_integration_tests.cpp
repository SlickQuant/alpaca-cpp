// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Integration tests against the live Market Data API.
//
// Skipped unless credentials are configured. Unlike the trading tests, these work with
// either paper or live keys: data.alpaca.markets is not account-scoped, so the paper pair
// (APCA_PAPER_API_*) and the unprefixed pair are both accepted.
// Every request is read-only and pinned to the IEX feed, which needs no data subscription.

#include <gtest/gtest.h>

#include <format>
#include <string>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <alpaca/data_client.hpp>
#include <alpaca/data_client_awaitable.hpp>
#include <alpaca/error.hpp>

namespace alpaca::tests {

namespace {

/// A window well in the past, so the data exists and the result is stable over time.
constexpr const char *window_start = "2024-01-02";
constexpr const char *window_end = "2024-01-06";

template <typename Awaitable>
auto run_awaitable(Awaitable &&awaitable) {
    asio::io_context io;
    auto future = asio::co_spawn(io, std::forward<Awaitable>(awaitable), asio::use_future);
    io.run();
    return future.get();
}

}   // namespace

class DataIntegration : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // `data_client` defaults to the paper environment, so it resolves the paper pair
        // first and falls back to the unprefixed one. Gate on the same resolution, or a
        // paper-only setup would skip a group its credentials can in fact reach.
        if (credentials::from_env(environment::paper).empty()) {
            blocker_ = "APCA_PAPER_API_KEY_ID / APCA_PAPER_API_SECRET_KEY "
                       "(or APCA_API_KEY_ID / APCA_API_SECRET_KEY) not set";
            return;
        }

        // Probe once so a credentials problem is reported as one clear reason rather than
        // as a dozen unrelated-looking failures.
        data_client probe;
        probe.set_retry_policy({1, 0, 0});
        try {
            probe.get_stock_exchange_codes();
            blocker_.clear();
        }
        catch (const api_error &e) {
            blocker_ = std::format("market data unreachable (HTTP {}: {})", e.http_status, e.message);
        }
        catch (const std::exception &e) {
            blocker_ = std::format("market data unreachable: {}", e.what());
        }
    }

    void SetUp() override {
        if (!blocker_.empty()) {
            GTEST_SKIP() << "skipping market data integration tests: " << blocker_;
        }
    }

    /// The IEX feed is the one available without a data subscription.
    static bar_query iex_bars(std::vector<std::string> symbols) {
        bar_query query;
        query.symbols = std::move(symbols);
        query.timeframe = timeframes::one_day;
        query.start = window_start;
        query.end = window_end;
        query.feed = data_feed::iex;
        return query;
    }

    data_client client_;
    static inline std::string blocker_;
};

TEST_F(DataIntegration, TargetsTheMarketDataHost) {
    EXPECT_EQ(client_.base_url(), "https://data.alpaca.markets");
}

// ---------------------------------------------------------------------------
// Stocks
// ---------------------------------------------------------------------------

TEST_F(DataIntegration, GetStockBars) {
    const auto bars = client_.get_stock_bars(iex_bars({"AAPL"}));

    ASSERT_TRUE(bars.contains("AAPL"));
    const auto &aapl = bars.at("AAPL");
    ASSERT_FALSE(aapl.empty());

    for (const auto &b : aapl) {
        EXPECT_GT(b.timestamp, 0ull);
        EXPECT_GT(b.open, 0.);
        EXPECT_GE(b.high, b.low);
        EXPECT_GT(b.volume, 0.);
    }

    // Daily bars over a 4-day window, ascending by default.
    EXPECT_LE(aapl.size(), 5u);
    for (size_t i = 1; i < aapl.size(); ++i) {
        EXPECT_LT(aapl[i - 1].timestamp, aapl[i].timestamp);
    }
}

TEST_F(DataIntegration, GetStockBarsForMultipleSymbols) {
    const auto bars = client_.get_stock_bars(iex_bars({"AAPL", "MSFT"}));

    EXPECT_TRUE(bars.contains("AAPL"));
    EXPECT_TRUE(bars.contains("MSFT"));
    EXPECT_FALSE(bars.at("MSFT").empty());
}

TEST_F(DataIntegration, PaginationMergesEveryPage) {
    // A full regular session (14:30-21:00 UTC) is 390 minute bars, so a 100-row page size
    // forces at least four pages. A shorter window would fit in one page and would not
    // exercise next_page_token at all.
    bar_query query = iex_bars({"AAPL"});
    query.timeframe = timeframes::one_minute;
    query.start = "2024-01-02T14:30:00Z";
    query.end = "2024-01-02T21:00:00Z";
    query.limit = 100;

    const auto bars = client_.get_stock_bars(query);

    ASSERT_TRUE(bars.contains("AAPL"));
    EXPECT_GT(bars.at("AAPL").size(), 100u) << "pagination did not follow next_page_token";

    // A single page returns at most the limit, and hands back a token to continue.
    const auto page = client_.get_stock_bars_page(query);
    ASSERT_TRUE(page.bars.contains("AAPL"));
    EXPECT_LE(page.bars.at("AAPL").size(), 100u);
    EXPECT_FALSE(page.next_page_token.empty());
}

TEST_F(DataIntegration, GetStockTradesAndQuotes) {
    history_query query;
    query.symbols = {"AAPL"};
    query.start = "2024-01-02T14:30:00Z";
    query.end = "2024-01-02T14:31:00Z";
    query.feed = data_feed::iex;
    query.limit = 50;

    const auto trades = client_.get_stock_trades_page(query);
    ASSERT_TRUE(trades.trades.contains("AAPL"));
    ASSERT_FALSE(trades.trades.at("AAPL").empty());
    EXPECT_GT(trades.trades.at("AAPL").front().price, 0.);

    const auto quotes = client_.get_stock_quotes_page(query);
    ASSERT_TRUE(quotes.quotes.contains("AAPL"));
    ASSERT_FALSE(quotes.quotes.at("AAPL").empty());
}

TEST_F(DataIntegration, GetLatestStockQuoteAndTrade) {
    // The last trade persists after the close, so it is always populated.
    const auto trade = client_.get_latest_stock_trade("AAPL", data_feed::iex);
    EXPECT_GT(trade.timestamp, 0ull);
    EXPECT_GT(trade.price, 0.);

    // The quote is not: IEX stops quoting outside regular hours and returns an empty
    // book, so asserting a positive ask would make this test fail every weekend. Assert
    // what holds in both sessions, and check coherence only when a book is actually up.
    const auto quote = client_.get_latest_stock_quote("AAPL", data_feed::iex);
    EXPECT_GT(quote.timestamp, 0ull);
    EXPECT_GE(quote.bid_price, 0.);
    EXPECT_GE(quote.ask_price, 0.);
    if (quote.bid_price > 0. && quote.ask_price > 0.) {
        EXPECT_GE(quote.ask_price, quote.bid_price) << "crossed quote";
    }
}

TEST_F(DataIntegration, GetStockSnapshot) {
    const auto snapshot = client_.get_stock_snapshot("AAPL", data_feed::iex);

    EXPECT_GT(snapshot.daily_bar.close, 0.);
    EXPECT_GT(snapshot.prev_daily_bar.close, 0.);
}

TEST_F(DataIntegration, GetReferenceCodeMaps) {
    const auto exchanges = client_.get_stock_exchange_codes();
    ASSERT_FALSE(exchanges.empty());
    EXPECT_TRUE(exchanges.contains("V")) << "IEX exchange code missing";

    const auto conditions = client_.get_stock_condition_codes("trade", "A");
    EXPECT_FALSE(conditions.empty());
}

TEST_F(DataIntegration, UnknownSymbolYieldsNoDataRatherThanAnError) {
    // A symbol with no data is an empty result, not a failure — the distinction the SDK's
    // throw-on-error design exists to preserve.
    const auto bars = client_.get_stock_bars(iex_bars({"NOTAREALSYMBOL"}));
    EXPECT_TRUE(bars.empty() || bars.at("NOTAREALSYMBOL").empty());
}

TEST_F(DataIntegration, BadCredentialsRaiseUnauthorized) {
    data_client bad_client(credentials("not-a-key", "not-a-secret"));
    bad_client.set_retry_policy({1, 0, 0});

    try {
        bad_client.get_stock_exchange_codes();
        FAIL() << "expected api_error";
    }
    catch (const api_error &e) {
        EXPECT_TRUE(e.is_unauthorized()) << "status was " << e.http_status;
    }
}

// ---------------------------------------------------------------------------
// Crypto — no subscription required
// ---------------------------------------------------------------------------

TEST_F(DataIntegration, GetCryptoBars) {
    crypto_bar_query query;
    query.symbols = {"BTC/USD"};
    query.timeframe = timeframes::one_day;
    query.start = window_start;
    query.end = window_end;

    const auto bars = client_.get_crypto_bars(query);

    ASSERT_TRUE(bars.contains("BTC/USD"));
    ASSERT_FALSE(bars.at("BTC/USD").empty());
    EXPECT_GT(bars.at("BTC/USD").front().close, 0.);
}

TEST_F(DataIntegration, GetLatestCryptoOrderbook) {
    const auto books = client_.get_latest_crypto_orderbooks({"BTC/USD"});

    ASSERT_TRUE(books.contains("BTC/USD"));
    const auto &book = books.at("BTC/USD");
    EXPECT_GT(book.timestamp, 0ull);
    ASSERT_FALSE(book.bids.empty());
    ASSERT_FALSE(book.asks.empty());
    EXPECT_GT(book.best_ask(), book.best_bid()) << "crossed book";
}

TEST_F(DataIntegration, GetLatestCryptoQuotes) {
    const auto quotes = client_.get_latest_crypto_quotes({"BTC/USD", "ETH/USD"});

    EXPECT_TRUE(quotes.contains("BTC/USD"));
    EXPECT_TRUE(quotes.contains("ETH/USD"));
}

// ---------------------------------------------------------------------------
// News and screener — no subscription required
// ---------------------------------------------------------------------------

TEST_F(DataIntegration, GetNews) {
    news_query query;
    query.symbols = {"AAPL"};
    query.limit = 10;
    query.start = window_start;
    query.end = window_end;

    const auto page = client_.get_news_page(query);

    ASSERT_FALSE(page.news.empty());
    EXPECT_FALSE(page.news.front().headline.empty());
    EXPECT_GT(page.news.front().created_at, 0ull);
}

TEST_F(DataIntegration, GetMostActivesAndMovers) {
    const auto actives = client_.get_most_actives({}, 5);
    ASSERT_FALSE(actives.items.empty());
    EXPECT_LE(actives.items.size(), 5u);
    EXPECT_FALSE(actives.items.front().symbol.empty());

    const auto movers = client_.get_movers("stocks", 5);
    EXPECT_LE(movers.gainers.size(), 5u);
    EXPECT_LE(movers.losers.size(), 5u);
}

// ---------------------------------------------------------------------------
// Corporate actions
// ---------------------------------------------------------------------------

TEST_F(DataIntegration, GetCorporateActions) {
    corporate_action_query query;
    query.symbols = {"AAPL"};
    query.start = "2024-01-01";
    query.end = "2024-12-31";
    query.limit = 50;

    const auto actions = client_.get_corporate_actions(query);

    // AAPL paid dividends through 2024, so the window is not empty.
    ASSERT_FALSE(actions.empty());
    for (const auto &a : actions) {
        EXPECT_NE(a.type, corporate_action_type::unknown)
            << "unmapped corporate action group for " << a.symbol;
        EXPECT_FALSE(a.symbol.empty());
    }
}

// ---------------------------------------------------------------------------
// Logos — binary, not JSON
// ---------------------------------------------------------------------------

TEST_F(DataIntegration, GetLogoReturnsPngBytes) {
    std::string logo;
    try {
        logo = client_.get_logo("AAPL");
    }
    catch (const api_error &e) {
        if (e.http_status == 403) {
            // Logos are a separately entitled product; not every data plan includes them.
            // Reaching this branch still proves the raw-body path surfaces errors rather
            // than trying to parse a non-JSON response.
            GTEST_SKIP() << "account's data subscription does not include logos: " << e.message;
        }
        throw;
    }

    ASSERT_GT(logo.size(), 8u);
    // PNG magic number, proving the raw path returned bytes rather than parsing them as JSON.
    EXPECT_EQ(logo.substr(1, 3), "PNG");
}

// ---------------------------------------------------------------------------
// Coroutine client
// ---------------------------------------------------------------------------

class DataIntegrationAwaitable : public DataIntegration {};

TEST_F(DataIntegrationAwaitable, GetStockBars) {
    data_client_awaitable client;

    const auto bars = run_awaitable(client.get_stock_bars(iex_bars({"AAPL"})));

    ASSERT_TRUE(bars.contains("AAPL"));
    EXPECT_FALSE(bars.at("AAPL").empty());
}

TEST_F(DataIntegrationAwaitable, GetLatestQuoteAndExchangeCodes) {
    data_client_awaitable client;

    const auto quote = run_awaitable(client.get_latest_stock_quote("AAPL", data_feed::iex));
    EXPECT_GT(quote.timestamp, 0ull);

    const auto exchanges = run_awaitable(client.get_stock_exchange_codes());
    EXPECT_FALSE(exchanges.empty());
}

TEST_F(DataIntegrationAwaitable, ErrorsPropagateThroughTheCoroutine) {
    data_client_awaitable bad_client(credentials("not-a-key", "not-a-secret"));
    bad_client.set_retry_policy({1, 0, 0});

    EXPECT_THROW(run_awaitable(bad_client.get_stock_exchange_codes()), api_error);
}

}   // namespace alpaca::tests
