// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline Market Data model tests. Fixtures mirror real Alpaca payloads, including the
// single-letter wire keys and the places where the same key means different things.

#include <gtest/gtest.h>

#include <alpaca/data/auction.hpp>
#include <alpaca/data/bar.hpp>
#include <alpaca/data/corporate_action.hpp>
#include <alpaca/data/fixed_income.hpp>
#include <alpaca/data/forex.hpp>
#include <alpaca/data/news.hpp>
#include <alpaca/data/orderbook.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/screener.hpp>
#include <alpaca/data/snapshot.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/detail/response.hpp>

namespace alpaca::tests {

// ---------------------------------------------------------------------------
// Bars
// ---------------------------------------------------------------------------

TEST(BarModel, ParsesSingleLetterKeys) {
    const json j = json::parse(R"({
        "t": "2022-01-03T09:00:00Z",
        "o": 178.26, "h": 178.34, "l": 177.76, "c": 178.08,
        "v": 60937, "n": 1727, "vw": 177.954244
    })");

    bar b;
    from_json(j, b);

    EXPECT_EQ(b.timestamp, to_nanoseconds("2022-01-03T09:00:00Z"));
    EXPECT_DOUBLE_EQ(b.open, 178.26);
    EXPECT_DOUBLE_EQ(b.high, 178.34);
    EXPECT_DOUBLE_EQ(b.low, 177.76);
    EXPECT_DOUBLE_EQ(b.close, 178.08);
    EXPECT_DOUBLE_EQ(b.volume, 60937.0);
    EXPECT_EQ(b.trade_count, 1727u);
    EXPECT_DOUBLE_EQ(b.vwap, 177.954244);
}

TEST(BarModel, FractionalCryptoVolumeSurvives) {
    // Crypto volumes are fractional, so volume must not be an integer type.
    const json j = json::parse(R"({"t":"2022-05-27T10:18:00Z","o":28999,"h":29003,
                                   "l":28999,"c":29003,"v":0.01,"n":4,"vw":29001})");
    bar b;
    from_json(j, b);
    EXPECT_DOUBLE_EQ(b.volume, 0.01);
}

// ---------------------------------------------------------------------------
// Trades
// ---------------------------------------------------------------------------

TEST(TradeModel, ParsesStockTradeWithConditionArray) {
    const json j = json::parse(R"({
        "t": "2021-02-06T13:04:56.334320128Z",
        "x": "C", "p": 387.62, "s": 100, "i": 52983525029461,
        "c": ["@", "T", "I"], "z": "C"
    })");

    trade t;
    from_json(j, t);

    EXPECT_EQ(t.timestamp, to_nanoseconds("2021-02-06T13:04:56.334320128Z"));
    EXPECT_EQ(t.exchange, "C");
    EXPECT_DOUBLE_EQ(t.price, 387.62);
    EXPECT_DOUBLE_EQ(t.size, 100.0);
    EXPECT_EQ(t.id, 52983525029461ull);
    EXPECT_EQ(t.tape, "C");
    ASSERT_EQ(t.conditions.size(), 3u);
    EXPECT_EQ(t.conditions[0], "@");
}

TEST(TradeModel, ParsesOptionTradeWithSingleConditionString) {
    // Options send `c` as one string where stocks send an array; both must land in the
    // same field so callers never branch on asset class.
    const json j = json::parse(R"({"t":"2024-02-28T15:30:28.046330624Z","x":"W",
                                   "p":0.15,"s":1,"c":"A"})");

    trade t;
    from_json(j, t);

    ASSERT_EQ(t.conditions.size(), 1u);
    EXPECT_EQ(t.conditions[0], "A");
    EXPECT_DOUBLE_EQ(t.price, 0.15);
}

TEST(TradeModel, ParsesCryptoTakerSide) {
    const json j = json::parse(R"({"t":"2022-05-27T10:18:00Z","p":28999,"s":0.01,
                                   "i":123,"tks":"B"})");
    trade t;
    from_json(j, t);

    EXPECT_EQ(t.taker_side, "B");
    EXPECT_TRUE(t.conditions.empty());
    EXPECT_TRUE(t.exchange.empty());
}

// ---------------------------------------------------------------------------
// Quotes
// ---------------------------------------------------------------------------

