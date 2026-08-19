// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <nlohmann/json.hpp>

#include <alpaca/auth.hpp>
#include <alpaca/streaming/stream_message.hpp>


namespace alpaca::detail {

using json = nlohmann::json;

/// Where a stream is in its connect / authenticate / ready lifecycle.
enum class stream_state : uint8_t {
    disconnected,
    connecting,     ///< socket opening
    authenticating, ///< socket up, auth sent, waiting for the acknowledgement
    ready,          ///< authenticated; subscriptions are live
};

/// How a stream behaves when the socket drops.
///
/// Alpaca closes idle or overloaded connections routinely, so reconnecting is the normal
/// case rather than an error path. On reconnect the stream re-authenticates and replays
/// its subscriptions, so a caller that set handlers once keeps receiving data without
/// having to notice the gap.
struct reconnect_policy {
    bool enabled = true;
    uint32_t initial_backoff_ms = 500;      ///< doubled after each failed attempt
    uint32_t max_backoff_ms = 30000;
    uint32_t max_attempts = 0;              ///< 0 means keep trying indefinitely
};

/// Shared websocket plumbing for every Alpaca stream.
///
/// Owns the socket, the authentication handshake, the reconnect loop and the JSON
/// framing. Derived classes supply only the two things that actually differ between the
/// account stream and the market data streams: how to recognise the authentication
/// acknowledgement, and how to dispatch a payload to typed handlers.
///
/// **Threading.** Handlers are invoked directly on the websocket service thread, with no
/// intermediate queue — that is what keeps the message path allocation- and lock-free.
/// Two consequences the caller has to respect:
///   * Register handlers *before* calling `connect()`. They are read without a lock on
///     the hot path, so mutating them on a live connection is a data race.
///   * Do not block inside a handler. Anything slow belongs on your own thread; a handler
///     that blocks stalls the socket and Alpaca will eventually disconnect it.
///
/// Subscription bookkeeping does take a lock, but only on `subscribe`/`unsubscribe` and
/// on reconnect — never on the inbound message path.
class stream_base {
public:
    stream_base(std::string url, credentials creds);
    virtual ~stream_base();

    // The callbacks handed to the socket capture `this`, so the object cannot move.
    stream_base(const stream_base &) = delete;
    stream_base& operator=(const stream_base &) = delete;

    /// Transport came up and authenticated. Fires again after every reconnect.
    void on_connected(std::function<void()> h) { on_connected_ = std::move(h); }
    /// Socket dropped. A reconnect may already be scheduled — see `reconnect_policy`.
    void on_disconnected(std::function<void()> h) { on_disconnected_ = std::move(h); }
    /// An `{"T":"error"}` frame from Alpaca (bad credentials, symbol limit, and so on).
    void on_error(std::function<void(const stream_error &)> h) { on_error_ = std::move(h); }
    /// A transport-level failure reported by the socket itself.
    void on_transport_error(std::function<void(const std::string &)> h) {
        on_transport_error_ = std::move(h);
    }

    /// Opens the socket and blocks until authenticated, or until `timeout` elapses.
    /// Returns false on timeout; the reconnect loop keeps trying if it is enabled.
    bool connect(std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /// Closes the socket and cancels any pending reconnect. Idempotent.
    void disconnect();

    stream_state state() const noexcept { return state_.load(std::memory_order_acquire); }
    bool is_ready() const noexcept { return state() == stream_state::ready; }
    std::string_view url() const noexcept { return url_; }

    void set_reconnect_policy(const reconnect_policy &policy);

protected:
    /// Sends one text frame. Safe to call from a handler.
    void send(const std::string &payload);

    /// Called on the service thread once the stream is authenticated, and again after
    /// every reconnect. Derived classes replay their subscriptions here.
    virtual void on_ready() = 0;

    /// One inbound JSON value. Array frames are split, so this always receives a single
    /// message object. Derived classes recognise their authentication acknowledgement
    /// here and call `mark_ready()`.
    virtual void handle_payload(const json &payload) = 0;

    /// Promotes the stream to `ready`, unblocking `connect()` and triggering `on_ready()`.
    void mark_ready();

    /// Routes an Alpaca protocol error to the caller's handler.
    void report_error(const stream_error &error);

    /// Closes the socket and joins the reconnect thread without touching any virtual.
    ///
    /// Every derived destructor **must** call this first. Once a derived destructor has
    /// run, its overrides are gone, and an in-flight callback reaching `handle_payload`
    /// would land on a half-destroyed object.
    void shutdown() noexcept;

    const credentials& creds() const noexcept { return creds_; }

private:
    void handle_socket_connected();
    void handle_socket_disconnected();
    void handle_socket_data(const char *data, std::size_t len);
    void handle_socket_error(std::string error);
    void reconnect_loop();
    void schedule_reconnect();

    struct socket_holder;   ///< keeps Boost.Beast out of this header

    std::string url_;
    credentials creds_;
    std::unique_ptr<socket_holder> socket_;

    std::atomic<stream_state> state_{stream_state::disconnected};

    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
    std::function<void(const stream_error &)> on_error_;
    std::function<void(const std::string &)> on_transport_error_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    reconnect_policy reconnect_{};
    uint32_t reconnect_attempt_ = 0;
    bool reconnect_pending_ = false;
    /// An error frame arrived before the stream authenticated, so the handshake is
    /// never going to complete and `connect()` should stop waiting for it.
    bool auth_failed_ = false;
    bool stopping_ = false;
    bool closing_ = false;      ///< a caller asked to disconnect; suppress reconnect
    std::thread reconnect_thread_;
};

}   // namespace alpaca::detail
