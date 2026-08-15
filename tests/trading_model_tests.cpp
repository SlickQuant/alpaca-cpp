// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline model tests. Every fixture below is a real Alpaca response payload shape, so
// these catch field renames and type surprises without needing credentials or network.

#include <gtest/gtest.h>

#include <alpaca/trading/account.hpp>
#include <alpaca/trading/activity.hpp>
#include <alpaca/trading/asset.hpp>
#include <alpaca/trading/calendar.hpp>
#include <alpaca/trading/crypto_funding.hpp>
#include <alpaca/trading/locate.hpp>
#include <alpaca/trading/option_contract.hpp>
#include <alpaca/trading/order.hpp>
#include <alpaca/trading/position.hpp>
#include <alpaca/trading/watchlist.hpp>

namespace alpaca::tests {

// ---------------------------------------------------------------------------
// Account
// ---------------------------------------------------------------------------

TEST(AccountModel, ParsesNumericStringsIntoDoubles) {
    const json j = json::parse(R"({
        "id": "904837e3-3b76-47ec-b432-046db621571b",
        "account_number": "PA3ALX2VJUKW",
        "status": "ACTIVE",
        "crypto_status": "ACTIVE",
        "options_approved_level": "2",
        "options_trading_level": "2",
        "currency": "USD",
        "buying_power": "400000.32",
        "regt_buying_power": "200000.16",
        "daytrading_buying_power": "400000.32",
        "effective_buying_power": "400000.32",
        "non_marginable_buying_power": "100000.08",
        "options_buying_power": "200000.16",
        "bod_dtbp": "400000.32",
        "cash": "100000.08",
        "accrued_fees": "0",
        "pending_transfer_in": "0",
        "portfolio_value": "100000.08",
        "pattern_day_trader": false,
        "trading_blocked": false,
        "transfers_blocked": false,
        "account_blocked": false,
        "created_at": "2019-06-12T22:47:07.99658Z",
        "trade_suspended_by_user": false,
        "multiplier": "4",
        "shorting_enabled": true,
        "equity": "100000.08",
        "last_equity": "99872.11",
        "long_market_value": "0",
        "short_market_value": "0",
        "position_market_value": "0",
        "initial_margin": "0",
        "maintenance_margin": "0",
        "last_maintenance_margin": "0",
        "sma": "0",
        "daytrade_count": 3,
        "balance_asof": "2024-05-30",
        "crypto_tier": "1"
    })");

    account a;
    from_json(j, a);

    EXPECT_EQ(a.id, "904837e3-3b76-47ec-b432-046db621571b");
    EXPECT_EQ(a.account_number, "PA3ALX2VJUKW");
    EXPECT_EQ(a.status, account_status::active);
    EXPECT_EQ(a.crypto_status, account_status::active);
    EXPECT_EQ(a.options_approved_level, options_approved_level::level_2);
    EXPECT_EQ(a.currency, "USD");

    EXPECT_DOUBLE_EQ(a.buying_power, 400000.32);
    EXPECT_DOUBLE_EQ(a.cash, 100000.08);
    EXPECT_DOUBLE_EQ(a.equity, 100000.08);
    EXPECT_DOUBLE_EQ(a.last_equity, 99872.11);
    EXPECT_DOUBLE_EQ(a.multiplier, 4.0);

    EXPECT_EQ(a.daytrade_count, 3);
    EXPECT_FALSE(a.pattern_day_trader);
    EXPECT_TRUE(a.shorting_enabled);

    EXPECT_EQ(a.created_at, to_nanoseconds("2019-06-12T22:47:07.99658Z"));
    EXPECT_EQ(a.balance_asof, to_nanoseconds("2024-05-30"));
}

TEST(AccountModel, MissingFieldsKeepTheirDefaults) {
    // Alpaca omits fields depending on account type; parsing must not throw.
    const json j = json::parse(R"({"id":"abc","status":"ACTIVE"})");

    account a;
    ASSERT_NO_THROW(from_json(j, a));
    EXPECT_EQ(a.id, "abc");
    EXPECT_DOUBLE_EQ(a.buying_power, 0.0);
    EXPECT_EQ(a.created_at, 0ull);
}

