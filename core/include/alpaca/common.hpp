// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace alpaca {

// ---------------------------------------------------------------------------
// Enum plumbing
//
// Every Alpaca enum needs the same three things: the enumerators, a wire-format
// `to_string`, and a `to_<enum>` parser. Writing those by hand is where the sibling
// SDKs accumulated hundreds of lines of near-identical switch statements, so each
// enum here is declared once as an X-macro list and the three pieces are generated.
//
// Every enum carries an `unknown` enumerator, which is what an unrecognised wire
// value parses to — Alpaca adds enumerators over time and an SDK that throws on a
// new status is worse than one that reports `unknown`.
// ---------------------------------------------------------------------------

#define ALPACA_ENUM_ENUMERATOR(name, id, str)   id,
#define ALPACA_ENUM_TO_STRING(name, id, str)    case name::id: return str;
#define ALPACA_ENUM_FROM_STRING(name, id, str)  if (s == str) return name::id;

#define ALPACA_DEFINE_ENUM(name, XLIST)                                                 \
    enum class name : uint8_t {                                                         \
        unknown,                                                                        \
        XLIST(ALPACA_ENUM_ENUMERATOR, name)                                             \
    };                                                                                  \
    inline std::string_view to_string_view(name value) noexcept {                       \
        switch (value) {                                                                \
            XLIST(ALPACA_ENUM_TO_STRING, name)                                          \
            case name::unknown: break;                                                  \
        }                                                                               \
        return "";                                                                      \
    }                                                                                   \
    inline std::string to_string(name value) { return std::string(to_string_view(value)); } \
    inline name to_##name(std::string_view s) noexcept {                                \
        XLIST(ALPACA_ENUM_FROM_STRING, name)                                            \
        return name::unknown;                                                           \
    }

// ---------------------------------------------------------------------------
// Shared enums
// ---------------------------------------------------------------------------

#define ALPACA_SORT_DIRECTION(X, N) \
    X(N, asc,  "asc")               \
    X(N, desc, "desc")
ALPACA_DEFINE_ENUM(sort_direction, ALPACA_SORT_DIRECTION)

#define ALPACA_ASSET_CLASS(X, N)        \
    X(N, us_equity, "us_equity")        \
    X(N, us_option, "us_option")        \
    X(N, crypto,    "crypto")
ALPACA_DEFINE_ENUM(asset_class, ALPACA_ASSET_CLASS)

#define ALPACA_ASSET_STATUS(X, N)   \
    X(N, active,   "active")        \
    X(N, inactive, "inactive")
ALPACA_DEFINE_ENUM(asset_status, ALPACA_ASSET_STATUS)

#define ALPACA_ASSET_EXCHANGE(X, N) \
    X(N, amex,     "AMEX")          \
    X(N, arca,     "ARCA")          \
    X(N, bats,     "BATS")          \
    X(N, nyse,     "NYSE")          \
    X(N, nasdaq,   "NASDAQ")        \
    X(N, nysearca, "NYSEARCA")      \
    X(N, otc,      "OTC")           \
    X(N, crypto,   "CRYPTO")        \
    X(N, empty,    "")
ALPACA_DEFINE_ENUM(asset_exchange, ALPACA_ASSET_EXCHANGE)

#define ALPACA_ORDER_SIDE(X, N) \
    X(N, buy,  "buy")           \
    X(N, sell, "sell")
ALPACA_DEFINE_ENUM(order_side, ALPACA_ORDER_SIDE)

#define ALPACA_ORDER_TYPE(X, N)             \
    X(N, market,        "market")           \
    X(N, limit,         "limit")            \
    X(N, stop,          "stop")             \
    X(N, stop_limit,    "stop_limit")       \
    X(N, trailing_stop, "trailing_stop")
ALPACA_DEFINE_ENUM(order_type, ALPACA_ORDER_TYPE)

#define ALPACA_TIME_IN_FORCE(X, N)  \
    X(N, day, "day")                \
    X(N, gtc, "gtc")                \
    X(N, opg, "opg")                \
    X(N, cls, "cls")                \
    X(N, ioc, "ioc")                \
    X(N, fok, "fok")
ALPACA_DEFINE_ENUM(time_in_force, ALPACA_TIME_IN_FORCE)

#define ALPACA_ORDER_CLASS(X, N)    \
    X(N, simple,  "simple")         \
    X(N, bracket, "bracket")        \
    X(N, oco,     "oco")            \
    X(N, oto,     "oto")            \
    X(N, mleg,    "mleg")
