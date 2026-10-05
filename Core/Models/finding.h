#pragma once

#include <string>
#include <vector>

#include "evidence.h"

enum class Severity
{
    Info,
    Low
};

enum class Confidence
{
    Low,
    Medium,
    High
};

struct Finding
{
    std::string ruleId;
    std::string category;

    Severity severity;
    Confidence confidence;

    std::string title;
    std::string description;

    std::string evidence;

    Evidence requestEvidence;

    std::vector<Evidence> relatedEvidence;
};

void PrintFinding(
    const Finding& finding);