TEST(AccountConfigurationsModel, Parses) {
    const json j = json::parse(R"({
        "dtbp_check": "both",
        "no_shorting": false,
        "trade_confirm_email": "all",
        "suspend_trade": false,
        "fractional_trading": true,
        "max_margin_multiplier": "4",
        "max_options_trading_level": "3",
        "pdt_check": "entry",
        "ptp_no_exception_entry": false
    })");

    account_configurations c;
    from_json(j, c);

    EXPECT_EQ(c.dtbp_check, dtbp_check::both);
    EXPECT_EQ(c.trade_confirm_email, trade_confirm_email::all);
    EXPECT_TRUE(c.fractional_trading);
    EXPECT_FALSE(c.no_shorting);
    EXPECT_EQ(c.max_margin_multiplier, "4");
    EXPECT_TRUE(c.pdt_check_enabled);
}

TEST(AccountConfigurationsUpdate, SerialisesOnlySetFields) {
    account_configurations_update update;
    update.no_shorting = true;

    const json j = update.to_json();
    EXPECT_EQ(j.size(), 1u);
    EXPECT_EQ(j["no_shorting"], true);
    EXPECT_FALSE(j.contains("dtbp_check"));

    update.dtbp_check = alpaca::dtbp_check::entry;
    EXPECT_EQ(update.to_json()["dtbp_check"], "entry");
}

TEST(PortfolioHistoryModel, ParsesParallelArraysAndEpochSeconds) {
    const json j = json::parse(R"({
        "timestamp": [1580826600, 1580827500, 1580828400],
        "equity": [27423.73, 27408.19, 27515.97],
        "profit_loss": [11.8, -3.74, 104.04],
        "profit_loss_pct": [0.000430, -0.000136, 0.003797],
        "base_value": 27411.93,
        "timeframe": "15Min"
    })");

    portfolio_history h;
    from_json(j, h);

    ASSERT_EQ(h.timestamp.size(), 3u);
    ASSERT_EQ(h.equity.size(), 3u);
    // This endpoint reports epoch seconds as numbers, unlike RFC-3339 everywhere else.
    EXPECT_EQ(h.timestamp[0], 1580826600ull * 1'000'000'000ull);
    EXPECT_DOUBLE_EQ(h.equity[2], 27515.97);
    EXPECT_DOUBLE_EQ(h.profit_loss[1], -3.74);
    EXPECT_DOUBLE_EQ(h.base_value, 27411.93);
    EXPECT_EQ(h.timeframe, "15Min");
}

// ---------------------------------------------------------------------------
// Order
// ---------------------------------------------------------------------------

