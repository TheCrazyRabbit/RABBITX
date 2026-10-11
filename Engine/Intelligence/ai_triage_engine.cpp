#include "ai_triage_engine.h"
#include "pipeline.h"
#include "parameter_intelligence.h"
#include "Core/Models/strict_json.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <set>
#include <sstream>
#include <thread>

namespace
{
    void Strings(std::ostream& out, const std::vector<std::string>& values)
    { out << '['; for(size_t i=0;i<values.size();++i){if(i)out<<',';out<<StrictJson::Quote(values[i]);}out<<']'; }
    std::string Text(const StrictJson::Value& value, size_t max = 2000)
    {
        auto s=value.String();
        if(s.empty() || s.size()>max) throw std::runtime_error("Empty or oversized text field");
        for(unsigned char c:s)if(c<32 && c!='\n' && c!='\t')throw std::runtime_error("Control character in text");
        return s;
    }
    std::vector<std::string> References(const StrictJson::Value& value)
    {
        if(value.kind!=StrictJson::Value::Kind::Array || value.array.empty() || value.array.size()>64)
            throw std::runtime_error("Evidence and fact references must be nonempty bounded arrays");
        std::vector<std::string> out;std::set<std::string> seen;
        for(const auto& v:value.array){auto s=Text(v,80);if(!seen.insert(s).second)throw std::runtime_error("Duplicate reference");out.push_back(s);}return out;
    }
}

TriageContext AITriageEngine::BuildContext(const AuditResult& audit)
{
    TriageContext context;
    auto add=[&](TriageSubject subject)
    {
        if(context.subjects.size()>=64 || subject.evidenceIds.empty()) {++context.omittedSubjects;return;}
        subject.url=ParameterIntelligence::RedactSensitiveValues(subject.url);
        if(subject.url.size()>8192) {++context.omittedSubjects;return;}
        if(subject.evidenceIds.size()>4)subject.evidenceIds.resize(4);
        for(const auto& id:subject.evidenceIds)
        {
            const auto it=std::find_if(audit.evidenceCatalog.begin(),audit.evidenceCatalog.end(),[&](const Evidence& e){return e.id==id;});
            if(it==audit.evidenceCatalog.end()) continue;
            TriageFact fact;
            fact.id="fact-"+std::to_string(context.facts.size()+1);
            fact.evidenceId=id;
            fact.description=subject.kind=="endpoint"
                ? "A discovery response (HTTP " + std::to_string(it->statusCode) + ") contains this endpoint reference. " + subject.description + " classification and risk are heuristic candidates, not proof of vulnerability."
                : "The local analyzer recorded rule " + subject.description + " on HTTP status " + std::to_string(it->statusCode) + ". The rule is an observation, not proof of exploitability.";
            context.facts.push_back(fact);subject.factIds.push_back(fact.id);
        }
        if(!subject.factIds.empty()) context.subjects.push_back(std::move(subject));
    };
    for(size_t i=0;i<audit.endpoints.size();++i)
    {
        TriageSubject s; s.id="endpoint-"+std::to_string(i+1);s.kind="endpoint";
        s.url=audit.endpoints[i].url;s.riskScore=audit.endpoints[i].riskScore.value;
        s.description=std::string(EndpointClassToString(audit.endpoints[i].classification));
        for(const auto& p:audit.endpoints[i].parameters)
            if(s.parameterSignals.size()<16)s.parameterSignals.push_back(ParameterTypeToString(p.type));
        for(const auto& f:audit.endpoints[i].riskScore.factors)
            if(s.riskFactors.size()<16 && f.signal.size()+f.reason.size()<2000)s.riskFactors.push_back(f.signal+": "+f.reason);
        s.evidenceIds=audit.endpoints[i].discoveryEvidenceIds; add(std::move(s));
    }
    for(size_t i=0;i<audit.findings.size();++i)
    {
        const auto& f=audit.findings[i]; TriageSubject s;
        s.id="finding-"+std::to_string(i+1);s.kind="finding";
        s.url=f.requestEvidence.requestedUrl;s.description=f.ruleId;
        s.riskScore=f.status==FindingStatus::Candidate?35:10;
        if(!f.requestEvidence.id.empty())s.evidenceIds.push_back(f.requestEvidence.id);
        for(const auto& e:f.relatedEvidence)if(!e.id.empty()&&s.evidenceIds.size()<64)s.evidenceIds.push_back(e.id);
        add(std::move(s));
    }
    return context;
}

