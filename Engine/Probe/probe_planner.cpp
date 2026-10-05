#include "probe_planner.h"

#include <algorithm>
#include <set>
#include <string>

namespace
{
    bool HasHint(
        const Endpoint& endpoint,
        const std::string& hint)
    {
        return std::find(
            endpoint.riskHints.begin(),
            endpoint.riskHints.end(),
            hint) !=
            endpoint.riskHints.end();
    }

    void AddPlan(
        std::vector<ProbePlan>& plans,
        std::set<std::string>& known,
        ProbePlanType type,
        const Endpoint& endpoint,
        const std::string& reason,
        const std::vector<std::string>& parameters)
    {
        const std::string key =
            std::string(ProbePlanTypeToString(type))
            + "|"
            + endpoint.url;

        if (!known.insert(key).second)
            return;

        ProbePlan plan;

        plan.type = type;
        plan.endpointUrl = endpoint.url;
        plan.method = endpoint.method;
        plan.reason = reason;
        plan.parameters = parameters;

        plans.push_back(plan);
    }
}

std::vector<ProbePlan>
ProbePlanner::BuildPlans(
    const std::vector<Endpoint>& endpoints)
{
    std::vector<ProbePlan> plans;

    std::set<std::string> known;

    for (const auto& endpoint : endpoints)
    {
        std::vector<std::string>
            identifierParameters;

        std::vector<std::string>
            searchParameters;

        std::vector<std::string>
            paginationParameters;

        for (const auto& parameter :
            endpoint.parameters)
        {
            const std::string& name =
                parameter.name;

            if (parameter.type == ParameterType::Identifier ||
                name == "id" ||
                name == "user_id" ||
                name == "userid" ||
                name == "account_id" ||
                name == "accountid" ||
                name == "object_id" ||
                name == "resource_id")
            {
                identifierParameters.push_back(
                    name);
            }

            if (parameter.type == ParameterType::SearchInput ||
                name == "q" ||
                name == "query" ||
                name == "search" ||
                name == "keyword")
            {
                searchParameters.push_back(
                    name);
            }

            if (parameter.type == ParameterType::Pagination ||
                name == "page" ||
                name == "page_size" ||
                name == "limit" ||
                name == "offset")
            {
                paginationParameters.push_back(
                    name);
            }
        }

        if (HasHint(
            endpoint,
            "identifier-parameter"))
        {
            AddPlan(
                plans,
                known,
                ProbePlanType::IdentifierVariation,
                endpoint,
                "Endpoint contains an object identifier that may warrant controlled identifier-behavior testing.",
                identifierParameters);
        }

        if (HasHint(
            endpoint,
            "search-input"))
        {
            AddPlan(
                plans,
                known,
                ProbePlanType::SearchParameterAnalysis,
                endpoint,
                "Endpoint accepts a search-like parameter that may produce meaningful response differences.",
                searchParameters);
        }

        if (HasHint(
            endpoint,
            "pagination-parameter"))
        {
            AddPlan(
                plans,
                known,
                ProbePlanType::PaginationAnalysis,
                endpoint,
                "Endpoint exposes pagination controls that may affect result boundaries or access behavior.",
                paginationParameters);
        }

        if (endpoint.classification ==
            EndpointClass::GraphQL)
        {
            AddPlan(
                plans,
                known,
                ProbePlanType::GraphQLInspection,
                endpoint,
                "GraphQL endpoint detected; schema and operation behavior should be inspected before deeper testing.",
                {});
        }

        if (endpoint.classification ==
            EndpointClass::Authentication)
        {
            AddPlan(
                plans,
                known,
                ProbePlanType::AuthenticationInspection,
                endpoint,
                "Authentication-related endpoint detected; authentication behavior should be characterized before active testing.",
                {});
        }
    }

    return plans;
}
