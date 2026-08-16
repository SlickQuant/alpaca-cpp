// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/awaitable.hpp>

#include <alpaca/data_client.hpp>

namespace asio = boost::asio;

namespace alpaca {

/// Coroutine mirror of `data_client`.
///
/// Every method has the same name, parameters and semantics as its synchronous twin and
/// returns `asio::awaitable<T>` instead of `T`; a failed call throws `alpaca::api_error`
/// when awaited.
///
/// @code
///   alpaca::data_client_awaitable data;
///
///   asio::awaitable<void> run() {
///       alpaca::bar_query query;
///       query.symbols = {"AAPL"};
///       query.feed = alpaca::data_feed::iex;
///       auto bars = co_await data.get_stock_bars(query);
///   }
/// @endcode
class data_client_awaitable {
public:
    explicit data_client_awaitable(credentials creds = {},
                                   environment env = environment::paper,
                                   uint32_t requests_per_minute = 200);

    data_client_awaitable(credentials creds, std::string base_url,
                          uint32_t requests_per_minute = 200);

    std::string_view base_url() const noexcept { return ctx_.base_url(); }
    void set_base_url(std::string_view url) { ctx_.set_base_url(url); }

    rate_limiter& limiter() noexcept { return ctx_.limiter(); }
    void set_retry_policy(const detail::retry_policy &policy) noexcept { ctx_.set_retry_policy(policy); }

    const detail::request_context& context() const noexcept { return ctx_; }

    // --- stocks: historical ------------------------------------------------

    asio::awaitable<bars_by_symbol> get_stock_bars(bar_query query) const;
    asio::awaitable<trades_by_symbol> get_stock_trades(history_query query) const;
    asio::awaitable<quotes_by_symbol> get_stock_quotes(history_query query) const;
    asio::awaitable<auctions_by_symbol> get_stock_auctions(history_query query) const;

    asio::awaitable<data_client::bars_page> get_stock_bars_page(bar_query query) const;
    asio::awaitable<data_client::trades_page> get_stock_trades_page(history_query query) const;
    asio::awaitable<data_client::quotes_page> get_stock_quotes_page(history_query query) const;
    asio::awaitable<data_client::auctions_page> get_stock_auctions_page(history_query query) const;

    // --- stocks: latest ----------------------------------------------------

    asio::awaitable<bar_by_symbol> get_latest_stock_bars(latest_query query) const;
    asio::awaitable<trade_by_symbol> get_latest_stock_trades(latest_query query) const;
    asio::awaitable<quote_by_symbol> get_latest_stock_quotes(latest_query query) const;
    asio::awaitable<snapshots_by_symbol> get_stock_snapshots(latest_query query) const;

    asio::awaitable<bar> get_latest_stock_bar(std::string symbol,
                                              std::optional<alpaca::data_feed> feed = {}) const;
    asio::awaitable<trade> get_latest_stock_trade(std::string symbol,
                                                  std::optional<alpaca::data_feed> feed = {}) const;
    asio::awaitable<quote> get_latest_stock_quote(std::string symbol,
                                                  std::optional<alpaca::data_feed> feed = {}) const;
    asio::awaitable<snapshot> get_stock_snapshot(std::string symbol,
                                                 std::optional<alpaca::data_feed> feed = {}) const;

    // --- stocks: reference -------------------------------------------------

    asio::awaitable<code_map> get_stock_condition_codes(std::string tick_type, std::string tape) const;
    asio::awaitable<code_map> get_stock_exchange_codes() const;

    // --- crypto ------------------------------------------------------------

    asio::awaitable<bars_by_symbol> get_crypto_bars(crypto_bar_query query,
                                                    crypto_location loc = crypto_location::us) const;
    asio::awaitable<trades_by_symbol> get_crypto_trades(crypto_history_query query,
                                                        crypto_location loc = crypto_location::us) const;
    asio::awaitable<quotes_by_symbol> get_crypto_quotes(crypto_history_query query,
                                                        crypto_location loc = crypto_location::us) const;

    asio::awaitable<bar_by_symbol> get_latest_crypto_bars(
        std::vector<std::string> symbols, crypto_location loc = crypto_location::us) const;
    asio::awaitable<trade_by_symbol> get_latest_crypto_trades(
        std::vector<std::string> symbols, crypto_location loc = crypto_location::us) const;
    asio::awaitable<quote_by_symbol> get_latest_crypto_quotes(
        std::vector<std::string> symbols, crypto_location loc = crypto_location::us) const;
    asio::awaitable<orderbooks_by_symbol> get_latest_crypto_orderbooks(
        std::vector<std::string> symbols, crypto_location loc = crypto_location::us) const;
    asio::awaitable<snapshots_by_symbol> get_crypto_snapshots(
        std::vector<std::string> symbols, crypto_location loc = crypto_location::us) const;

    // --- options -----------------------------------------------------------

    asio::awaitable<bars_by_symbol> get_option_bars(option_bar_query query) const;
    asio::awaitable<trades_by_symbol> get_option_trades(option_history_query query) const;

    asio::awaitable<trade_by_symbol> get_latest_option_trades(option_latest_query query) const;
    asio::awaitable<quote_by_symbol> get_latest_option_quotes(option_latest_query query) const;
    asio::awaitable<option_snapshots_by_symbol> get_option_snapshots(option_latest_query query) const;

    asio::awaitable<option_snapshots_by_symbol> get_option_chain(
        std::string underlying_symbol, option_chain_query query = {}) const;
    asio::awaitable<option_chain_page> get_option_chain_page(
        std::string underlying_symbol, option_chain_query query = {}) const;

    asio::awaitable<code_map> get_option_condition_codes(std::string tick_type) const;
    asio::awaitable<code_map> get_option_exchange_codes() const;

    // --- news --------------------------------------------------------------

    asio::awaitable<std::vector<news_article>> get_news(news_query query = {}) const;
    asio::awaitable<news_page> get_news_page(news_query query = {}) const;

    // --- screener ----------------------------------------------------------

    asio::awaitable<most_actives> get_most_actives(std::optional<std::string> by = {},
                                                   std::optional<uint32_t> top = {}) const;
    asio::awaitable<movers> get_movers(std::string market_type = "stocks",
                                       std::optional<uint32_t> top = {}) const;

    // --- corporate actions -------------------------------------------------

    asio::awaitable<std::vector<corporate_action>> get_corporate_actions(
        corporate_action_query query) const;
    asio::awaitable<corporate_action_page> get_corporate_actions_page(
        corporate_action_query query) const;

    // --- forex -------------------------------------------------------------

    asio::awaitable<forex_rates_by_pair> get_forex_rates(forex_query query) const;
    asio::awaitable<forex_rate_by_pair> get_latest_forex_rates(
        std::vector<std::string> currency_pairs) const;

    // --- fixed income ------------------------------------------------------

    asio::awaitable<fixed_income_prices_by_isin> get_latest_fixed_income_prices(
        std::vector<std::string> isins) const;
    asio::awaitable<fixed_income_quotes_by_isin> get_latest_fixed_income_quotes(
        std::vector<std::string> isins, std::optional<double> trade_size = {}) const;

    // --- logos -------------------------------------------------------------

    asio::awaitable<std::string> get_logo(std::string symbol,
                                          std::optional<bool> placeholder = {}) const;

private:
    detail::request_context ctx_;
    environment env_;
};

}   // namespace alpaca