std::string AITriageEngine::SerializeContext(const TriageContext& context)
{
    std::ostringstream out;out << "{\"schemaVersion\":\"1.0\",\"untrustedInput\":true,\"omittedSubjects\":" << context.omittedSubjects << ",\"subjects\":[";
    for(size_t i=0;i<context.subjects.size();++i)
    {
        if(i)out<<',';const auto& s=context.subjects[i];
        out<<"{\"id\":"<<StrictJson::Quote(s.id)<<",\"url\":"<<StrictJson::Quote(s.url)<<",\"kind\":"<<StrictJson::Quote(s.kind)
            <<",\"riskScore\":"<<s.riskScore<<",\"description\":"<<StrictJson::Quote(s.description)<<",\"evidenceIds\":";
        Strings(out,s.evidenceIds);out<<",\"factIds\":";Strings(out,s.factIds);
        out<<",\"parameterSignals\":";Strings(out,s.parameterSignals);out<<",\"riskFactors\":";Strings(out,s.riskFactors);out<<'}';
    }
    out<<"],\"facts\":[";
    for(size_t i=0;i<context.facts.size();++i)
    {if(i)out<<',';const auto& f=context.facts[i];out<<"{\"id\":"<<StrictJson::Quote(f.id)<<",\"evidenceId\":"<<StrictJson::Quote(f.evidenceId)<<",\"description\":"<<StrictJson::Quote(f.description)<<'}';}
    out<<"]}";return out.str();
}

bool AITriageEngine::Validate(const std::string& json, const TriageContext& context,
    std::vector<TriageItem>& items, std::string& error)
{
    items.clear();error.clear();std::vector<TriageItem> valid;
    try
    {
        auto root=StrictJson::Parse(json);root.Fields({"schemaVersion","items"});
        if(root.At("schemaVersion").String()!="1.0")throw std::runtime_error("Unsupported AI schema version");
        const auto& list=root.At("items");
        if(list.kind!=StrictJson::Value::Kind::Array || list.array.size()>64)throw std::runtime_error("Invalid items array");
        std::set<std::string> subjects;
        for(const auto& row:list.array)
        {
            row.Fields({"subjectId","priority","assessment","evidenceIds","factIds","possibleRisk","uncertainty","missingValidation","nextAction"});
            TriageItem item;item.subjectId=Text(row.At("subjectId"),80);
            const auto subject=std::find_if(context.subjects.begin(),context.subjects.end(),[&](const TriageSubject& s){return s.id==item.subjectId;});
            if(subject==context.subjects.end() || !subjects.insert(item.subjectId).second)throw std::runtime_error("Unknown or repeated subject");
            item.priority=row.At("priority").Integer();if(item.priority<0||item.priority>100)throw std::runtime_error("Priority out of range");
            item.assessment=Text(row.At("assessment"),40);
            if(item.assessment!="candidate"&&item.assessment!="informational")throw std::runtime_error("AI cannot assert confirmed findings");
            item.evidenceIds=References(row.At("evidenceIds"));item.factIds=References(row.At("factIds"));
            for(const auto& id:item.evidenceIds)
                if(std::find(subject->evidenceIds.begin(),subject->evidenceIds.end(),id)==subject->evidenceIds.end())throw std::runtime_error("Evidence does not support this subject");
            for(const auto& id:item.factIds)
            {
                if(std::find(subject->factIds.begin(),subject->factIds.end(),id)==subject->factIds.end())throw std::runtime_error("Fact does not support this subject");
                auto fact=std::find_if(context.facts.begin(),context.facts.end(),[&](const TriageFact& f){return f.id==id;});
                if(fact==context.facts.end()||std::find(item.evidenceIds.begin(),item.evidenceIds.end(),fact->evidenceId)==item.evidenceIds.end())throw std::runtime_error("Fact requires its supporting evidence");
            }
            for(const auto& id:item.evidenceIds)
                if(std::none_of(context.facts.begin(),context.facts.end(),[&](const TriageFact& f){return f.evidenceId==id&&std::find(item.factIds.begin(),item.factIds.end(),f.id)!=item.factIds.end();}))throw std::runtime_error("Evidence requires a supporting fact");
            item.possibleRisk=Text(row.At("possibleRisk"));item.uncertainty=Text(row.At("uncertainty"));item.missingValidation=Text(row.At("missingValidation"));
            const auto& action=row.At("nextAction");action.Fields({"kind","method","url","reason"});
            item.nextAction.kind=Text(action.At("kind"),40);item.nextAction.method=Text(action.At("method"),10);
            item.nextAction.url=Text(action.At("url"),8192);item.nextAction.reason=Text(action.At("reason"));
            if(item.nextAction.url!=subject->url)throw std::runtime_error("Action endpoint differs from evidence-backed subject");
            if(!((item.nextAction.kind=="manual_review"&&item.nextAction.method=="NONE") || (item.nextAction.kind=="baseline_get"&&item.nextAction.method=="GET")))throw std::runtime_error("Action outside the recommendation allowlist");
            valid.push_back(std::move(item));
        }
        std::stable_sort(valid.begin(),valid.end(),[](const TriageItem& a,const TriageItem& b){return a.priority>b.priority;});
        items=std::move(valid);return true;
    }
    catch(const std::exception& e) {error=e.what();return false;}
}

