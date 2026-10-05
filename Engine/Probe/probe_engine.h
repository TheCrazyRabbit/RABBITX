#pragma once

#include <string>
#include <vector>

#include "probe.h"
#include "probe_result.h"
#include "audit_configuration.h"
#include "request_budget.h"
#include "request_rate_limiter.h"
#include "scope_policy.h"

class ProbeEngine
{
public:
    static ProbeResult Execute(
        const Probe& probe,
        const std::string& url,
        const AuditConfiguration& configuration,
        RequestBudget& requestBudget,
        RequestRateLimiter& requestRateLimiter);

    static std::vector<Probe> DefaultProbes();
};
