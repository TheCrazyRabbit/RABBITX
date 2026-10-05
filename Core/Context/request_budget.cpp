#include "request_budget.h"

RequestBudget::RequestBudget(
    size_t maxRequestsPerRun,
    size_t minRequestIntervalMs)
{
    stats_.maxRequestsPerRun = maxRequestsPerRun;
    stats_.minRequestIntervalMs = minRequestIntervalMs;
    stats_.remainingRequests = maxRequestsPerRun;
}

bool RequestBudget::TryBeginRequest()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (stats_.attemptedRequests >= stats_.maxRequestsPerRun)
    {
        ++stats_.budgetRejectedRequests;
        return false;
    }

    ++stats_.attemptedRequests;
    --stats_.remainingRequests;
    return true;
}

void RequestBudget::RecordSuccessfulRequest()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.successfulRequests;
}

void RequestBudget::RecordFailedRequest()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.failedRequests;
}

void RequestBudget::RecordScopeRejectedRequest()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.scopeRejectedRequests;
}

RequestBudgetStats RequestBudget::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}