TEST(OrderModel, ParsesAFilledLimitOrder) {
    const json j = json::parse(R"({
        "id": "61e69015-8549-4bfd-b9c3-01e75843f47d",
        "client_order_id": "eb9e2aaa-f71a-4f51-b5b4-52a6c565dad4",
        "created_at": "2021-03-16T18:38:01.942282Z",
        "updated_at": "2021-03-16T18:38:01.942282Z",
        "submitted_at": "2021-03-16T18:38:01.937734Z",
        "filled_at": "2021-03-16T18:38:02.123456Z",
        "expired_at": null,
        "canceled_at": null,
        "failed_at": null,
        "replaced_at": null,
        "replaced_by": null,
        "replaces": null,
        "asset_id": "b0b6dd9d-8b9b-48a9-ba46-b9d54906e415",
        "symbol": "AAPL",
        "asset_class": "us_equity",
        "notional": null,
        "qty": "1",
        "filled_qty": "1",
        "filled_avg_price": "121.5",
        "order_class": "simple",
        "order_type": "limit",
        "type": "limit",
        "side": "buy",
        "time_in_force": "day",
        "limit_price": "120.5",
        "stop_price": null,
        "status": "filled",
        "extended_hours": false,
        "legs": null,
        "trail_percent": null,
        "trail_price": null,
        "hwm": null
    })");

    order o;
    from_json(j, o);

    EXPECT_EQ(o.id, "61e69015-8549-4bfd-b9c3-01e75843f47d");
    EXPECT_EQ(o.symbol, "AAPL");
    EXPECT_EQ(o.side, order_side::buy);
    EXPECT_EQ(o.type, order_type::limit);
    EXPECT_EQ(o.order_class, order_class::simple);
    EXPECT_EQ(o.time_in_force, time_in_force::day);
    EXPECT_EQ(o.status, order_status::filled);
    EXPECT_EQ(o.asset_class, asset_class::us_equity);

    ASSERT_TRUE(o.qty.has_value());
    EXPECT_DOUBLE_EQ(*o.qty, 1.0);
    EXPECT_DOUBLE_EQ(o.filled_qty, 1.0);
    ASSERT_TRUE(o.filled_avg_price.has_value());
    EXPECT_DOUBLE_EQ(*o.filled_avg_price, 121.5);
    ASSERT_TRUE(o.limit_price.has_value());
    EXPECT_DOUBLE_EQ(*o.limit_price, 120.5);

    EXPECT_TRUE(o.is_filled());
    EXPECT_FALSE(o.is_open());
}

TEST(OrderModel, NullPricesStayUnsetRatherThanBecomingZero) {
    // The whole reason these fields are optional: a market order genuinely has no limit
    // price, and reporting 0 would be a different and dangerous statement.
    const json j = json::parse(R"({
        "id": "abc", "symbol": "AAPL", "type": "market", "side": "buy",
        "status": "new", "qty": "1", "filled_qty": "0",
        "limit_price": null, "stop_price": null, "filled_avg_price": null, "notional": null
    })");

    order o;
    from_json(j, o);

    EXPECT_FALSE(o.limit_price.has_value());
    EXPECT_FALSE(o.stop_price.has_value());
    EXPECT_FALSE(o.filled_avg_price.has_value());
    EXPECT_FALSE(o.notional.has_value());
    EXPECT_TRUE(o.is_open());
}

TEST(OrderModel, ParsesNestedBracketLegs) {
    const json j = json::parse(R"({
        "id": "parent-id",
        "symbol": "AAPL",
        "side": "buy",
        "type": "limit",
        "order_class": "bracket",
        "status": "new",
        "qty": "10",
        "filled_qty": "0",
        "limit_price": "100",
        "legs": [
            {"id": "tp-id", "symbol": "AAPL", "side": "sell", "type": "limit",
             "status": "held", "qty": "10", "filled_qty": "0", "limit_price": "110"},
            {"id": "sl-id", "symbol": "AAPL", "side": "sell", "type": "stop",
             "status": "held", "qty": "10", "filled_qty": "0", "stop_price": "90"}
        ]
    })");

    order o;
    from_json(j, o);

    EXPECT_EQ(o.order_class, order_class::bracket);
    ASSERT_EQ(o.legs.size(), 2u);
    EXPECT_EQ(o.legs[0].id, "tp-id");
    EXPECT_EQ(o.legs[0].side, order_side::sell);
    ASSERT_TRUE(o.legs[0].limit_price.has_value());
    EXPECT_DOUBLE_EQ(*o.legs[0].limit_price, 110.0);
    EXPECT_EQ(o.legs[1].type, order_type::stop);
    ASSERT_TRUE(o.legs[1].stop_price.has_value());
    EXPECT_DOUBLE_EQ(*o.legs[1].stop_price, 90.0);
}