ALPACA_DEFINE_ENUM(order_class, ALPACA_ORDER_CLASS)

// `new` and `held` follow the wire spelling; `new_` is renamed only because `new` is a keyword.
#define ALPACA_ORDER_STATUS(X, N)                       \
    X(N, new_,                 "new")                   \
    X(N, partially_filled,     "partially_filled")      \
    X(N, filled,               "filled")                \
    X(N, done_for_day,         "done_for_day")          \
    X(N, canceled,             "canceled")              \
    X(N, expired,              "expired")               \
    X(N, replaced,             "replaced")              \
    X(N, pending_cancel,       "pending_cancel")        \
    X(N, pending_replace,      "pending_replace")       \
    X(N, pending_review,       "pending_review")        \
    X(N, accepted,             "accepted")              \
    X(N, pending_new,          "pending_new")           \
    X(N, accepted_for_bidding, "accepted_for_bidding")  \
    X(N, stopped,              "stopped")               \
    X(N, rejected,             "rejected")              \
    X(N, suspended,            "suspended")             \
    X(N, calculated,           "calculated")            \
    X(N, held,                 "held")
ALPACA_DEFINE_ENUM(order_status, ALPACA_ORDER_STATUS)

/// Which orders `list_orders` returns.
#define ALPACA_ORDER_STATUS_FILTER(X, N)    \
    X(N, open,   "open")                    \
    X(N, closed, "closed")                  \
    X(N, all,    "all")
ALPACA_DEFINE_ENUM(order_status_filter, ALPACA_ORDER_STATUS_FILTER)

// `long`/`short` are keywords, hence the trailing underscore.
#define ALPACA_POSITION_SIDE(X, N)  \
    X(N, long_,  "long")            \
    X(N, short_, "short")
ALPACA_DEFINE_ENUM(position_side, ALPACA_POSITION_SIDE)

#define ALPACA_POSITION_INTENT(X, N)        \
    X(N, buy_to_open,   "buy_to_open")      \
    X(N, buy_to_close,  "buy_to_close")     \
    X(N, sell_to_open,  "sell_to_open")     \
    X(N, sell_to_close, "sell_to_close")
ALPACA_DEFINE_ENUM(position_intent, ALPACA_POSITION_INTENT)

#define ALPACA_CONTRACT_TYPE(X, N)  \
    X(N, call, "call")              \
    X(N, put,  "put")
ALPACA_DEFINE_ENUM(contract_type, ALPACA_CONTRACT_TYPE)

#define ALPACA_CONTRACT_STYLE(X, N)     \
    X(N, american, "american")          \
    X(N, european, "european")
ALPACA_DEFINE_ENUM(contract_style, ALPACA_CONTRACT_STYLE)

/// Market data source. `iex` is the only feed available without a data subscription.
#define ALPACA_DATA_FEED(X, N)              \
    X(N, iex,         "iex")                \
    X(N, sip,         "sip")                \
    X(N, delayed_sip, "delayed_sip")        \
    X(N, boats,       "boats")              \
    X(N, overnight,   "overnight")          \
    X(N, otc,         "otc")
ALPACA_DEFINE_ENUM(data_feed, ALPACA_DATA_FEED)

/// Options data source.
#define ALPACA_OPTION_FEED(X, N)        \
    X(N, opra,       "opra")            \
    X(N, indicative, "indicative")
ALPACA_DEFINE_ENUM(option_feed, ALPACA_OPTION_FEED)

/// Crypto venue location. `us_1` and `eu_1` are Kraken-backed.
#define ALPACA_CRYPTO_LOCATION(X, N)    \
    X(N, us,   "us")                    \
    X(N, us_1, "us-1")                  \
    X(N, eu_1, "eu-1")
ALPACA_DEFINE_ENUM(crypto_location, ALPACA_CRYPTO_LOCATION)

/// Corporate-action price adjustment applied to historical bars.
#define ALPACA_ADJUSTMENT(X, N)     \
    X(N, raw,      "raw")           \
    X(N, split,    "split")         \
    X(N, dividend, "dividend")      \
    X(N, all,      "all")
ALPACA_DEFINE_ENUM(adjustment, ALPACA_ADJUSTMENT)

