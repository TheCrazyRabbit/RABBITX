#include "probe_executor.h"

namespace
{
    Evidence BuildEvidence(
        const HttpResponse& response)
    {
        Evidence evidence;
        evidence.method = response.method;
        evidence.requestedUrl = response.requestedUrl;
        evidence.finalUrl = response.finalUrl;
        evidence.statusCode = response.statusCode;
        evidence.observation =
            "Planned probe response captured.";
        return evidence;
    }
}

std::vector<ProbeResult>
ProbeExecutor::Execute(
    const std::vector<ProbeTask>& tasks,
    const AuditConfiguration& configuration,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter
)
{
    std::vector<ProbeResult> results;


    for (const auto& task : tasks)
    {
        results.push_back(
            ExecuteOne(task, configuration, requestBudget, requestRateLimiter)
        );
    }


    return results;
}



ProbeResult
ProbeExecutor::ExecuteOne(
    const ProbeTask& task,
    const AuditConfiguration& configuration,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter
)
{
    ProbeResult result;


    result.probe.id =
    ProbePlanTypeToString(task.type);

result.probe.name =
    ProbePlanTypeToString(task.type);

result.probe.description =
    task.reason;


    try
    {
        result.response =
            HttpClient::Get(
                task.url,
                configuration,
                requestBudget,
                requestRateLimiter
            );

        result.error = result.response.error;

        if (result.response.statusCode == 0)
        {
            return result;
        }

        result.evidence = BuildEvidence(
            result.response);

        result.executed = true;
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
    }


    return result;
}
