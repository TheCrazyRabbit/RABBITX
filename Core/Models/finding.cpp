#include "finding.h"

#include <iostream>

#include "parameter_intelligence.h"

namespace
{
    const char* SeverityToString(
        Severity severity)
    {
        switch (severity)
        {
        case Severity::Low:
            return "LOW";

        case Severity::Info:
            return "INFO";

        default:
            return "UNKNOWN";
        }
    }

    const char* ConfidenceToString(
        Confidence confidence)
    {
        switch (confidence)
        {
        case Confidence::Low:
            return "LOW";

        case Confidence::Medium:
            return "MEDIUM";

        case Confidence::High:
            return "HIGH";

        default:
            return "UNKNOWN";
        }
    }
}

void PrintFinding(
    const Finding& finding)
{
    std::cout
        << "["
        << SeverityToString(finding.severity)
        << "] "
        << finding.title
        << '\n';

    std::cout
        << "      Rule: "
        << finding.ruleId
        << '\n';

    std::cout
        << "      Category: "
        << finding.category
        << '\n';

    std::cout
        << "      Confidence: "
        << ConfidenceToString(finding.confidence)
        << '\n';

    std::cout
        << "      "
        << finding.description
        << '\n';

    if (!finding.evidence.empty())
    {
        std::cout
            << "      Evidence: "
            << finding.evidence
            << '\n';
    }

    const Evidence& evidence =
        finding.requestEvidence;

    const auto PrintEvidence =
        [](const Evidence& item)
    {
        if (item.requestedUrl.empty() &&
            item.finalUrl.empty())
        {
            return;
        }

        std::cout
            << "      Request: "
            << item.method
            << " "
            << ParameterIntelligence::RedactSensitiveValues(
                item.requestedUrl)
            << '\n';

        std::cout
            << "      Response: "
            << item.statusCode
            << " "
            << ParameterIntelligence::RedactSensitiveValues(
                item.finalUrl)
            << '\n';

        if (!item.observation.empty())
        {
            std::cout
                << "      Observation: "
                << item.observation
                << '\n';
        }
    };

    PrintEvidence(evidence);

    for (const auto& related : finding.relatedEvidence)
    {
        PrintEvidence(related);
    }

    if (!finding.relatedEvidence.empty())
    {
        std::cout
            << "      Aggregated observations: "
            << finding.relatedEvidence.size() + 1
            << '\n';
    }

    std::cout << '\n';
}
