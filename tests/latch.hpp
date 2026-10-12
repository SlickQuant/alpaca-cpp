// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Slick Quant
// https://github.com/SlickQuant/alpaca-cpp

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace alpaca::tests {

/// A one-shot latch a stream handler can trip from the service thread.
///
/// Handlers run on the websocket thread, so every assertion about "did a message
/// arrive" has to hand the result back to the test thread. Wrapping that once keeps the
/// tests readable and stops each of them growing its own mutex.
class latch {
public:
    void trip() {
        {
            std::lock_guard lock(mutex_);
            tripped_ = true;
        }
        cv_.notify_all();
    }

    bool wait(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return tripped_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool tripped_ = false;
};

}   // namespace alpaca::tests
