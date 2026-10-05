#pragma once

#include <vector>

#include "probe_task.h"
#include "probe.h"
#include "probe_result.h"
#include "audit_configuration.h"
#include "request_budget.h"
#include "request_rate_limiter.h"
#include "scope_policy.h"

class ProbeExecutor
{

public:

    static std::vector<ProbeResult>
        Execute(
            const std::vector<ProbeTask>& tasks,
            const AuditConfiguration& configuration,
            RequestBudget& requestBudget,
            RequestRateLimiter& requestRateLimiter
        );


private:

    static ProbeResult
        ExecuteOne(
            const ProbeTask& task,
            const AuditConfiguration& configuration,
            RequestBudget& requestBudget,
            RequestRateLimiter& requestRateLimiter
        );

};
