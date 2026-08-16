// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for the streaming payload models. No credentials, no network — every
// case here is a captured frame shape from Alpaca's websocket documentation.

#include <gtest/gtest.h>

#include <alpaca/data/bar.hpp>
#include <alpaca/data/news.hpp>
#include <alpaca/data/orderbook.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/streaming/stream_message.hpp>

namespace alpaca::tests {

// ---------------------------------------------------------------------------
// Reuse of the REST models
//
// The whole point of not declaring stream_trade / stream_quote / stream_bar is that the
// stream sends the same single-letter keys as the REST API. If that ever stops being
// true these tests are what catches it.
// ---------------------------------------------------------------------------

TEST(StreamReuse, StockTradeFrameParsesAsRestTrade) {
    const json j = json::parse(R"({
        "T":"t","S":"AAPL","i":12345,"x":"V","p":178.26,"s":100,
        "c":["@","F"],"t":"2024-03-01T14:30:00.123456789Z","z":"C"
    })");

    trade t;
    from_json(j, t);

    EXPECT_EQ(t.id, 12345u);
    EXPECT_EQ(t.exchange, "V");
    EXPECT_DOUBLE_EQ(t.price, 178.26);
    EXPECT_DOUBLE_EQ(t.size, 100.);
    EXPECT_EQ(t.tape, "C");
    ASSERT_EQ(t.conditions.size(), 2u);
    EXPECT_EQ(t.conditions[0], "@");
    EXPECT_GT(t.timestamp, 0ull);
}

TEST(StreamReuse, StockQuoteFrameParsesAsRestQuote) {
    const json j = json::parse(R"({
        "T":"q","S":"AAPL","bx":"V","bp":178.24,"bs":3,"ax":"V","ap":178.28,"as":5,
        "c":["R"],"t":"2024-03-01T14:30:00Z","z":"C"
    })");

    quote q;
    from_json(j, q);

    EXPECT_DOUBLE_EQ(q.bid_price, 178.24);
    EXPECT_DOUBLE_EQ(q.ask_price, 178.28);
    EXPECT_DOUBLE_EQ(q.bid_size, 3.);
    EXPECT_NEAR(q.mid_price(), 178.26, 1e-9);
    EXPECT_NEAR(q.spread(), 0.04, 1e-9);
}

TEST(StreamReuse, BarFrameParsesAsRestBar) {
    const json j = json::parse(R"({
        "T":"b","S":"AAPL","o":178.1,"h":178.4,"l":178.0,"c":178.26,
        "v":12345,"vw":178.2,"n":42,"t":"2024-03-01T14:30:00Z"
    })");

    bar b;
    from_json(j, b);

    EXPECT_DOUBLE_EQ(b.open, 178.1);
    EXPECT_DOUBLE_EQ(b.close, 178.26);
    EXPECT_DOUBLE_EQ(b.vwap, 178.2);
    EXPECT_EQ(b.trade_count, 42u);
}

TEST(StreamReuse, CryptoTradeCarriesTakerSide) {
    const json j = json::parse(R"({
        "T":"t","S":"BTC/USD","p":62946.27,"s":0.01,"t":"2024-03-01T14:30:00Z",
        "i":99,"tks":"B"
    })");

    trade t;
    from_json(j, t);

    EXPECT_DOUBLE_EQ(t.price, 62946.27);
    EXPECT_EQ(t.taker_side, "B");
}

TEST(StreamReuse, NewsFrameParsesAsRestArticle) {
    const json j = json::parse(R"({
        "T":"n","id":24843171,"headline":"Apple beats","summary":"...",
        "author":"Benzinga","created_at":"2024-03-01T14:30:00Z",
        "updated_at":"2024-03-01T14:31:00Z","url":"https://example.com",
        "source":"benzinga","symbols":["AAPL"]
    })");

    news_article a;
    from_json(j, a);

    EXPECT_EQ(a.id, 24843171);
    EXPECT_EQ(a.headline, "Apple beats");
    EXPECT_EQ(a.source, "benzinga");
    ASSERT_EQ(a.symbols.size(), 1u);
    EXPECT_EQ(a.symbols[0], "AAPL");
    EXPECT_GT(a.created_at, 0ull);
}

