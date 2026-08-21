// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <alpaca/auth.hpp>
#include <alpaca/common.hpp>
#include <alpaca/data/auction.hpp>
#include <alpaca/data/bar.hpp>
#include <alpaca/data/corporate_action.hpp>
#include <alpaca/data/fixed_income.hpp>
#include <alpaca/data/forex.hpp>
#include <alpaca/data/news.hpp>
#include <alpaca/data/orderbook.hpp>
#include <alpaca/data/query.hpp>
#include <alpaca/data/quote.hpp>
#include <alpaca/data/screener.hpp>
#include <alpaca/data/snapshot.hpp>
#include <alpaca/data/timeframe.hpp>
#include <alpaca/data/trade.hpp>
#include <alpaca/detail/request_context.hpp>
#include <alpaca/environment.hpp>

namespace alpaca {

/// Maps condition or exchange codes to their names, e.g. {"@": "Regular Sale"}.
using code_map = std::unordered_map<std::string, std::string>;

/// Synchronous client for the Alpaca Market Data API.
///
/// Historical endpoints come in two flavours. The plain `get_*` methods follow
/// `next_page_token` to completion and merge every page, which is what you want for a
/// bounded window. The `*_page` variants return a single page plus the token so you can
/// drive paging yourself — use those for open-ended windows, where fetching everything
/// could be unbounded.
///
/// Errors surface as `alpaca::api_error`, so an empty result always means "no data in that
/// window" rather than a failed request.
///
/// `data.alpaca.markets` is not account-scoped, so paper and live keys both work. Empty
/// credentials are resolved from the environment exactly as for `trading_client`.
///
/// @code
///   alpaca::data_client data;
///
///   alpaca::bar_query query;
///   query.symbols = {"AAPL", "MSFT"};
///   query.timeframe = alpaca::timeframes::one_day;
///   query.start = "2024-01-02";
///   query.end = "2024-02-01";
///   query.feed = alpaca::data_feed::iex;     // the only feed without a subscription
///
///   auto bars = data.get_stock_bars(query);
///   for (const auto &b : bars["AAPL"]) { ... }
/// @endcode
class data_client {
public:
    explicit data_client(credentials creds = {},
                         environment env = environment::paper,
                         uint32_t requests_per_minute = 200);

    data_client(credentials creds, std::string base_url, uint32_t requests_per_minute = 200);

    std::string_view base_url() const noexcept { return ctx_.base_url(); }
    void set_base_url(std::string_view url) { ctx_.set_base_url(url); }

    /// The environment the credentials were resolved for. `data.alpaca.markets` itself is
    /// not account-scoped, so this only records which key pair the client is using.
    environment env() const noexcept { return env_; }

    rate_limiter& limiter() noexcept { return ctx_.limiter(); }
    void set_retry_policy(const detail::retry_policy &policy) noexcept { ctx_.set_retry_policy(policy); }

    const detail::request_context& context() const noexcept { return ctx_; }

    // --- stocks: historical ------------------------------------------------

    bars_by_symbol get_stock_bars(const bar_query &query) const;
    trades_by_symbol get_stock_trades(const history_query &query) const;
    quotes_by_symbol get_stock_quotes(const history_query &query) const;
    auctions_by_symbol get_stock_auctions(const history_query &query) const;

    /// Single-page variants; inspect `next_page_token` to continue.
    struct bars_page { bars_by_symbol bars; std::string next_page_token; };
    struct trades_page { trades_by_symbol trades; std::string next_page_token; };
    struct quotes_page { quotes_by_symbol quotes; std::string next_page_token; };
    struct auctions_page { auctions_by_symbol auctions; std::string next_page_token; };

    bars_page get_stock_bars_page(const bar_query &query) const;
    trades_page get_stock_trades_page(const history_query &query) const;
    quotes_page get_stock_quotes_page(const history_query &query) const;
    auctions_page get_stock_auctions_page(const history_query &query) const;

    // --- stocks: latest ----------------------------------------------------

    bar_by_symbol get_latest_stock_bars(const latest_query &query) const;
    trade_by_symbol get_latest_stock_trades(const latest_query &query) const;
    quote_by_symbol get_latest_stock_quotes(const latest_query &query) const;
    snapshots_by_symbol get_stock_snapshots(const latest_query &query) const;

