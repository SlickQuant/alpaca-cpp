// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <alpaca/streaming/stream_base.hpp>

namespace alpaca::detail {

/// Returns the `S` field of a message without copying it, or an empty string when the
/// message carries no symbol. The reference is valid for the duration of the callback.
const std::string& symbol_of(const json &message);

/// Parses `message` into `T` and hands it to `handler` along with its symbol.
///
/// The early return matters: with no handler attached there is no reason to pay for the
/// parse, and on a wildcard subscription that is the difference between ignoring a
/// channel and deserialising every message on it.
template <typename T, typename Handler>
void deliver(const Handler &handler, const json &message) {
    if (!handler) {
        return;
    }
    T value;
    from_json(message, value);
    handler(symbol_of(message), value);
}

/// Everything the stock, crypto and news streams share.
///
/// All three speak one control protocol — `{"T":"success"}` to acknowledge the connect
/// and the auth, `{"T":"error"}` for problems, `{"T":"subscription"}` to echo the
/// server's view of the subscription set — and one subscribe format that differs only in
/// which channel names are legal. That is all handled here; the asset-class streams
/// implement `dispatch` and expose typed `subscribe_*` wrappers.
class data_stream_base : public stream_base {
public:
    /// Fires on every `{"T":"subscription"}` frame with the server's full current set.
    void on_subscription(std::function<void(const subscriptions &)> h) {
        on_subscription_ = std::move(h);
    }

    /// The subscription set this stream will replay on its next reconnect. This is the
    /// client's intent, which is authoritative for reconnect; `on_subscription` reports
    /// what the server actually accepted.
    std::map<std::string, std::set<std::string>, std::less<>> pending_channels() const;

protected:
    data_stream_base(std::string url, credentials creds)
        : stream_base(std::move(url), std::move(creds))
    {}

    /// Adds `symbols` to `channel` and sends a subscribe frame when the stream is live.
    /// Subscribing before `connect()` is legal and is the usual pattern — the set is
    /// replayed as soon as the stream authenticates.
    void subscribe(std::string_view channel, const std::vector<std::string> &symbols);
    void unsubscribe(std::string_view channel, const std::vector<std::string> &symbols);

    void on_ready() override;
    void handle_payload(const json &payload) override;

    /// Dispatches one data message. `type` is the `T` value with the control types
    /// already handled.
    virtual void dispatch(std::string_view type, const json &message) = 0;

private:
    /// Builds `{"action":<action>,"<channel>":[...]}` for the whole current set.
    std::string build_frame(std::string_view action) const;

    mutable std::mutex subs_mutex_;
    std::map<std::string, std::set<std::string>, std::less<>> channels_;
    std::function<void(const subscriptions &)> on_subscription_;
};

}   // namespace alpaca::detail
