#include "probe_engine.h"

#include "http_client.h"

namespace
{
    Evidence BuildEvidence(
        const HttpResponse& response)
    {
        Evidence evidence;

        evidence.method =
            response.method;

        evidence.requestedUrl =
            response.requestedUrl;

        evidence.finalUrl =
            response.finalUrl;

        evidence.statusCode =
            response.statusCode;

        evidence.observation =
            "Baseline HTTP response captured.";

        return evidence;
    }
}

ProbeResult ProbeEngine::Execute(
    const Probe& probe,
    const std::string& url,
    const AuditConfiguration& configuration,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter)
{
    ProbeResult result;

    result.probe = probe;

    if (url.empty())
    {
        result.error =
            "Probe URL cannot be empty.";

        return result;
    }

    result.response =
        HttpClient::Get(
            url,
            configuration,
            requestBudget,
            requestRateLimiter);

    result.error = result.response.error;

    if (result.response.statusCode == 0)
    {
        return result;
    }

    result.evidence =
        BuildEvidence(result.response);

    result.executed = true;

    return result;
}

std::vector<Probe> ProbeEngine::DefaultProbes()
{
    return
    {
        {
            "http.baseline",
            "HTTP Baseline",
            "Perform a controlled baseline GET request."
        }
    };
}
