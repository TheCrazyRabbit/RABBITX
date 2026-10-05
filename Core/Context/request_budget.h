#pragma once

#include <cstddef>
#include <mutex>

#include "request_rate_limiter.h"

constexpr size_t DefaultMaxRequestsPerRun = 50;

struct RequestBudgetStats
{
    size_t maxRequestsPerRun = DefaultMaxRequestsPerRun;
    size_t minRequestIntervalMs = DefaultMinRequestIntervalMs;
    size_t attemptedRequests = 0;
    size_t successfulRequests = 0;
    size_t failedRequests = 0;
    size_t scopeRejectedRequests = 0;
    size_t budgetRejectedRequests = 0;
    size_t remainingRequests = DefaultMaxRequestsPerRun;
};

class RequestBudget
{
public:
    explicit RequestBudget(
        size_t maxRequestsPerRun = DefaultMaxRequestsPerRun,
        size_t minRequestIntervalMs = DefaultMinRequestIntervalMs);

    bool TryBeginRequest();

    void RecordSuccessfulRequest();
    void RecordFailedRequest();
    void RecordScopeRejectedRequest();

    RequestBudgetStats Snapshot() const;

private:
    mutable std::mutex mutex_;
    RequestBudgetStats stats_;
};
