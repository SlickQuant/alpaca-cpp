// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <functional>

#include <alpaca/environment.hpp>
#include <alpaca/streaming/stream_base.hpp>

namespace alpaca {

/// The account event stream: every order lifecycle transition, as it happens.
///
/// This is a different protocol from the market data streams even though it shares the
/// authentication frame. Messages are wrapped as `{"stream":..., "data":...}` rather than
/// tagged with a single-letter `T`, subscribing means sending a `listen` action, and one
/// object arrives per frame instead of a batch. It therefore derives from `stream_base`
/// directly rather than from the market data layer.
///
/// The `order` inside every update is the same `alpaca::order` the REST API returns, so
/// an application can hand a streamed order to code written against `trading_client`.
///
/// Defaults to paper. Handlers must be registered before `connect()` and must not block;
/// see `detail::stream_base`.
///
/// @code
///   alpaca::trade_update_stream stream;             // paper + env credentials
///
///   stream.on_trade_update([](const alpaca::trade_update &u) {
///       std::cout << alpaca::to_string(u.event) << ' ' << u.order.symbol;
///       if (u.price) { std::cout << " @ " << *u.price; }
///       std::cout << '\n';
///   });
///
///   stream.connect();
/// @endcode
class trade_update_stream : public detail::stream_base {
public:
    explicit trade_update_stream(credentials creds = {}, environment env = environment::paper);

    ~trade_update_stream() override { shutdown(); }

    void on_trade_update(std::function<void(const trade_update &)> h) {
        on_trade_update_ = std::move(h);
    }

    environment env() const noexcept { return env_; }

protected:
    void on_ready() override;
    void handle_payload(const json &payload) override;

private:
    environment env_;
    std::function<void(const trade_update &)> on_trade_update_;
};

}   // namespace alpaca
