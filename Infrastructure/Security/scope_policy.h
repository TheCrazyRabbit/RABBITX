#pragma once

#include <string>
#include <vector>

struct ScopePolicy
{
    // Host rules match one hostname. An optional port makes the rule origin-specific.
    std::vector<std::string> allowedHosts;

    // Domain rules match the domain itself and any subdomain.
    std::vector<std::string> allowedDomains;

    // Path rules are case-sensitive path prefixes with segment boundaries.
    std::vector<std::string> allowedPaths;
};

struct ScopeDecision
{
    bool allowed = false;
    std::string reason;
};

namespace ScopePolicyEngine
{
    bool CreateDefault(
        const std::string& targetUrl,
        ScopePolicy& policy,
        std::string& error);

    bool AddHost(
        ScopePolicy& policy,
        const std::string& host,
        std::string& error);

    bool AddDomain(
        ScopePolicy& policy,
        const std::string& domain,
        std::string& error);

    bool AddPath(
        ScopePolicy& policy,
        const std::string& pathPrefix,
        std::string& error);

    bool Validate(
        const ScopePolicy& policy,
        std::string& error);

    ScopeDecision Check(
        const ScopePolicy& policy,
        const std::string& url);
}