TEST(OrderModel, OpenStatusClassification) {
    const auto with_status = [](const char *status) {
        json j = json::parse(R"({"id":"x","symbol":"AAPL"})");
        j["status"] = status;
        order o;
        from_json(j, o);
        return o;
    };

    EXPECT_TRUE(with_status("new").is_open());
    EXPECT_TRUE(with_status("partially_filled").is_open());
    EXPECT_TRUE(with_status("accepted").is_open());
    EXPECT_TRUE(with_status("pending_new").is_open());
    EXPECT_TRUE(with_status("held").is_open());

    EXPECT_FALSE(with_status("filled").is_open());
    EXPECT_FALSE(with_status("canceled").is_open());
    EXPECT_FALSE(with_status("expired").is_open());
    EXPECT_FALSE(with_status("rejected").is_open());
    EXPECT_FALSE(with_status("replaced").is_open());
    EXPECT_FALSE(with_status("done_for_day").is_open());
}

TEST(CancelOrderResult, ParsesPerOrderStatus) {
    const json j = json::parse(R"([
        {"id":"a","status":200},
        {"id":"b","status":422}
    ])");

    const auto results = j.get<std::vector<cancel_order_result>>();
    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].is_ok());
    EXPECT_FALSE(results[1].is_ok());
    EXPECT_EQ(results[1].status, 422u);
}

// ---------------------------------------------------------------------------
// Position
// ---------------------------------------------------------------------------

TEST(PositionModel, Parses) {
    const json j = json::parse(R"({
        "asset_id": "904837e3-3b76-47ec-b432-046db621571b",
        "symbol": "AAPL",
        "exchange": "NASDAQ",
        "asset_class": "us_equity",
        "asset_marginable": true,
        "avg_entry_price": "100.0",
        "qty": "5",
        "qty_available": "4",
        "side": "long",
        "market_value": "600.0",
        "cost_basis": "500.0",
        "unrealized_pl": "100.0",
        "unrealized_plpc": "0.20",
        "unrealized_intraday_pl": "10.0",
        "unrealized_intraday_plpc": "0.0084",
        "current_price": "120.0",
        "lastday_price": "119.0",
        "change_today": "0.0084"
    })");

    position p;
    from_json(j, p);

    EXPECT_EQ(p.symbol, "AAPL");
    EXPECT_EQ(p.exchange, asset_exchange::nasdaq);
    EXPECT_EQ(p.asset_class, asset_class::us_equity);
    EXPECT_EQ(p.side, position_side::long_);
    EXPECT_TRUE(p.asset_marginable);
    EXPECT_DOUBLE_EQ(p.qty, 5.0);
    EXPECT_DOUBLE_EQ(p.qty_available, 4.0);
    EXPECT_DOUBLE_EQ(p.avg_entry_price, 100.0);
    EXPECT_DOUBLE_EQ(p.unrealized_pl, 100.0);
    EXPECT_DOUBLE_EQ(p.current_price, 120.0);
}

TEST(PositionModel, ShortPositionHasNegativeQuantity) {
    const json j = json::parse(R"({"symbol":"AAPL","side":"short","qty":"-5","market_value":"-600"})");

    position p;
    from_json(j, p);

    EXPECT_EQ(p.side, position_side::short_);
    EXPECT_DOUBLE_EQ(p.qty, -5.0);
}

TEST(ClosePositionResult, UnwrapsTheNestedOrderUnderBody) {
    const json j = json::parse(R"([{
        "symbol": "AAPL",
        "status": 200,
        "body": {"id":"order-id","symbol":"AAPL","side":"sell","status":"accepted","qty":"5"}
    }])");

    const auto results = j.get<std::vector<close_position_result>>();
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].is_ok());
    EXPECT_EQ(results[0].order.id, "order-id");
    EXPECT_EQ(results[0].order.side, order_side::sell);
}

// ---------------------------------------------------------------------------
// Asset
// ---------------------------------------------------------------------------

