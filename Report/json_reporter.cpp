#include "json_reporter.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "parameter_intelligence.h"

namespace
{
    std::string EscapeJson(
        const std::string& value)
    {
        std::ostringstream out;

        for (const unsigned char ch : value)
        {
            switch (ch)
            {
            case '\"':
                out << "\\\"";
                break;

            case '\\':
                out << "\\\\";
                break;

            case '\b':
                out << "\\b";
                break;

            case '\f':
                out << "\\f";
                break;

            case '\n':
                out << "\\n";
                break;

            case '\r':
                out << "\\r";
                break;

            case '\t':
                out << "\\t";
                break;

            default:
                if (ch < 0x20)
                {
                    const char* hex =
                        "0123456789abcdef";

                    out << "\\u00"
                        << hex[(ch >> 4) & 0x0F]
                        << hex[ch & 0x0F];
                }
                else
                {
                    out << static_cast<char>(ch);
                }
                break;
            }
        }

        return out.str();
    }

    const char* SeverityToString(
        Severity severity)
    {
        switch (severity)
        {
        case Severity::Info:
            return "info";

        case Severity::Low:
            return "low";

        default:
            return "unknown";
        }
    }

    const char* ConfidenceToString(
        Confidence confidence)
    {
        switch (confidence)
        {
        case Confidence::Low:
            return "low";

        case Confidence::Medium:
            return "medium";

        case Confidence::High:
            return "high";

        default:
            return "unknown";
        }
    }

    std::string Quote(
        const std::string& value)
    {
        return "\"" +
            EscapeJson(value) +
            "\"";
    }

    void WriteStringArray(
        std::ostringstream& out,
        const std::vector<std::string>& values)
    {
        out << "[";

        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                out << ", ";

            out << Quote(values[i]);
        }

        out << "]";
    }

    std::string QuoteUrl(
        const std::string& value)
    {
        return Quote(
            ParameterIntelligence::RedactSensitiveValues(
                value));
    }

    std::string SafeParameterValue(
        const Parameter& parameter)
    {
        if (ParameterClassifier::ShouldRedactValue(
                parameter.name) &&
            !parameter.value.empty())
        {
            return "[REDACTED]";
        }

        return parameter.value;
    }

    void WriteEvidence(
        std::ostringstream& out,
        const Evidence& evidence,
        const std::string& indent)
    {
        out << indent << "{\n";

        out << indent
            << "  \"method\": "
            << Quote(evidence.method)
            << ",\n";

        out << indent
            << "  \"requestedUrl\": "
            << QuoteUrl(evidence.requestedUrl)
            << ",\n";

        out << indent
            << "  \"finalUrl\": "
            << QuoteUrl(evidence.finalUrl)
            << ",\n";

        out << indent
            << "  \"statusCode\": "
            << evidence.statusCode
            << ",\n";

        out << indent
            << "  \"requestHeaders\": "
            << Quote(evidence.requestHeaders)
            << ",\n";

        out << indent
            << "  \"responseHeaders\": "
            << Quote(evidence.responseHeaders)
            << ",\n";

        out << indent
            << "  \"bodyExcerpt\": "
            << Quote(evidence.bodyExcerpt)
            << ",\n";

        out << indent
            << "  \"observation\": "
            << Quote(evidence.observation)
            << '\n';

        out << indent << "}";
    }

    void WriteProbe(
        std::ostringstream& out,
        const ProbeResult& probe,
        const std::string& indent)
    {
        out << indent << "{\n";

        out << indent
            << "  \"id\": "
            << Quote(probe.probe.id)
            << ",\n";

        out << indent
            << "  \"name\": "
            << Quote(probe.probe.name)
            << ",\n";

        out << indent
            << "  \"description\": "
            << Quote(probe.probe.description)
            << ",\n";

        out << indent
            << "  \"executed\": "
            << (probe.executed ? "true" : "false")
            << ",\n";

        out << indent
            << "  \"error\": "
            << Quote(probe.error)
            << ",\n";

        out << indent
            << "  \"requestDisposition\": "
            << Quote(HttpRequestDispositionToString(
                probe.response.disposition))
            << ",\n";

        out << indent
            << "  \"statusCode\": "
            << probe.response.statusCode
            << ",\n";

        out << indent
            << "  \"requestedUrl\": "
            << QuoteUrl(probe.response.requestedUrl)
            << ",\n";

        out << indent
            << "  \"finalUrl\": "
            << QuoteUrl(probe.response.finalUrl)
            << '\n';

        out << indent << "}";
    }

    void WriteFinding(
        std::ostringstream& out,
        const Finding& finding,
        const std::string& indent,
        bool includeEvidence)
    {
        out << indent << "{\n";

        out << indent
            << "  \"ruleId\": "
            << Quote(finding.ruleId)
            << ",\n";

        out << indent
            << "  \"category\": "
            << Quote(finding.category)
            << ",\n";

        out << indent
            << "  \"severity\": "
            << Quote(
                SeverityToString(
                    finding.severity))
            << ",\n";

        out << indent
            << "  \"confidence\": "
            << Quote(
                ConfidenceToString(
                    finding.confidence))
            << ",\n";

        out << indent
            << "  \"title\": "
            << Quote(finding.title)
            << ",\n";

        out << indent
            << "  \"description\": "
            << Quote(finding.description)
            << (includeEvidence ? ",\n" : "\n");

        if (includeEvidence)
        {
            out << indent
                << "  \"evidence\": "
                << Quote(finding.evidence)
                << ",\n";

            out << indent
                << "  \"requestEvidence\":\n";

            WriteEvidence(
                out,
                finding.requestEvidence,
                indent + "  ");

            out << ",\n"
                << indent
                << "  \"evidenceCount\": "
                << finding.relatedEvidence.size() + 1
                << ",\n"
                << indent
                << "  \"relatedEvidence\": [\n";

            for (size_t i = 0;
                i < finding.relatedEvidence.size();
                ++i)
            {
                WriteEvidence(
                    out,
                    finding.relatedEvidence[i],
                    indent + "    ");

                if (i + 1 < finding.relatedEvidence.size())
                    out << ',';

                out << '\n';
            }

            out << indent
                << "  ]\n"
                << indent;
        }

        out << "}";
    }
}

