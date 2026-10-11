#include "probe_executor.h"
#include <set>



std::vector<ProbeResult>
ProbeExecutor::Execute(
    const std::vector<ProbeTask>& tasks,
    const AuditConfiguration& configuration,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter
)
{
    std::vector<ProbeResult> results;
    std::set<std::string> visited;


    for (const auto& task : tasks)
    {
        // Plans may express several review reasons for the same baseline GET.
        if(!visited.insert(task.method+" "+task.url).second)continue;
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
    if(task.method!="GET")
    {
        result.error="Probe method rejected: only baseline GET is implemented.";
        return result;
    }


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

        if (result.response.statusCode == 0 || result.response.disposition != HttpRequestDisposition::Succeeded || !result.response.error.empty())
        {
            return result;
        }

        result.evidence = CaptureEvidence(
            result.response);

        result.executed = true;
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
    }


    return result;
}