TEST(AssetModel, ParsesClassFieldSpelledAsClass) {
    // The assets endpoint spells asset_class as "class".
    const json j = json::parse(R"({
        "id": "904837e3-3b76-47ec-b432-046db621571b",
        "class": "us_equity",
        "exchange": "NASDAQ",
        "symbol": "AAPL",
        "name": "Apple Inc. Common Stock",
        "status": "active",
        "tradable": true,
        "marginable": true,
        "shortable": true,
        "easy_to_borrow": true,
        "fractionable": true,
        "maintenance_margin_requirement": 30,
        "attributes": ["options_enabled", "fractional_eh_enabled"]
    })");

    asset a;
    from_json(j, a);

    EXPECT_EQ(a.symbol, "AAPL");
    EXPECT_EQ(a.asset_class, asset_class::us_equity);
    EXPECT_EQ(a.exchange, asset_exchange::nasdaq);
    EXPECT_EQ(a.status, asset_status::active);
    EXPECT_TRUE(a.tradable);
    EXPECT_TRUE(a.fractionable);
    EXPECT_DOUBLE_EQ(a.maintenance_margin_requirement, 30.0);
    ASSERT_EQ(a.attributes.size(), 2u);
    EXPECT_EQ(a.attributes[0], "options_enabled");
}

TEST(AssetModel, ParsesClassFieldSpelledAsAssetClass) {
    const json j = json::parse(R"({"symbol":"BTC/USD","asset_class":"crypto","exchange":"CRYPTO"})");

    asset a;
    from_json(j, a);

    EXPECT_EQ(a.asset_class, asset_class::crypto);
    EXPECT_EQ(a.exchange, asset_exchange::crypto);
}

TEST(AssetQuery, BuildsFilters) {
    asset_query q;
    q.status = asset_status::active;
    q.asset_class = alpaca::asset_class::us_equity;
    q.attributes = {"options_enabled"};

    EXPECT_EQ(q.build().str(), "?status=active&asset_class=us_equity&attributes=options_enabled");
}

// ---------------------------------------------------------------------------
// Calendar and clock
// ---------------------------------------------------------------------------

TEST(CalendarModel, KeepsRawStringsAndDerivesNanoseconds) {
    const json j = json::parse(R"({
        "date": "2024-01-03",
        "open": "09:30",
        "close": "16:00",
        "settlement_date": "2024-01-05"
    })");

    calendar_day c;
    from_json(j, c);

    EXPECT_EQ(c.date, "2024-01-03");
    EXPECT_EQ(c.open, "09:30");
    EXPECT_EQ(c.settlement_date, "2024-01-05");
    EXPECT_EQ(c.date_ns, to_nanoseconds("2024-01-03"));
    EXPECT_EQ(c.open_ns, (9 * 3600 + 30 * 60) * 1'000'000'000ull);
    EXPECT_EQ(c.close_ns, 16ull * 3600 * 1'000'000'000ull);
}

TEST(CalendarModel, HandlesHalfDayClose) {
    const json j = json::parse(R"({"date":"2024-11-29","open":"09:30","close":"13:00"})");

    calendar_day c;
    from_json(j, c);
    EXPECT_EQ(c.close_ns, 13ull * 3600 * 1'000'000'000ull);
}

TEST(ClockModel, Parses) {
    const json j = json::parse(R"({
        "timestamp": "2024-01-03T14:30:00.000000000-05:00",
        "is_open": true,
        "next_open": "2024-01-04T09:30:00-05:00",
        "next_close": "2024-01-03T16:00:00-05:00"
    })");

    market_clock c;
    from_json(j, c);

    EXPECT_TRUE(c.is_open);
    EXPECT_EQ(c.timestamp, to_nanoseconds("2024-01-03T19:30:00Z"));
    EXPECT_EQ(c.next_close, to_nanoseconds("2024-01-03T21:00:00Z"));
}

// ---------------------------------------------------------------------------
// Activities
// ---------------------------------------------------------------------------

TEST(ActivityModel, ParsesTradeActivity) {
    const json j = json::parse(R"({
        "activity_type": "FILL",
        "id": "20220224100000000::1234",
        "cum_qty": "1",
        "leaves_qty": "0",
        "price": "163.09",
        "qty": "1",
        "side": "buy",
        "symbol": "AAPL",
        "transaction_time": "2022-02-24T15:00:00.123456Z",
        "order_id": "order-uuid",
        "type": "fill",
        "order_status": "filled"
    })");

    account_activity a;
    from_json(j, a);

    EXPECT_TRUE(a.is_trade_activity());
    EXPECT_EQ(a.activity_type, activity_type::fill);
    EXPECT_EQ(a.symbol, "AAPL");
    EXPECT_EQ(a.side, order_side::buy);
    EXPECT_EQ(a.order_status, order_status::filled);
    EXPECT_DOUBLE_EQ(a.price, 163.09);
    EXPECT_DOUBLE_EQ(a.qty, 1.0);
    EXPECT_EQ(a.transaction_time, to_nanoseconds("2022-02-24T15:00:00.123456Z"));
}

