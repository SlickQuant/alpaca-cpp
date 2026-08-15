// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/awaitable.hpp>

#include <alpaca/auth.hpp>
#include <alpaca/common.hpp>
#include <alpaca/detail/request_context.hpp>
#include <alpaca/environment.hpp>
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

namespace asio = boost::asio;

namespace alpaca {

/// Coroutine mirror of `trading_client`.
///
/// Every method has the same name, parameters and semantics as its synchronous twin and
/// returns `asio::awaitable<T>` instead of `T`. Errors surface identically: awaiting a
/// failed call throws `alpaca::api_error`.
///
/// @code
///   alpaca::trading_client_awaitable client;
///
///   asio::awaitable<void> run() {
///       auto acct = co_await client.get_account();
///       auto positions = co_await client.list_positions();
///   }
/// @endcode
class trading_client_awaitable {
public:
    explicit trading_client_awaitable(credentials creds = credentials::from_env(),
                                      environment env = environment::paper,
                                      uint32_t requests_per_minute = 200);

    trading_client_awaitable(credentials creds, std::string base_url,
                             uint32_t requests_per_minute = 200);

    std::string_view base_url() const noexcept { return ctx_.base_url(); }
    void set_base_url(std::string_view url) { ctx_.set_base_url(url); }

    environment env() const noexcept { return env_; }

    rate_limiter& limiter() noexcept { return ctx_.limiter(); }
    void set_retry_policy(const detail::retry_policy &policy) noexcept { ctx_.set_retry_policy(policy); }

    const detail::request_context& context() const noexcept { return ctx_; }

    // --- account -----------------------------------------------------------

    asio::awaitable<alpaca::account> get_account() const;
    asio::awaitable<account_configurations> get_account_configurations() const;
    asio::awaitable<account_configurations> update_account_configurations(
        account_configurations_update update) const;
    asio::awaitable<portfolio_history> get_portfolio_history(portfolio_history_query query = {}) const;

    asio::awaitable<std::vector<account_activity>> get_activities(activity_query query = {}) const;
    asio::awaitable<std::vector<account_activity>> get_activities_by_type(
        alpaca::activity_type type, activity_query query = {}) const;

    // --- orders ------------------------------------------------------------

    asio::awaitable<alpaca::order> submit_order(order_request request) const;
    asio::awaitable<std::vector<alpaca::order>> list_orders(order_query query = {}) const;
    asio::awaitable<alpaca::order> get_order(std::string order_id, bool nested = false) const;
    asio::awaitable<alpaca::order> get_order_by_client_order_id(std::string client_order_id) const;
    asio::awaitable<alpaca::order> replace_order(std::string order_id,
                                                 replace_order_request request) const;
    asio::awaitable<void> cancel_order(std::string order_id) const;
    asio::awaitable<std::vector<cancel_order_result>> cancel_all_orders() const;

    // --- positions ---------------------------------------------------------

    asio::awaitable<std::vector<alpaca::position>> list_positions() const;
    asio::awaitable<alpaca::position> get_position(std::string symbol_or_asset_id) const;
    asio::awaitable<alpaca::order> close_position(std::string symbol_or_asset_id,
                                                  close_position_request request = {}) const;
    asio::awaitable<std::vector<close_position_result>> close_all_positions(bool cancel_orders = false) const;

    asio::awaitable<void> exercise_option_position(std::string symbol_or_contract_id) const;
    asio::awaitable<void> do_not_exercise_option_position(std::string symbol_or_contract_id) const;

    // --- assets and contracts ----------------------------------------------

    asio::awaitable<std::vector<alpaca::asset>> list_assets(asset_query query = {}) const;
    asio::awaitable<alpaca::asset> get_asset(std::string symbol_or_asset_id) const;

    asio::awaitable<std::vector<option_contract>> list_option_contracts(
        option_contract_query query = {}) const;
    asio::awaitable<option_contract_page> list_option_contracts_page(
        option_contract_query query = {}) const;
    asio::awaitable<option_contract> get_option_contract(std::string symbol_or_id) const;

    // --- watchlists --------------------------------------------------------

    asio::awaitable<std::vector<watchlist>> list_watchlists() const;
    asio::awaitable<watchlist> create_watchlist(watchlist_request request) const;
    asio::awaitable<watchlist> get_watchlist(std::string watchlist_id) const;
    asio::awaitable<watchlist> get_watchlist_by_name(std::string name) const;
    asio::awaitable<watchlist> update_watchlist(std::string watchlist_id,
                                                watchlist_request request) const;
    asio::awaitable<watchlist> update_watchlist_by_name(std::string name,
                                                        watchlist_request request) const;
    asio::awaitable<watchlist> add_asset_to_watchlist(std::string watchlist_id, std::string symbol) const;
    asio::awaitable<watchlist> add_asset_to_watchlist_by_name(std::string name, std::string symbol) const;
    asio::awaitable<watchlist> remove_asset_from_watchlist(std::string watchlist_id,
                                                           std::string symbol) const;
    asio::awaitable<void> delete_watchlist(std::string watchlist_id) const;
    asio::awaitable<void> delete_watchlist_by_name(std::string name) const;

    // --- market calendar ---------------------------------------------------

    asio::awaitable<std::vector<calendar_day>> get_calendar(calendar_query query = {}) const;
    asio::awaitable<market_clock> get_clock() const;

    // --- crypto funding ----------------------------------------------------

    asio::awaitable<std::vector<crypto_wallet>> list_crypto_wallets(
        std::optional<std::string> asset = {}, std::optional<std::string> chain = {}) const;
    asio::awaitable<std::vector<crypto_transfer>> list_crypto_transfers(
        std::optional<std::string> asset = {}) const;
    asio::awaitable<crypto_transfer> get_crypto_transfer(std::string transfer_id) const;
    asio::awaitable<crypto_transfer> request_crypto_withdrawal(crypto_withdrawal_request request) const;
    asio::awaitable<crypto_transfer_estimate> get_crypto_transfer_estimate(
        std::string asset, std::string from_address, std::string to_address, std::string amount) const;

    asio::awaitable<std::vector<whitelisted_address>> list_whitelisted_addresses() const;
    asio::awaitable<whitelisted_address> create_whitelisted_address(
        whitelisted_address_request request) const;
    asio::awaitable<void> delete_whitelisted_address(std::string whitelisted_address_id) const;

    // --- short locates -----------------------------------------------------

    asio::awaitable<std::vector<locate>> list_locates(locate_query query = {}) const;
    asio::awaitable<locate_page> list_locates_page(locate_query query = {}) const;
    asio::awaitable<locate> create_locate(locate_request request) const;
    asio::awaitable<locate> get_locate(std::string locate_id) const;
    asio::awaitable<std::vector<locate_quote>> get_locate_quotes(std::vector<std::string> symbols) const;

private:
    detail::request_context ctx_;
    environment env_;
};

}   // namespace alpaca
