#pragma once

#include <string>
#include <vector>

#include "parameter.h"

namespace ParameterIntelligence
{
    std::vector<Parameter> Analyze(
        const std::string& url);

    std::string RedactSensitiveValues(
        const std::string& url);
}

class ParameterClassifier
{
public:
    static ParameterType Classify(
        const std::string& name);

    static bool ShouldRedactValue(
        const std::string& name);

    static std::vector<ParameterRisk> ClassifyRisks(
        const Parameter& parameter);
};

class ParameterAnalyzer
{
public:
    static std::vector<Parameter> Analyze(
        const std::string& url);
};
