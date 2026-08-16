// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for timeframes and Market Data query-string construction.

#include <gtest/gtest.h>

#include <alpaca/data/corporate_action.hpp>
#include <alpaca/data/forex.hpp>
#include <alpaca/data/news.hpp>
#include <alpaca/data/query.hpp>
#include <alpaca/data/timeframe.hpp>

namespace alpaca::tests {

// ---------------------------------------------------------------------------
// Timeframe
// ---------------------------------------------------------------------------

TEST(Timeframe, SerialisesToAlpacaSpelling) {
    EXPECT_EQ(timeframes::one_minute.to_string(), "1Min");
    EXPECT_EQ(timeframes::five_minutes.to_string(), "5Min");
    EXPECT_EQ(timeframes::fifteen_minutes.to_string(), "15Min");
    EXPECT_EQ(timeframes::one_hour.to_string(), "1Hour");
    EXPECT_EQ(timeframes::four_hours.to_string(), "4Hour");
    EXPECT_EQ(timeframes::one_day.to_string(), "1Day");
    EXPECT_EQ(timeframes::one_week.to_string(), "1Week");
    EXPECT_EQ(timeframes::one_month.to_string(), "1Month");
}

TEST(Timeframe, ArbitraryIntervals) {
    EXPECT_EQ(timeframe::minutes(7).to_string(), "7Min");
    EXPECT_EQ(timeframe::hours(2).to_string(), "2Hour");
}

TEST(Timeframe, ValidityMatchesAlpacasLimits) {
    // Minutes 1-59, hours 1-23, and exactly 1 for day/week/month.
    EXPECT_TRUE(timeframe::minutes(1).valid());
    EXPECT_TRUE(timeframe::minutes(59).valid());
    EXPECT_FALSE(timeframe::minutes(60).valid());
    EXPECT_FALSE(timeframe::minutes(0).valid());

    EXPECT_TRUE(timeframe::hours(23).valid());
    EXPECT_FALSE(timeframe::hours(24).valid());

    EXPECT_TRUE(timeframes::one_day.valid());
    EXPECT_FALSE((timeframe{2, timeframe_unit::day}).valid());
    EXPECT_FALSE((timeframe{2, timeframe_unit::month}).valid());
}

TEST(Timeframe, DefaultIsOneDayAndComparesByValue) {
    EXPECT_EQ(timeframe{}, timeframes::one_day);
    EXPECT_NE(timeframes::one_minute, timeframes::five_minutes);
}

// ---------------------------------------------------------------------------
// Historical queries
// ---------------------------------------------------------------------------

TEST(HistoryQuery, EmptyQueryProducesEmptyString) {
    EXPECT_TRUE(history_query{}.build().empty());
}

TEST(HistoryQuery, BuildsFullParameterSet) {
    history_query q;
    q.symbols = {"AAPL", "MSFT"};
    q.start = "2024-01-02";
    q.end = "2024-02-01";
    q.limit = 500;
    q.feed = data_feed::iex;
    q.sort = sort_direction::asc;

    EXPECT_EQ(q.build().str(),
              "?symbols=AAPL%2CMSFT&start=2024-01-02&end=2024-02-01&limit=500&sort=asc&feed=iex");
}

TEST(BarQuery, AppendsTimeframeAndAdjustment) {
    bar_query q;
    q.symbols = {"AAPL"};
    q.timeframe = timeframes::fifteen_minutes;
    q.adjustment = adjustment::split;
    q.feed = data_feed::sip;

    EXPECT_EQ(q.build().str(), "?symbols=AAPL&feed=sip&timeframe=15Min&adjustment=split");
}

TEST(BarQuery, DefaultsToOneDayAndOmitsAdjustment) {
    bar_query q;
    q.symbols = {"AAPL"};
    EXPECT_EQ(q.build().str(), "?symbols=AAPL&timeframe=1Day");
}

TEST(LatestQuery, BuildsSymbolsAndFeed) {
    latest_query q;
    q.symbols = {"AAPL", "MSFT"};
    q.feed = data_feed::iex;
    EXPECT_EQ(q.build().str(), "?symbols=AAPL%2CMSFT&feed=iex");
}

TEST(CryptoQuery, HasNoFeedOrCurrency) {
    // The venue is a path segment for crypto, so no feed parameter is sent.
    crypto_bar_query q;
    q.symbols = {"BTC/USD"};
    q.timeframe = timeframes::one_hour;

    // The slash in the pair must be percent-encoded.
    EXPECT_EQ(q.build().str(), "?symbols=BTC%2FUSD&timeframe=1Hour");
}

TEST(OptionQuery, UsesOptionFeedRatherThanStockFeed) {
    option_latest_query q;
    q.symbols = {"AAPL240628C00200000"};
    q.feed = option_feed::opra;
    EXPECT_EQ(q.build().str(), "?symbols=AAPL240628C00200000&feed=opra");

    option_bar_query bars;
    bars.symbols = {"AAPL240628C00200000"};
    bars.timeframe = timeframes::one_day;
    EXPECT_EQ(bars.build().str(), "?symbols=AAPL240628C00200000&timeframe=1Day");
}

TEST(OptionChainQuery, BuildsFilters) {
    option_chain_query q;
    q.type = contract_type::call;
    q.strike_price_gte = 190;
    q.strike_price_lte = 210;
    q.expiration_date = "2024-06-28";
    q.limit = 50;

    EXPECT_EQ(q.build().str(),
              "?limit=50&type=call&strike_price_gte=190&strike_price_lte=210"
              "&expiration_date=2024-06-28");
}

TEST(OptionChainQuery, DefaultIsEmpty) {
    EXPECT_TRUE(option_chain_query{}.build().empty());
}

// ---------------------------------------------------------------------------
// Other queries
// ---------------------------------------------------------------------------

TEST(NewsQuery, BuildsFilters) {
    news_query q;
    q.symbols = {"AAPL"};
    q.limit = 50;
    q.include_content = true;
    q.exclude_contentless = false;
    q.sort = sort_direction::desc;

    EXPECT_EQ(q.build().str(),
              "?symbols=AAPL&limit=50&sort=desc&include_content=true&exclude_contentless=false");
}

TEST(ForexQuery, BuildsCurrencyPairs) {
    forex_query q;
    q.currency_pairs = {"USDJPY", "USDMXN"};
    q.timeframe = "1Min";

    EXPECT_EQ(q.build().str(), "?currency_pairs=USDJPY%2CUSDMXN&timeframe=1Min");
}

TEST(CorporateActionQuery, BuildsFilters) {
    corporate_action_query q;
    q.symbols = {"AAPL"};
    q.types = {"cash_dividend", "forward_split"};
    q.start = "2024-01-01";
    q.end = "2024-12-31";
    q.limit = 100;

    EXPECT_EQ(q.build().str(),
              "?symbols=AAPL&types=cash_dividend%2Cforward_split"
              "&start=2024-01-01&end=2024-12-31&limit=100");
}

TEST(Queries, PageTokenIsReplacedNotDuplicated) {
    // Pagination swaps page_token in place; a duplicated key would make Alpaca 422.
    bar_query q;
    q.symbols = {"AAPL"};
    q.page_token = "first";

    auto built = q.build();
    built.set("page_token", "second");

    const auto s = built.str();
    EXPECT_NE(s.find("page_token=second"), std::string::npos);
    EXPECT_EQ(s.find("page_token=first"), std::string::npos);
    EXPECT_EQ(s.find("page_token"), s.rfind("page_token"));
}

}   // namespace alpaca::tests
