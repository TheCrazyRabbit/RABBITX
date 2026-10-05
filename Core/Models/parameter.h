#pragma once

#include <string>
#include <vector>

enum class ParameterType
{
    Generic,
    Identifier,
    SearchInput,
    Pagination,
    Filter,
    RedirectTarget,
    Callback,
    OAuthControl,
    UrlOrHost,
    FilePath,
    Credential
};

enum class ParameterRiskType
{
    IDORCandidate,
    SQLInjectionCandidate,
    XSSCandidate,
    SSRFCandidate,
    OpenRedirectCandidate,
    OAuthReviewCandidate,
    SensitiveValueExposureCandidate,
    PathTraversalCandidate
};

enum class ParameterRiskLevel
{
    Low,
    Medium,
    High
};

enum class ParameterConfidence
{
    Low,
    Medium,
    High
};

struct ParameterRisk
{
    ParameterRiskType type =
        ParameterRiskType::IDORCandidate;

    ParameterRiskLevel level =
        ParameterRiskLevel::Low;

    ParameterConfidence confidence =
        ParameterConfidence::Low;

    std::string reason;
};

struct Parameter
{
    std::string name;
    std::string value;
    std::string location;

    ParameterType type =
        ParameterType::Generic;

    std::vector<ParameterRisk> risks;
};

const char* ParameterTypeToString(
    ParameterType type);

const char* ParameterRiskTypeToString(
    ParameterRiskType type);

const char* ParameterRiskLevelToString(
    ParameterRiskLevel level);

const char* ParameterConfidenceToString(
    ParameterConfidence confidence);