TEST(StreamReuse, OrderbookFrameSetsResetFlag) {
    const json j = json::parse(R"({
        "T":"o","S":"BTC/USD","t":"2024-03-01T14:30:00Z",
        "b":[{"p":62946.27,"s":0.77},{"p":62933.10,"s":1.54}],
        "a":[{"p":63026.20,"s":0.78}],
        "r":true
    })");

    orderbook book;
    from_json(j, book);

    EXPECT_TRUE(book.reset);
    ASSERT_EQ(book.bids.size(), 2u);
    EXPECT_DOUBLE_EQ(book.best_bid(), 62946.27);
    EXPECT_DOUBLE_EQ(book.best_ask(), 63026.20);
    EXPECT_NEAR(book.spread(), 79.93, 1e-9);
}

TEST(StreamReuse, RestOrderbookLeavesResetFalse) {
    // The REST endpoint always answers a complete book and sends no `r`.
    const json j = json::parse(R"({"t":"2024-03-01T14:30:00Z","b":[],"a":[]})");

    orderbook book;
    from_json(j, book);

    EXPECT_FALSE(book.reset);
}

// ---------------------------------------------------------------------------
// Stream-only payloads
// ---------------------------------------------------------------------------

TEST(StreamMessages, TradingStatus) {
    const json j = json::parse(R"({
        "T":"s","S":"AAPL","sc":"H","sm":"Trading Halt","rc":"T12",
        "rm":"News Pending","t":"2024-03-01T14:30:00Z","z":"C"
    })");

    trading_status s;
    from_json(j, s);

    EXPECT_EQ(s.status_code, "H");
    EXPECT_EQ(s.status_message, "Trading Halt");
    EXPECT_EQ(s.reason_code, "T12");
    EXPECT_EQ(s.reason_message, "News Pending");
    EXPECT_EQ(s.tape, "C");
}

TEST(StreamMessages, Luld) {
    const json j = json::parse(R"({
        "T":"l","S":"AAPL","u":180.5,"d":176.0,"i":"B","t":"2024-03-01T14:30:00Z","z":"C"
    })");

    luld l;
    from_json(j, l);

    EXPECT_DOUBLE_EQ(l.limit_up_price, 180.5);
    EXPECT_DOUBLE_EQ(l.limit_down_price, 176.0);
    EXPECT_EQ(l.indicator, "B");
}

TEST(StreamMessages, TradeCorrectionKeepsBothPrints) {
    const json j = json::parse(R"({
        "T":"c","S":"AAPL","x":"V",
        "oi":1,"op":178.26,"os":100,"oc":["@"],
        "ci":2,"cp":178.30,"cs":150,"cc":["@","F"],
        "t":"2024-03-01T14:30:00Z","z":"C"
    })");

    trade_correction c;
    from_json(j, c);

    EXPECT_EQ(c.original_id, 1u);
    EXPECT_DOUBLE_EQ(c.original_price, 178.26);
    EXPECT_EQ(c.corrected_id, 2u);
    EXPECT_DOUBLE_EQ(c.corrected_price, 178.30);
    ASSERT_EQ(c.corrected_conditions.size(), 2u);
}

TEST(StreamMessages, TradeCancelError) {
    const json j = json::parse(R"({
        "T":"x","S":"AAPL","i":7,"x":"V","p":178.26,"s":100,"a":"C",
        "t":"2024-03-01T14:30:00Z","z":"C"
    })");

    trade_cancel_error c;
    from_json(j, c);

    EXPECT_EQ(c.id, 7u);
    EXPECT_EQ(c.action, "C");
    EXPECT_DOUBLE_EQ(c.price, 178.26);
}

TEST(StreamMessages, Imbalance) {
    const json j = json::parse(R"({"T":"i","S":"AAPL","p":178.5,"z":"C","t":"2024-03-01T14:30:00Z"})");

    imbalance i;
    from_json(j, i);

    EXPECT_DOUBLE_EQ(i.price, 178.5);
    EXPECT_EQ(i.tape, "C");
}

TEST(StreamMessages, StreamError) {
    const json j = json::parse(R"({"T":"error","code":401,"msg":"not authenticated"})");

    stream_error e;
    from_json(j, e);

    EXPECT_EQ(e.code, 401);
    EXPECT_EQ(e.message, "not authenticated");
}

