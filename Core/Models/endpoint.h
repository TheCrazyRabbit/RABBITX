#pragma once

#include <string>
#include <vector>

#include "parameter.h"

enum class EndpointClass
{
    Unknown,
    Resource,
    Search,
    GraphQL,
    Authentication,
    Administrative,
    Upload,
    Download
};

enum class EndpointRiskLevel
{
    Low,
    Medium,
    High,
    Critical
};

struct RiskScoreFactor
{
    std::string signal;
    int points = 0;
    std::string reason;
};

struct RiskScore
{
    int value = 0;
    EndpointRiskLevel level =
        EndpointRiskLevel::Low;

    std::vector<RiskScoreFactor> factors;
};

struct Endpoint
{
    std::string url;
    std::string method;
    std::string source;
    std::string parameterHint;

    EndpointClass classification =
        EndpointClass::Unknown;

    std::vector<std::string> riskHints;

    std::vector<Parameter> parameters;

    RiskScore riskScore;
};

const char* EndpointClassToString(
    EndpointClass classification);

const char* EndpointRiskLevelToString(
    EndpointRiskLevel level);
