#include "probe_plan.h"

const char* ProbePlanTypeToString(
    ProbePlanType type)
{
    switch (type)
    {
    case ProbePlanType::IdentifierVariation:
        return "identifier-variation";

    case ProbePlanType::SearchParameterAnalysis:
        return "search-parameter-analysis";

    case ProbePlanType::PaginationAnalysis:
        return "pagination-analysis";

    case ProbePlanType::GraphQLInspection:
        return "graphql-inspection";

    case ProbePlanType::AuthenticationInspection:
        return "authentication-inspection";

    case ProbePlanType::None:
    default:
        return "none";
    }
}