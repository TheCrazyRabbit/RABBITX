#include "analyzer.h"

#include <set>

#include "security_checks.h"

namespace
{
    void AppendUnique(
        std::vector<Finding>& target,
        const std::vector<Finding>& source,
        std::set<std::string>& knownRules)
    {
        for (const auto& finding : source)
        {
            if (knownRules.insert(
                finding.ruleId).second)
            {
                target.push_back(finding);
            }
        }
    }

    void AttachEvidence(
        std::vector<Finding>& findings,
        const Evidence& evidence)
    {
        for (auto& finding : findings)
        {
            finding.requestEvidence =
                evidence;
        }
    }
}

std::vector<Finding> Analyzer::Analyze(
    const HttpResponse& response)
{
    std::vector<Finding> findings;

    std::set<std::string> knownRules;

    AppendUnique(
        findings,
        SecurityChecks::CheckSecurityHeaders(response),
        knownRules);

    AppendUnique(
        findings,
        SecurityChecks::CheckCors(response),
        knownRules);

    AppendUnique(
        findings,
        SecurityChecks::CheckCookies(response),
        knownRules);

    AppendUnique(
        findings,
        SecurityChecks::CheckInformationDisclosure(response),
        knownRules);

    AppendUnique(
        findings,
        SecurityChecks::CheckRedirectSecurity(response),
        knownRules);

    return findings;
}

std::vector<Finding> Analyzer::AnalyzeProbe(
    const ProbeResult& result)
{
    if (!result.executed)
        return {};

    auto findings =
        Analyze(result.response);

    AttachEvidence(
        findings,
        result.evidence);

    return findings;
}