TEST(ActivityModel, ParsesNonTradeActivity) {
    const json j = json::parse(R"({
        "activity_type": "DIV",
        "id": "20220224000000000::5678",
        "date": "2022-02-24",
        "net_amount": "1.02",
        "symbol": "AAPL",
        "qty": "2",
        "per_share_amount": "0.51",
        "description": "AAPL Cash Dividend",
        "status": "executed"
    })");

    account_activity a;
    from_json(j, a);

    EXPECT_FALSE(a.is_trade_activity());
    EXPECT_EQ(a.activity_type, activity_type::div);
    EXPECT_EQ(a.date, "2022-02-24");
    EXPECT_DOUBLE_EQ(a.net_amount, 1.02);
    EXPECT_DOUBLE_EQ(a.per_share_amount, 0.51);
    EXPECT_EQ(a.description, "AAPL Cash Dividend");
}

// ---------------------------------------------------------------------------
// Watchlist
// ---------------------------------------------------------------------------

TEST(WatchlistModel, ParsesWithNestedAssets) {
    const json j = json::parse(R"({
        "id": "fb306e55-16d3-4118-8c3d-c1615fcd4c03",
        "account_id": "1d5493c9-ea39-4377-aa94-340734c368ae",
        "created_at": "2019-10-30T07:54:42.981322Z",
        "updated_at": "2019-10-30T07:54:42.981322Z",
        "name": "Primary Watchlist",
        "assets": [
            {"id":"a1","class":"us_equity","exchange":"NASDAQ","symbol":"AAPL","status":"active","tradable":true}
        ]
    })");

    watchlist w;
    from_json(j, w);

    EXPECT_EQ(w.name, "Primary Watchlist");
    EXPECT_EQ(w.created_at, to_nanoseconds("2019-10-30T07:54:42.981322Z"));
    ASSERT_EQ(w.assets.size(), 1u);
    EXPECT_EQ(w.assets[0].symbol, "AAPL");
}

TEST(WatchlistModel, ListEndpointOmitsAssets) {
    const json j = json::parse(R"({"id":"x","name":"Primary Watchlist"})");

    watchlist w;
    ASSERT_NO_THROW(from_json(j, w));
    EXPECT_TRUE(w.assets.empty());
}

// ---------------------------------------------------------------------------
// Option contracts
// ---------------------------------------------------------------------------

TEST(OptionContractModel, Parses) {
    const json j = json::parse(R"({
        "id": "9bd6b0d0-6b1e-4b3f-a8f0-000000000000",
        "symbol": "AAPL240628C00200000",
        "name": "AAPL Jun 28 2024 200 Call",
        "status": "active",
        "tradable": true,
        "expiration_date": "2024-06-28",
        "root_symbol": "AAPL",
        "underlying_symbol": "AAPL",
        "underlying_asset_id": "b0b6dd9d-8b9b-48a9-ba46-b9d54906e415",
        "type": "call",
        "style": "american",
        "strike_price": "200",
        "multiplier": "100",
        "size": "100",
        "open_interest": "1234",
        "open_interest_date": "2024-06-20",
        "close_price": "5.25",
        "close_price_date": "2024-06-20",
        "deliverables": [
            {"type":"equity","symbol":"AAPL","asset_id":"b0b6dd9d","amount":"100",
             "allocation_percentage":"100","settlement_type":"T+1","settlement_method":"BTOB",
             "delayed_settlement":false}
        ]
    })");

    option_contract c;
    from_json(j, c);

    EXPECT_EQ(c.symbol, "AAPL240628C00200000");
    EXPECT_EQ(c.type, contract_type::call);
    EXPECT_EQ(c.style, contract_style::american);
    EXPECT_DOUBLE_EQ(c.strike_price, 200.0);
    EXPECT_DOUBLE_EQ(c.multiplier, 100.0);
    EXPECT_EQ(c.expiration_date, "2024-06-28");
    EXPECT_EQ(c.expiration_date_ns, to_nanoseconds("2024-06-28"));
    ASSERT_TRUE(c.open_interest.has_value());
    EXPECT_DOUBLE_EQ(*c.open_interest, 1234.0);
    ASSERT_EQ(c.deliverables.size(), 1u);
    EXPECT_EQ(c.deliverables[0].settlement_type, "T+1");
}

