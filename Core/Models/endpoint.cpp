#include "endpoint.h"

const char* EndpointClassToString(
    EndpointClass classification)
{
    switch (classification)
    {
    case EndpointClass::Resource:
        return "RESOURCE";

    case EndpointClass::Search:
        return "SEARCH";

    case EndpointClass::GraphQL:
        return "GRAPHQL";

    case EndpointClass::Authentication:
        return "AUTH";

    case EndpointClass::Administrative:
        return "ADMIN";

    case EndpointClass::Upload:
        return "UPLOAD";

    case EndpointClass::Download:
        return "DOWNLOAD";

    case EndpointClass::Unknown:
    default:
        return "UNKNOWN";
    }
}

const char* EndpointRiskLevelToString(
    EndpointRiskLevel level)
{
    switch (level)
    {
    case EndpointRiskLevel::Medium:
        return "MEDIUM";

    case EndpointRiskLevel::High:
        return "HIGH";

    case EndpointRiskLevel::Critical:
        return "CRITICAL";

    case EndpointRiskLevel::Low:
    default:
        return "LOW";
    }
}
