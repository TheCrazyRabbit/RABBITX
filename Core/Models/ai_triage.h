#pragma once
#include <string>
#include <vector>

struct TriageAction
{
    std::string kind; // manual_review or baseline_get; no executable payload
    std::string method;
    std::string url;
    std::string reason;
    std::string policyDecision;
    bool requiresApproval = true;
    bool executed = false;
};
struct TriageItem
{
    std::string subjectId;
    int priority = 0;
    std::string assessment; // candidate or informational; never confirmed
    std::vector<std::string> evidenceIds;
    std::vector<std::string> factIds;
    std::string possibleRisk;
    std::string uncertainty;
    std::string missingValidation;
    TriageAction nextAction;
};
struct TriageFact
{
    std::string id;
    std::string evidenceId;
    std::string description;
};
struct TriageSubject
{
    std::string id;
    std::string url;
    std::string kind;
    int riskScore = 0;
    std::string description;
    std::vector<std::string> evidenceIds;
    std::vector<std::string> factIds;
    std::vector<std::string> parameterSignals;
    std::vector<std::string> riskFactors;
};
struct TriageContext
{
    std::vector<TriageSubject> subjects;
    std::vector<TriageFact> facts;
    size_t omittedSubjects = 0;
};
struct AITriageResult
{
    std::string status = "disabled";
    std::string provider = "none";
    bool isMock = false;
    std::string error;
    TriageContext context;
    std::vector<TriageItem> items;
};
