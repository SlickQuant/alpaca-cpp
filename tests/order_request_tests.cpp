// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline tests for order request serialisation. Alpaca requires every numeric field as a
// decimal string and rejects scientific notation, so these assert the exact wire payload.

#include <gtest/gtest.h>

#include <alpaca/trading/locate.hpp>
#include <alpaca/trading/order.hpp>
#include <alpaca/trading/position.hpp>
#include <alpaca/trading/watchlist.hpp>

namespace alpaca::tests {

TEST(OrderRequest, MarketOrder) {
    const auto j = order_request::market("AAPL", 10, order_side::buy).to_json();

    EXPECT_EQ(j["symbol"], "AAPL");
    EXPECT_EQ(j["side"], "buy");
    EXPECT_EQ(j["type"], "market");
    EXPECT_EQ(j["time_in_force"], "day");
    EXPECT_EQ(j["qty"], "10");
    EXPECT_FALSE(j.contains("limit_price"));
    EXPECT_FALSE(j.contains("notional"));
}

TEST(OrderRequest, NotionalMarketOrder) {
    const auto j = order_request::market_notional("AAPL", 500.25, order_side::buy).to_json();

    EXPECT_EQ(j["notional"], "500.25");
    EXPECT_FALSE(j.contains("qty"));
    EXPECT_EQ(j["time_in_force"], "day");
}

TEST(OrderRequest, LimitOrderWithGtc) {
    const auto j = order_request::limit("MSFT", 5, order_side::sell, 412.5,
                                        time_in_force::gtc).to_json();

    EXPECT_EQ(j["type"], "limit");
    EXPECT_EQ(j["side"], "sell");
    EXPECT_EQ(j["limit_price"], "412.5");
    EXPECT_EQ(j["time_in_force"], "gtc");
}

TEST(OrderRequest, StopAndStopLimit) {
    const auto stop = order_request::stop("AAPL", 1, order_side::sell, 90).to_json();
    EXPECT_EQ(stop["type"], "stop");
    EXPECT_EQ(stop["stop_price"], "90");
    EXPECT_FALSE(stop.contains("limit_price"));

    const auto stop_limit = order_request::stop_limit("AAPL", 1, order_side::sell, 90, 89.5).to_json();
    EXPECT_EQ(stop_limit["type"], "stop_limit");
    EXPECT_EQ(stop_limit["stop_price"], "90");
    EXPECT_EQ(stop_limit["limit_price"], "89.5");
}

TEST(OrderRequest, TrailingStopByPriceAndPercent) {
    const auto by_price = order_request::trailing_stop_price("AAPL", 1, order_side::sell, 2.5).to_json();
    EXPECT_EQ(by_price["type"], "trailing_stop");
    EXPECT_EQ(by_price["trail_price"], "2.5");
    EXPECT_FALSE(by_price.contains("trail_percent"));

    const auto by_pct = order_request::trailing_stop_percent("AAPL", 1, order_side::sell, 1.5).to_json();
    EXPECT_EQ(by_pct["trail_percent"], "1.5");
    EXPECT_FALSE(by_pct.contains("trail_price"));
}

TEST(OrderRequest, FractionalQuantitiesKeepTheirPrecision) {
    // Fractional share quantities must survive as decimals, not become 0 or 1e-09.
    const auto j = order_request::market("AAPL", 0.001, order_side::buy).to_json();
    EXPECT_EQ(j["qty"], "0.001");

    const auto tiny = order_request::market("BTC/USD", 0.000000001, order_side::buy).to_json();
    EXPECT_EQ(tiny["qty"], "0.000000001");
}

TEST(OrderRequest, BracketOrderCarriesBothChildLegs) {
    const auto j = order_request::bracket(
        order_request::limit("AAPL", 10, order_side::buy, 100, time_in_force::gtc),
        110.0,   // take profit
        90.0     // stop loss
    ).to_json();

    EXPECT_EQ(j["order_class"], "bracket");
    EXPECT_EQ(j["limit_price"], "100");
    ASSERT_TRUE(j.contains("take_profit"));
    EXPECT_EQ(j["take_profit"]["limit_price"], "110");
    ASSERT_TRUE(j.contains("stop_loss"));
    EXPECT_EQ(j["stop_loss"]["stop_price"], "90");
    EXPECT_FALSE(j["stop_loss"].contains("limit_price"));
}

TEST(OrderRequest, BracketWithStopLimitChild) {
    const auto j = order_request::bracket(
        order_request::limit("AAPL", 10, order_side::buy, 100), 110.0, 90.0, 89.0).to_json();

    EXPECT_EQ(j["stop_loss"]["stop_price"], "90");
    EXPECT_EQ(j["stop_loss"]["limit_price"], "89");
}

TEST(OrderRequest, MultiLegOmitsTopLevelSymbolAndSide) {
    // mleg orders carry symbol/side per leg; sending a top-level symbol makes Alpaca reject them.
    const auto j = order_request::multi_leg({
        {"AAPL240628C00200000", 1, order_side::buy, position_intent::buy_to_open},
        {"AAPL240628C00210000", 1, order_side::sell, position_intent::sell_to_open},
    }, 1).to_json();

    EXPECT_EQ(j["order_class"], "mleg");
    EXPECT_FALSE(j.contains("symbol"));
    EXPECT_FALSE(j.contains("side"));
    EXPECT_EQ(j["qty"], "1");

    ASSERT_TRUE(j.contains("legs"));
    ASSERT_EQ(j["legs"].size(), 2u);
    EXPECT_EQ(j["legs"][0]["symbol"], "AAPL240628C00200000");
    EXPECT_EQ(j["legs"][0]["ratio_qty"], "1");
    EXPECT_EQ(j["legs"][0]["side"], "buy");
    EXPECT_EQ(j["legs"][0]["position_intent"], "buy_to_open");
    EXPECT_EQ(j["legs"][1]["side"], "sell");
}

TEST(OrderRequest, OptionalFlagsOnlyAppearWhenSet) {
    order_request r = order_request::limit("AAPL", 1, order_side::buy, 100);
    EXPECT_FALSE(r.to_json().contains("extended_hours"));
    EXPECT_FALSE(r.to_json().contains("client_order_id"));
    EXPECT_FALSE(r.to_json().contains("position_intent"));

    r.extended_hours = true;
    r.client_order_id = "my-id-123";
    r.position_intent = position_intent::buy_to_open;

    const auto j = r.to_json();
    EXPECT_EQ(j["extended_hours"], true);
    EXPECT_EQ(j["client_order_id"], "my-id-123");
    EXPECT_EQ(j["position_intent"], "buy_to_open");
}

TEST(ReplaceOrderRequest, SerialisesOnlySetFields) {
    replace_order_request r;
    EXPECT_TRUE(r.to_json().empty());

    r.qty = 5;
    r.limit_price = 101.25;

    const auto j = r.to_json();
    EXPECT_EQ(j.size(), 2u);
    EXPECT_EQ(j["qty"], "5");
    EXPECT_EQ(j["limit_price"], "101.25");
    EXPECT_FALSE(j.contains("stop_price"));
}

TEST(OrderQuery, BuildsFilters) {
    order_query q;
    q.status = order_status_filter::open;
    q.limit = 100;
    q.direction = sort_direction::desc;
    q.nested = true;
    q.symbols = {"AAPL", "MSFT"};

    EXPECT_EQ(q.build().str(),
              "?status=open&limit=100&direction=desc&nested=true&symbols=AAPL%2CMSFT");
}

TEST(OrderQuery, DefaultQueryIsEmpty) {
    EXPECT_TRUE(order_query{}.build().empty());
}

TEST(ClosePositionRequest, BuildsQuantitySelector) {
    close_position_request r;
    EXPECT_TRUE(r.build().empty());

    r.qty = 5;
    EXPECT_EQ(r.build().str(), "?qty=5");

    close_position_request pct;
    pct.percentage = 50;
    EXPECT_EQ(pct.build().str(), "?percentage=50");
}

TEST(WatchlistRequest, Serialises) {
    const watchlist_request r{"My List", {"AAPL", "MSFT"}};
    const auto j = r.to_json();

    EXPECT_EQ(j["name"], "My List");
    ASSERT_EQ(j["symbols"].size(), 2u);
    EXPECT_EQ(j["symbols"][0], "AAPL");
}

TEST(LocateRequest, Serialises) {
    locate_request r;
    r.symbol = "GME";
    r.requested_qty = 100;
    r.limit_price = 0.25;

    const auto j = r.to_json();
    EXPECT_EQ(j["symbol"], "GME");
    EXPECT_EQ(j["requested_qty"], 100);
    EXPECT_EQ(j["limit_price"], "0.25");
    EXPECT_FALSE(j.contains("all_or_none"));
}

}   // namespace alpaca::tests