/// Account activity type. `fill` and `partial_fill` are trade activities; the rest are
/// non-trade activities with a different payload shape.
#define ALPACA_ACTIVITY_TYPE(X, N)          \
    X(N, fill,         "FILL")              \
    X(N, trans,        "TRANS")             \
    X(N, misc,         "MISC")              \
    X(N, acatc,        "ACATC")             \
    X(N, acats,        "ACATS")             \
    X(N, cfee,         "CFEE")              \
    X(N, csd,          "CSD")               \
    X(N, csw,          "CSW")               \
    X(N, div,          "DIV")               \
    X(N, divcgl,       "DIVCGL")            \
    X(N, divcgs,       "DIVCGS")            \
    X(N, divfee,       "DIVFEE")            \
    X(N, divft,        "DIVFT")             \
    X(N, divnra,       "DIVNRA")            \
    X(N, divroc,       "DIVROC")            \
    X(N, divtw,        "DIVTW")             \
    X(N, divtxex,      "DIVTXEX")           \
    X(N, fee,          "FEE")               \
    X(N, int_,         "INT")               \
    X(N, intnra,       "INTNRA")            \
    X(N, inttw,        "INTTW")             \
    X(N, jnl,          "JNL")               \
    X(N, jnlc,         "JNLC")              \
    X(N, jnls,         "JNLS")              \
    X(N, ma,           "MA")                \
    X(N, nc,           "NC")                \
    X(N, opasn,        "OPASN")             \
    X(N, opcsh,        "OPCSH")             \
    X(N, opexc,        "OPEXC")             \
    X(N, opexp,        "OPEXP")             \
    X(N, optrd,        "OPTRD")             \
    X(N, ptc,          "PTC")               \
    X(N, ptr,          "PTR")               \
    X(N, reorg,        "REORG")             \
    X(N, sc,           "SC")                \
    X(N, sso,          "SSO")               \
    X(N, ssp,          "SSP")               \
    X(N, swp,          "SWP")               \
    X(N, voidedtrade,  "VOIDEDTRADE")       \
    X(N, wht,          "WHT")
ALPACA_DEFINE_ENUM(activity_type, ALPACA_ACTIVITY_TYPE)

/// Direction an activity list is paged in.
#define ALPACA_ACTIVITY_DIRECTION(X, N) \
    X(N, asc,  "asc")                   \
    X(N, desc, "desc")
ALPACA_DEFINE_ENUM(activity_direction, ALPACA_ACTIVITY_DIRECTION)

/// Options trading approval level on an account.
#define ALPACA_OPTIONS_APPROVED_LEVEL(X, N) \
    X(N, disabled, "0")                     \
    X(N, level_1,  "1")                     \
    X(N, level_2,  "2")                     \
    X(N, level_3,  "3")
ALPACA_DEFINE_ENUM(options_approved_level, ALPACA_OPTIONS_APPROVED_LEVEL)

/// Day-trade buying power multiplier / margin tier reported on the account.
#define ALPACA_ACCOUNT_STATUS(X, N)                     \
    X(N, onboarding,        "ONBOARDING")               \
    X(N, submission_failed, "SUBMISSION_FAILED")        \
    X(N, submitted,         "SUBMITTED")                \
    X(N, account_updated,   "ACCOUNT_UPDATED")          \
    X(N, approval_pending,  "APPROVAL_PENDING")         \
    X(N, active,            "ACTIVE")                   \
    X(N, rejected,          "REJECTED")                 \
    X(N, disabled,          "DISABLED")                 \
    X(N, disable_pending,   "DISABLE_PENDING")          \
    X(N, account_closed,    "ACCOUNT_CLOSED")           \
    X(N, paper_only,        "PAPER_ONLY")
ALPACA_DEFINE_ENUM(account_status, ALPACA_ACCOUNT_STATUS)

/// How day-trade margin calls are handled, from account configurations.
#define ALPACA_DTBP_CHECK(X, N)         \
    X(N, both,     "both")              \
    X(N, entry,    "entry")             \
    X(N, exit,     "exit")
ALPACA_DEFINE_ENUM(dtbp_check, ALPACA_DTBP_CHECK)

#define ALPACA_TRADE_CONFIRM_EMAIL(X, N)    \
    X(N, all,  "all")                       \
    X(N, none, "none")
ALPACA_DEFINE_ENUM(trade_confirm_email, ALPACA_TRADE_CONFIRM_EMAIL)

}   // namespace alpaca
