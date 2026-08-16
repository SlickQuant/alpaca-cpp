// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/streaming/stream_base.hpp>

#include <algorithm>
#include <utility>

#include <boost/beast/core/flat_buffer.hpp>
#include <slick/net/websocket.hpp>

namespace alpaca::detail {

/// The socket lives here rather than in the header so Boost.Beast stays out of every
/// translation unit that merely wants to receive quotes.
struct stream_base::socket_holder {
    slick::net::Websocket<boost::beast::flat_buffer> ws;

    socket_holder(std::string url,
                  std::function<void()> &&on_connected,
                  std::function<void()> &&on_disconnected,
                  std::function<void(const char *, std::size_t)> &&on_data,
                  std::function<void(std::string &&)> &&on_error)
        : ws(std::move(url), std::move(on_connected), std::move(on_disconnected),
             std::move(on_data), std::move(on_error))
    {}
};

stream_base::stream_base(std::string url, credentials creds)
    : url_(std::move(url))
    , creds_(std::move(creds))
{
    socket_ = std::make_unique<socket_holder>(
        url_,
        [this] { handle_socket_connected(); },
        [this] { handle_socket_disconnected(); },
        [this](const char *data, std::size_t len) { handle_socket_data(data, len); },
        [this](std::string &&err) { handle_socket_error(std::move(err)); });
}

stream_base::~stream_base() {
    // Backstop only. A derived class that skipped shutdown() has already destroyed its
    // overrides by the time we get here, so this cannot save it — see the declaration.
    shutdown();
}

void stream_base::shutdown() noexcept {
    try {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
            closing_ = true;
        }
        cv_.notify_all();

        if (reconnect_thread_.joinable()) {
            reconnect_thread_.join();
        }
        if (socket_) {
            socket_->ws.close();
        }
        state_.store(stream_state::disconnected, std::memory_order_release);
    }
    catch (...) {
        // Destructor path: nothing useful to do with an exception here.
    }
}

void stream_base::set_reconnect_policy(const reconnect_policy &policy) {
    std::lock_guard lock(mutex_);
    reconnect_ = policy;
}

bool stream_base::connect(std::chrono::milliseconds timeout) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return false;
        }
        closing_ = false;
        reconnect_attempt_ = 0;
        auth_failed_ = false;
        if (!reconnect_thread_.joinable()) {
            reconnect_thread_ = std::thread([this] { reconnect_loop(); });
        }
    }

    state_.store(stream_state::connecting, std::memory_order_release);
    socket_->ws.open();

    std::unique_lock lock(mutex_);
    cv_.wait_for(lock, timeout, [this] {
        return stopping_ || auth_failed_
            || state_.load(std::memory_order_acquire) == stream_state::ready;
    });
    return state_.load(std::memory_order_acquire) == stream_state::ready;
}

void stream_base::disconnect() {
    {
        std::lock_guard lock(mutex_);
        closing_ = true;
        reconnect_pending_ = false;
    }
    cv_.notify_all();

    if (socket_) {
        socket_->ws.close();
    }
    state_.store(stream_state::disconnected, std::memory_order_release);
}

void stream_base::send(const std::string &payload) {
    if (socket_) {
        socket_->ws.send(payload.data(), payload.size());
    }
}

void stream_base::handle_socket_connected() {
    // The socket is up but the session is not usable until Alpaca acknowledges the
    // credentials, so the state stays below `ready` until the derived class says so.
    state_.store(stream_state::authenticating, std::memory_order_release);
    send(creds_.stream_auth_message());
}

void stream_base::handle_socket_disconnected() {
    const auto previous = state_.exchange(stream_state::disconnected, std::memory_order_acq_rel);

    if (previous != stream_state::disconnected && on_disconnected_) {
        on_disconnected_();
    }

    schedule_reconnect();

    // Deliberately no notify here. A connect() in progress keeps waiting out its
    // timeout rather than failing on the first dropped socket, because the reconnect
    // just scheduled above may well authenticate inside the remaining budget — and a
    // connect that succeeds on the second attempt is a success, not a failure.
}

void stream_base::handle_socket_data(const char *data, std::size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }

    json parsed;
    try {
        parsed = json::parse(data, data + len);
    }
    catch (const std::exception &e) {
        report_error({0, std::string("malformed stream frame: ") + e.what()});
        return;
    }

    // The market data streams batch messages into one array per frame; the account
    // stream sends a bare object. Both funnel into the same per-message handler.
    if (parsed.is_array()) {
        for (const auto &message : parsed) {
            handle_payload(message);
        }
    }
    else {
        handle_payload(parsed);
    }
}

void stream_base::handle_socket_error(std::string error) {
    if (on_transport_error_) {
        on_transport_error_(error);
    }
}

void stream_base::mark_ready() {
    state_.store(stream_state::ready, std::memory_order_release);
    {
        std::lock_guard lock(mutex_);
        reconnect_attempt_ = 0;
    }
    cv_.notify_all();

    // Replay subscriptions before telling the caller we are up, so that by the time
    // on_connected fires the stream is genuinely carrying the data it carried before.
    on_ready();

    if (on_connected_) {
        on_connected_();
    }
}

void stream_base::report_error(const stream_error &error) {
    // An error that lands before the handshake completes means the handshake is not
    // going to complete — bad key, malformed auth, or a refused account. Release
    // connect() now instead of making it sit out a timeout that cannot change the
    // outcome. Errors after `ready` are ordinary in-band problems and do not stop
    // anything.
    if (state() != stream_state::ready) {
        {
            std::lock_guard lock(mutex_);
            auth_failed_ = true;
        }
        cv_.notify_all();
    }

    if (on_error_) {
        on_error_(error);
    }
}

void stream_base::schedule_reconnect() {
    std::lock_guard lock(mutex_);
    if (stopping_ || closing_ || !reconnect_.enabled) {
        return;
    }
    if (reconnect_.max_attempts != 0 && reconnect_attempt_ >= reconnect_.max_attempts) {
        return;
    }
    reconnect_pending_ = true;
    cv_.notify_all();
}

void stream_base::reconnect_loop() {
    std::unique_lock lock(mutex_);
    for (;;) {
        cv_.wait(lock, [this] { return stopping_ || reconnect_pending_; });
        if (stopping_) {
            return;
        }

        reconnect_pending_ = false;

        // Exponential backoff, capped. Doubling from the attempt count rather than a
        // running value keeps a successful connect (which zeroes it) authoritative.
        const uint64_t shift = std::min<uint64_t>(reconnect_attempt_, 20);
        const uint64_t backoff = std::min<uint64_t>(
            static_cast<uint64_t>(reconnect_.initial_backoff_ms) << shift,
            reconnect_.max_backoff_ms);
        ++reconnect_attempt_;

        // Wakes early if someone disconnects or destroys the stream mid-backoff.
        if (cv_.wait_for(lock, std::chrono::milliseconds(backoff),
                         [this] { return stopping_ || closing_; })) {
            if (stopping_) {
                return;
            }
            continue;
        }

        lock.unlock();
        state_.store(stream_state::connecting, std::memory_order_release);
        socket_->ws.open();
        lock.lock();
    }
}

}   // namespace alpaca::detail
