// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <atomic>
#include <cstdint>

namespace alpaca {

/// Lock-free client-side request pacer.
///
/// Alpaca rejects traffic above the account's per-minute request budget (200/min on the
/// free plan, higher on paid plans) with HTTP 429. Rather than discovering that after the
/// fact, every request passes through this limiter first.
///
/// The implementation is a GCRA (virtual scheduling) token bucket held in a single atomic
/// word: the "theoretical arrival time" of the next request. Admission is a compare-exchange
/// loop with no mutex, so concurrent callers on many threads never serialise on a lock — they
/// only serialise on the bucket arithmetic itself.
class rate_limiter {
public:
    /// @param requests_per_minute sustained budget; 0 disables limiting entirely.
    /// @param burst maximum number of requests admitted back-to-back. Defaults to the
    ///        full per-minute budget, matching how Alpaca meters traffic.
    explicit rate_limiter(uint32_t requests_per_minute = 200, uint32_t burst = 0) noexcept;

    rate_limiter(const rate_limiter &other) noexcept;
    rate_limiter& operator=(const rate_limiter &other) noexcept;

    /// Reserves a slot, sleeping the calling thread if the budget is exhausted.
    void acquire();

    /// Reserves a slot only if one is available right now. Never sleeps.
    bool try_acquire() noexcept;

    /// Nanoseconds the caller would have to wait for a slot, without reserving one.
    uint64_t wait_time_ns() const noexcept;

    void set_rate(uint32_t requests_per_minute, uint32_t burst = 0) noexcept;
    uint32_t rate() const noexcept { return rate_.load(std::memory_order_relaxed); }

    bool enabled() const noexcept { return rate_.load(std::memory_order_relaxed) != 0; }

    /// Records a 429 from the server: every subsequent acquire() waits out `retry_after_seconds`.
    /// Called by request_context when Alpaca sends a Retry-After header.
    void notify_rate_limited(uint32_t retry_after_seconds) noexcept;

    /// Monotonic clock used by the limiter, in nanoseconds. Exposed for tests.
    static uint64_t now_ns() noexcept;

private:
    /// Nanoseconds between two sustained requests.
    uint64_t interval_ns() const noexcept { return interval_ns_.load(std::memory_order_relaxed); }
    /// How far ahead of `now` the theoretical arrival time may run before admission blocks.
    uint64_t burst_ns() const noexcept { return burst_ns_.load(std::memory_order_relaxed); }

    std::atomic_uint32_t rate_;
    std::atomic_uint64_t interval_ns_;
    std::atomic_uint64_t burst_ns_;
    /// Theoretical arrival time of the next admitted request.
    std::atomic_uint64_t tat_;
    /// Hard barrier set by a server-side 429.
    std::atomic_uint64_t blocked_until_;
};

}   // namespace alpaca
