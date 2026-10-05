#pragma once

#include <map>
#include <string>

#include "request_budget.h"
#include "request_rate_limiter.h"
#include "scope_policy.h"
#include "audit_configuration.h"

enum class HttpRequestDisposition
{
    NotAttempted,
    Succeeded,
    Failed,
    ScopeRejected,
    BudgetRejected
};

const char* HttpRequestDispositionToString(
    HttpRequestDisposition disposition);

struct HttpResponse
{
    int statusCode = 0;

    bool isHttps = false;

    std::string method;

    std::string requestedUrl;
    std::string finalUrl;

    std::string error;

    HttpRequestDisposition disposition =
        HttpRequestDisposition::NotAttempted;

    std::map<std::string, std::string> headers;

    std::string body;
};

class HttpClient
{
public:
    static HttpResponse Get(
        const std::string& url);

    static HttpResponse Get(
        const std::string& url,
        const ScopePolicy& scopePolicy);

    static HttpResponse Get(
        const std::string& url,
        const ScopePolicy& scopePolicy,
        RequestBudget& requestBudget,
        RequestRateLimiter& requestRateLimiter);

    static HttpResponse Get(
        const std::string& url,
        const AuditConfiguration& configuration,
        RequestBudget& requestBudget,
        RequestRateLimiter& requestRateLimiter);
};
