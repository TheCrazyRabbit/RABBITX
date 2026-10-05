#include "pipeline.h"

#include <map>
#include <string>

#include "analyzer.h"
#include "endpoint_intelligence.h"
#include "probe_engine.h"
#include "probe_planner.h"
#include "probe_executor.h"

namespace
{
    std::string FindingKey(
        const Finding& finding)
    {
        std::string key = finding.ruleId;
        key.push_back('\x1f');

        key += finding.requestEvidence.requestedUrl.empty()
            ? finding.requestEvidence.finalUrl
            : finding.requestEvidence.requestedUrl;

        return key;
    }

    bool SameEvidence(
        const Evidence& left,
        const Evidence& right)
    {
        return left.method == right.method &&
            left.requestedUrl == right.requestedUrl &&
            left.finalUrl == right.finalUrl &&
            left.statusCode == right.statusCode &&
            left.requestHeaders == right.requestHeaders &&
            left.responseHeaders == right.responseHeaders &&
            left.bodyExcerpt == right.bodyExcerpt &&
            left.observation == right.observation;
    }

    void AddEvidenceIfNew(
        Finding& target,
        const Evidence& evidence)
    {
        if (evidence.requestedUrl.empty() &&
            evidence.finalUrl.empty())
        {
            return;
        }

        if (target.requestEvidence.requestedUrl.empty() &&
            target.requestEvidence.finalUrl.empty())
        {
            target.requestEvidence = evidence;
            return;
        }

        if (SameEvidence(
            target.requestEvidence,
            evidence))
        {
            return;
        }

        for (const auto& existing : target.relatedEvidence)
        {
            if (SameEvidence(existing, evidence))
                return;
        }

        target.relatedEvidence.push_back(evidence);
    }

    void MergeFindings(
        std::vector<Finding>& target,
        const std::vector<Finding>& source)
    {
        std::map<std::string, size_t> known;

        for (size_t i = 0; i < target.size(); ++i)
        {
            known[FindingKey(target[i])] = i;
        }

        for (const auto& incoming : source)
        {
            const std::string key = FindingKey(incoming);
            const auto existing = known.find(key);

            if (existing == known.end())
            {
                known[key] = target.size();
                target.push_back(incoming);
                continue;
            }

            Finding& merged = target[existing->second];

            if (incoming.severity == Severity::Low)
                merged.severity = Severity::Low;

            if (incoming.confidence == Confidence::High ||
                (incoming.confidence == Confidence::Medium &&
                    merged.confidence == Confidence::Low))
            {
                merged.confidence = incoming.confidence;
            }

            AddEvidenceIfNew(
                merged,
                incoming.requestEvidence);

            for (const auto& evidence : incoming.relatedEvidence)
            {
                AddEvidenceIfNew(merged, evidence);
            }
        }
    }
}

AuditResult Pipeline::Run(
    const std::string& url)
{
    ScopePolicy policy;
    std::string error;

    ScopePolicyEngine::CreateDefault(
        url,
        policy,
        error);

    return Run(url, policy);
}

AuditResult Pipeline::Run(
    const std::string& url,
    const ScopePolicy& scopePolicy,
    size_t maxRequestsPerRun,
    size_t minRequestIntervalMs)
{
    AuditConfiguration configuration;
    configuration.targetUrl = url;
    configuration.scope = scopePolicy;
    configuration.maxRequests = maxRequestsPerRun;
    configuration.minIntervalMs = minRequestIntervalMs;

    return Run(configuration);
}

AuditResult Pipeline::Run(
    const AuditConfiguration& configuration)
{
    AuditResult result;

    RequestBudget requestBudget(
        configuration.maxRequests,
        configuration.minIntervalMs);
    RequestRateLimiter requestRateLimiter(configuration.minIntervalMs);

    result.targetUrl = configuration.targetUrl;
    result.scopePolicy = configuration.scope;
    result.configuration = configuration;

    const auto probes =
        ProbeEngine::DefaultProbes();

    for (const auto& probe : probes)
    {
        const ProbeResult probeResult =
            ProbeEngine::Execute(
                probe,
                configuration.targetUrl,
                configuration,
                requestBudget,
                requestRateLimiter);

        result.probes.push_back(
            probeResult);

        const auto findings =
            Analyzer::AnalyzeProbe(
                probeResult);

        MergeFindings(
            result.findings,
            findings);

        if (probeResult.executed)
        {
            const auto endpoints =
                EndpointIntelligence::Analyze(
                    probeResult.response);

            result.endpoints.insert(
                result.endpoints.end(),
                endpoints.begin(),
                endpoints.end());
        }
    }

    result.probePlans =
        ProbePlanner::BuildPlans(
            result.endpoints);

    std::vector<ProbeTask> tasks;


    for (const auto& plan : result.probePlans)
    {
        tasks.push_back(
            MakeProbeTask(plan)
        );
    }

    const auto probeResults =
        ProbeExecutor::Execute(
            tasks,
            configuration,
            requestBudget,
            requestRateLimiter);



    for (const auto& probeResult : probeResults)
    {
        result.probes.push_back(
            probeResult
        );


        const auto findings =
            Analyzer::AnalyzeProbe(
                probeResult
            );


        MergeFindings(
            result.findings,
            findings);
    }

    result.requestBudget = requestBudget.Snapshot();
    return result;
}
