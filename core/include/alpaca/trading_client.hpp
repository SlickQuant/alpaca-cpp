// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

namespace alpaca {

/// Synchronous client for the Alpaca Trading API (v2).
///
/// Every method throws `alpaca::api_error` when Alpaca answers with a non-2xx status, so a
/// returned empty vector always means "the account really has none of these" and never
/// "the request failed". Rate limiting and retry-with-backoff are applied automatically.
///
/// Defaults to **paper trading** and to credentials from `APCA_API_KEY_ID` /
/// `APCA_API_SECRET_KEY`. Pointing at live money is always an explicit act:
///
/// @code
///   alpaca::trading_client paper;                                    // paper, env credentials
///   alpaca::trading_client live({key, secret}, alpaca::environment::live);
/// @endcode
class trading_client {
public:
    explicit trading_client(credentials creds = credentials::from_env(),
                            environment env = environment::paper,
                            uint32_t requests_per_minute = 200);

    /// Points the client at an explicit base URL (a proxy, or a URL from `APCA_API_BASE_URL`).
    trading_client(credentials creds, std::string base_url, uint32_t requests_per_minute = 200);

    std::string_view base_url() const noexcept { return ctx_.base_url(); }
    void set_base_url(std::string_view url) { ctx_.set_base_url(url); }

    environment env() const noexcept { return env_; }

    rate_limiter& limiter() noexcept { return ctx_.limiter(); }
    void set_retry_policy(const detail::retry_policy &policy) noexcept { ctx_.set_retry_policy(policy); }

    const detail::request_context& context() const noexcept { return ctx_; }

    // --- account -----------------------------------------------------------

    alpaca::account get_account() const;
    account_configurations get_account_configurations() const;
    account_configurations update_account_configurations(const account_configurations_update &update) const;
    portfolio_history get_portfolio_history(const portfolio_history_query &query = {}) const;

    std::vector<account_activity> get_activities(const activity_query &query = {}) const;
    std::vector<account_activity> get_activities_by_type(alpaca::activity_type type,
                                                         const activity_query &query = {}) const;

    // --- orders ------------------------------------------------------------

    alpaca::order submit_order(const order_request &request) const;
    std::vector<alpaca::order> list_orders(const order_query &query = {}) const;
    alpaca::order get_order(std::string_view order_id, bool nested = false) const;
    alpaca::order get_order_by_client_order_id(std::string_view client_order_id) const;
    alpaca::order replace_order(std::string_view order_id, const replace_order_request &request) const;

    /// Requests cancellation. Alpaca answers 204 and cancels asynchronously, so a successful
    /// return means "accepted", not "canceled" — watch the order status or the trade stream.
    void cancel_order(std::string_view order_id) const;
    std::vector<cancel_order_result> cancel_all_orders() const;

    // --- positions ---------------------------------------------------------

    std::vector<alpaca::position> list_positions() const;
    alpaca::position get_position(std::string_view symbol_or_asset_id) const;
    alpaca::order close_position(std::string_view symbol_or_asset_id,
                                 const close_position_request &request = {}) const;
    std::vector<close_position_result> close_all_positions(bool cancel_orders = false) const;

    /// Exercises an option position. Alpaca answers 200 with an empty body.
    void exercise_option_position(std::string_view symbol_or_contract_id) const;
    void do_not_exercise_option_position(std::string_view symbol_or_contract_id) const;

    // --- assets and contracts ----------------------------------------------

    std::vector<alpaca::asset> list_assets(const asset_query &query = {}) const;
    alpaca::asset get_asset(std::string_view symbol_or_asset_id) const;

    /// Returns every matching contract, following `next_page_token` across pages.
    std::vector<option_contract> list_option_contracts(const option_contract_query &query = {}) const;
    /// Returns a single page, letting the caller drive pagination.
    option_contract_page list_option_contracts_page(const option_contract_query &query = {}) const;
    option_contract get_option_contract(std::string_view symbol_or_id) const;

    // --- watchlists --------------------------------------------------------

    std::vector<watchlist> list_watchlists() const;
    watchlist create_watchlist(const watchlist_request &request) const;
    watchlist get_watchlist(std::string_view watchlist_id) const;
    watchlist get_watchlist_by_name(std::string_view name) const;
    watchlist update_watchlist(std::string_view watchlist_id, const watchlist_request &request) const;
    watchlist update_watchlist_by_name(std::string_view name, const watchlist_request &request) const;
    watchlist add_asset_to_watchlist(std::string_view watchlist_id, std::string_view symbol) const;
    watchlist add_asset_to_watchlist_by_name(std::string_view name, std::string_view symbol) const;
    watchlist remove_asset_from_watchlist(std::string_view watchlist_id, std::string_view symbol) const;
    void delete_watchlist(std::string_view watchlist_id) const;
    void delete_watchlist_by_name(std::string_view name) const;

    // --- market calendar ---------------------------------------------------

    std::vector<calendar_day> get_calendar(const calendar_query &query = {}) const;
    market_clock get_clock() const;

    // --- crypto funding ----------------------------------------------------

    std::vector<crypto_wallet> list_crypto_wallets(std::optional<std::string> asset = {},
                                                   std::optional<std::string> chain = {}) const;
    std::vector<crypto_transfer> list_crypto_transfers(std::optional<std::string> asset = {}) const;
    crypto_transfer get_crypto_transfer(std::string_view transfer_id) const;
    crypto_transfer request_crypto_withdrawal(const crypto_withdrawal_request &request) const;
    crypto_transfer_estimate get_crypto_transfer_estimate(std::string_view asset,
                                                          std::string_view from_address,
                                                          std::string_view to_address,
                                                          std::string_view amount) const;

    std::vector<whitelisted_address> list_whitelisted_addresses() const;
    whitelisted_address create_whitelisted_address(const whitelisted_address_request &request) const;
    void delete_whitelisted_address(std::string_view whitelisted_address_id) const;

    // --- short locates -----------------------------------------------------

    /// Returns every matching locate, following `next_page_token` across pages.
    std::vector<locate> list_locates(const locate_query &query = {}) const;
    locate_page list_locates_page(const locate_query &query = {}) const;
    locate create_locate(const locate_request &request) const;
    locate get_locate(std::string_view locate_id) const;
    std::vector<locate_quote> get_locate_quotes(const std::vector<std::string> &symbols) const;

private:
    detail::request_context ctx_;
    environment env_;
};

}   // namespace alpaca
