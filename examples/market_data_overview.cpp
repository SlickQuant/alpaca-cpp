// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Read-only tour of the Market Data API: a stock snapshot, a bounded window of daily bars,
// a crypto order book and recent news. Every request here works on a free account — stocks
// are pinned to the IEX feed, and crypto and news need no data subscription at all.
//
// data.alpaca.markets is not account-scoped, so either key pair works:
//
//   set APCA_PAPER_API_KEY_ID=PK...
//   set APCA_PAPER_API_SECRET_KEY=...
//   market_data_overview [SYMBOL]

#include <algorithm>
#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <alpaca/data_client.hpp>
#include <alpaca/error.hpp>
#include <alpaca/utils.hpp>

namespace {

/// A YYYY-MM-DD date `n` days back, which is all the historical endpoints need for a bound.
std::string days_ago(int n) {
    const auto day = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())
                   - std::chrono::days(n);
    return std::format("{:%Y-%m-%d}", std::chrono::year_month_day(day));
}

void print_snapshot(const std::string &symbol, const alpaca::snapshot &snap) {
    const auto &q = snap.latest_quote;

    std::cout << "Snapshot " << symbol << '\n'
              << std::fixed << std::setprecision(2)
              << "  last trade   " << snap.latest_trade.price
              << " x " << snap.latest_trade.size
              << "  at " << alpaca::to_rfc3339(snap.latest_trade.timestamp) << '\n'
              << "  quote        " << q.bid_price << " / " << q.ask_price;

    // Outside regular hours IEX often quotes one side only, and mid/spread are meaningless
    // on a one-sided book — reporting them would be worse than reporting nothing.
    if (q.bid_price > 0. && q.ask_price > 0.) {
        std::cout << "  (mid " << q.mid_price() << ", spread " << q.spread() << ")";
    }
    else {
        std::cout << "  (one-sided)";
    }

    std::cout << '\n'
              << "  daily bar    O " << snap.daily_bar.open
              << "  H " << snap.daily_bar.high
              << "  L " << snap.daily_bar.low
              << "  C " << snap.daily_bar.close
              << "  V " << snap.daily_bar.volume << '\n'
              << "  prev close   " << snap.prev_daily_bar.close << '\n';
}

void print_bars(const std::string &symbol, const std::vector<alpaca::bar> &bars) {
    std::cout << "\nDaily bars for " << symbol << " (" << bars.size() << ")\n";
    for (const auto &b : bars) {
        std::cout << "  " << alpaca::to_date_string(b.timestamp)
                  << std::fixed << std::setprecision(2)
                  << "  O " << std::setw(9) << b.open
                  << "  H " << std::setw(9) << b.high
                  << "  L " << std::setw(9) << b.low
                  << "  C " << std::setw(9) << b.close
                  << "  vwap " << std::setw(9) << b.vwap
                  << "  trades " << b.trade_count << '\n';
    }
}

void print_orderbook(const std::string &symbol, const alpaca::orderbook &book) {
    std::cout << "\nCrypto book " << symbol
              << std::fixed << std::setprecision(2)
              << "  best " << book.best_bid() << " / " << book.best_ask()
              << "  (spread " << book.spread() << ")\n";

    const size_t depth = std::min<size_t>(5, std::max(book.bids.size(), book.asks.size()));
    for (size_t i = 0; i < depth; ++i) {
        std::cout << "  ";
        if (i < book.bids.size()) {
            std::cout << std::setw(12) << book.bids[i].size << " @ " << std::setw(12) << book.bids[i].price;
        }
        else {
            std::cout << std::setw(27) << ' ';
        }
        std::cout << "   |   ";
        if (i < book.asks.size()) {
            std::cout << std::setw(12) << book.asks[i].price << " x " << std::setw(12) << book.asks[i].size;
        }
        std::cout << '\n';
    }
}

void print_news(const std::vector<alpaca::news_article> &articles) {
    std::cout << "\nNews (" << articles.size() << ")\n";
    for (const auto &a : articles) {
        std::cout << "  " << alpaca::to_rfc3339(a.created_at).substr(0, 16)
                  << "  [" << a.source << "] " << a.headline << '\n';
    }
}

}   // namespace

int main(int argc, char **argv) {
    const std::string symbol = argc > 1 ? argv[1] : "AAPL";

    try {
        // Credentials come from the environment, exactly as for the trading client.
        const alpaca::data_client data;
        std::cout << "Connected to " << data.base_url() << "\n\n";

        print_snapshot(symbol, data.get_stock_snapshot(symbol, alpaca::data_feed::iex));

        // A bounded window, so the fetch-everything method is the right one — it follows
        // next_page_token to completion rather than silently returning the first page.
        alpaca::bar_query bars;
        bars.symbols   = {symbol};
        bars.timeframe = alpaca::timeframes::one_day;
        bars.start     = days_ago(10);
        bars.end       = days_ago(1);
        bars.feed      = alpaca::data_feed::iex;

        auto history = data.get_stock_bars(bars);
        print_bars(symbol, history[symbol]);

        // Crypto needs no subscription, and the venue is a path segment rather than a feed.
        const std::string pair = "BTC/USD";
        auto books = data.get_latest_crypto_orderbooks({pair});
        if (auto it = books.find(pair); it != books.end()) {
            print_orderbook(pair, it->second);
        }

        // Open-ended window, so this is the `_page` variant's job. `get_news` would follow
        // next_page_token through the entire archive for the symbol, five articles a page.
        alpaca::news_query news;
        news.symbols = {symbol};
        news.limit   = 5;
        news.sort    = alpaca::sort_direction::desc;
        print_news(data.get_news_page(news).news);

        return 0;
    }
    catch (const alpaca::api_error &e) {
        std::cerr << "Alpaca rejected the request: HTTP " << e.http_status
                  << " (code " << e.code << ") " << e.message << '\n';
        if (e.is_unauthorized()) {
            std::cerr << "Check APCA_PAPER_API_KEY_ID / APCA_PAPER_API_SECRET_KEY (or the "
                         "unprefixed pair). Either paper or live keys work here — "
                         "data.alpaca.markets is not account-scoped.\n";
        }
        else if (e.http_status == 403) {
            std::cerr << "That endpoint needs a data subscription. The IEX feed, crypto and "
                         "news are the parts available on a free account.\n";
        }
        return 1;
    }
    catch (const std::exception &e) {
        std::cerr << "Failed: " << e.what() << '\n';
        return 1;
    }
}
