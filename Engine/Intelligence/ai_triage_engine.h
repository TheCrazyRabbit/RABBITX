#pragma once
#include <memory>
#include "Core/Models/ai_triage.h"
#include "Infrastructure/AI/ai_provider.h"
struct AuditResult;
namespace AITriageEngine
{
    // Future client entitlement integration point: gate calls here before Run.
    // Existing audit/report functionality is independent of this optional module.
    TriageContext BuildContext(const AuditResult& audit);
    std::string SerializeContext(const TriageContext& context);
    bool Validate(const std::string& json, const TriageContext& context,
        std::vector<TriageItem>& items, std::string& error);
    void CheckAction(TriageAction& action, const AuditResult& audit);
    AITriageResult Run(const AuditResult& audit, const std::shared_ptr<IAIProvider>& provider,
        int timeoutMs = 2000);
    std::string Serialize(const AITriageResult& result);
}
