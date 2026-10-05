#include "endpoint_risk_engine.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <utility>

namespace
{
    std::string Lower(
        std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c));
            });

        return value;
    }

    bool Contains(
        const std::string& value,
        const std::string& keyword)
    {
        return Lower(value).find(
            Lower(keyword)) !=
            std::string::npos;
    }

    void AddHint(
        Endpoint& endpoint,
        const std::string& hint)
    {
        if (std::find(
            endpoint.riskHints.begin(),
            endpoint.riskHints.end(),
            hint) ==
            endpoint.riskHints.end())
        {
            endpoint.riskHints.push_back(
                hint);
        }
    }

    void AddFactor(
        RiskScore& score,
        const std::string& signal,
        int points,
        const std::string& reason)
    {
        if (points <= 0)
            return;

        RiskScoreFactor factor;
        factor.signal = signal;
        factor.points = points;
        factor.reason = reason;

        score.factors.push_back(factor);
        score.value += points;
    }

    void AddClassificationFactor(
        RiskScore& score,
        EndpointClass classification)
    {
        switch (classification)
        {
        case EndpointClass::Resource:
            AddFactor(
                score,
                "Resource endpoint",
                10,
                "Object-oriented API paths can expose records that need authorization checks.");
            break;

        case EndpointClass::Search:
            AddFactor(
                score,
                "Search endpoint",
                7,
                "Search endpoints commonly process user-controlled input.");
            break;

        case EndpointClass::GraphQL:
            AddFactor(
                score,
                "GraphQL endpoint",
                12,
                "GraphQL exposes a structured API surface that merits operation and authorization review.");
            break;

        case EndpointClass::Authentication:
            AddFactor(
                score,
                "Authentication endpoint",
                14,
                "Authentication and token flows can affect account access.");
            break;

        case EndpointClass::Administrative:
            AddFactor(
                score,
                "Administrative endpoint",
                16,
                "Administrative paths may expose privileged operations.");
            break;

        case EndpointClass::Upload:
            AddFactor(
                score,
                "Upload endpoint",
                12,
                "Upload handling may involve file validation and storage controls.");
            break;

        case EndpointClass::Download:
            AddFactor(
                score,
                "Download endpoint",
                10,
                "Download handling may expose access-control and path-selection behavior.");
            break;

        case EndpointClass::Unknown:
        default:
            break;
        }
    }

    void AddHintFactors(
        RiskScore& score,
        const Endpoint& endpoint)
    {
        for (const auto& hint : endpoint.riskHints)
        {
            if (hint == "object-reference")
            {
                AddFactor(score, "Object reference hint", 7,
                    "The endpoint path suggests access to an individual object.");
            }
            else if (hint == "authentication-surface")
            {
                AddFactor(score, "Authentication surface hint", 5,
                    "The endpoint path is associated with authentication or session handling.");
            }
            else if (hint == "graphql-surface")
            {
                AddFactor(score, "GraphQL surface hint", 5,
                    "The endpoint path identifies a GraphQL interface.");
            }
            else if (hint == "administrative-surface")
            {
                AddFactor(score, "Administrative surface hint", 6,
                    "The endpoint path suggests privileged functionality.");
            }
            else if (hint == "file-upload-surface")
            {
                AddFactor(score, "File upload surface hint", 5,
                    "The endpoint path suggests file upload functionality.");
            }
            else if (hint == "file-download-surface")
            {
                AddFactor(score, "File download surface hint", 4,
                    "The endpoint path suggests file download functionality.");
            }
            else if (hint == "search-parameter")
            {
                AddFactor(score, "Search surface hint", 3,
                    "The endpoint path suggests search behavior.");
            }
            else if (hint == "identifier-parameter")
            {
                AddFactor(score, "Identifier parameter hint", 7,
                    "A parameter name suggests that the request selects a specific object.");
            }
            else if (hint == "pagination-parameter")
            {
                AddFactor(score, "Pagination parameter hint", 2,
                    "A parameter controls a result boundary or page window.");
            }
            else if (hint == "url-like-parameter")
            {
                AddFactor(score, "URL-like parameter hint", 4,
                    "A parameter may contain a redirect destination or remote resource.");
            }
            else if (hint == "file-related-parameter")
            {
                AddFactor(score, "File-related parameter hint", 4,
                    "A parameter may select a file or filesystem path.");
            }
            else if (hint == "search-input")
            {
                AddFactor(score, "Search input hint", 3,
                    "A parameter name suggests user-controlled search input.");
            }
        }
    }

    int RiskWeight(
        const ParameterRisk& risk)
    {
        int points = 0;

        switch (risk.type)
        {
        case ParameterRiskType::IDORCandidate:
            points = 26;
            break;

        case ParameterRiskType::SQLInjectionCandidate:
        case ParameterRiskType::XSSCandidate:
            points = 12;
            break;

        case ParameterRiskType::SSRFCandidate:
            points = 15;
            break;

        case ParameterRiskType::OpenRedirectCandidate:
            points = 8;
            break;

        case ParameterRiskType::OAuthReviewCandidate:
            points = 14;
            break;

        case ParameterRiskType::SensitiveValueExposureCandidate:
            points = 28;
            break;

        case ParameterRiskType::PathTraversalCandidate:
            points = 14;
            break;
        }

        if (risk.level == ParameterRiskLevel::Low)
            points = std::max(4, points - 6);
        else if (risk.level == ParameterRiskLevel::High)
            points += 4;

        if (risk.confidence == ParameterConfidence::Low)
            points = std::max(1, points - 3);

        return points;
    }

    RiskScore CalculateRiskScore(
        const Endpoint& endpoint)
    {
        RiskScore score;

        AddClassificationFactor(
            score,
            endpoint.classification);

        AddHintFactors(
            score,
            endpoint);

        std::map<ParameterRiskType, RiskScoreFactor>
            strongestByRiskType;

        for (const auto& parameter : endpoint.parameters)
        {
            for (const auto& risk : parameter.risks)
            {
                const int points = RiskWeight(risk);
                auto existing = strongestByRiskType.find(risk.type);

                if (existing != strongestByRiskType.end() &&
                    existing->second.points >= points)
                {
                    continue;
                }

                RiskScoreFactor factor;
                factor.signal = parameter.name + ": " +
                    ParameterRiskTypeToString(risk.type);
                factor.points = points;
                factor.reason = risk.reason;
                strongestByRiskType[risk.type] = factor;
            }
        }

        for (const auto& entry : strongestByRiskType)
        {
            AddFactor(
                score,
                entry.second.signal,
                entry.second.points,
                entry.second.reason);
        }

        std::stable_sort(
            score.factors.begin(),
            score.factors.end(),
            [](const RiskScoreFactor& left,
                const RiskScoreFactor& right)
            {
                return left.points > right.points;
            });

        int cappedTotal = 0;

        for (auto& factor : score.factors)
        {
            const int remaining =
                100 - cappedTotal;

            factor.points =
                std::min(factor.points, remaining);

            cappedTotal += factor.points;
        }

        score.value = cappedTotal;

        if (score.value >= 80)
            score.level = EndpointRiskLevel::Critical;
        else if (score.value >= 50)
            score.level = EndpointRiskLevel::High;
        else if (score.value >= 20)
            score.level = EndpointRiskLevel::Medium;
        else
            score.level = EndpointRiskLevel::Low;

        return score;
    }
}

