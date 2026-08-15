// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp
//
// Offline unit tests for the lock-free token bucket. No credentials, no network.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <alpaca/rate_limiter.hpp>

namespace alpaca::tests {

TEST(RateLimiter, ZeroRateDisablesLimiting) {
    rate_limiter limiter(0);
    EXPECT_FALSE(limiter.enabled());
    EXPECT_EQ(limiter.wait_time_ns(), 0ull);

    // Never blocks, no matter how many times it is called.
    for (int i = 0; i < 10'000; ++i) {
        ASSERT_TRUE(limiter.try_acquire());
    }
}

TEST(RateLimiter, AdmitsExactlyTheBurstThenRefuses) {
    // 60 per minute with a burst of 5: the first five go through instantly, the sixth
    // has to wait for the bucket to drain.
    rate_limiter limiter(60, 5);

    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.try_acquire()) << "token " << i << " should have been admitted";
    }
    EXPECT_FALSE(limiter.try_acquire());
    EXPECT_GT(limiter.wait_time_ns(), 0ull);
}

TEST(RateLimiter, BurstDefaultsToTheFullPerMinuteBudget) {
    rate_limiter limiter(200);
    for (int i = 0; i < 200; ++i) {
        ASSERT_TRUE(limiter.try_acquire()) << "token " << i;
    }
    EXPECT_FALSE(limiter.try_acquire());
}

TEST(RateLimiter, BucketRefillsOverTime) {
    // 6000/min == one token every 10ms, burst 1.
    rate_limiter limiter(6000, 1);
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_FALSE(limiter.try_acquire());

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_TRUE(limiter.try_acquire());
}

TEST(RateLimiter, AcquireBlocksUntilASlotIsAvailable) {
    // 6000/min == 10ms per token, burst 1: the second acquire must wait roughly one interval.
    rate_limiter limiter(6000, 1);
    limiter.acquire();

    const auto start = std::chrono::steady_clock::now();
    limiter.acquire();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 5);
}

TEST(RateLimiter, SetRateResetsTheBudget) {
    rate_limiter limiter(60, 2);
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_FALSE(limiter.try_acquire());

    limiter.set_rate(600, 10);
    EXPECT_EQ(limiter.rate(), 600u);
    EXPECT_TRUE(limiter.try_acquire());
}

TEST(RateLimiter, ServerRateLimitBlocksSubsequentAcquires) {
    rate_limiter limiter(6000, 100);
    EXPECT_TRUE(limiter.try_acquire());

    limiter.notify_rate_limited(2);
    EXPECT_FALSE(limiter.try_acquire());
    EXPECT_GT(limiter.wait_time_ns(), 1'000'000'000ull);
}

TEST(RateLimiter, ServerRateLimitOnlyEverExtendsTheBarrier) {
    rate_limiter limiter(6000, 100);
    limiter.notify_rate_limited(10);
    const uint64_t long_wait = limiter.wait_time_ns();

    // A shorter Retry-After must not shrink an outstanding barrier.
    limiter.notify_rate_limited(1);
    EXPECT_GT(limiter.wait_time_ns(), long_wait / 2);
}

TEST(RateLimiter, ConcurrentAcquiresNeverExceedTheBurst) {
    // The whole point of the CAS loop: N threads racing must not over-admit. With a burst
    // of 50 and an interval long enough that no meaningful refill happens during the test,
    // exactly 50 tokens may be handed out in total.
    constexpr uint32_t burst = 50;
    constexpr int thread_count = 8;
    constexpr int attempts_per_thread = 200;

    rate_limiter limiter(60, burst);   // one token per second; no refill within the test
    std::atomic_int admitted{0};
    std::atomic_bool go{false};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < attempts_per_thread; ++i) {
                if (limiter.try_acquire()) {
                    admitted.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(admitted.load(), static_cast<int>(burst));
}

TEST(RateLimiter, CopyStartsWithAFreshBudget) {
    rate_limiter limiter(60, 2);
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_FALSE(limiter.try_acquire());

    rate_limiter copy(limiter);
    EXPECT_EQ(copy.rate(), 60u);
    EXPECT_TRUE(copy.try_acquire());
}

}   // namespace alpaca::tests
