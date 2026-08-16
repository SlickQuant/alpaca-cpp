// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/data_client_awaitable.hpp>
#include <alpaca/detail/response.hpp>
#include <alpaca/error.hpp>

#include <format>

namespace alpaca {

using detail::append_page;
using detail::merge_symbol_page;
using detail::to_model;
using detail::to_symbol_map;

namespace {

std::string page_token_of(const json &j) {
    if (j.is_object() && j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        return j["next_page_token"].get<std::string>();
    }
    return {};
}

std::string crypto_prefix(crypto_location loc) {
    return std::format("/v1beta3/crypto/{}", to_string(loc));
}

template <typename T>
T first_or_default(const std::unordered_map<std::string, T> &map, std::string_view symbol) {
    const auto it = map.find(std::string(symbol));
    return it != map.end() ? it->second : T{};
}

}   // namespace

data_client_awaitable::data_client_awaitable(credentials creds, environment env,
                                             uint32_t requests_per_minute)
    : ctx_(std::string(data_base_url(env)), credentials::resolve(std::move(creds), env),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(env)
{}

data_client_awaitable::data_client_awaitable(credentials creds, std::string base_url,
                                             uint32_t requests_per_minute)
    : ctx_(std::move(base_url), credentials::resolve(std::move(creds), environment::paper),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(environment::paper)
{}

// ---------------------------------------------------------------------------
// Stocks: historical
// ---------------------------------------------------------------------------

asio::awaitable<bars_by_symbol> data_client_awaitable::get_stock_bars(bar_query query) const {
    bars_by_symbol bars;
    co_await ctx_.async_paginate("/v2/stocks/bars", query.build(),
        [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    co_return bars;
}

asio::awaitable<trades_by_symbol> data_client_awaitable::get_stock_trades(history_query query) const {
    trades_by_symbol trades;
    co_await ctx_.async_paginate("/v2/stocks/trades", query.build(),
        [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    co_return trades;
}

asio::awaitable<quotes_by_symbol> data_client_awaitable::get_stock_quotes(history_query query) const {
    quotes_by_symbol quotes;
    co_await ctx_.async_paginate("/v2/stocks/quotes", query.build(),
        [&quotes](const json &page) { merge_symbol_page(page, "quotes", quotes); });
    co_return quotes;
}

asio::awaitable<auctions_by_symbol> data_client_awaitable::get_stock_auctions(
        history_query query) const {
    auctions_by_symbol auctions;
    co_await ctx_.async_paginate("/v2/stocks/auctions", query.build(),
        [&auctions](const json &page) { merge_symbol_page(page, "auctions", auctions); });
    co_return auctions;
}

asio::awaitable<data_client::bars_page> data_client_awaitable::get_stock_bars_page(
        bar_query query) const {
    const json j = co_await ctx_.async_get("/v2/stocks/bars", query.build());
    data_client::bars_page page;
    merge_symbol_page(j, "bars", page.bars);
    page.next_page_token = page_token_of(j);
    co_return page;
}

asio::awaitable<data_client::trades_page> data_client_awaitable::get_stock_trades_page(
        history_query query) const {
    const json j = co_await ctx_.async_get("/v2/stocks/trades", query.build());
    data_client::trades_page page;
    merge_symbol_page(j, "trades", page.trades);
    page.next_page_token = page_token_of(j);
    co_return page;
}

asio::awaitable<data_client::quotes_page> data_client_awaitable::get_stock_quotes_page(
        history_query query) const {
    const json j = co_await ctx_.async_get("/v2/stocks/quotes", query.build());
    data_client::quotes_page page;
    merge_symbol_page(j, "quotes", page.quotes);
    page.next_page_token = page_token_of(j);
    co_return page;
}

asio::awaitable<data_client::auctions_page> data_client_awaitable::get_stock_auctions_page(
        history_query query) const {
    const json j = co_await ctx_.async_get("/v2/stocks/auctions", query.build());
    data_client::auctions_page page;
    merge_symbol_page(j, "auctions", page.auctions);
    page.next_page_token = page_token_of(j);
    co_return page;
}

// ---------------------------------------------------------------------------
// Stocks: latest
// ---------------------------------------------------------------------------

asio::awaitable<bar_by_symbol> data_client_awaitable::get_latest_stock_bars(
        latest_query query) const {
    co_return to_symbol_map<bar>(co_await ctx_.async_get("/v2/stocks/bars/latest", query.build()),
                                 "bars");
}

asio::awaitable<trade_by_symbol> data_client_awaitable::get_latest_stock_trades(
        latest_query query) const {
    co_return to_symbol_map<trade>(co_await ctx_.async_get("/v2/stocks/trades/latest", query.build()),
                                   "trades");
}

asio::awaitable<quote_by_symbol> data_client_awaitable::get_latest_stock_quotes(
        latest_query query) const {
    co_return to_symbol_map<quote>(co_await ctx_.async_get("/v2/stocks/quotes/latest", query.build()),
                                   "quotes");
}

asio::awaitable<snapshots_by_symbol> data_client_awaitable::get_stock_snapshots(
        latest_query query) const {
    const json j = co_await ctx_.async_get("/v2/stocks/snapshots", query.build());
    co_return j.is_object() ? j.get<snapshots_by_symbol>() : snapshots_by_symbol{};
}

asio::awaitable<bar> data_client_awaitable::get_latest_stock_bar(
        std::string symbol, std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {symbol};
    query.feed = feed;
    co_return first_or_default(co_await get_latest_stock_bars(std::move(query)), symbol);
}

asio::awaitable<trade> data_client_awaitable::get_latest_stock_trade(
        std::string symbol, std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {symbol};
    query.feed = feed;
    co_return first_or_default(co_await get_latest_stock_trades(std::move(query)), symbol);
}

asio::awaitable<quote> data_client_awaitable::get_latest_stock_quote(
        std::string symbol, std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {symbol};
    query.feed = feed;
    co_return first_or_default(co_await get_latest_stock_quotes(std::move(query)), symbol);
}

asio::awaitable<snapshot> data_client_awaitable::get_stock_snapshot(
        std::string symbol, std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {symbol};
    query.feed = feed;
    co_return first_or_default(co_await get_stock_snapshots(std::move(query)), symbol);
}

// ---------------------------------------------------------------------------
// Stocks: reference
// ---------------------------------------------------------------------------

asio::awaitable<code_map> data_client_awaitable::get_stock_condition_codes(
        std::string tick_type, std::string tape) const {
    query_builder q;
    q.add("tape", tape);
    const json j = co_await ctx_.async_get(std::format("/v2/stocks/meta/conditions/{}", tick_type), q);
    co_return j.is_object() ? j.get<code_map>() : code_map{};
}

asio::awaitable<code_map> data_client_awaitable::get_stock_exchange_codes() const {
    const json j = co_await ctx_.async_get("/v2/stocks/meta/exchanges");
    co_return j.is_object() ? j.get<code_map>() : code_map{};
}

// ---------------------------------------------------------------------------
// Crypto
// ---------------------------------------------------------------------------

asio::awaitable<bars_by_symbol> data_client_awaitable::get_crypto_bars(
        crypto_bar_query query, crypto_location loc) const {
    bars_by_symbol bars;
    co_await ctx_.async_paginate(crypto_prefix(loc) + "/bars", query.build(),
        [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    co_return bars;
}

asio::awaitable<trades_by_symbol> data_client_awaitable::get_crypto_trades(
        crypto_history_query query, crypto_location loc) const {
    trades_by_symbol trades;
    co_await ctx_.async_paginate(crypto_prefix(loc) + "/trades", query.build(),
        [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    co_return trades;
}

asio::awaitable<quotes_by_symbol> data_client_awaitable::get_crypto_quotes(
        crypto_history_query query, crypto_location loc) const {
    quotes_by_symbol quotes;
    co_await ctx_.async_paginate(crypto_prefix(loc) + "/quotes", query.build(),
        [&quotes](const json &page) { merge_symbol_page(page, "quotes", quotes); });
    co_return quotes;
}

asio::awaitable<bar_by_symbol> data_client_awaitable::get_latest_crypto_bars(
        std::vector<std::string> symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    co_return to_symbol_map<bar>(co_await ctx_.async_get(crypto_prefix(loc) + "/latest/bars", q),
                                 "bars");
}

asio::awaitable<trade_by_symbol> data_client_awaitable::get_latest_crypto_trades(
        std::vector<std::string> symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    co_return to_symbol_map<trade>(co_await ctx_.async_get(crypto_prefix(loc) + "/latest/trades", q),
                                   "trades");
}

asio::awaitable<quote_by_symbol> data_client_awaitable::get_latest_crypto_quotes(
        std::vector<std::string> symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    co_return to_symbol_map<quote>(co_await ctx_.async_get(crypto_prefix(loc) + "/latest/quotes", q),
                                   "quotes");
}

asio::awaitable<orderbooks_by_symbol> data_client_awaitable::get_latest_crypto_orderbooks(
        std::vector<std::string> symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    co_return to_symbol_map<orderbook>(
        co_await ctx_.async_get(crypto_prefix(loc) + "/latest/orderbooks", q), "orderbooks");
}

asio::awaitable<snapshots_by_symbol> data_client_awaitable::get_crypto_snapshots(
        std::vector<std::string> symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    co_return to_symbol_map<snapshot>(co_await ctx_.async_get(crypto_prefix(loc) + "/snapshots", q),
                                      "snapshots");
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

asio::awaitable<bars_by_symbol> data_client_awaitable::get_option_bars(
        option_bar_query query) const {
    bars_by_symbol bars;
    co_await ctx_.async_paginate("/v1beta1/options/bars", query.build(),
        [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    co_return bars;
}

asio::awaitable<trades_by_symbol> data_client_awaitable::get_option_trades(
        option_history_query query) const {
    trades_by_symbol trades;
    co_await ctx_.async_paginate("/v1beta1/options/trades", query.build(),
        [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    co_return trades;
}

asio::awaitable<trade_by_symbol> data_client_awaitable::get_latest_option_trades(
        option_latest_query query) const {
    co_return to_symbol_map<trade>(
        co_await ctx_.async_get("/v1beta1/options/trades/latest", query.build()), "trades");
}

asio::awaitable<quote_by_symbol> data_client_awaitable::get_latest_option_quotes(
        option_latest_query query) const {
    co_return to_symbol_map<quote>(
        co_await ctx_.async_get("/v1beta1/options/quotes/latest", query.build()), "quotes");
}

asio::awaitable<option_snapshots_by_symbol> data_client_awaitable::get_option_snapshots(
        option_latest_query query) const {
    co_return to_symbol_map<option_snapshot>(
        co_await ctx_.async_get("/v1beta1/options/snapshots", query.build()), "snapshots");
}

asio::awaitable<option_snapshots_by_symbol> data_client_awaitable::get_option_chain(
        std::string underlying_symbol, option_chain_query query) const {
    option_snapshots_by_symbol snapshots;
    co_await ctx_.async_paginate(
        std::format("/v1beta1/options/snapshots/{}", underlying_symbol), query.build(),
        [&snapshots](const json &page) {
            if (page.is_object() && page.contains("snapshots") && page["snapshots"].is_object()) {
                for (const auto &[symbol, entry] : page["snapshots"].items()) {
                    option_snapshot s;
                    from_json(entry, s);
                    snapshots.emplace(symbol, std::move(s));
                }
            }
        });
    co_return snapshots;
}

asio::awaitable<option_chain_page> data_client_awaitable::get_option_chain_page(
        std::string underlying_symbol, option_chain_query query) const {
    co_return to_model<option_chain_page>(co_await ctx_.async_get(
        std::format("/v1beta1/options/snapshots/{}", underlying_symbol), query.build()));
}

asio::awaitable<code_map> data_client_awaitable::get_option_condition_codes(
        std::string tick_type) const {
    const json j = co_await ctx_.async_get(
        std::format("/v1beta1/options/meta/conditions/{}", tick_type));
    co_return j.is_object() ? j.get<code_map>() : code_map{};
}

asio::awaitable<code_map> data_client_awaitable::get_option_exchange_codes() const {
    const json j = co_await ctx_.async_get("/v1beta1/options/meta/exchanges");
    co_return j.is_object() ? j.get<code_map>() : code_map{};
}

// ---------------------------------------------------------------------------
// News
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<news_article>> data_client_awaitable::get_news(news_query query) const {
    std::vector<news_article> articles;
    co_await ctx_.async_paginate("/v1beta1/news", query.build(),
        [&articles](const json &page) { append_page(page, "news", articles); });
    co_return articles;
}

asio::awaitable<news_page> data_client_awaitable::get_news_page(news_query query) const {
    co_return to_model<news_page>(co_await ctx_.async_get("/v1beta1/news", query.build()));
}

// ---------------------------------------------------------------------------
// Screener
// ---------------------------------------------------------------------------

asio::awaitable<most_actives> data_client_awaitable::get_most_actives(
        std::optional<std::string> by, std::optional<uint32_t> top) const {
    query_builder q;
    q.add("by", by);
    q.add("top", top);
    co_return to_model<most_actives>(
        co_await ctx_.async_get("/v1beta1/screener/stocks/most-actives", q));
}

asio::awaitable<movers> data_client_awaitable::get_movers(std::string market_type,
                                                          std::optional<uint32_t> top) const {
    query_builder q;
    q.add("top", top);
    co_return to_model<movers>(
        co_await ctx_.async_get(std::format("/v1beta1/screener/{}/movers", market_type), q));
}

// ---------------------------------------------------------------------------
// Corporate actions
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<corporate_action>> data_client_awaitable::get_corporate_actions(
        corporate_action_query query) const {
    std::vector<corporate_action> actions;
    co_await ctx_.async_paginate("/v1/corporate-actions", query.build(),
        [&actions](const json &page) { append_corporate_actions(page, actions); });
    co_return actions;
}

asio::awaitable<corporate_action_page> data_client_awaitable::get_corporate_actions_page(
        corporate_action_query query) const {
    co_return to_model<corporate_action_page>(
        co_await ctx_.async_get("/v1/corporate-actions", query.build()));
}

// ---------------------------------------------------------------------------
// Forex
// ---------------------------------------------------------------------------

asio::awaitable<forex_rates_by_pair> data_client_awaitable::get_forex_rates(
        forex_query query) const {
    forex_rates_by_pair rates;
    co_await ctx_.async_paginate("/v1beta1/forex/rates", query.build(),
        [&rates](const json &page) { merge_symbol_page(page, "rates", rates); });
    co_return rates;
}

asio::awaitable<forex_rate_by_pair> data_client_awaitable::get_latest_forex_rates(
        std::vector<std::string> currency_pairs) const {
    query_builder q;
    q.add("currency_pairs", currency_pairs);
    co_return to_symbol_map<forex_rate>(
        co_await ctx_.async_get("/v1beta1/forex/latest/rates", q), "rates");
}

// ---------------------------------------------------------------------------
// Fixed income
// ---------------------------------------------------------------------------

asio::awaitable<fixed_income_prices_by_isin> data_client_awaitable::get_latest_fixed_income_prices(
        std::vector<std::string> isins) const {
    query_builder q;
    q.add("isins", isins);
    co_return to_symbol_map<fixed_income_price>(
        co_await ctx_.async_get("/v1beta1/fixed_income/latest/prices", q), "prices");
}

asio::awaitable<fixed_income_quotes_by_isin> data_client_awaitable::get_latest_fixed_income_quotes(
        std::vector<std::string> isins, std::optional<double> trade_size) const {
    query_builder q;
    q.add("isins", isins);
    q.add("trade_size", trade_size);
    co_return to_symbol_map<fixed_income_quote>(
        co_await ctx_.async_get("/v1beta1/fixed_income/latest/quotes", q), "quotes");
}

// ---------------------------------------------------------------------------
// Logos
// ---------------------------------------------------------------------------

asio::awaitable<std::string> data_client_awaitable::get_logo(
        std::string symbol, std::optional<bool> placeholder) const {
    query_builder q;
    q.add("placeholder", placeholder);
    co_return co_await ctx_.async_get_raw(std::format("/v1beta1/logos/{}", symbol), q);
}

}   // namespace alpaca