namespace JsonReporter
{
    std::string Serialize(
        const AuditResult& result)
    {
        std::ostringstream out;

        out << "{\n";

        out << "  \"reportSchemaVersion\": \"1.3\",\n";

        out << "  \"targetUrl\": "
            << QuoteUrl(result.targetUrl)
            << ",\n";

        out << "  \"scopePolicy\": {\n"
            << "    \"allowedHosts\": ";
        WriteStringArray(
            out,
            result.scopePolicy.allowedHosts);
        out << ",\n"
            << "    \"allowedDomains\": ";
        WriteStringArray(
            out,
            result.scopePolicy.allowedDomains);
        out << ",\n"
            << "    \"allowedPaths\": ";
        WriteStringArray(
            out,
            result.scopePolicy.allowedPaths);
        out << "\n  },\n";

        const auto& configuration = result.configuration;
        out << "  \"effectiveConfiguration\": {\n"
            << "    \"targetUrl\": " << QuoteUrl(configuration.targetUrl) << ",\n"
            << "    \"projectName\": " << Quote(configuration.projectName) << ",\n"
            << "    \"maxRequests\": " << configuration.maxRequests << ",\n"
            << "    \"minIntervalMs\": " << configuration.minIntervalMs << ",\n"
            << "    \"timeoutMs\": " << configuration.timeoutMs << ",\n"
            << "    \"timeoutProfile\": "
            << Quote(configuration.useLegacyTimeoutProfile ? "legacy" : "uniform") << ",\n"
            << "    \"followRedirects\": "
            << (configuration.followRedirects ? "true" : "false") << ",\n"
            << "    \"outputFormat\": "
            << Quote(AuditOutputFormatToString(configuration.outputFormat)) << ",\n"
            << "    \"outputPath\": " << Quote(configuration.outputPath) << ",\n"
            << "    \"persistProject\": "
            << (configuration.persistProject ? "true" : "false") << ",\n"
            << "    \"reportOptions\": {\n"
            << "      \"includeEvidence\": "
            << (configuration.reportOptions.includeEvidence ? "true" : "false") << ",\n"
            << "      \"includeProbeHistory\": "
            << (configuration.reportOptions.includeProbeHistory ? "true" : "false") << "\n"
            << "    }\n"
            << "  },\n";

        const auto& budget = result.requestBudget;
        out << "  \"requestBudget\": {\n"
            << "    \"maxRequestsPerRun\": " << budget.maxRequestsPerRun << ",\n"
            << "    \"minRequestIntervalMs\": " << budget.minRequestIntervalMs << ",\n"
            << "    \"attemptedRequests\": " << budget.attemptedRequests << ",\n"
            << "    \"successfulRequests\": " << budget.successfulRequests << ",\n"
            << "    \"failedRequests\": " << budget.failedRequests << ",\n"
            << "    \"scopeRejectedRequests\": " << budget.scopeRejectedRequests << ",\n"
            << "    \"budgetRejectedRequests\": " << budget.budgetRejectedRequests << ",\n"
            << "    \"remainingRequests\": " << budget.remainingRequests << "\n"
            << "  },\n";

        out << "  \"probeCount\": "
            << result.probes.size()
            << ",\n";

        out << "  \"findingCount\": "
            << result.findings.size()
            << ",\n";

        out << "  \"endpointCount\": "
            << result.endpoints.size()
            << ",\n";

        size_t lowRiskCount = 0;
        size_t mediumRiskCount = 0;
        size_t highRiskCount = 0;
        size_t criticalRiskCount = 0;
        size_t totalRiskScore = 0;

        for (const auto& endpoint : result.endpoints)
        {
            totalRiskScore += static_cast<size_t>(
                endpoint.riskScore.value);

            switch (endpoint.riskScore.level)
            {
            case EndpointRiskLevel::Medium:
                ++mediumRiskCount;
                break;

            case EndpointRiskLevel::High:
                ++highRiskCount;
                break;

            case EndpointRiskLevel::Critical:
                ++criticalRiskCount;
                break;

            case EndpointRiskLevel::Low:
            default:
                ++lowRiskCount;
                break;
            }
        }

        const size_t averageRiskScore =
            result.endpoints.empty()
            ? 0
            : totalRiskScore / result.endpoints.size();

        out << "  \"riskSummary\": {\n"
            << "    \"low\": " << lowRiskCount << ",\n"
            << "    \"medium\": " << mediumRiskCount << ",\n"
            << "    \"high\": " << highRiskCount << ",\n"
            << "    \"critical\": " << criticalRiskCount << ",\n"
            << "    \"averageScore\": " << averageRiskScore << "\n"
            << "  },\n";

        out << "  \"endpoints\": [\n";

        for (size_t i = 0;
            i < result.endpoints.size();
            ++i)
        {
            const auto& endpoint =
                result.endpoints[i];

            out << "    {\n";

            out << "      \"url\": "
                << QuoteUrl(endpoint.url)
                << ",\n";

            out << "      \"method\": "
                << Quote(endpoint.method)
                << ",\n";

            out << "      \"source\": "
                << Quote(endpoint.source)
                << ",\n";

            out << "      \"parameterHint\": "
                << Quote(endpoint.parameterHint)
                << ",\n";

            out << "      \"classification\": "
                << Quote(
                    EndpointClassToString(
                        endpoint.classification))
                << ",\n";

            out << "      \"riskScore\": {\n"
                << "        \"score\": "
                << endpoint.riskScore.value
                << ",\n"
                << "        \"level\": "
                << Quote(
                    EndpointRiskLevelToString(
                        endpoint.riskScore.level))
                << ",\n"
                << "        \"factors\": [\n";

            for (size_t f = 0;
                f < endpoint.riskScore.factors.size();
                ++f)
            {
                const auto& factor =
                    endpoint.riskScore.factors[f];

                out << "          {\n"
                    << "            \"signal\": "
                    << Quote(factor.signal)
                    << ",\n"
                    << "            \"points\": "
                    << factor.points
                    << ",\n"
                    << "            \"reason\": "
                    << Quote(factor.reason)
                    << '\n'
                    << "          }";

                if (f + 1 < endpoint.riskScore.factors.size())
                    out << ',';

                out << '\n';
            }

            out << "        ]\n"
                << "      },\n";

            out << "      \"riskHints\": [\n";

            for (size_t h = 0;
                h < endpoint.riskHints.size();
                ++h)
            {
                out << "        "
                    << Quote(endpoint.riskHints[h]);

                if (h + 1 <
                    endpoint.riskHints.size())
                {
                    out << ',';
                }

                out << '\n';
            }

            out << "      ],\n";

            out << "      \"parameters\": [\n";

            for (size_t p = 0;
                p < endpoint.parameters.size();
                ++p)
            {
                const auto& parameter =
                    endpoint.parameters[p];

                out << "        {\n";

                out << "          \"name\": "
                    << Quote(parameter.name)
                    << ",\n";

                out << "          \"value\": "
                    << Quote(SafeParameterValue(parameter))
                    << ",\n";

                out << "          \"location\": "
                    << Quote(parameter.location)
                    << ",\n";

                out << "          \"type\": "
                    << Quote(
                        ParameterTypeToString(
                            parameter.type))
                    << ",\n";

                out << "          \"risks\": [\n";

                for (size_t r = 0;
                    r < parameter.risks.size();
                    ++r)
                {
                    const auto& risk =
                        parameter.risks[r];

                    out << "            {\n"
                        << "              \"type\": "
                        << Quote(
                            ParameterRiskTypeToString(
                                risk.type))
                        << ",\n"
                        << "              \"level\": "
                        << Quote(
                            ParameterRiskLevelToString(
                                risk.level))
                        << ",\n"
                        << "              \"confidence\": "
                        << Quote(
                            ParameterConfidenceToString(
                                risk.confidence))
                        << ",\n"
                        << "              \"reason\": "
                        << Quote(risk.reason)
                        << '\n'
                        << "            }";

                    if (r + 1 <
                        parameter.risks.size())
                    {
                        out << ',';
                    }

                    out << '\n';
                }

                out << "          ]\n";

                out << "        }";

                if (p + 1 <
                    endpoint.parameters.size())
                {
                    out << ',';
                }

                out << '\n';
            }

            out << "      ]\n";
            out << "    }";

            if (i + 1 <
                result.endpoints.size())
            {
                out << ',';
            }

            out << '\n';
        }

        out << "  ],\n";

        out << "  \"probes\": [\n";

        if (configuration.reportOptions.includeProbeHistory)
        {
            for (size_t i = 0;
                i < result.probes.size();
                ++i)
            {
                WriteProbe(
                    out,
                    result.probes[i],
                    "    ");

                if (i + 1 < result.probes.size())
                    out << ',';

                out << '\n';
            }
        }

        out << "  ],\n";

        out << "  \"findings\": [\n";

        for (size_t i = 0;
            i < result.findings.size();
            ++i)
        {
            WriteFinding(
                out,
                result.findings[i],
                "    ",
                configuration.reportOptions.includeEvidence);

            if (i + 1 <
                result.findings.size())
            {
                out << ',';
            }

            out << '\n';
        }

        out << "  ]\n";
        out << "}\n";

        return out.str();
    }

    bool WriteToFile(
        const AuditResult& result,
        const std::string& path,
        std::string& error)
    {
        std::ofstream file(
            std::filesystem::u8path(path),
            std::ios::binary);

        if (!file.is_open())
        {
            error =
                "Unable to open output file: " +
                path;

            return false;
        }

        const std::string json =
            Serialize(result);

        file.write(
            json.data(),
            static_cast<std::streamsize>(
                json.size()));

        if (!file.good())
        {
            error =
                "Failed to write output file: " +
                path;

            return false;
        }

        return true;
    }
}
