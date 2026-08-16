// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <alpaca/common.hpp>
#include <alpaca/trading/order.hpp>
#include <alpaca/utils.hpp>

using json = nlohmann::json;

namespace alpaca {

// ---------------------------------------------------------------------------
// Control messages
//
// Every market data stream speaks the same control protocol regardless of which
// asset class it carries, so these live here rather than in a per-stream header.
// ---------------------------------------------------------------------------

/// An `{"T":"error"}` frame. Alpaca reports protocol problems in-band rather than by
/// closing the socket, so this is delivered to `on_error` instead of raising — a stream
/// that threw from its read loop would take the connection down with it.
struct stream_error {
    int code = 0;
    std::string message;    ///< `msg`
};

inline void from_json(const json &j, stream_error &e) {
    INT_FROM_JSON(j, e, code);
    VARIABLE_FROM_JSON_KEY(j, e, message, "msg");
}

/// An `{"T":"subscription"}` frame: the server's authoritative view of what this
/// connection is subscribed to, echoed after every subscribe and unsubscribe.
///
/// Alpaca sends the complete set each time, not a delta, so replacing the previous
/// value is correct and the stream uses it to reconcile after a reconnect.
struct subscriptions {
    std::vector<std::string> trades;
    std::vector<std::string> quotes;
    std::vector<std::string> bars;
    std::vector<std::string> updated_bars;      ///< `updatedBars`
    std::vector<std::string> daily_bars;        ///< `dailyBars`
    std::vector<std::string> statuses;
    std::vector<std::string> lulds;
    std::vector<std::string> imbalances;
    std::vector<std::string> corrections;
    std::vector<std::string> cancel_errors;     ///< `cancelErrors`
    std::vector<std::string> orderbooks;        ///< crypto only
    std::vector<std::string> news;              ///< news stream only

