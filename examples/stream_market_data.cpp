// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Live market data over websockets: subscribes to trades, quotes and minute bars and
// prints them as they arrive. Read-only; places no orders.
//
// Defaults to Alpaca's synthetic `test` feed, which streams FAKEPACA around the clock so
// the example does something visible outside market hours. Pass real symbols to switch
// to the IEX feed instead.
//
//   set APCA_PAPER_API_KEY_ID=PK...
//   set APCA_PAPER_API_SECRET_KEY=...
//   stream_market_data              # synthetic test feed
//   stream_market_data AAPL MSFT    # real symbols on the IEX feed

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <alpaca/streaming/stock_data_stream.hpp>

namespace {

std::atomic<bool> running{true};

void handle_signal(int) { running.store(false, std::memory_order_release); }

/// Handlers run on the websocket service thread, so all of them share one lock rather
/// than interleaving half-written lines on stdout. Printing is also the reason this
/// example is not a latency benchmark — real consumers hand work to their own thread.
std::mutex print_mutex;

// Lines below end with std::endl rather than '\n' deliberately. stdout is block buffered
// when it is not a terminal, so piping this into a file or a pager would otherwise show
// nothing until the buffer filled — which, on a quiet feed, is minutes.

void print_trade(const std::string &symbol, const alpaca::trade &t) {
    std::lock_guard lock(print_mutex);
    std::cout << std::fixed << std::setprecision(2)
              << "TRADE  " << std::left << std::setw(10) << symbol << std::right
              << std::setw(10) << t.price << " x " << std::setw(8) << t.size
              << "  " << t.exchange << std::endl;
}

void print_quote(const std::string &symbol, const alpaca::quote &q) {
    std::lock_guard lock(print_mutex);
    std::cout << std::fixed << std::setprecision(2)
              << "QUOTE  " << std::left << std::setw(10) << symbol << std::right
              << std::setw(10) << q.bid_price << " / " << std::setw(10) << q.ask_price;
    if (q.bid_price > 0. && q.ask_price > 0.) {
        std::cout << "  (spread " << q.spread() << ")";
    }
    std::cout << std::endl;
}

void print_bar(const std::string &symbol, const alpaca::bar &b) {
    std::lock_guard lock(print_mutex);
    std::cout << std::fixed << std::setprecision(2)
              << "BAR    " << std::left << std::setw(10) << symbol << std::right
              << "  O " << b.open << "  H " << b.high << "  L " << b.low
              << "  C " << b.close << "  V " << b.volume << std::endl;
}

}   // namespace

int main(int argc, char **argv) {
    std::signal(SIGINT, handle_signal);

    std::vector<std::string> symbols(argv + 1, argv + argc);
    const bool use_test_feed = symbols.empty();
    if (use_test_feed) {
        symbols = {"FAKEPACA"};
    }

    // The test feed lives at its own URL rather than being a member of the feed enum,
    // because it is not a real market. A stream is deliberately neither copyable nor
    // movable — the socket callbacks capture `this` — so it is held by pointer here
    // rather than picked with a ternary.
    const auto owned = use_test_feed
        ? std::make_unique<alpaca::stock_data_stream>(
              alpaca::credentials{}, alpaca::stock_data_stream::test_feed_url())
        : std::make_unique<alpaca::stock_data_stream>(
              alpaca::credentials{}, alpaca::data_feed::iex);
    auto &stream = *owned;

    // Every handler must be registered before connect(): they are read without a lock on
    // the message path, so attaching one to a live stream would be a data race.
    stream.on_trade(print_trade);
    stream.on_quote(print_quote);
    stream.on_bar(print_bar);

    stream.on_error([](const alpaca::stream_error &e) {
        std::lock_guard lock(print_mutex);
        std::cerr << "stream error " << e.code << ": " << e.message << '\n';
    });
    stream.on_disconnected([] {
        std::lock_guard lock(print_mutex);
        std::cerr << "disconnected — reconnecting\n";
    });

    stream.subscribe_trades(symbols);
    stream.subscribe_quotes(symbols);
    stream.subscribe_bars(symbols);

    std::cout << "Connecting to " << stream.url() << '\n';
    if (!stream.connect()) {
        std::cerr << "Could not authenticate. Check APCA_PAPER_API_KEY_ID / "
                     "APCA_PAPER_API_SECRET_KEY (or the unprefixed pair).\n";
        return 1;
    }

    std::cout << "Streaming ";
    for (const auto &s : symbols) { std::cout << s << ' '; }
    std::cout << "— Ctrl-C to stop\n" << std::endl;

    while (running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nStopping.\n";
    stream.disconnect();
    return 0;
}
