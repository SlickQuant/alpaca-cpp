// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/data_client.hpp>
#include <alpaca/detail/response.hpp>
#include <alpaca/error.hpp>

#include <format>

namespace alpaca {

using detail::append_page;
using detail::merge_symbol_page;
using detail::to_model;
using detail::to_symbol_map;

namespace {

/// Reads `next_page_token` out of a response, empty when the page is the last one.
std::string page_token_of(const json &j) {
    if (j.is_object() && j.contains("next_page_token") && !j["next_page_token"].is_null()) {
        return j["next_page_token"].get<std::string>();
    }
    return {};
}

/// Path prefix for a crypto venue, e.g. "/v1beta3/crypto/us".
std::string crypto_prefix(crypto_location loc) {
    return std::format("/v1beta3/crypto/{}", to_string(loc));
}

/// Single-element symbol lookup shared by the `get_latest_*` convenience wrappers.
/// Returns a default-constructed value when the symbol is absent, which happens for a
/// symbol with no data in the requested feed rather than as an error.
template <typename T>
T first_or_default(const std::unordered_map<std::string, T> &map, std::string_view symbol) {
    const auto it = map.find(std::string(symbol));
    return it != map.end() ? it->second : T{};
}

}   // namespace

data_client::data_client(credentials creds, environment env, uint32_t requests_per_minute)
    : ctx_(std::string(data_base_url(env)), credentials::resolve(std::move(creds), env),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(env)
{}

data_client::data_client(credentials creds, std::string base_url, uint32_t requests_per_minute)
    : ctx_(std::move(base_url), credentials::resolve(std::move(creds), environment::paper),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(environment::paper)
{}

// ---------------------------------------------------------------------------
// Stocks: historical
// ---------------------------------------------------------------------------

bars_by_symbol data_client::get_stock_bars(const bar_query &query) const {
    bars_by_symbol bars;
    ctx_.paginate("/v2/stocks/bars", query.build(),
                  [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    return bars;
}

trades_by_symbol data_client::get_stock_trades(const history_query &query) const {
    trades_by_symbol trades;
    ctx_.paginate("/v2/stocks/trades", query.build(),
                  [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    return trades;
}

quotes_by_symbol data_client::get_stock_quotes(const history_query &query) const {
    quotes_by_symbol quotes;
    ctx_.paginate("/v2/stocks/quotes", query.build(),
                  [&quotes](const json &page) { merge_symbol_page(page, "quotes", quotes); });
    return quotes;
}

auctions_by_symbol data_client::get_stock_auctions(const history_query &query) const {
    auctions_by_symbol auctions;
    ctx_.paginate("/v2/stocks/auctions", query.build(),
                  [&auctions](const json &page) { merge_symbol_page(page, "auctions", auctions); });
    return auctions;
}

data_client::bars_page data_client::get_stock_bars_page(const bar_query &query) const {
    const json j = ctx_.get("/v2/stocks/bars", query.build());
    bars_page page;
    merge_symbol_page(j, "bars", page.bars);
    page.next_page_token = page_token_of(j);
    return page;
}

data_client::trades_page data_client::get_stock_trades_page(const history_query &query) const {
    const json j = ctx_.get("/v2/stocks/trades", query.build());
    trades_page page;
    merge_symbol_page(j, "trades", page.trades);
    page.next_page_token = page_token_of(j);
    return page;
}

data_client::quotes_page data_client::get_stock_quotes_page(const history_query &query) const {
    const json j = ctx_.get("/v2/stocks/quotes", query.build());
    quotes_page page;
    merge_symbol_page(j, "quotes", page.quotes);
    page.next_page_token = page_token_of(j);
    return page;
}

data_client::auctions_page data_client::get_stock_auctions_page(const history_query &query) const {
    const json j = ctx_.get("/v2/stocks/auctions", query.build());
    auctions_page page;
    merge_symbol_page(j, "auctions", page.auctions);
    page.next_page_token = page_token_of(j);
    return page;
}

// ---------------------------------------------------------------------------
// Stocks: latest
// ---------------------------------------------------------------------------

bar_by_symbol data_client::get_latest_stock_bars(const latest_query &query) const {
    return to_symbol_map<bar>(ctx_.get("/v2/stocks/bars/latest", query.build()), "bars");
}

trade_by_symbol data_client::get_latest_stock_trades(const latest_query &query) const {
    return to_symbol_map<trade>(ctx_.get("/v2/stocks/trades/latest", query.build()), "trades");
}

quote_by_symbol data_client::get_latest_stock_quotes(const latest_query &query) const {
    return to_symbol_map<quote>(ctx_.get("/v2/stocks/quotes/latest", query.build()), "quotes");
}

snapshots_by_symbol data_client::get_stock_snapshots(const latest_query &query) const {
    // This endpoint answers a bare symbol-keyed object with no wrapper key.
    const json j = ctx_.get("/v2/stocks/snapshots", query.build());
    return j.is_object() ? j.get<snapshots_by_symbol>() : snapshots_by_symbol{};
}

bar data_client::get_latest_stock_bar(std::string_view symbol,
                                      std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {std::string(symbol)};
    query.feed = feed;
    return first_or_default(get_latest_stock_bars(query), symbol);
}

trade data_client::get_latest_stock_trade(std::string_view symbol,
                                          std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {std::string(symbol)};
    query.feed = feed;
    return first_or_default(get_latest_stock_trades(query), symbol);
}

quote data_client::get_latest_stock_quote(std::string_view symbol,
                                          std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {std::string(symbol)};
    query.feed = feed;
    return first_or_default(get_latest_stock_quotes(query), symbol);
}

snapshot data_client::get_stock_snapshot(std::string_view symbol,
                                         std::optional<alpaca::data_feed> feed) const {
    latest_query query;
    query.symbols = {std::string(symbol)};
    query.feed = feed;
    return first_or_default(get_stock_snapshots(query), symbol);
}

// ---------------------------------------------------------------------------
// Stocks: reference
// ---------------------------------------------------------------------------

code_map data_client::get_stock_condition_codes(std::string_view tick_type,
                                                std::string_view tape) const {
    query_builder q;
    q.add("tape", tape);
    const json j = ctx_.get(std::format("/v2/stocks/meta/conditions/{}", tick_type), q);
    return j.is_object() ? j.get<code_map>() : code_map{};
}

code_map data_client::get_stock_exchange_codes() const {
    const json j = ctx_.get("/v2/stocks/meta/exchanges");
    return j.is_object() ? j.get<code_map>() : code_map{};
}

// ---------------------------------------------------------------------------
// Crypto
// ---------------------------------------------------------------------------

bars_by_symbol data_client::get_crypto_bars(const crypto_bar_query &query,
                                            crypto_location loc) const {
    bars_by_symbol bars;
    ctx_.paginate(crypto_prefix(loc) + "/bars", query.build(),
                  [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    return bars;
}

trades_by_symbol data_client::get_crypto_trades(const crypto_history_query &query,
                                                crypto_location loc) const {
    trades_by_symbol trades;
    ctx_.paginate(crypto_prefix(loc) + "/trades", query.build(),
                  [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    return trades;
}

quotes_by_symbol data_client::get_crypto_quotes(const crypto_history_query &query,
                                                crypto_location loc) const {
    quotes_by_symbol quotes;
    ctx_.paginate(crypto_prefix(loc) + "/quotes", query.build(),
                  [&quotes](const json &page) { merge_symbol_page(page, "quotes", quotes); });
    return quotes;
}

bar_by_symbol data_client::get_latest_crypto_bars(const std::vector<std::string> &symbols,
                                                  crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    return to_symbol_map<bar>(ctx_.get(crypto_prefix(loc) + "/latest/bars", q), "bars");
}

trade_by_symbol data_client::get_latest_crypto_trades(const std::vector<std::string> &symbols,
                                                      crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    return to_symbol_map<trade>(ctx_.get(crypto_prefix(loc) + "/latest/trades", q), "trades");
}

quote_by_symbol data_client::get_latest_crypto_quotes(const std::vector<std::string> &symbols,
                                                      crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    return to_symbol_map<quote>(ctx_.get(crypto_prefix(loc) + "/latest/quotes", q), "quotes");
}

orderbooks_by_symbol data_client::get_latest_crypto_orderbooks(
        const std::vector<std::string> &symbols, crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    return to_symbol_map<orderbook>(ctx_.get(crypto_prefix(loc) + "/latest/orderbooks", q),
                                    "orderbooks");
}

snapshots_by_symbol data_client::get_crypto_snapshots(const std::vector<std::string> &symbols,
                                                      crypto_location loc) const {
    query_builder q;
    q.add("symbols", symbols);
    return to_symbol_map<snapshot>(ctx_.get(crypto_prefix(loc) + "/snapshots", q), "snapshots");
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

bars_by_symbol data_client::get_option_bars(const option_bar_query &query) const {
    bars_by_symbol bars;
    ctx_.paginate("/v1beta1/options/bars", query.build(),
                  [&bars](const json &page) { merge_symbol_page(page, "bars", bars); });
    return bars;
}

trades_by_symbol data_client::get_option_trades(const option_history_query &query) const {
    trades_by_symbol trades;
    ctx_.paginate("/v1beta1/options/trades", query.build(),
                  [&trades](const json &page) { merge_symbol_page(page, "trades", trades); });
    return trades;
}

trade_by_symbol data_client::get_latest_option_trades(const option_latest_query &query) const {
    return to_symbol_map<trade>(ctx_.get("/v1beta1/options/trades/latest", query.build()), "trades");
}

quote_by_symbol data_client::get_latest_option_quotes(const option_latest_query &query) const {
    return to_symbol_map<quote>(ctx_.get("/v1beta1/options/quotes/latest", query.build()), "quotes");
}

option_snapshots_by_symbol data_client::get_option_snapshots(const option_latest_query &query) const {
    return to_symbol_map<option_snapshot>(ctx_.get("/v1beta1/options/snapshots", query.build()),
                                          "snapshots");
}

option_snapshots_by_symbol data_client::get_option_chain(std::string_view underlying_symbol,
                                                         const option_chain_query &query) const {
    option_snapshots_by_symbol snapshots;
    ctx_.paginate(std::format("/v1beta1/options/snapshots/{}", underlying_symbol), query.build(),
                  [&snapshots](const json &page) {
                      if (page.is_object() && page.contains("snapshots") &&
                          page["snapshots"].is_object()) {
                          for (const auto &[symbol, entry] : page["snapshots"].items()) {
                              option_snapshot s;
                              from_json(entry, s);
                              snapshots.emplace(symbol, std::move(s));
                          }
                      }
                  });
    return snapshots;
}

option_chain_page data_client::get_option_chain_page(std::string_view underlying_symbol,
                                                     const option_chain_query &query) const {
    return to_model<option_chain_page>(
        ctx_.get(std::format("/v1beta1/options/snapshots/{}", underlying_symbol), query.build()));
}

code_map data_client::get_option_condition_codes(std::string_view tick_type) const {
    const json j = ctx_.get(std::format("/v1beta1/options/meta/conditions/{}", tick_type));
    return j.is_object() ? j.get<code_map>() : code_map{};
}

code_map data_client::get_option_exchange_codes() const {
    const json j = ctx_.get("/v1beta1/options/meta/exchanges");
    return j.is_object() ? j.get<code_map>() : code_map{};
}

// ---------------------------------------------------------------------------
// News
// ---------------------------------------------------------------------------

std::vector<news_article> data_client::get_news(const news_query &query) const {
    std::vector<news_article> articles;
    ctx_.paginate("/v1beta1/news", query.build(),
                  [&articles](const json &page) { append_page(page, "news", articles); });
    return articles;
}

news_page data_client::get_news_page(const news_query &query) const {
    return to_model<news_page>(ctx_.get("/v1beta1/news", query.build()));
}

// ---------------------------------------------------------------------------
// Screener
// ---------------------------------------------------------------------------

most_actives data_client::get_most_actives(std::optional<std::string> by,
                                           std::optional<uint32_t> top) const {
    query_builder q;
    q.add("by", by);
    q.add("top", top);
    return to_model<most_actives>(ctx_.get("/v1beta1/screener/stocks/most-actives", q));
}

movers data_client::get_movers(std::string_view market_type, std::optional<uint32_t> top) const {
    query_builder q;
    q.add("top", top);
    return to_model<movers>(ctx_.get(std::format("/v1beta1/screener/{}/movers", market_type), q));
}

// ---------------------------------------------------------------------------
// Corporate actions
// ---------------------------------------------------------------------------

std::vector<corporate_action> data_client::get_corporate_actions(
        const corporate_action_query &query) const {
    std::vector<corporate_action> actions;
    ctx_.paginate("/v1/corporate-actions", query.build(),
                  [&actions](const json &page) { append_corporate_actions(page, actions); });
    return actions;
}

corporate_action_page data_client::get_corporate_actions_page(
        const corporate_action_query &query) const {
    return to_model<corporate_action_page>(ctx_.get("/v1/corporate-actions", query.build()));
}

// ---------------------------------------------------------------------------
// Forex
// ---------------------------------------------------------------------------

forex_rates_by_pair data_client::get_forex_rates(const forex_query &query) const {
    forex_rates_by_pair rates;
    ctx_.paginate("/v1beta1/forex/rates", query.build(),
                  [&rates](const json &page) { merge_symbol_page(page, "rates", rates); });
    return rates;
}

forex_rate_by_pair data_client::get_latest_forex_rates(
        const std::vector<std::string> &currency_pairs) const {
    query_builder q;
    q.add("currency_pairs", currency_pairs);
    return to_symbol_map<forex_rate>(ctx_.get("/v1beta1/forex/latest/rates", q), "rates");
}

// ---------------------------------------------------------------------------
// Fixed income
// ---------------------------------------------------------------------------

fixed_income_prices_by_isin data_client::get_latest_fixed_income_prices(
        const std::vector<std::string> &isins) const {
    query_builder q;
    q.add("isins", isins);
    return to_symbol_map<fixed_income_price>(
        ctx_.get("/v1beta1/fixed_income/latest/prices", q), "prices");
}

fixed_income_quotes_by_isin data_client::get_latest_fixed_income_quotes(
        const std::vector<std::string> &isins, std::optional<double> trade_size) const {
    query_builder q;
    q.add("isins", isins);
    q.add("trade_size", trade_size);
    return to_symbol_map<fixed_income_quote>(
        ctx_.get("/v1beta1/fixed_income/latest/quotes", q), "quotes");
}

// ---------------------------------------------------------------------------
// Logos
// ---------------------------------------------------------------------------

std::string data_client::get_logo(std::string_view symbol, std::optional<bool> placeholder) const {
    query_builder q;
    q.add("placeholder", placeholder);
    return ctx_.get_raw(std::format("/v1beta1/logos/{}", symbol), q);
}

}   // namespace alpaca
