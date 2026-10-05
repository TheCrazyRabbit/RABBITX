#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>

constexpr size_t DefaultMinRequestIntervalMs = 200;

class RequestRateLimiter
{
public:
    explicit RequestRateLimiter(
        size_t minRequestIntervalMs = DefaultMinRequestIntervalMs);

    void WaitForSlot();

private:
    const std::chrono::milliseconds interval_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point lastRequestAt_{};
    bool hasSentRequest_ = false;
};
