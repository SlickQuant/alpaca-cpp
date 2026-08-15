// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Full order lifecycle against PAPER trading: submit a resting limit order, read it back,
// replace it, then cancel it.
//
// The order is deliberately priced far below the market so it rests rather than filling.
// This example refuses to run against a live account.
//
//   set APCA_API_KEY_ID=PK...
//   set APCA_API_SECRET_KEY=...
//   place_and_cancel_order [SYMBOL]

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <alpaca/error.hpp>
#include <alpaca/trading_client.hpp>

namespace {

/// Waits for the order to leave the open state, so the example reports what actually
/// happened rather than assuming the cancel took effect immediately.
bool wait_until_closed(const alpaca::trading_client &client, const std::string &order_id) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (!client.get_order(order_id).is_open()) {
            return true;
        }
    }
    return false;
}

}   // namespace

int main(int argc, char **argv) {
    const std::string symbol = argc > 1 ? argv[1] : "AAPL";

    try {
        const alpaca::trading_client client;   // paper by default
        if (client.env() != alpaca::environment::paper) {
            std::cerr << "refusing to run against a live account\n";
            return 1;
        }

        std::cout << "Submitting a resting limit order for " << symbol << " on "
                  << client.base_url() << "\n";

        auto request = alpaca::order_request::limit(symbol, 1, alpaca::order_side::buy,
                                                    1.00, alpaca::time_in_force::gtc);
        request.client_order_id = "alpaca-cpp-example-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

        const auto submitted = client.submit_order(request);
        std::cout << "  submitted " << submitted.id
                  << "  status=" << alpaca::to_string(submitted.status) << '\n';

        try {
            const auto fetched = client.get_order(submitted.id);
            std::cout << "  read back  qty=" << (fetched.qty ? alpaca::to_string(*fetched.qty) : "-")
                      << "  limit=" << (fetched.limit_price ? alpaca::to_string(*fetched.limit_price) : "-")
                      << '\n';

            alpaca::replace_order_request replacement;
            replacement.limit_price = 1.50;
            const auto replaced = client.replace_order(submitted.id, replacement);
            std::cout << "  replaced   " << replaced.id << "  new limit="
                      << (replaced.limit_price ? alpaca::to_string(*replaced.limit_price) : "-") << '\n';

            client.cancel_order(replaced.id);
            std::cout << "  cancel requested\n";

            std::cout << (wait_until_closed(client, replaced.id)
                              ? "  order reached a terminal state\n"
                              : "  order still open after 5s — check it manually\n");
        }
        catch (...) {
            // Never leave a working order behind because a later step failed.
            try { client.cancel_order(submitted.id); } catch (...) {}
            throw;
        }

        return 0;
    }
    catch (const alpaca::api_error &e) {
        std::cerr << "Alpaca rejected the request: HTTP " << e.http_status
                  << " (code " << e.code << ") " << e.message << '\n';
        return 1;
    }
    catch (const std::exception &e) {
        std::cerr << "Failed: " << e.what() << '\n';
        return 1;
    }
}