TEST(QuoteModel, ParsesAndDerivesSpreadAndMid) {
    const json j = json::parse(R"({
        "t": "2021-02-06T13:35:08.946977536Z",
        "bx": "C", "bp": 387.7, "bs": 1,
        "ax": "C", "ap": 387.8, "as": 1,
        "c": ["R"], "z": "C"
    })");

    quote q;
    from_json(j, q);

    EXPECT_DOUBLE_EQ(q.bid_price, 387.7);
    EXPECT_DOUBLE_EQ(q.ask_price, 387.8);
    EXPECT_EQ(q.bid_exchange, "C");
    EXPECT_EQ(q.ask_exchange, "C");
    EXPECT_NEAR(q.spread(), 0.1, 1e-9);
    EXPECT_NEAR(q.mid_price(), 387.75, 1e-9);
}

TEST(QuoteModel, OptionQuoteSingleConditionString) {
    const json j = json::parse(R"({"t":"2024-02-28T15:30:28Z","bx":"W","bp":0.15,"bs":164,
                                   "ap":0.16,"as":669,"ax":"w","c":"A"})");
    quote q;
    from_json(j, q);

    ASSERT_EQ(q.conditions.size(), 1u);
    EXPECT_EQ(q.conditions[0], "A");
    EXPECT_DOUBLE_EQ(q.bid_size, 164.0);
}

// ---------------------------------------------------------------------------
// Snapshots
// ---------------------------------------------------------------------------

TEST(SnapshotModel, ParsesAllFiveSections) {
    const json j = json::parse(R"({
        "latestTrade": {"t":"2022-01-03T20:00:00Z","p":182.01,"s":100,"x":"V","c":["@"],"z":"C"},
        "latestQuote": {"t":"2022-01-03T20:00:01Z","bp":182.0,"bs":2,"ap":182.05,"as":3,"bx":"V","ax":"V"},
        "minuteBar":   {"t":"2022-01-03T19:59:00Z","o":181.9,"h":182.1,"l":181.8,"c":182.0,"v":1000,"n":10,"vw":182.0},
        "dailyBar":    {"t":"2022-01-03T05:00:00Z","o":177.8,"h":182.9,"l":177.7,"c":182.0,"v":104487900,"n":700000,"vw":180.0},
        "prevDailyBar":{"t":"2021-12-31T05:00:00Z","o":178.1,"h":179.2,"l":177.3,"c":177.6,"v":64062300,"n":500000,"vw":178.0}
    })");

    snapshot s;
    from_json(j, s);

    EXPECT_DOUBLE_EQ(s.latest_trade.price, 182.01);
    EXPECT_DOUBLE_EQ(s.latest_quote.ask_price, 182.05);
    EXPECT_DOUBLE_EQ(s.minute_bar.close, 182.0);
    EXPECT_DOUBLE_EQ(s.daily_bar.high, 182.9);
    EXPECT_DOUBLE_EQ(s.prev_daily_bar.close, 177.6);
}

TEST(OptionSnapshotModel, ParsesGreeksAndImpliedVolatility) {
    const json j = json::parse(R"({
        "latestQuote": {"t":"2024-02-28T15:30:28Z","bp":0.15,"bs":164,"ap":0.16,"as":669},
        "greeks": {"delta":0.52,"gamma":0.03,"theta":-0.08,"vega":0.11,"rho":0.02},
        "impliedVolatility": 0.2841
    })");

    option_snapshot s;
    from_json(j, s);

    ASSERT_TRUE(s.greeks.has_value());
    EXPECT_DOUBLE_EQ(s.greeks->delta, 0.52);
    EXPECT_DOUBLE_EQ(s.greeks->theta, -0.08);
    ASSERT_TRUE(s.implied_volatility.has_value());
    EXPECT_DOUBLE_EQ(*s.implied_volatility, 0.2841);
}

TEST(OptionSnapshotModel, AbsentGreeksStayUnsetRatherThanZero) {
    // Alpaca omits greeks for contracts it cannot price. A defaulted delta of 0 would be
    // indistinguishable from a genuinely delta-neutral position.
    const json j = json::parse(R"({"latestQuote":{"t":"2024-02-28T15:30:28Z","bp":0.15}})");

    option_snapshot s;
    from_json(j, s);

    EXPECT_FALSE(s.greeks.has_value());
    EXPECT_FALSE(s.implied_volatility.has_value());
}

TEST(OptionChainPage, ParsesSnapshotsAndToken) {
    const json j = json::parse(R"({
        "snapshots": {
            "AAPL240628C00200000": {"impliedVolatility": 0.3},
            "AAPL240628P00200000": {"impliedVolatility": 0.4}
        },
        "next_page_token": "tok"
    })");

    option_chain_page p;
    from_json(j, p);

    EXPECT_EQ(p.snapshots.size(), 2u);
    EXPECT_EQ(p.next_page_token, "tok");
}

