#include "console_ui.h"

#include <iostream>

#include "parameter_intelligence.h"

namespace
{
    std::string DisplayParameterValue(
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
}

namespace
{
    const char* ProtocolToString(
        const HttpResponse& response)
    {
        return response.isHttps
            ? "HTTPS"
            : "HTTP";
    }
}

namespace ConsoleUI
{
    void PrintBanner()
    {
        std::cout << R"(
========================================
              RABBITX
       API Security Intelligence
========================================
)" << '\n';
    }

    std::string ReadTargetUrl()
    {
        std::cout << "Target URL: ";

        std::string url;
        std::getline(std::cin, url);

        return url;
    }

    void PrintRequestStart()
    {
        std::cout
            << "\n[*] Starting audit...\n";
    }

    void PrintResponseSummary(
        const HttpResponse& response)
    {
        std::cout
            << "[+] Method: "
            << response.method
            << '\n';

        std::cout
            << "[+] Protocol: "
            << ProtocolToString(response)
            << '\n';

        std::cout
            << "[+] HTTP Status: "
            << response.statusCode
            << '\n';

        std::cout
            << "[+] Final URL: "
            << ParameterIntelligence::RedactSensitiveValues(
                response.finalUrl)
            << '\n';

        std::cout
            << "[+] Headers: "
            << response.headers.size()
            << '\n';

        std::cout
            << "[+] Body: "
            << response.body.size()
            << " bytes\n";
    }

    void PrintRequestError(
        const HttpResponse& response)
    {
        std::cout
            << "[-] Request failed.\n";

        if (!response.error.empty())
        {
            std::cout
                << "    Error: "
                << response.error
                << '\n';
        }
    }

    void PrintInvalidUrl()
    {
        std::cout
            << "[-] URL cannot be empty.\n";
    }

    void PrintProbeStart(
        const Probe& probe)
    {
        std::cout
            << "\n[ Probe ]\n"
            << "[*] "
            << probe.id
            << " - "
            << probe.name
            << '\n';
    }

    void PrintProbeResult(
        const ProbeResult& result)
    {
        if (!result.executed)
        {
            if (result.response.disposition ==
                HttpRequestDisposition::BudgetRejected)
            {
                std::cout << "[-] Request budget rejected the HTTP request.\n";
            }
            else if (result.response.disposition ==
                HttpRequestDisposition::ScopeRejected)
            {
                std::cout << "[-] Scope policy rejected the HTTP request.\n";
            }
            else
            {
                std::cout << "[-] Probe did not complete.\n";
            }

            if (!result.error.empty())
            {
                std::cout
                    << "    Error: "
                    << result.error
                    << '\n';
            }

            return;
        }

        std::cout
            << "[+] Probe completed.\n";

        PrintResponseSummary(
            result.response);
    }

    void PrintEndpoints(
        const std::vector<Endpoint>& endpoints)
    {
        std::cout
            << "\n[ Endpoints ]\n\n";

        if (endpoints.empty())
        {
            std::cout
                << "[+] No candidate endpoints discovered.\n";

            return;
        }

        for (const auto& endpoint :
            endpoints)
        {
            std::cout
                << "[+] "
                << endpoint.method
                << " "
                << ParameterIntelligence::RedactSensitiveValues(
                    endpoint.url)
                << '\n';

            std::cout
                << "    Source: "
                << endpoint.source
                << '\n';

            std::cout
                << "    Class: "
                << EndpointClassToString(
                    endpoint.classification)
                << '\n';

            std::cout
                << "    Risk Score: "
                << endpoint.riskScore.value
                << "/100 ("
                << EndpointRiskLevelToString(
                    endpoint.riskScore.level)
                << ")\n";

            if (!endpoint.riskScore.factors.empty())
            {
                std::cout
                    << "    Score Factors:\n";

                for (const auto& factor :
                    endpoint.riskScore.factors)
                {
                    std::cout
                        << "      - +"
                        << factor.points
                        << " "
                        << factor.signal
                        << ": "
                        << factor.reason
                        << '\n';
                }
            }

            if (!endpoint.parameterHint.empty())
            {
                std::cout
                    << "    "
                    << endpoint.parameterHint
                    << '\n';

                for (const auto& parameter :
                    endpoint.parameters)
                {
                    std::cout
                        << "      - "
                        << parameter.name
                        << " = "
                        << DisplayParameterValue(parameter)
                        << " ["
                        << parameter.location
                        << "] Type: "
                        << ParameterTypeToString(
                            parameter.type)
                        << '\n';

                    for (const auto& risk :
                        parameter.risks)
                    {
                        std::cout
                            << "        Risk Candidate: "
                            << ParameterRiskTypeToString(
                                risk.type)
                            << " | Risk: "
                            << ParameterRiskLevelToString(
                                risk.level)
                            << " | Confidence: "
                            << ParameterConfidenceToString(
                                risk.confidence)
                            << '\n'
                            << "          Reason: "
                            << risk.reason
                            << '\n';
                    }
                }
            }

            if (!endpoint.riskHints.empty())
            {
                std::cout
                    << "    Risk Hints:\n";

                for (const auto& hint :
                    endpoint.riskHints)
                {
                    std::cout
                        << "      - "
                        << hint
                        << '\n';
                }
            }

            std::cout << '\n';
        }

        std::cout
            << "[+] Endpoint candidates: "
            << endpoints.size()
            << '\n';
    }

    void PrintProbePlans(
        const std::vector<ProbePlan>& plans)
    {
        std::cout
            << "\n[ Probe Plans ]\n\n";

        if (plans.empty())
        {
            std::cout
                << "[+] No next-step probes planned.\n";

            return;
        }

        for (const auto& plan :
            plans)
        {
            std::cout
                << "[>] "
                << ProbePlanTypeToString(
                    plan.type)
                << '\n';

            std::cout
                << "    "
                << plan.method
                << " "
                << ParameterIntelligence::RedactSensitiveValues(
                    plan.endpointUrl)
                << '\n';

            std::cout
                << "    Reason: "
                << plan.reason
                << '\n';

            if (!plan.parameters.empty())
            {
                std::cout
                    << "    Parameters: ";

                for (size_t i = 0;
                    i < plan.parameters.size();
                    ++i)
                {
                    if (i > 0)
                        std::cout << ", ";

                    std::cout
                        << plan.parameters[i];
                }

                std::cout << '\n';
            }

            std::cout << '\n';
        }

        std::cout
            << "[+] Planned probes: "
            << plans.size()
            << '\n';
    }

    void PrintFindings(
        const std::vector<Finding>& findings)
    {
        std::cout
            << "\n[ Findings ]\n\n";

        if (findings.empty())
        {
            std::cout
                << "[+] No configured security findings.\n";

            return;
        }

        for (const auto& finding :
            findings)
        {
            PrintFinding(finding);
        }

        std::cout
            << "[+] Analysis completed: "
            << findings.size()
            << " finding(s).\n";
    }

    void WaitForExit()
    {
        std::cout
            << "\nPress Enter to exit...";

        std::cin.get();
    }
}
