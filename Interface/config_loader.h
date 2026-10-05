#pragma once

#include <optional>
#include <string>
#include <vector>

#include "audit_configuration.h"

struct AuditConfigurationOverrides
{
    std::optional<std::string> targetUrl;
    std::optional<std::string> projectName;
    std::optional<size_t> maxRequests;
    std::optional<size_t> minIntervalMs;
    std::optional<int> timeoutMs;
    std::optional<bool> followRedirects;
    std::optional<ScopePolicy> scope;
    std::optional<AuditOutputFormat> outputFormat;
    std::optional<std::string> outputPath;
    std::optional<bool> persistProject;
    std::optional<bool> includeEvidence;
    std::optional<bool> includeProbeHistory;
};

namespace ConfigurationFile
{
    bool Load(
        const std::string& path,
        AuditConfigurationOverrides& overrides,
        std::string& error);

    void Apply(
        const AuditConfigurationOverrides& overrides,
        AuditConfiguration& configuration);
}