TEST(OptionContractModel, NullOpenInterestStaysUnset) {
    const json j = json::parse(R"({"symbol":"X","open_interest":null,"close_price":null})");

    option_contract c;
    from_json(j, c);

    EXPECT_FALSE(c.open_interest.has_value());
    EXPECT_FALSE(c.close_price.has_value());
}

TEST(OptionContractPage, ParsesEnvelopeAndToken) {
    const json j = json::parse(R"({
        "option_contracts": [{"symbol":"AAPL240628C00200000","type":"call"}],
        "next_page_token": "abc123"
    })");

    option_contract_page p;
    from_json(j, p);

    ASSERT_EQ(p.contracts.size(), 1u);
    EXPECT_EQ(p.next_page_token, "abc123");
}

TEST(OptionContractPage, NullTokenMeansLastPage) {
    const json j = json::parse(R"({"option_contracts":[],"next_page_token":null})");

    option_contract_page p;
    from_json(j, p);

    EXPECT_TRUE(p.next_page_token.empty());
}

// ---------------------------------------------------------------------------
// Crypto funding and locates
// ---------------------------------------------------------------------------

TEST(CryptoWalletModel, Parses) {
    const json j = json::parse(R"({
        "address": "0xabc123",
        "chain": "ETH",
        "created_at": "2024-01-02T03:04:05Z"
    })");

    crypto_wallet w;
    from_json(j, w);

    EXPECT_EQ(w.address, "0xabc123");
    EXPECT_EQ(w.chain, "ETH");
    EXPECT_EQ(w.created_at, to_nanoseconds("2024-01-02T03:04:05Z"));
}

TEST(LocateModel, ParsesNullableFields) {
    const json j = json::parse(R"({
        "id": "locate-uuid",
        "symbol": "GME",
        "requested_qty": 100,
        "located_qty": null,
        "all_or_none": true,
        "status": "active",
        "created_at": "2024-01-02T03:04:05Z",
        "expires_at": null,
        "limit_price": "0.25",
        "located_price": null,
        "total_fee": null,
        "rejection_reason": null
    })");

    locate l;
    from_json(j, l);

    EXPECT_EQ(l.symbol, "GME");
    EXPECT_EQ(l.requested_qty, 100);
    EXPECT_FALSE(l.located_qty.has_value());
    EXPECT_TRUE(l.all_or_none);
    ASSERT_TRUE(l.limit_price.has_value());
    EXPECT_DOUBLE_EQ(*l.limit_price, 0.25);
    EXPECT_FALSE(l.total_fee.has_value());
    EXPECT_EQ(l.expires_at, 0ull);
}

TEST(LocatePage, ParsesEnvelope) {
    const json j = json::parse(R"({
        "locates": [{"id":"a","symbol":"GME","requested_qty":10}],
        "next_page_token": "tok"
    })");

    locate_page p;
    from_json(j, p);

    ASSERT_EQ(p.locates.size(), 1u);
    EXPECT_EQ(p.next_page_token, "tok");
}

}   // namespace alpaca::tests