// ---------------------------------------------------------------------------
// Auctions
// ---------------------------------------------------------------------------

TEST(AuctionModel, ParsesOpeningAndClosingArrays) {
    // In this payload `c` is the closing-auction array, not a condition code, and `d` is
    // the trading date — the same letters mean other things on the trade/quote endpoints.
    const json j = json::parse(R"({
        "d": "2024-01-03",
        "o": [{"t":"2024-01-03T14:30:00.1Z","x":"P","p":183.98,"c":"Q","s":1000}],
        "c": [{"t":"2024-01-03T21:00:00.2Z","x":"P","p":184.25,"c":"M","s":2000}]
    })");

    daily_auctions a;
    from_json(j, a);

    EXPECT_EQ(a.date, "2024-01-03");
    EXPECT_EQ(a.date_ns, to_nanoseconds("2024-01-03"));
    ASSERT_EQ(a.opening.size(), 1u);
    ASSERT_EQ(a.closing.size(), 1u);
    EXPECT_DOUBLE_EQ(a.opening[0].price, 183.98);
    EXPECT_EQ(a.opening[0].condition, "Q");
    EXPECT_DOUBLE_EQ(a.closing[0].price, 184.25);
    EXPECT_DOUBLE_EQ(a.closing[0].size, 2000.0);
}

// ---------------------------------------------------------------------------
// Order books
// ---------------------------------------------------------------------------

TEST(OrderbookModel, ParsesLevelsAndTopOfBook) {
    const json j = json::parse(R"({
        "t": "2022-06-24T08:00:14.137774336Z",
        "b": [{"p":20846,"s":0.1902},{"p":20845,"s":0.5}],
        "a": [{"p":20902,"s":0.0097},{"p":20903,"s":1.0}]
    })");

    orderbook o;
    from_json(j, o);

    ASSERT_EQ(o.bids.size(), 2u);
    ASSERT_EQ(o.asks.size(), 2u);
    EXPECT_DOUBLE_EQ(o.best_bid(), 20846.0);
    EXPECT_DOUBLE_EQ(o.best_ask(), 20902.0);
    EXPECT_DOUBLE_EQ(o.spread(), 56.0);
    EXPECT_DOUBLE_EQ(o.bids[0].size, 0.1902);
}

TEST(OrderbookModel, EmptyBookReportsZeroRatherThanCrashing) {
    const json j = json::parse(R"({"t":"2022-06-24T08:00:14Z","b":[],"a":[]})");

    orderbook o;
    from_json(j, o);

    EXPECT_DOUBLE_EQ(o.best_bid(), 0.0);
    EXPECT_DOUBLE_EQ(o.best_ask(), 0.0);
}

// ---------------------------------------------------------------------------
// News
// ---------------------------------------------------------------------------

TEST(NewsModel, ParsesArticleWithImages) {
    const json j = json::parse(R"({
        "id": 24843171,
        "headline": "Apple Reports Q1 Earnings",
        "author": "Benzinga Newsdesk",
        "created_at": "2024-02-01T21:30:00Z",
        "updated_at": "2024-02-01T21:35:00Z",
        "summary": "Apple beat estimates.",
        "content": "<p>Full text</p>",
        "url": "https://example.com/article",
        "source": "benzinga",
        "symbols": ["AAPL"],
        "images": [{"size":"large","url":"https://example.com/large.jpg"},
                   {"size":"thumb","url":"https://example.com/thumb.jpg"}]
    })");

    news_article n;
    from_json(j, n);

    EXPECT_EQ(n.id, 24843171);
    EXPECT_EQ(n.headline, "Apple Reports Q1 Earnings");
    EXPECT_EQ(n.source, "benzinga");
    ASSERT_EQ(n.symbols.size(), 1u);
    EXPECT_EQ(n.symbols[0], "AAPL");
    ASSERT_EQ(n.images.size(), 2u);
    EXPECT_EQ(n.images[0].size, "large");
    EXPECT_EQ(n.created_at, to_nanoseconds("2024-02-01T21:30:00Z"));
}

TEST(NewsModel, NullUrlIsTolerated) {
    const json j = json::parse(R"({"id":1,"headline":"h","url":null,"symbols":[]})");

    news_article n;
    ASSERT_NO_THROW(from_json(j, n));
    EXPECT_TRUE(n.url.empty());
}

