#include "parameter.h"

const char* ParameterTypeToString(
    ParameterType type)
{
    switch (type)
    {
    case ParameterType::Identifier:
        return "IDENTIFIER";

    case ParameterType::SearchInput:
        return "SEARCH_INPUT";

    case ParameterType::Pagination:
        return "PAGINATION";

    case ParameterType::Filter:
        return "FILTER";

    case ParameterType::RedirectTarget:
        return "REDIRECT_TARGET";

    case ParameterType::Callback:
        return "CALLBACK";

    case ParameterType::OAuthControl:
        return "OAUTH_CONTROL";

    case ParameterType::UrlOrHost:
        return "URL_OR_HOST";

    case ParameterType::FilePath:
        return "FILE_PATH";

    case ParameterType::Credential:
        return "CREDENTIAL";

    case ParameterType::Generic:
    default:
        return "GENERIC";
    }
}

const char* ParameterRiskTypeToString(
    ParameterRiskType type)
{
    switch (type)
    {
    case ParameterRiskType::IDORCandidate:
        return "IDOR candidate";

    case ParameterRiskType::SQLInjectionCandidate:
        return "SQL injection candidate";

    case ParameterRiskType::XSSCandidate:
        return "XSS candidate";

    case ParameterRiskType::SSRFCandidate:
        return "SSRF candidate";

    case ParameterRiskType::OpenRedirectCandidate:
        return "Open redirect candidate";

    case ParameterRiskType::OAuthReviewCandidate:
        return "OAuth review candidate";

    case ParameterRiskType::SensitiveValueExposureCandidate:
        return "Sensitive value exposure candidate";

    case ParameterRiskType::PathTraversalCandidate:
        return "Path traversal candidate";

    default:
        return "Unknown candidate";
    }
}

const char* ParameterRiskLevelToString(
    ParameterRiskLevel level)
{
    switch (level)
    {
    case ParameterRiskLevel::High:
        return "HIGH";

    case ParameterRiskLevel::Medium:
        return "MEDIUM";

    case ParameterRiskLevel::Low:
    default:
        return "LOW";
    }
}

const char* ParameterConfidenceToString(
    ParameterConfidence confidence)
{
    switch (confidence)
    {
    case ParameterConfidence::High:
        return "HIGH";

    case ParameterConfidence::Medium:
        return "MEDIUM";

    case ParameterConfidence::Low:
    default:
        return "LOW";
    }
}
