// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/trading_client_awaitable.hpp>
#include <alpaca/detail/response.hpp>
#include <alpaca/error.hpp>

#include <format>

namespace alpaca {

using detail::append_page;
using detail::to_model;
using detail::to_vector;

trading_client_awaitable::trading_client_awaitable(credentials creds, environment env,
                                                   uint32_t requests_per_minute)
    : ctx_(std::string(trading_base_url(env)), credentials::resolve(std::move(creds), env),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(env)
{}

trading_client_awaitable::trading_client_awaitable(credentials creds, std::string base_url,
                                                   uint32_t requests_per_minute)
    : ctx_(std::move(base_url), credentials::resolve(std::move(creds), environment::paper),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(environment::paper)
{}

// ---------------------------------------------------------------------------
// Account
// ---------------------------------------------------------------------------

asio::awaitable<account> trading_client_awaitable::get_account() const {
    co_return to_model<account>(co_await ctx_.async_get("/v2/account"));
}

asio::awaitable<account_configurations> trading_client_awaitable::get_account_configurations() const {
    co_return to_model<account_configurations>(co_await ctx_.async_get("/v2/account/configurations"));
}

asio::awaitable<account_configurations> trading_client_awaitable::update_account_configurations(
        account_configurations_update update) const {
    co_return to_model<account_configurations>(
        co_await ctx_.async_patch("/v2/account/configurations", update.to_json()));
}

asio::awaitable<portfolio_history> trading_client_awaitable::get_portfolio_history(
        portfolio_history_query query) const {
    co_return to_model<portfolio_history>(
        co_await ctx_.async_get("/v2/account/portfolio/history", query.build()));
}

asio::awaitable<std::vector<account_activity>> trading_client_awaitable::get_activities(
        activity_query query) const {
    co_return to_vector<account_activity>(
        co_await ctx_.async_get("/v2/account/activities", query.build()));
}

asio::awaitable<std::vector<account_activity>> trading_client_awaitable::get_activities_by_type(
        alpaca::activity_type type, activity_query query) const {
    const auto path = std::format("/v2/account/activities/{}", to_string(type));
    co_return to_vector<account_activity>(co_await ctx_.async_get(path, query.build()));
}

// ---------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------

asio::awaitable<order> trading_client_awaitable::submit_order(order_request request) const {
    co_return to_model<order>(co_await ctx_.async_post("/v2/orders", request.to_json()));
}

asio::awaitable<std::vector<order>> trading_client_awaitable::list_orders(order_query query) const {
    co_return to_vector<order>(co_await ctx_.async_get("/v2/orders", query.build()));
}

asio::awaitable<order> trading_client_awaitable::get_order(std::string order_id, bool nested) const {
    query_builder q;
    if (nested) {
        q.add("nested", true);
    }
    co_return to_model<order>(co_await ctx_.async_get(std::format("/v2/orders/{}", order_id), q));
}

asio::awaitable<order> trading_client_awaitable::get_order_by_client_order_id(
        std::string client_order_id) const {
    query_builder q;
    q.add("client_order_id", client_order_id);
    co_return to_model<order>(co_await ctx_.async_get("/v2/orders:by_client_order_id", q));
}

asio::awaitable<order> trading_client_awaitable::replace_order(std::string order_id,
                                                               replace_order_request request) const {
    co_return to_model<order>(
        co_await ctx_.async_patch(std::format("/v2/orders/{}", order_id), request.to_json()));
}

asio::awaitable<void> trading_client_awaitable::cancel_order(std::string order_id) const {
    co_await ctx_.async_del(std::format("/v2/orders/{}", order_id));
}

asio::awaitable<std::vector<cancel_order_result>> trading_client_awaitable::cancel_all_orders() const {
    co_return to_vector<cancel_order_result>(co_await ctx_.async_del("/v2/orders"));
}

// ---------------------------------------------------------------------------
// Positions
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<position>> trading_client_awaitable::list_positions() const {
    co_return to_vector<position>(co_await ctx_.async_get("/v2/positions"));
}

asio::awaitable<position> trading_client_awaitable::get_position(std::string symbol_or_asset_id) const {
    co_return to_model<position>(
        co_await ctx_.async_get(std::format("/v2/positions/{}", symbol_or_asset_id)));
}

asio::awaitable<order> trading_client_awaitable::close_position(std::string symbol_or_asset_id,
                                                                close_position_request request) const {
    const auto path = std::format("/v2/positions/{}", symbol_or_asset_id) + request.build().str();
    co_return to_model<order>(co_await ctx_.async_del(path));
}

asio::awaitable<std::vector<close_position_result>> trading_client_awaitable::close_all_positions(
        bool cancel_orders) const {
    query_builder q;
    q.add("cancel_orders", cancel_orders);
    co_return to_vector<close_position_result>(
        co_await ctx_.async_del(std::string("/v2/positions") + q.str()));
}

asio::awaitable<void> trading_client_awaitable::exercise_option_position(
        std::string symbol_or_contract_id) const {
    co_await ctx_.async_post(std::format("/v2/positions/{}/exercise", symbol_or_contract_id),
                             json::object());
}

asio::awaitable<void> trading_client_awaitable::do_not_exercise_option_position(
        std::string symbol_or_contract_id) const {
    co_await ctx_.async_post(std::format("/v2/positions/{}/do-not-exercise", symbol_or_contract_id),
                             json::object());
}

// ---------------------------------------------------------------------------
// Assets and contracts
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<asset>> trading_client_awaitable::list_assets(asset_query query) const {
    co_return to_vector<asset>(co_await ctx_.async_get("/v2/assets", query.build()));
}

asio::awaitable<asset> trading_client_awaitable::get_asset(std::string symbol_or_asset_id) const {
    co_return to_model<asset>(co_await ctx_.async_get(std::format("/v2/assets/{}", symbol_or_asset_id)));
}

asio::awaitable<option_contract_page> trading_client_awaitable::list_option_contracts_page(
        option_contract_query query) const {
    co_return to_model<option_contract_page>(
        co_await ctx_.async_get("/v2/options/contracts", query.build()));
}

asio::awaitable<std::vector<option_contract>> trading_client_awaitable::list_option_contracts(
        option_contract_query query) const {
    std::vector<option_contract> contracts;
    co_await ctx_.async_paginate("/v2/options/contracts", query.build(),
        [&contracts](const json &page) { append_page(page, "option_contracts", contracts); });
    co_return contracts;
}

asio::awaitable<option_contract> trading_client_awaitable::get_option_contract(
        std::string symbol_or_id) const {
    co_return to_model<option_contract>(
        co_await ctx_.async_get(std::format("/v2/options/contracts/{}", symbol_or_id)));
}

// ---------------------------------------------------------------------------
// Watchlists
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<watchlist>> trading_client_awaitable::list_watchlists() const {
    co_return to_vector<watchlist>(co_await ctx_.async_get("/v2/watchlists"));
}

asio::awaitable<watchlist> trading_client_awaitable::create_watchlist(watchlist_request request) const {
    co_return to_model<watchlist>(co_await ctx_.async_post("/v2/watchlists", request.to_json()));
}

asio::awaitable<watchlist> trading_client_awaitable::get_watchlist(std::string watchlist_id) const {
    co_return to_model<watchlist>(co_await ctx_.async_get(std::format("/v2/watchlists/{}", watchlist_id)));
}

asio::awaitable<watchlist> trading_client_awaitable::get_watchlist_by_name(std::string name) const {
    query_builder q;
    q.add("name", name);
    co_return to_model<watchlist>(co_await ctx_.async_get("/v2/watchlists:by_name", q));
}

asio::awaitable<watchlist> trading_client_awaitable::update_watchlist(std::string watchlist_id,
                                                                      watchlist_request request) const {
    co_return to_model<watchlist>(
        co_await ctx_.async_put(std::format("/v2/watchlists/{}", watchlist_id), request.to_json()));
}

asio::awaitable<watchlist> trading_client_awaitable::update_watchlist_by_name(
        std::string name, watchlist_request request) const {
    query_builder q;
    q.add("name", name);
    co_return to_model<watchlist>(
        co_await ctx_.async_put(std::string("/v2/watchlists:by_name") + q.str(), request.to_json()));
}

asio::awaitable<watchlist> trading_client_awaitable::add_asset_to_watchlist(std::string watchlist_id,
                                                                            std::string symbol) const {
    // The body is a named local rather than an inline `json{{"symbol", symbol}}` argument:
    // an initializer_list temporary whose backing array has to survive a suspension point
    // ICEs GCC 11 and 13 (build_special_member_call, cp/call.cc) inside morph_fn_to_coro.
    // Fixed in GCC 14, but both are inside our supported range. Keep it hoisted.
    const json body{{"symbol", symbol}};
    co_return to_model<watchlist>(
        co_await ctx_.async_post(std::format("/v2/watchlists/{}", watchlist_id), body));
}

asio::awaitable<watchlist> trading_client_awaitable::add_asset_to_watchlist_by_name(
        std::string name, std::string symbol) const {
    query_builder q;
    q.add("name", name);
    // Hoisted for the same reason as add_asset_to_watchlist above.
    const json body{{"symbol", symbol}};
    co_return to_model<watchlist>(
        co_await ctx_.async_post(std::string("/v2/watchlists:by_name") + q.str(), body));
}

asio::awaitable<watchlist> trading_client_awaitable::remove_asset_from_watchlist(
        std::string watchlist_id, std::string symbol) const {
    co_return to_model<watchlist>(
        co_await ctx_.async_del(std::format("/v2/watchlists/{}/{}", watchlist_id, symbol)));
}

asio::awaitable<void> trading_client_awaitable::delete_watchlist(std::string watchlist_id) const {
    co_await ctx_.async_del(std::format("/v2/watchlists/{}", watchlist_id));
}

asio::awaitable<void> trading_client_awaitable::delete_watchlist_by_name(std::string name) const {
    query_builder q;
    q.add("name", name);
    co_await ctx_.async_del(std::string("/v2/watchlists:by_name") + q.str());
}

// ---------------------------------------------------------------------------
// Market calendar
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<calendar_day>> trading_client_awaitable::get_calendar(
        calendar_query query) const {
    co_return to_vector<calendar_day>(co_await ctx_.async_get("/v2/calendar", query.build()));
}

asio::awaitable<market_clock> trading_client_awaitable::get_clock() const {
    co_return to_model<market_clock>(co_await ctx_.async_get("/v2/clock"));
}

// ---------------------------------------------------------------------------
// Crypto funding
// ---------------------------------------------------------------------------

asio::awaitable<std::vector<crypto_wallet>> trading_client_awaitable::list_crypto_wallets(
        std::optional<std::string> asset, std::optional<std::string> chain) const {
    query_builder q;
    q.add("asset", asset);
    q.add("chain", chain);

    const json response = co_await ctx_.async_get("/v2/wallets", q);
    if (response.is_object()) {
        co_return std::vector<crypto_wallet>{to_model<crypto_wallet>(response)};
    }
    co_return to_vector<crypto_wallet>(response);
}

asio::awaitable<std::vector<crypto_transfer>> trading_client_awaitable::list_crypto_transfers(
        std::optional<std::string> asset) const {
    query_builder q;
    q.add("asset", asset);
    co_return to_vector<crypto_transfer>(co_await ctx_.async_get("/v2/wallets/transfers", q));
}

asio::awaitable<crypto_transfer> trading_client_awaitable::get_crypto_transfer(
        std::string transfer_id) const {
    co_return to_model<crypto_transfer>(
        co_await ctx_.async_get(std::format("/v2/wallets/transfers/{}", transfer_id)));
}

asio::awaitable<crypto_transfer> trading_client_awaitable::request_crypto_withdrawal(
        crypto_withdrawal_request request) const {
    co_return to_model<crypto_transfer>(
        co_await ctx_.async_post("/v2/wallets/transfers", request.to_json()));
}

asio::awaitable<crypto_transfer_estimate> trading_client_awaitable::get_crypto_transfer_estimate(
        std::string asset, std::string from_address, std::string to_address, std::string amount) const {
    query_builder q;
    q.add("asset", asset);
    q.add("from_address", from_address);
    q.add("to_address", to_address);
    q.add("amount", amount);
    co_return to_model<crypto_transfer_estimate>(co_await ctx_.async_get("/v2/wallets/fees/estimate", q));
}

asio::awaitable<std::vector<whitelisted_address>>
trading_client_awaitable::list_whitelisted_addresses() const {
    co_return to_vector<whitelisted_address>(co_await ctx_.async_get("/v2/wallets/whitelists"));
}

asio::awaitable<whitelisted_address> trading_client_awaitable::create_whitelisted_address(
        whitelisted_address_request request) const {
    co_return to_model<whitelisted_address>(
        co_await ctx_.async_post("/v2/wallets/whitelists", request.to_json()));
}

asio::awaitable<void> trading_client_awaitable::delete_whitelisted_address(
        std::string whitelisted_address_id) const {
    co_await ctx_.async_del(std::format("/v2/wallets/whitelists/{}", whitelisted_address_id));
}

// ---------------------------------------------------------------------------
// Short locates
// ---------------------------------------------------------------------------

asio::awaitable<locate_page> trading_client_awaitable::list_locates_page(locate_query query) const {
    co_return to_model<locate_page>(co_await ctx_.async_get("/v1/locates", query.build()));
}

asio::awaitable<std::vector<locate>> trading_client_awaitable::list_locates(locate_query query) const {
    std::vector<locate> locates;
    co_await ctx_.async_paginate("/v1/locates", query.build(),
        [&locates](const json &page) { append_page(page, "locates", locates); });
    co_return locates;
}

asio::awaitable<locate> trading_client_awaitable::create_locate(locate_request request) const {
    co_return to_model<locate>(co_await ctx_.async_post("/v1/locates", request.to_json()));
}

asio::awaitable<locate> trading_client_awaitable::get_locate(std::string locate_id) const {
    co_return to_model<locate>(co_await ctx_.async_get(std::format("/v1/locates/{}", locate_id)));
}

asio::awaitable<std::vector<locate_quote>> trading_client_awaitable::get_locate_quotes(
        std::vector<std::string> symbols) const {
    query_builder q;
    q.add("symbols", symbols);
    const json response = co_await ctx_.async_get("/v1/locates/quotes", q);
    if (response.is_object() && response.contains("quotes")) {
        co_return to_vector<locate_quote>(response["quotes"]);
    }
    co_return to_vector<locate_quote>(response);
}

}   // namespace alpaca
