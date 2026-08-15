// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#include <alpaca/rate_limiter.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

namespace alpaca {

namespace {

constexpr uint64_t nanos_per_minute = 60'000'000'000ull;

uint64_t compute_interval(uint32_t requests_per_minute) noexcept {
    return requests_per_minute == 0 ? 0 : nanos_per_minute / requests_per_minute;
}

}   // namespace

uint64_t rate_limiter::now_ns() noexcept {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

rate_limiter::rate_limiter(uint32_t requests_per_minute, uint32_t burst) noexcept
    : rate_(requests_per_minute)
    , interval_ns_(compute_interval(requests_per_minute))
    , burst_ns_(0)
    , tat_(0)
    , blocked_until_(0)
{
    const uint32_t effective_burst = burst != 0 ? burst : requests_per_minute;
    burst_ns_.store(compute_interval(requests_per_minute) * effective_burst, std::memory_order_relaxed);
}

rate_limiter::rate_limiter(const rate_limiter &other) noexcept
    : rate_(other.rate_.load(std::memory_order_relaxed))
    , interval_ns_(other.interval_ns_.load(std::memory_order_relaxed))
    , burst_ns_(other.burst_ns_.load(std::memory_order_relaxed))
    , tat_(0)                                   // a copied limiter starts with a fresh budget
    , blocked_until_(other.blocked_until_.load(std::memory_order_relaxed))
{}

rate_limiter& rate_limiter::operator=(const rate_limiter &other) noexcept {
    if (this != &other) {
        rate_.store(other.rate_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        interval_ns_.store(other.interval_ns_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        burst_ns_.store(other.burst_ns_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        tat_.store(0, std::memory_order_relaxed);
        blocked_until_.store(other.blocked_until_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    return *this;
}

void rate_limiter::set_rate(uint32_t requests_per_minute, uint32_t burst) noexcept {
    const uint64_t interval = compute_interval(requests_per_minute);
    const uint32_t effective_burst = burst != 0 ? burst : requests_per_minute;
    rate_.store(requests_per_minute, std::memory_order_relaxed);
    interval_ns_.store(interval, std::memory_order_relaxed);
    burst_ns_.store(interval * effective_burst, std::memory_order_relaxed);
    tat_.store(0, std::memory_order_relaxed);
}

void rate_limiter::notify_rate_limited(uint32_t retry_after_seconds) noexcept {
    const uint64_t until = now_ns() + static_cast<uint64_t>(retry_after_seconds) * 1'000'000'000ull;
    uint64_t current = blocked_until_.load(std::memory_order_relaxed);
    // Only ever push the barrier further out.
    while (until > current &&
           !blocked_until_.compare_exchange_weak(current, until,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
    }
}

bool rate_limiter::try_acquire() noexcept {
    const uint64_t interval = interval_ns();
    if (interval == 0) {
        return true;    // limiting disabled
    }

    const uint64_t now = now_ns();
    if (now < blocked_until_.load(std::memory_order_acquire)) {
        return false;
    }

    const uint64_t burst = burst_ns();
    uint64_t tat = tat_.load(std::memory_order_acquire);
    for (;;) {
        const uint64_t new_tat = std::max(tat, now) + interval;
        // The request is admissible only if its arrival time stays within the burst window.
        if (new_tat > now + burst) {
            return false;
        }
        if (tat_.compare_exchange_weak(tat, new_tat,
                                       std::memory_order_release,
                                       std::memory_order_acquire)) {
            return true;
        }
    }
}

void rate_limiter::acquire() {
    const uint64_t interval = interval_ns();
    if (interval == 0) {
        return;         // limiting disabled
    }

    for (;;) {
        // Wait out any server-imposed 429 barrier first.
        const uint64_t blocked_until = blocked_until_.load(std::memory_order_acquire);
        uint64_t now = now_ns();
        if (now < blocked_until) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(blocked_until - now));
            continue;
        }

        const uint64_t burst = burst_ns();
        uint64_t tat = tat_.load(std::memory_order_acquire);
        uint64_t sleep_ns = 0;
        bool reserved = false;

        for (;;) {
            now = now_ns();
            const uint64_t new_tat = std::max(tat, now) + interval;
            // Reserve the slot unconditionally, then sleep until it comes due. Reserving
            // before sleeping is what keeps concurrent callers correctly ordered.
            if (tat_.compare_exchange_weak(tat, new_tat,
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {
                const uint64_t due = new_tat > burst ? new_tat - burst : 0;
                sleep_ns = due > now ? due - now : 0;
                reserved = true;
                break;
            }
        }

        if (reserved) {
            if (sleep_ns != 0) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
            }
            return;
        }
    }
}

uint64_t rate_limiter::wait_time_ns() const noexcept {
    const uint64_t interval = interval_ns();
    if (interval == 0) {
        return 0;
    }

    const uint64_t now = now_ns();
    const uint64_t blocked_until = blocked_until_.load(std::memory_order_acquire);
    if (now < blocked_until) {
        return blocked_until - now;
    }

    const uint64_t tat = tat_.load(std::memory_order_acquire);
    const uint64_t new_tat = std::max(tat, now) + interval;
    const uint64_t burst = burst_ns();
    const uint64_t due = new_tat > burst ? new_tat - burst : 0;
    return due > now ? due - now : 0;
}

}   // namespace alpaca
