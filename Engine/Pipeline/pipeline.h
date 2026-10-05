#pragma once

#include <string>
#include <vector>

#include "endpoint.h"
#include "audit_configuration.h"
#include "finding.h"
#include "probe.h"
#include "probe_plan.h"
#include "probe_result.h"
#include "request_budget.h"
#include "request_rate_limiter.h"
#include "scope_policy.h"

struct AuditResult
{
    std::string targetUrl;

    std::vector<ProbeResult> probes;

    std::vector<Finding> findings;

    std::vector<Endpoint> endpoints;

    std::vector<ProbePlan> probePlans;

    ScopePolicy scopePolicy;

    RequestBudgetStats requestBudget;

    AuditConfiguration configuration;
};

class Pipeline
{
public:
    static AuditResult Run(
        const std::string& url);

    static AuditResult Run(
        const std::string& url,
        const ScopePolicy& scopePolicy,
        size_t maxRequestsPerRun = DefaultMaxRequestsPerRun,
        size_t minRequestIntervalMs = DefaultMinRequestIntervalMs);

    static AuditResult Run(
        const AuditConfiguration& configuration);
};