void EndpointRiskEngine::Analyze(
    Endpoint& endpoint)
{
    endpoint.riskScore = RiskScore();

    const std::string url =
        Lower(endpoint.url);

    /*
     * Authentication surface
     */
    if (Contains(url, "/oauth") ||
        Contains(url, "/auth") ||
        Contains(url, "/login") ||
        Contains(url, "/token") ||
        Contains(url, "/session"))
    {
        endpoint.classification =
            EndpointClass::Authentication;

        AddHint(
            endpoint,
            "authentication-surface");
    }

    /*
     * GraphQL
     */
    else if (Contains(url, "/graphql"))
    {
        endpoint.classification =
            EndpointClass::GraphQL;

        AddHint(
            endpoint,
            "graphql-surface");
    }

    /*
     * Administrative surface
     */
    else if (Contains(url, "/admin") ||
        Contains(url, "/manage") ||
        Contains(url, "/internal"))
    {
        endpoint.classification =
            EndpointClass::Administrative;

        AddHint(
            endpoint,
            "administrative-surface");
    }

    /*
     * Upload / download
     */
    else if (Contains(url, "/upload"))
    {
        endpoint.classification =
            EndpointClass::Upload;

        AddHint(
            endpoint,
            "file-upload-surface");
    }
    else if (Contains(url, "/download"))
    {
        endpoint.classification =
            EndpointClass::Download;

        AddHint(
            endpoint,
            "file-download-surface");
    }

    /*
     * Search
     */
    else if (Contains(url, "/search") ||
        Contains(url, "/query") ||
        Contains(url, "/find"))
    {
        endpoint.classification =
            EndpointClass::Search;

        AddHint(
            endpoint,
            "search-parameter");
    }

    /*
     * Resource / object endpoint
     */
    else if (Contains(url, "/user") ||
        Contains(url, "/users") ||
        Contains(url, "/account") ||
        Contains(url, "/accounts") ||
        Contains(url, "/item") ||
        Contains(url, "/items") ||
        Contains(url, "/resource") ||
        Contains(url, "/object"))
    {
        endpoint.classification =
            EndpointClass::Resource;

        AddHint(
            endpoint,
            "object-reference");
    }

    /*
     * Parameter-driven risk hints
     */
    for (const auto& parameter :
        endpoint.parameters)
    {
        const std::string name =
            Lower(parameter.name);

        if (parameter.type == ParameterType::Identifier ||
            name == "id" ||
            name == "user_id" ||
            name == "userid" ||
            name == "account_id" ||
            name == "accountid" ||
            name == "object_id" ||
            name == "resource_id")
        {
            AddHint(
                endpoint,
                "identifier-parameter");
        }

        if (parameter.type == ParameterType::Pagination ||
            name == "page" ||
            name == "page_size" ||
            name == "limit" ||
            name == "offset")
        {
            AddHint(
                endpoint,
                "pagination-parameter");
        }

        if (parameter.type == ParameterType::UrlOrHost ||
            parameter.type == ParameterType::RedirectTarget ||
            parameter.type == ParameterType::Callback ||
            name == "url" ||
            name == "uri" ||
            name == "redirect" ||
            name == "redirect_url" ||
            name == "callback")
        {
            AddHint(
                endpoint,
                "url-like-parameter");
        }

        if (parameter.type == ParameterType::FilePath ||
            name == "file" ||
            name == "filename" ||
            name == "path")
        {
            AddHint(
                endpoint,
                "file-related-parameter");
        }

        if (parameter.type == ParameterType::SearchInput ||
            name == "q" ||
            name == "query" ||
            name == "search" ||
            name == "keyword")
        {
            AddHint(
                endpoint,
                "search-input");
        }
    }

    /*
     * Unknown endpoint with parameters
     */
    if (endpoint.classification ==
        EndpointClass::Unknown &&
        !endpoint.parameters.empty())
    {
        endpoint.classification =
            EndpointClass::Resource;
    }

    endpoint.riskScore =
        CalculateRiskScore(endpoint);
}