void AITriageEngine::CheckAction(TriageAction& action, const AuditResult& audit)
{
    action.executed=false;action.requiresApproval=true;
    const auto scope=ScopePolicyEngine::Check(audit.scopePolicy,action.url);
    if(!scope.allowed){action.policyDecision="blocked_scope";return;}
    const bool known=action.url==audit.targetUrl||std::any_of(audit.endpoints.begin(),audit.endpoints.end(),[&](const Endpoint& e){return e.url==action.url;});
    if(!known){action.policyDecision="blocked_endpoint_or_redacted_url";return;}
    if(action.kind=="manual_review"&&action.method=="NONE") {action.policyDecision="manual_review_only";return;}
    if(action.kind!="baseline_get"||action.method!="GET") {action.policyDecision="blocked_method";return;}
    if(audit.requestBudget.remainingRequests==0){action.policyDecision="blocked_budget";return;}
    action.policyDecision="requires_user_approval_and_send_time_scope_budget_rate_check";
    // No executor call exists here. A future execution entry point must recheck
    // current policy and approval, then use HttpClient with the same run budget/rate limiter.
}

AITriageResult AITriageEngine::Run(const AuditResult& audit, const std::shared_ptr<IAIProvider>& provider, int timeoutMs)
{
    AITriageResult result;result.context=BuildContext(audit);
    if(!provider){result.status="unavailable";result.error="No AI provider configured. Core audit remains available.";return result;}
    result.provider=provider->Name();result.isMock=provider->IsMock();
    if(timeoutMs<1||timeoutMs>60000){result.status="invalid_configuration";result.error="AI timeout must be between 1 and 60000 ms.";return result;}
    if(result.context.subjects.empty()){result.status="no_evidence";return result;}
    auto cancelled=std::make_shared<std::atomic_bool>(false);
    auto input=SerializeContext(result.context);
    std::packaged_task<AIProviderReply()> task([provider,input,timeoutMs,cancelled]{return provider->Generate(input,timeoutMs,cancelled);});
    auto future=task.get_future();std::thread worker(std::move(task));
    if(future.wait_for(std::chrono::milliseconds(timeoutMs))!=std::future_status::ready)
    {cancelled->store(true);worker.detach();result.status="timeout";result.error="AI provider deadline exceeded; audit evidence retained.";return result;}
    worker.join();
    try
    {
        auto reply=future.get();
        if(reply.status!=AIProviderReply::Status::Ok)
        {result.status=reply.status==AIProviderReply::Status::Timeout?"timeout":"unavailable";result.error="AI provider did not return a usable response; audit evidence retained.";return result;}
        if(!Validate(reply.json,result.context,result.items,result.error)){result.status="invalid_output";return result;}
        for(auto& item:result.items)CheckAction(item.nextAction,audit);
        result.status="completed";
    }
    catch(...) {result.status="provider_error";result.error="AI provider failed; raw provider errors are not logged.";}
    return result;
}

std::string AITriageEngine::Serialize(const AITriageResult& result)
{
    std::ostringstream out;out<<"{\"schemaVersion\":\"1.0\",\"status\":"<<StrictJson::Quote(result.status)<<",\"provider\":"<<StrictJson::Quote(result.provider)
        <<",\"isMock\":"<<(result.isMock?"true":"false")<<",\"error\":"<<StrictJson::Quote(result.error)<<",\"context\":"<<SerializeContext(result.context)<<",\"items\":[";
    for(size_t i=0;i<result.items.size();++i)
    {
        if(i)out<<',';const auto& t=result.items[i];
        out<<"{\"subjectId\":"<<StrictJson::Quote(t.subjectId)<<",\"priority\":"<<t.priority<<",\"assessment\":"<<StrictJson::Quote(t.assessment)<<",\"evidenceIds\":";
        Strings(out,t.evidenceIds);out<<",\"factIds\":";Strings(out,t.factIds);
        out<<",\"possibleRisk\":"<<StrictJson::Quote(t.possibleRisk)<<",\"uncertainty\":"<<StrictJson::Quote(t.uncertainty)<<",\"missingValidation\":"<<StrictJson::Quote(t.missingValidation)
            <<",\"nextAction\":{\"kind\":"<<StrictJson::Quote(t.nextAction.kind)<<",\"method\":"<<StrictJson::Quote(t.nextAction.method)<<",\"url\":"<<StrictJson::Quote(t.nextAction.url)
            <<",\"reason\":"<<StrictJson::Quote(t.nextAction.reason)<<",\"policyDecision\":"<<StrictJson::Quote(t.nextAction.policyDecision)<<",\"requiresApproval\":true,\"executed\":false}}";
    }
    out<<"]}";return out.str();
}
