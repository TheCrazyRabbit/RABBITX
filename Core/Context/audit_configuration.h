#pragma once

#include <cstddef>
#include <string>

#include "request_budget.h"
#include "scope_policy.h"

constexpr int DefaultTimeoutMs = 15000;
constexpr int MaximumTimeoutMs = 600000;

enum class AuditOutputFormat
{
    Console,
    Json,
    Html
};

const char* AuditOutputFormatToString(
    AuditOutputFormat format);

bool AuditOutputFormatFromString(
    const std::string& value,
    AuditOutputFormat& format);

struct ReportOptions
{
    bool includeEvidence = true;
    bool includeProbeHistory = true;
};

struct AuditConfiguration
{
    std::string targetUrl;
    std::string projectName;

    size_t maxRequests = DefaultMaxRequestsPerRun;
    size_t minIntervalMs = DefaultMinRequestIntervalMs;
    int timeoutMs = DefaultTimeoutMs;

    // Preserves the original 10s connect and 15s send/receive defaults.
    bool useLegacyTimeoutProfile = true;
    bool followRedirects = true;

    ScopePolicy scope;

    AuditOutputFormat outputFormat = AuditOutputFormat::Console;
    std::string outputPath;
    ReportOptions reportOptions;
    bool persistProject = true;
};