TEST(StreamMessages, SubscriptionEchoUsesCamelCaseChannels) {
    const json j = json::parse(R"({
        "T":"subscription","trades":["AAPL"],"quotes":["AAPL","MSFT"],"bars":[],
        "updatedBars":["AAPL"],"dailyBars":[],"statuses":["*"],"lulds":[],
        "corrections":["AAPL"],"cancelErrors":["AAPL"],"orderbooks":[],"news":[]
    })");

    subscriptions s;
    from_json(j, s);

    EXPECT_FALSE(s.empty());
    ASSERT_EQ(s.trades.size(), 1u);
    EXPECT_EQ(s.trades[0], "AAPL");
    EXPECT_EQ(s.quotes.size(), 2u);
    ASSERT_EQ(s.updated_bars.size(), 1u);
    ASSERT_EQ(s.cancel_errors.size(), 1u);
    ASSERT_EQ(s.statuses.size(), 1u);
    EXPECT_EQ(s.statuses[0], "*");
    EXPECT_TRUE(s.bars.empty());
}

TEST(StreamMessages, EmptySubscriptionIsReportedEmpty) {
    const json j = json::parse(R"({"T":"subscription","trades":[],"quotes":[]})");

    subscriptions s;
    from_json(j, s);

    EXPECT_TRUE(s.empty());
}

// ---------------------------------------------------------------------------
// Trade updates
// ---------------------------------------------------------------------------

TEST(TradeUpdate, FillCarriesExecutionDetailAndFullOrder) {
    const json j = json::parse(R"({
        "event":"fill",
        "execution_id":"2f63ea93-423d-4169-b3f6-3fdafc10c418",
        "order":{"id":"abc","symbol":"AAPL","side":"buy","status":"filled",
                 "qty":"10","filled_qty":"10","order_type":"market","type":"market"},
        "position_qty":"10",
        "price":"105.89",
        "qty":"10",
        "timestamp":"2022-04-19T17:45:05.024916716Z"
    })");

    trade_update u;
    from_json(j, u);

    EXPECT_EQ(u.event, trade_update_event::fill);
    EXPECT_EQ(u.execution_id, "2f63ea93-423d-4169-b3f6-3fdafc10c418");
    EXPECT_EQ(u.order.symbol, "AAPL");
    ASSERT_TRUE(u.price.has_value());
    EXPECT_DOUBLE_EQ(*u.price, 105.89);
    ASSERT_TRUE(u.qty.has_value());
    EXPECT_DOUBLE_EQ(*u.qty, 10.);
    ASSERT_TRUE(u.position_qty.has_value());
    EXPECT_GT(u.timestamp, 0ull);
}

TEST(TradeUpdate, NewEventLeavesExecutionFieldsUnset) {
    // A `new` event has no price or qty. They must stay nullopt rather than defaulting
    // to 0, which would read as a genuine zero-price fill.
    const json j = json::parse(R"({
        "event":"new",
        "order":{"id":"abc","symbol":"AAPL","status":"new"},
        "timestamp":"2022-04-19T17:45:05Z"
    })");

    trade_update u;
    from_json(j, u);

    EXPECT_EQ(u.event, trade_update_event::new_order);
    EXPECT_FALSE(u.price.has_value());
    EXPECT_FALSE(u.qty.has_value());
    EXPECT_FALSE(u.position_qty.has_value());
}

TEST(TradeUpdate, LegacyAtTimestampIsAccepted) {
    const json j = json::parse(R"({
        "event":"canceled","order":{"id":"abc"},"at":"2022-04-19T17:45:05Z"
    })");

    trade_update u;
    from_json(j, u);

    EXPECT_EQ(u.event, trade_update_event::canceled);
    EXPECT_GT(u.timestamp, 0ull);
}

TEST(TradeUpdate, UnrecognisedEventBecomesUnknownRatherThanThrowing) {
    const json j = json::parse(R"({"event":"some_future_event","order":{"id":"abc"}})");

    trade_update u;
    EXPECT_NO_THROW(from_json(j, u));
    EXPECT_EQ(u.event, trade_update_event::unknown);
}

TEST(TradeUpdate, EventEnumRoundTripsThroughTheWireFormat) {
    // `new` is a keyword, so the enumerator is new_order while the wire value stays "new".
    EXPECT_EQ(to_string(trade_update_event::new_order), "new");
    EXPECT_EQ(to_trade_update_event("new"), trade_update_event::new_order);

    EXPECT_EQ(to_string(trade_update_event::partial_fill), "partial_fill");
    EXPECT_EQ(to_trade_update_event("partial_fill"), trade_update_event::partial_fill);

    EXPECT_EQ(to_trade_update_event("order_cancel_rejected"),
              trade_update_event::order_cancel_rejected);
    EXPECT_EQ(to_trade_update_event(""), trade_update_event::unknown);
}

}   // namespace alpaca::tests