// ---------------------------------------------------------------------------
// Screener
// ---------------------------------------------------------------------------

TEST(ScreenerModel, ParsesMostActives) {
    const json j = json::parse(R"({
        "most_actives": [{"symbol":"TSLA","volume":123456789,"trade_count":987654},
                         {"symbol":"AAPL","volume":98765432,"trade_count":654321}],
        "last_updated": "2024-02-01T21:30:00Z"
    })");

    most_actives m;
    from_json(j, m);

    ASSERT_EQ(m.items.size(), 2u);
    EXPECT_EQ(m.items[0].symbol, "TSLA");
    EXPECT_EQ(m.items[0].volume, 123456789ull);
    EXPECT_EQ(m.last_updated, to_nanoseconds("2024-02-01T21:30:00Z"));
}

TEST(ScreenerModel, ParsesMovers) {
    const json j = json::parse(R"({
        "gainers": [{"symbol":"ABC","price":10.5,"change":2.5,"percent_change":31.25}],
        "losers":  [{"symbol":"XYZ","price":4.0,"change":-1.0,"percent_change":-20.0}],
        "market_type": "stocks",
        "last_updated": "2024-02-01T21:30:00Z"
    })");

    movers m;
    from_json(j, m);

    ASSERT_EQ(m.gainers.size(), 1u);
    ASSERT_EQ(m.losers.size(), 1u);
    EXPECT_DOUBLE_EQ(m.gainers[0].percent_change, 31.25);
    EXPECT_DOUBLE_EQ(m.losers[0].change, -1.0);
    EXPECT_EQ(m.market_type, "stocks");
}

// ---------------------------------------------------------------------------
// Corporate actions
// ---------------------------------------------------------------------------

TEST(CorporateActionModel, FlattensGroupedArraysAndTagsTheType) {
    // Alpaca groups the response by type; the SDK flattens it into one typed vector so
    // callers do not have to switch across fifteen parallel arrays.
    const json j = json::parse(R"({
        "corporate_actions": {
            "cash_dividends": [
                {"id":"a1","symbol":"AAPL","ex_date":"2024-02-09","payable_date":"2024-02-15",
                 "record_date":"2024-02-12","process_date":"2024-02-09","rate":0.24}
            ],
            "forward_splits": [
                {"id":"b1","symbol":"NVDA","ex_date":"2024-06-10","process_date":"2024-06-10",
                 "old_rate":1,"new_rate":10}
            ],
            "name_changes": [
                {"id":"c1","symbol":"META","old_symbol":"FB","new_symbol":"META",
                 "process_date":"2022-06-09"}
            ]
        },
        "next_page_token": null
    })");

    corporate_action_page p;
    from_json(j, p);

    ASSERT_EQ(p.actions.size(), 3u);
    EXPECT_TRUE(p.next_page_token.empty());

    const auto find = [&p](corporate_action_type type) {
        return std::find_if(p.actions.begin(), p.actions.end(),
                            [type](const corporate_action &a) { return a.type == type; });
    };

    const auto dividend = find(corporate_action_type::cash_dividend);
    ASSERT_NE(dividend, p.actions.end());
    EXPECT_EQ(dividend->symbol, "AAPL");
    ASSERT_TRUE(dividend->rate.has_value());
    EXPECT_DOUBLE_EQ(*dividend->rate, 0.24);
    EXPECT_EQ(dividend->ex_date_ns, to_nanoseconds("2024-02-09"));

    const auto split = find(corporate_action_type::forward_split);
    ASSERT_NE(split, p.actions.end());
    EXPECT_EQ(split->symbol, "NVDA");
    ASSERT_TRUE(split->new_rate.has_value());
    EXPECT_DOUBLE_EQ(*split->new_rate, 10.0);

    const auto rename = find(corporate_action_type::name_change);
    ASSERT_NE(rename, p.actions.end());
    EXPECT_EQ(rename->old_symbol, "FB");
    EXPECT_EQ(rename->new_symbol, "META");
}

TEST(CorporateActionModel, EveryGroupNameMapsToAKnownType) {
    // The flattening depends on stripping the plural "s"; a group that failed to map
    // would silently produce `unknown` entries.
    for (std::string_view group : {"reverse_splits", "forward_splits", "unit_splits",
                                   "cash_dividends", "stock_dividends", "spin_offs",
                                   "cash_mergers", "stock_mergers", "stock_and_cash_mergers",
                                   "redemptions", "name_changes", "worthless_removals",
                                   "rights_distributions", "partial_calls", "reorganizations"}) {
        EXPECT_NE(corporate_action_type_from_group(group), corporate_action_type::unknown)
            << "group " << group << " did not map to a known type";
    }
}

