// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/trading_client.hpp>
#include <alpaca/detail/response.hpp>
#include <alpaca/error.hpp>

#include <format>

namespace alpaca {

using detail::append_page;
using detail::to_model;
using detail::to_vector;

trading_client::trading_client(credentials creds, environment env, uint32_t requests_per_minute)
    : ctx_(std::string(trading_base_url(env)), credentials::resolve(std::move(creds), env),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(env)
{}

trading_client::trading_client(credentials creds, std::string base_url, uint32_t requests_per_minute)
    : ctx_(std::move(base_url), credentials::resolve(std::move(creds), environment::paper),
           detail::auth_scheme::api_key, requests_per_minute)
    , env_(environment::paper)
{}

// ---------------------------------------------------------------------------
// Account
// ---------------------------------------------------------------------------

account trading_client::get_account() const {
    return to_model<account>(ctx_.get("/v2/account"));
}

account_configurations trading_client::get_account_configurations() const {
    return to_model<account_configurations>(ctx_.get("/v2/account/configurations"));
}

account_configurations trading_client::update_account_configurations(
        const account_configurations_update &update) const {
    return to_model<account_configurations>(ctx_.patch("/v2/account/configurations", update.to_json()));
}

portfolio_history trading_client::get_portfolio_history(const portfolio_history_query &query) const {
    return to_model<portfolio_history>(ctx_.get("/v2/account/portfolio/history", query.build()));
}

std::vector<account_activity> trading_client::get_activities(const activity_query &query) const {
    return to_vector<account_activity>(ctx_.get("/v2/account/activities", query.build()));
}

std::vector<account_activity> trading_client::get_activities_by_type(alpaca::activity_type type,
                                                                     const activity_query &query) const {
    const auto path = std::format("/v2/account/activities/{}", to_string(type));
    return to_vector<account_activity>(ctx_.get(path, query.build()));
}

// ---------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------

order trading_client::submit_order(const order_request &request) const {
    return to_model<order>(ctx_.post("/v2/orders", request.to_json()));
}

std::vector<order> trading_client::list_orders(const order_query &query) const {
    return to_vector<order>(ctx_.get("/v2/orders", query.build()));
}

order trading_client::get_order(std::string_view order_id, bool nested) const {
    query_builder q;
    if (nested) {
        q.add("nested", true);
    }
    return to_model<order>(ctx_.get(std::format("/v2/orders/{}", order_id), q));
}

order trading_client::get_order_by_client_order_id(std::string_view client_order_id) const {
    query_builder q;
    q.add("client_order_id", client_order_id);
    return to_model<order>(ctx_.get("/v2/orders:by_client_order_id", q));
}

order trading_client::replace_order(std::string_view order_id,
                                    const replace_order_request &request) const {
    return to_model<order>(ctx_.patch(std::format("/v2/orders/{}", order_id), request.to_json()));
}

void trading_client::cancel_order(std::string_view order_id) const {
    ctx_.del(std::format("/v2/orders/{}", order_id));
}

std::vector<cancel_order_result> trading_client::cancel_all_orders() const {
    return to_vector<cancel_order_result>(ctx_.del("/v2/orders"));
}

// ---------------------------------------------------------------------------
// Positions
// ---------------------------------------------------------------------------

std::vector<position> trading_client::list_positions() const {
    return to_vector<position>(ctx_.get("/v2/positions"));
}

position trading_client::get_position(std::string_view symbol_or_asset_id) const {
    return to_model<position>(ctx_.get(std::format("/v2/positions/{}", symbol_or_asset_id)));
}

order trading_client::close_position(std::string_view symbol_or_asset_id,
                                     const close_position_request &request) const {
    const auto path = std::format("/v2/positions/{}", symbol_or_asset_id);
    // DELETE with query parameters; slick-net's del() takes a body, so the qty/percentage
    // selector rides on the URL as Alpaca expects.
    const auto url_path = path + request.build().str();
    return to_model<order>(ctx_.del(url_path));
}

std::vector<close_position_result> trading_client::close_all_positions(bool cancel_orders) const {
    query_builder q;
    q.add("cancel_orders", cancel_orders);
    return to_vector<close_position_result>(ctx_.del(std::string("/v2/positions") + q.str()));
}

void trading_client::exercise_option_position(std::string_view symbol_or_contract_id) const {
    ctx_.post(std::format("/v2/positions/{}/exercise", symbol_or_contract_id), json::object());
}

void trading_client::do_not_exercise_option_position(std::string_view symbol_or_contract_id) const {
    ctx_.post(std::format("/v2/positions/{}/do-not-exercise", symbol_or_contract_id), json::object());
}

// ---------------------------------------------------------------------------
// Assets and contracts
// ---------------------------------------------------------------------------

std::vector<asset> trading_client::list_assets(const asset_query &query) const {
    return to_vector<asset>(ctx_.get("/v2/assets", query.build()));
}

asset trading_client::get_asset(std::string_view symbol_or_asset_id) const {
    return to_model<asset>(ctx_.get(std::format("/v2/assets/{}", symbol_or_asset_id)));
}

option_contract_page trading_client::list_option_contracts_page(const option_contract_query &query) const {
    return to_model<option_contract_page>(ctx_.get("/v2/options/contracts", query.build()));
}

std::vector<option_contract> trading_client::list_option_contracts(
        const option_contract_query &query) const {
    std::vector<option_contract> contracts;
    ctx_.paginate("/v2/options/contracts", query.build(), [&contracts](const json &page) {
        append_page(page, "option_contracts", contracts);
    });
    return contracts;
}

option_contract trading_client::get_option_contract(std::string_view symbol_or_id) const {
    return to_model<option_contract>(ctx_.get(std::format("/v2/options/contracts/{}", symbol_or_id)));
}

// ---------------------------------------------------------------------------
// Watchlists
// ---------------------------------------------------------------------------

std::vector<watchlist> trading_client::list_watchlists() const {
    return to_vector<watchlist>(ctx_.get("/v2/watchlists"));
}

watchlist trading_client::create_watchlist(const watchlist_request &request) const {
    return to_model<watchlist>(ctx_.post("/v2/watchlists", request.to_json()));
}

watchlist trading_client::get_watchlist(std::string_view watchlist_id) const {
    return to_model<watchlist>(ctx_.get(std::format("/v2/watchlists/{}", watchlist_id)));
}

watchlist trading_client::get_watchlist_by_name(std::string_view name) const {
    query_builder q;
    q.add("name", name);
    return to_model<watchlist>(ctx_.get("/v2/watchlists:by_name", q));
}

watchlist trading_client::update_watchlist(std::string_view watchlist_id,
                                           const watchlist_request &request) const {
    return to_model<watchlist>(ctx_.put(std::format("/v2/watchlists/{}", watchlist_id), request.to_json()));
}

watchlist trading_client::update_watchlist_by_name(std::string_view name,
                                                   const watchlist_request &request) const {
    query_builder q;
    q.add("name", name);
    return to_model<watchlist>(ctx_.put(std::string("/v2/watchlists:by_name") + q.str(), request.to_json()));
}

watchlist trading_client::add_asset_to_watchlist(std::string_view watchlist_id,
                                                 std::string_view symbol) const {
    return to_model<watchlist>(ctx_.post(std::format("/v2/watchlists/{}", watchlist_id),
                                         json{{"symbol", symbol}}));
}

watchlist trading_client::add_asset_to_watchlist_by_name(std::string_view name,
                                                         std::string_view symbol) const {
    query_builder q;
    q.add("name", name);
    return to_model<watchlist>(ctx_.post(std::string("/v2/watchlists:by_name") + q.str(),
                                         json{{"symbol", symbol}}));
}

watchlist trading_client::remove_asset_from_watchlist(std::string_view watchlist_id,
                                                      std::string_view symbol) const {
    return to_model<watchlist>(ctx_.del(std::format("/v2/watchlists/{}/{}", watchlist_id, symbol)));
}

void trading_client::delete_watchlist(std::string_view watchlist_id) const {
    ctx_.del(std::format("/v2/watchlists/{}", watchlist_id));
}

void trading_client::delete_watchlist_by_name(std::string_view name) const {
    query_builder q;
    q.add("name", name);
    ctx_.del(std::string("/v2/watchlists:by_name") + q.str());
}

// ---------------------------------------------------------------------------
// Market calendar
// ---------------------------------------------------------------------------

std::vector<calendar_day> trading_client::get_calendar(const calendar_query &query) const {
    return to_vector<calendar_day>(ctx_.get("/v2/calendar", query.build()));
}

market_clock trading_client::get_clock() const {
    return to_model<market_clock>(ctx_.get("/v2/clock"));
}

// ---------------------------------------------------------------------------
// Crypto funding
// ---------------------------------------------------------------------------

std::vector<crypto_wallet> trading_client::list_crypto_wallets(std::optional<std::string> asset,
                                                               std::optional<std::string> chain) const {
    query_builder q;
    q.add("asset", asset);
    q.add("chain", chain);

    // Alpaca returns a bare object when `asset` narrows the result to a single wallet.
    const json response = ctx_.get("/v2/wallets", q);
    if (response.is_object()) {
        return {to_model<crypto_wallet>(response)};
    }
    return to_vector<crypto_wallet>(response);
}

std::vector<crypto_transfer> trading_client::list_crypto_transfers(std::optional<std::string> asset) const {
    query_builder q;
    q.add("asset", asset);
    return to_vector<crypto_transfer>(ctx_.get("/v2/wallets/transfers", q));
}

crypto_transfer trading_client::get_crypto_transfer(std::string_view transfer_id) const {
    return to_model<crypto_transfer>(ctx_.get(std::format("/v2/wallets/transfers/{}", transfer_id)));
}

crypto_transfer trading_client::request_crypto_withdrawal(const crypto_withdrawal_request &request) const {
    return to_model<crypto_transfer>(ctx_.post("/v2/wallets/transfers", request.to_json()));
}

crypto_transfer_estimate trading_client::get_crypto_transfer_estimate(std::string_view asset,
                                                                      std::string_view from_address,
                                                                      std::string_view to_address,
                                                                      std::string_view amount) const {
    query_builder q;
    q.add("asset", asset);
    q.add("from_address", from_address);
    q.add("to_address", to_address);
    q.add("amount", amount);
    return to_model<crypto_transfer_estimate>(ctx_.get("/v2/wallets/fees/estimate", q));
}

std::vector<whitelisted_address> trading_client::list_whitelisted_addresses() const {
    return to_vector<whitelisted_address>(ctx_.get("/v2/wallets/whitelists"));
}

whitelisted_address trading_client::create_whitelisted_address(
        const whitelisted_address_request &request) const {
    return to_model<whitelisted_address>(ctx_.post("/v2/wallets/whitelists", request.to_json()));
}

void trading_client::delete_whitelisted_address(std::string_view whitelisted_address_id) const {
    ctx_.del(std::format("/v2/wallets/whitelists/{}", whitelisted_address_id));
}

// ---------------------------------------------------------------------------
// Short locates
// ---------------------------------------------------------------------------

locate_page trading_client::list_locates_page(const locate_query &query) const {
    return to_model<locate_page>(ctx_.get("/v1/locates", query.build()));
}

std::vector<locate> trading_client::list_locates(const locate_query &query) const {
    std::vector<locate> locates;
    ctx_.paginate("/v1/locates", query.build(), [&locates](const json &page) {
        append_page(page, "locates", locates);
    });
    return locates;
}

locate trading_client::create_locate(const locate_request &request) const {
    return to_model<locate>(ctx_.post("/v1/locates", request.to_json()));
}

locate trading_client::get_locate(std::string_view locate_id) const {
    return to_model<locate>(ctx_.get(std::format("/v1/locates/{}", locate_id)));
}

std::vector<locate_quote> trading_client::get_locate_quotes(const std::vector<std::string> &symbols) const {
    query_builder q;
    q.add("symbols", symbols);
    const json response = ctx_.get("/v1/locates/quotes", q);
    if (response.is_object() && response.contains("quotes")) {
        return to_vector<locate_quote>(response["quotes"]);
    }
    return to_vector<locate_quote>(response);
}

}   // namespace alpaca