    /// Single-symbol convenience wrappers over the multi-symbol endpoints.
    bar get_latest_stock_bar(std::string_view symbol,
                             std::optional<alpaca::data_feed> feed = {}) const;
    trade get_latest_stock_trade(std::string_view symbol,
                                 std::optional<alpaca::data_feed> feed = {}) const;
    quote get_latest_stock_quote(std::string_view symbol,
                                 std::optional<alpaca::data_feed> feed = {}) const;
    snapshot get_stock_snapshot(std::string_view symbol,
                                std::optional<alpaca::data_feed> feed = {}) const;

    // --- stocks: reference -------------------------------------------------

    /// @param tick_type "trade" or "quote". @param tape "A", "B" or "C".
    code_map get_stock_condition_codes(std::string_view tick_type, std::string_view tape) const;
    code_map get_stock_exchange_codes() const;

    // --- crypto ------------------------------------------------------------

    bars_by_symbol get_crypto_bars(const crypto_bar_query &query,
                                   crypto_location loc = crypto_location::us) const;
    trades_by_symbol get_crypto_trades(const crypto_history_query &query,
                                       crypto_location loc = crypto_location::us) const;
    quotes_by_symbol get_crypto_quotes(const crypto_history_query &query,
                                       crypto_location loc = crypto_location::us) const;

    bar_by_symbol get_latest_crypto_bars(const std::vector<std::string> &symbols,
                                         crypto_location loc = crypto_location::us) const;
    trade_by_symbol get_latest_crypto_trades(const std::vector<std::string> &symbols,
                                             crypto_location loc = crypto_location::us) const;
    quote_by_symbol get_latest_crypto_quotes(const std::vector<std::string> &symbols,
                                             crypto_location loc = crypto_location::us) const;
    orderbooks_by_symbol get_latest_crypto_orderbooks(const std::vector<std::string> &symbols,
                                                      crypto_location loc = crypto_location::us) const;
    snapshots_by_symbol get_crypto_snapshots(const std::vector<std::string> &symbols,
                                             crypto_location loc = crypto_location::us) const;

    // --- options -----------------------------------------------------------

    bars_by_symbol get_option_bars(const option_bar_query &query) const;
    trades_by_symbol get_option_trades(const option_history_query &query) const;

    trade_by_symbol get_latest_option_trades(const option_latest_query &query) const;
    quote_by_symbol get_latest_option_quotes(const option_latest_query &query) const;
    option_snapshots_by_symbol get_option_snapshots(const option_latest_query &query) const;

    /// Every contract on an underlying. Follows pagination to completion — a chain on a
    /// liquid name runs to thousands of contracts, so prefer the filtered query or the
    /// `_page` variant.
    option_snapshots_by_symbol get_option_chain(std::string_view underlying_symbol,
                                                const option_chain_query &query = {}) const;
    option_chain_page get_option_chain_page(std::string_view underlying_symbol,
                                            const option_chain_query &query = {}) const;

    code_map get_option_condition_codes(std::string_view tick_type) const;
    code_map get_option_exchange_codes() const;

    // --- news --------------------------------------------------------------

    std::vector<news_article> get_news(const news_query &query = {}) const;
    news_page get_news_page(const news_query &query = {}) const;

    // --- screener ----------------------------------------------------------

    /// @param by "volume" (default) or "trades". @param top 1-100.
    most_actives get_most_actives(std::optional<std::string> by = {},
                                  std::optional<uint32_t> top = {}) const;
    /// @param market_type "stocks" or "crypto". @param top 1-50.
    movers get_movers(std::string_view market_type = "stocks",
                      std::optional<uint32_t> top = {}) const;

    // --- corporate actions -------------------------------------------------

    std::vector<corporate_action> get_corporate_actions(const corporate_action_query &query) const;
    corporate_action_page get_corporate_actions_page(const corporate_action_query &query) const;

    // --- forex -------------------------------------------------------------

    forex_rates_by_pair get_forex_rates(const forex_query &query) const;
    forex_rate_by_pair get_latest_forex_rates(const std::vector<std::string> &currency_pairs) const;

    // --- fixed income ------------------------------------------------------

    fixed_income_prices_by_isin get_latest_fixed_income_prices(
        const std::vector<std::string> &isins) const;
    fixed_income_quotes_by_isin get_latest_fixed_income_quotes(
        const std::vector<std::string> &isins,
        std::optional<double> trade_size = {}) const;

    // --- logos -------------------------------------------------------------

    /// Returns raw PNG bytes, not JSON. `placeholder` asks Alpaca for a generated image
    /// when it has no logo on file.
    std::string get_logo(std::string_view symbol, std::optional<bool> placeholder = {}) const;

private:
    detail::request_context ctx_;
    environment env_;
};

}   // namespace alpaca
