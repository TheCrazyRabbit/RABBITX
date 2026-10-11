#include "pipeline.h"

#include <map>
#include <string>

#include "analyzer.h"
#include "endpoint_intelligence.h"
#include "probe_engine.h"
#include "probe_planner.h"
#include "probe_executor.h"
#include "ai_triage_engine.h"
#include <algorithm>
#include <memory>

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
        return SameEvidenceSnapshot(left, right);
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
            if(merged.evidence.find(incoming.evidence)==std::string::npos)
                merged.evidence += "; " + incoming.evidence;

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

    auto registerEvidence = [&](ProbeResult& probeResult)
    {
        for(auto& hop:probeResult.response.completedHopEvidence)
        {
            auto existing=std::find_if(result.evidenceCatalog.begin(),result.evidenceCatalog.end(),[&](const Evidence& e){return SameEvidenceSnapshot(e,hop);});
            if(existing!=result.evidenceCatalog.end())hop.id=existing->id;
            else {hop.id="evidence-"+std::to_string(result.evidenceCatalog.size()+1);result.evidenceCatalog.push_back(hop);}
        }
        if(!probeResult.executed) return;
        for(const auto& e:result.evidenceCatalog)
            if(SameEvidenceSnapshot(e,probeResult.evidence)){probeResult.evidence.id=e.id;return;}
        probeResult.evidence.id="evidence-"+std::to_string(result.evidenceCatalog.size()+1);
        result.evidenceCatalog.push_back(probeResult.evidence);
    };

    const auto probes =
        ProbeEngine::DefaultProbes();

    for (const auto& probe : probes)
    {
        ProbeResult probeResult =
            ProbeEngine::Execute(
                probe,
                configuration.targetUrl,
                configuration,
                requestBudget,
                requestRateLimiter);

        registerEvidence(probeResult);
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
            auto endpoints =
                EndpointIntelligence::Analyze(
                    probeResult.response);

            for(auto& endpoint:endpoints)
            {
                endpoint.discoveryEvidenceIds.push_back(probeResult.evidence.id);
                auto found=std::find_if(result.endpoints.begin(),result.endpoints.end(),[&](const Endpoint& e){return e.url==endpoint.url&&e.method==endpoint.method;});
                if(found==result.endpoints.end()) result.endpoints.push_back(std::move(endpoint));
                else if(std::find(found->discoveryEvidenceIds.begin(),found->discoveryEvidenceIds.end(),probeResult.evidence.id)==found->discoveryEvidenceIds.end())found->discoveryEvidenceIds.push_back(probeResult.evidence.id);
            }
        }
    }

    std::stable_sort(result.endpoints.begin(),result.endpoints.end(),[](const Endpoint& a,const Endpoint& b){return a.riskScore.value>b.riskScore.value;});
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

    auto probeResults =
        ProbeExecutor::Execute(
            tasks,
            configuration,
            requestBudget,
            requestRateLimiter);



    for (auto& probeResult : probeResults)
    {
        registerEvidence(probeResult);
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
    if(configuration.aiTriage)
        result.aiTriage=AITriageEngine::Run(result,std::make_shared<MockAIProvider>(),configuration.aiTimeoutMs);
    return result;
}
