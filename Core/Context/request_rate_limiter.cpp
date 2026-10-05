#include "request_rate_limiter.h"

#include <thread>

RequestRateLimiter::RequestRateLimiter(
    size_t minRequestIntervalMs)
    : interval_(minRequestIntervalMs)
{
}

void RequestRateLimiter::WaitForSlot()
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (hasSentRequest_)
    {
        const auto nextAllowed = lastRequestAt_ + interval_;
        const auto now = std::chrono::steady_clock::now();

        if (now < nextAllowed)
            std::this_thread::sleep_until(nextAllowed);
    }

    lastRequestAt_ = std::chrono::steady_clock::now();
    hasSentRequest_ = true;
}
