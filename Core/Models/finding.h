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

enum class FindingStatus { Informational, Candidate, Confirmed };
const char* FindingStatusToString(FindingStatus status);

struct Finding
{
    std::string ruleId;
    std::string category;

    Severity severity = Severity::Info;
    Confidence confidence = Confidence::Low;

    std::string title;
    std::string description;

    std::string evidence;

    Evidence requestEvidence;

    std::vector<Evidence> relatedEvidence;
    FindingStatus status = FindingStatus::Informational;
};

void PrintFinding(
    const Finding& finding);