// ---------------------------------------------------------------------------
// Forex and fixed income
// ---------------------------------------------------------------------------

TEST(ForexModel, ParsesRate) {
    const json j = json::parse(R"({"t":"2022-01-03T00:01:00Z","bp":114.192,"mp":115.144,"ap":115.18})");

    forex_rate r;
    from_json(j, r);

    EXPECT_DOUBLE_EQ(r.bid_price, 114.192);
    EXPECT_DOUBLE_EQ(r.mid_price, 115.144);
    EXPECT_DOUBLE_EQ(r.ask_price, 115.18);
}

TEST(FixedIncomeModel, ParsesPriceAndQuote) {
    const json price_json = json::parse(R"({"p":99.81,"t":"2026-05-21T06:56:01Z",
                                            "ytm":5.236154,"ytw":5.236154})");
    fixed_income_price p;
    from_json(price_json, p);
    EXPECT_DOUBLE_EQ(p.price, 99.81);
    EXPECT_DOUBLE_EQ(p.yield_to_maturity, 5.236154);

    const json quote_json = json::parse(R"({"t":"2026-05-21T06:56:01.882466873Z",
        "bp":99.81091667,"bs":1000000,"bms":1000,"bytm":5.236154,"bytw":5.236154,
        "ap":99.91958333,"as":1000000,"ams":1000,"aytm":2.226923,"aytw":2.226923})");
    fixed_income_quote q;
    from_json(quote_json, q);
    EXPECT_DOUBLE_EQ(q.bid_price, 99.81091667);
    EXPECT_DOUBLE_EQ(q.ask_yield_to_maturity, 2.226923);
    EXPECT_NEAR(q.spread(), 0.10866666, 1e-8);
}

// ---------------------------------------------------------------------------
// Symbol-keyed response handling
// ---------------------------------------------------------------------------

TEST(SymbolMap, MergesPagesPerSymbolRatherThanOverwriting) {
    // Alpaca pages across symbols, so one symbol can appear on several pages. Overwriting
    // instead of appending would silently drop history.
    const json page_one = json::parse(R"({"bars":{
        "AAPL":[{"t":"2024-01-02T05:00:00Z","c":1},{"t":"2024-01-03T05:00:00Z","c":2}]}})");
    const json page_two = json::parse(R"({"bars":{
        "AAPL":[{"t":"2024-01-04T05:00:00Z","c":3}],
        "MSFT":[{"t":"2024-01-02T05:00:00Z","c":10}]}})");

    bars_by_symbol bars;
    detail::merge_symbol_page(page_one, "bars", bars);
    detail::merge_symbol_page(page_two, "bars", bars);

    ASSERT_EQ(bars.size(), 2u);
    ASSERT_EQ(bars["AAPL"].size(), 3u);
    ASSERT_EQ(bars["MSFT"].size(), 1u);
    // Order must be preserved across the page boundary.
    EXPECT_DOUBLE_EQ(bars["AAPL"][0].close, 1.0);
    EXPECT_DOUBLE_EQ(bars["AAPL"][2].close, 3.0);
}

TEST(SymbolMap, MissingOrNullSectionYieldsEmptyMap) {
    bars_by_symbol bars;
    detail::merge_symbol_page(json::parse(R"({"bars":null})"), "bars", bars);
    detail::merge_symbol_page(json::parse(R"({})"), "bars", bars);
    EXPECT_TRUE(bars.empty());
}

TEST(SymbolMap, LatestEndpointsExtractSingleValuesPerSymbol) {
    const json j = json::parse(R"({"quotes":{
        "AAPL":{"t":"2024-01-02T20:00:00Z","bp":185.0,"ap":185.1},
        "MSFT":{"t":"2024-01-02T20:00:00Z","bp":370.0,"ap":370.2}}})");

    const auto quotes = detail::to_symbol_map<quote>(j, "quotes");

    ASSERT_EQ(quotes.size(), 2u);
    EXPECT_DOUBLE_EQ(quotes.at("AAPL").bid_price, 185.0);
    EXPECT_DOUBLE_EQ(quotes.at("MSFT").ask_price, 370.2);
}

}   // namespace alpaca::tests
