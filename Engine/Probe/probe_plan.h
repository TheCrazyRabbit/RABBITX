#pragma once

#include <string>
#include <vector>

#include "endpoint.h"

enum class ProbePlanType
{
    None,
    IdentifierVariation,
    SearchParameterAnalysis,
    PaginationAnalysis,
    GraphQLInspection,
    AuthenticationInspection
};

struct ProbePlan
{
    ProbePlanType type =
        ProbePlanType::None;

    std::string endpointUrl;
    std::string method;

    std::string reason;

    std::vector<std::string> parameters;
};

const char* ProbePlanTypeToString(
    ProbePlanType type);