    bool empty() const noexcept {
        return trades.empty() && quotes.empty() && bars.empty() && updated_bars.empty()
            && daily_bars.empty() && statuses.empty() && lulds.empty() && imbalances.empty()
            && corrections.empty() && cancel_errors.empty() && orderbooks.empty()
            && news.empty();
    }
};

inline void from_json(const json &j, subscriptions &s) {
    VARIABLE_FROM_JSON(j, s, trades);
    VARIABLE_FROM_JSON(j, s, quotes);
    VARIABLE_FROM_JSON(j, s, bars);
    VARIABLE_FROM_JSON_KEY(j, s, updated_bars, "updatedBars");
    VARIABLE_FROM_JSON_KEY(j, s, daily_bars, "dailyBars");
    VARIABLE_FROM_JSON(j, s, statuses);
    VARIABLE_FROM_JSON(j, s, lulds);
    VARIABLE_FROM_JSON(j, s, imbalances);
    VARIABLE_FROM_JSON(j, s, corrections);
    VARIABLE_FROM_JSON_KEY(j, s, cancel_errors, "cancelErrors");
    VARIABLE_FROM_JSON(j, s, orderbooks);
    VARIABLE_FROM_JSON(j, s, news);
}

// ---------------------------------------------------------------------------
// Stock-only payloads
//
// Trades, quotes, bars, order books and news articles are *not* redefined here: the
// stream sends them with the same single-letter keys as the REST Market Data API, so
// `alpaca::trade`, `quote`, `bar`, `orderbook` and `news_article` parse them as-is.
// Only the types with no REST equivalent are declared below. The symbol arrives as the
// `S` field on every payload and is handed to handlers as a separate argument, which is
// the one shape that works for both the reused and the stream-only types.
// ---------------------------------------------------------------------------

/// `{"T":"s"}` — a trading status change (halt, resume, and the reason for it).
struct trading_status {
    uint64_t timestamp = 0;         ///< `t` — nanoseconds since the Unix epoch
    std::string status_code;        ///< `sc`
    std::string status_message;     ///< `sm`
    std::string reason_code;        ///< `rc`
    std::string reason_message;     ///< `rm`
    std::string tape;               ///< `z`
};

inline void from_json(const json &j, trading_status &s) {
    TIMESTAMP_FROM_JSON_KEY(j, s, timestamp, "t");
    VARIABLE_FROM_JSON_KEY(j, s, status_code, "sc");
    VARIABLE_FROM_JSON_KEY(j, s, status_message, "sm");
    VARIABLE_FROM_JSON_KEY(j, s, reason_code, "rc");
    VARIABLE_FROM_JSON_KEY(j, s, reason_message, "rm");
    VARIABLE_FROM_JSON_KEY(j, s, tape, "z");
}

/// `{"T":"l"}` — a limit up / limit down band update.
struct luld {
    uint64_t timestamp = 0;         ///< `t`
    double limit_up_price = 0.;     ///< `u`
    double limit_down_price = 0.;   ///< `d`
    std::string indicator;          ///< `i`
    std::string tape;               ///< `z`
};

inline void from_json(const json &j, luld &l) {
    TIMESTAMP_FROM_JSON_KEY(j, l, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, l, limit_up_price, "u");
    DOUBLE_FROM_JSON_KEY(j, l, limit_down_price, "d");
    VARIABLE_FROM_JSON_KEY(j, l, indicator, "i");
    VARIABLE_FROM_JSON_KEY(j, l, tape, "z");
}

/// `{"T":"c"}` — a trade correction, carrying both the original and corrected prints.
struct trade_correction {
    uint64_t timestamp = 0;                         ///< `t`
    std::string exchange;                           ///< `x`
    uint64_t original_id = 0;                       ///< `oi`
    double original_price = 0.;                     ///< `op`
    double original_size = 0.;                      ///< `os`
    std::vector<std::string> original_conditions;   ///< `oc`
    uint64_t corrected_id = 0;                      ///< `ci`
    double corrected_price = 0.;                    ///< `cp`
    double corrected_size = 0.;                     ///< `cs`
    std::vector<std::string> corrected_conditions;  ///< `cc`
    std::string tape;                               ///< `z`
};

inline void from_json(const json &j, trade_correction &c) {
    TIMESTAMP_FROM_JSON_KEY(j, c, timestamp, "t");
    VARIABLE_FROM_JSON_KEY(j, c, exchange, "x");
    INT_FROM_JSON_KEY(j, c, original_id, "oi");
    DOUBLE_FROM_JSON_KEY(j, c, original_price, "op");
    DOUBLE_FROM_JSON_KEY(j, c, original_size, "os");
    VARIABLE_FROM_JSON_KEY(j, c, original_conditions, "oc");
    INT_FROM_JSON_KEY(j, c, corrected_id, "ci");
    DOUBLE_FROM_JSON_KEY(j, c, corrected_price, "cp");
    DOUBLE_FROM_JSON_KEY(j, c, corrected_size, "cs");
    VARIABLE_FROM_JSON_KEY(j, c, corrected_conditions, "cc");
    VARIABLE_FROM_JSON_KEY(j, c, tape, "z");
}

/// `{"T":"x"}` — a trade cancel or error print. `action` is `C` for a cancel and `E`
/// for an error.
struct trade_cancel_error {
    uint64_t timestamp = 0;     ///< `t`
    uint64_t id = 0;            ///< `i`
    std::string exchange;       ///< `x`
    double price = 0.;          ///< `p`
    double size = 0.;           ///< `s`
    std::string action;         ///< `a`
    std::string tape;           ///< `z`
};

inline void from_json(const json &j, trade_cancel_error &c) {
    TIMESTAMP_FROM_JSON_KEY(j, c, timestamp, "t");
    INT_FROM_JSON_KEY(j, c, id, "i");
    VARIABLE_FROM_JSON_KEY(j, c, exchange, "x");
    DOUBLE_FROM_JSON_KEY(j, c, price, "p");
    DOUBLE_FROM_JSON_KEY(j, c, size, "s");
    VARIABLE_FROM_JSON_KEY(j, c, action, "a");
    VARIABLE_FROM_JSON_KEY(j, c, tape, "z");
}

/// `{"T":"i"}` — an auction imbalance.
struct imbalance {
    uint64_t timestamp = 0;     ///< `t`
    double price = 0.;          ///< `p`
    std::string tape;           ///< `z`
};

inline void from_json(const json &j, imbalance &i) {
    TIMESTAMP_FROM_JSON_KEY(j, i, timestamp, "t");
    DOUBLE_FROM_JSON_KEY(j, i, price, "p");
    VARIABLE_FROM_JSON_KEY(j, i, tape, "z");
}

// ---------------------------------------------------------------------------
// Trade updates (account stream)
// ---------------------------------------------------------------------------

/// Lifecycle event carried by a `trade_updates` message.
#define ALPACA_TRADE_UPDATE_EVENT(X, N)                     \
    X(N, new_order,              "new")                     \
    X(N, fill,                   "fill")                    \
    X(N, partial_fill,           "partial_fill")            \
    X(N, canceled,               "canceled")                \
    X(N, expired,                "expired")                 \
    X(N, done_for_day,           "done_for_day")            \
    X(N, replaced,               "replaced")                \
    X(N, rejected,               "rejected")                \
    X(N, pending_new,            "pending_new")             \
    X(N, stopped,                "stopped")                 \
    X(N, pending_cancel,         "pending_cancel")          \
    X(N, pending_replace,        "pending_replace")         \
    X(N, calculated,             "calculated")              \
    X(N, suspended,              "suspended")               \
    X(N, order_replace_rejected, "order_replace_rejected")  \
    X(N, order_cancel_rejected,  "order_cancel_rejected")
ALPACA_DEFINE_ENUM(trade_update_event, ALPACA_TRADE_UPDATE_EVENT)

/// One `trade_updates` message from the account stream.
///
/// `order` is the same `alpaca::order` the REST API returns, so an application can hand
/// a streamed order to code written against `trading_client` unchanged. The numeric
/// fields are optional because they are only present on execution events — a `new`
/// event carries no price, and a defaulted 0.0 would read as a genuine zero fill.
struct trade_update {
    trade_update_event event = trade_update_event::unknown;
    alpaca::order order;
    std::string execution_id;
    std::optional<double> price;
    std::optional<double> qty;
    std::optional<double> position_qty;
    uint64_t timestamp = 0;     ///< nanoseconds since the Unix epoch
};

inline void from_json(const json &j, trade_update &u) {
    ENUM_FROM_JSON_WITH(j, u, event, to_trade_update_event);
    STRUCT_FROM_JSON(j, u, order);
    VARIABLE_FROM_JSON(j, u, execution_id);
    OPTIONAL_DOUBLE_FROM_JSON(j, u, price);
    OPTIONAL_DOUBLE_FROM_JSON(j, u, qty);
    OPTIONAL_DOUBLE_FROM_JSON(j, u, position_qty);
    // Alpaca spells this `timestamp`; older payloads use `at`.
    TIMESTAMP_FROM_JSON(j, u, timestamp);
    if (u.timestamp == 0) {
        TIMESTAMP_FROM_JSON_KEY(j, u, timestamp, "at");
    }
}

}   // namespace alpaca
