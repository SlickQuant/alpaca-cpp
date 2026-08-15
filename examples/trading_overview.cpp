// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Read-only tour of the Trading API: account, clock, positions and open orders.
// Places no orders, so it is safe to run at any time.
//
//   set APCA_API_KEY_ID=PK...
//   set APCA_API_SECRET_KEY=...
//   trading_overview

#include <iomanip>
#include <iostream>

#include <alpaca/error.hpp>
#include <alpaca/trading_client.hpp>

namespace {

void print_account(const alpaca::account &account) {
    std::cout << "Account " << account.account_number
              << "  status=" << alpaca::to_string(account.status) << '\n'
              << std::fixed << std::setprecision(2)
              << "  equity        " << account.equity << ' ' << account.currency << '\n'
              << "  cash          " << account.cash << '\n'
              << "  buying power  " << account.buying_power << '\n'
              << "  day trades    " << account.daytrade_count
              << (account.pattern_day_trader ? "  (PDT)" : "") << '\n';

    if (account.trading_blocked || account.account_blocked) {
        std::cout << "  ** trading is blocked on this account **\n";
    }
}

void print_positions(const std::vector<alpaca::position> &positions) {
    std::cout << "\nPositions (" << positions.size() << ")\n";
    for (const auto &p : positions) {
        std::cout << "  " << std::left << std::setw(12) << p.symbol << std::right
                  << std::setw(12) << p.qty
                  << std::setw(6) << alpaca::to_string(p.side)
                  << "  avg " << std::setw(10) << p.avg_entry_price
                  << "  last " << std::setw(10) << p.current_price
                  << "  P/L " << std::setw(12) << p.unrealized_pl << '\n';
    }
}

void print_open_orders(const std::vector<alpaca::order> &orders) {
    std::cout << "\nOpen orders (" << orders.size() << ")\n";
    for (const auto &o : orders) {
        std::cout << "  " << std::left << std::setw(12) << o.symbol << std::right
                  << std::setw(6) << alpaca::to_string(o.side)
                  << std::setw(15) << alpaca::to_string(o.type)
                  << "  qty " << (o.qty ? alpaca::to_string(*o.qty) : "-")
                  << "  limit " << (o.limit_price ? alpaca::to_string(*o.limit_price) : "-")
                  << "  " << alpaca::to_string(o.status) << '\n';
    }
}

}   // namespace

int main() {
    try {
        // Defaults to paper trading and to credentials from the environment.
        const alpaca::trading_client client;
        std::cout << "Connected to " << client.base_url() << "\n\n";

        print_account(client.get_account());

        const auto clock = client.get_clock();
        std::cout << "\nMarket is " << (clock.is_open ? "OPEN" : "closed")
                  << ", next " << (clock.is_open ? "close " : "open  ")
                  << alpaca::to_rfc3339(clock.is_open ? clock.next_close : clock.next_open) << '\n';

        print_positions(client.list_positions());

        alpaca::order_query open_orders;
        open_orders.status = alpaca::order_status_filter::open;
        print_open_orders(client.list_orders(open_orders));

        return 0;
    }
    catch (const alpaca::api_error &e) {
        std::cerr << "Alpaca rejected the request: HTTP " << e.http_status
                  << " (code " << e.code << ") " << e.message << '\n';
        if (e.is_unauthorized()) {
            std::cerr << "Check APCA_API_KEY_ID / APCA_API_SECRET_KEY. Paper keys start with "
                         "'PK'; live keys ('AK') are rejected by paper-api.alpaca.markets.\n";
        }
        return 1;
    }
    catch (const std::exception &e) {
        std::cerr << "Failed: " << e.what() << '\n';
        return 1;
    }
}
