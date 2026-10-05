#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "audit_configuration.h"
#include "request_budget.h"
#include "scope_policy.h"

struct ProjectAuditRun
{
    std::string savedAtUtc;
    std::string reportJson;
    RequestBudgetStats requestBudget;
};

struct Project
{
    std::string id;
    std::string name;
    std::string targetUrl;
    std::string createdAtUtc;
    std::string updatedAtUtc;

    ScopePolicy scopePolicy;

    AuditConfiguration defaultConfiguration;

    std::vector<ProjectAuditRun> runs;
};
