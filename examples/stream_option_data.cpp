// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Live option trades and quotes over the MessagePack stream. Read-only; places no orders.
//
// Uses the `indicative` feed, which needs no OPRA subscription. Pass `--opra` first to
// use the OPRA feed instead. Contracts are named by their OCC symbols.
//
//   set APCA_PAPER_API_KEY_ID=PK...
//   set APCA_PAPER_API_SECRET_KEY=...
//   stream_option_data AAPL260116C00200000 AAPL260116P00200000
//   stream_option_data --opra SPY260116C00600000

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <alpaca/streaming/option_data_stream.hpp>

namespace {

std::atomic<bool> running{true};

void handle_signal(int) { running.store(false, std::memory_order_release); }

/// Handlers run on the websocket service thread, so they share one lock rather than
/// interleaving half-written lines on stdout. Real consumers hand work to their own
/// thread instead of printing here.
std::mutex print_mutex;

// std::endl rather than '\n' on purpose: stdout is block buffered when it is not a
// terminal, and a quiet contract would otherwise show nothing for minutes.

void print_trade(const std::string &symbol, const alpaca::trade &t) {
    std::lock_guard lock(print_mutex);
    std::cout << std::fixed << std::setprecision(2)
              << "TRADE  " << std::left << std::setw(22) << symbol << std::right
              << std::setw(9) << t.price << " x " << std::setw(6) << t.size
              << "  " << t.exchange << std::endl;
}

void print_quote(const std::string &symbol, const alpaca::quote &q) {
    std::lock_guard lock(print_mutex);
    std::cout << std::fixed << std::setprecision(2)
              << "QUOTE  " << std::left << std::setw(22) << symbol << std::right
              << std::setw(9) << q.bid_price << " / " << std::setw(9) << q.ask_price
              << "  (" << q.bid_size << " x " << q.ask_size << ")" << std::endl;
}

}   // namespace

int main(int argc, char **argv) {
    std::signal(SIGINT, handle_signal);

    std::vector<std::string> symbols(argv + 1, argv + argc);
    auto feed = alpaca::option_feed::indicative;
    if (!symbols.empty() && symbols.front() == "--opra") {
        feed = alpaca::option_feed::opra;
        symbols.erase(symbols.begin());
    }
    if (symbols.empty()) {
        std::cerr << "usage: stream_option_data [--opra] <OCC symbol>...\n"
                     "  e.g. stream_option_data AAPL260116C00200000\n";
        return 2;
    }

    alpaca::option_data_stream stream({}, feed);

    // Every handler must be registered before connect(): they are read without a lock on
    // the message path, so attaching one to a live stream would be a data race.
    stream.on_trade(print_trade);
    stream.on_quote(print_quote);

    stream.on_error([](const alpaca::stream_error &e) {
        std::lock_guard lock(print_mutex);
        std::cerr << "stream error " << e.code << ": " << e.message << '\n';
    });
    stream.on_transport_error([](const std::string &error) {
        std::lock_guard lock(print_mutex);
        std::cerr << "transport error: " << error << '\n';
    });
    stream.on_disconnected([] {
        std::lock_guard lock(print_mutex);
        std::cerr << "disconnected — reconnecting\n";
    });

    // Quotes must name their contracts; only trades accept the "*" wildcard.
    stream.subscribe_trades(symbols);
    stream.subscribe_quotes(symbols);

    std::cout << "Connecting to " << stream.url() << '\n';
    if (!stream.connect()) {
        std::cerr << "Could not connect. Check APCA_PAPER_API_KEY_ID / "
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
