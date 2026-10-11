#include "pipeline.h"
#include "ai_triage_engine.h"
#include "analyzer.h"
#include "probe_executor.h"
#include "parameter_intelligence.h"
#include "endpoint_intelligence.h"
#include "endpoint_risk_engine.h"
#include "security_checks.h"
#include "json_reporter.h"
#include "html_reporter.h"
#include "project_store.h"
#include "config_loader.h"
#include "Core/Models/strict_json.h"
#include <iostream>
#include <functional>
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>

namespace
{
    int passed=0,failed=0;
    void Check(bool condition,const char* name)
    {if(condition){++passed;std::cout<<"PASS "<<name<<'\n';}else{++failed;std::cerr<<"FAIL "<<name<<'\n';}}
    std::string Replace(std::string s,const std::string& a,const std::string& b)
    {auto p=s.find(a);if(p==std::string::npos)throw std::runtime_error("Test fixture missing replacement");s.replace(p,a.size(),b);return s;}
    struct FakeProvider:IAIProvider
    {
        AIProviderReply reply;int delay=0;bool throws=false;
        std::string Name()const override{return "test-mock";}
        bool IsMock()const override{return true;}
        AIProviderReply Generate(const std::string&,int,const std::shared_ptr<std::atomic_bool>& cancel)override
        {for(int i=0;i<delay;++i){if(cancel->load())return {AIProviderReply::Status::Timeout,{}};std::this_thread::sleep_for(std::chrono::milliseconds(1));}if(throws)throw std::runtime_error("secret-provider-error");return reply;}
    };
    AuditResult Fixture()
    {
        AuditResult a;a.targetUrl="http://127.0.0.1:8787/api/users?id=1";
        std::string error;ScopePolicyEngine::CreateDefault(a.targetUrl,a.scopePolicy,error);
        a.configuration.targetUrl=a.targetUrl;a.configuration.scope=a.scopePolicy;
        HttpResponse response;response.method="GET";response.requestedUrl=a.targetUrl;response.finalUrl=a.targetUrl;
        response.statusCode=200;response.disposition=HttpRequestDisposition::Succeeded;response.body="private-body";
        response.headers.emplace("Set-Cookie","secret=private-cookie");
        Evidence e=CaptureEvidence(response);e.id="evidence-1";a.evidenceCatalog.push_back(e);
        Endpoint endpoint;endpoint.url=a.targetUrl;endpoint.method="GET";endpoint.parameters=ParameterIntelligence::Analyze(endpoint.url);
        EndpointRiskEngine::Analyze(endpoint);endpoint.discoveryEvidenceIds.push_back(e.id);a.endpoints.push_back(endpoint);
        ProbeResult p;p.executed=true;p.response=response;p.evidence=e;a.probes.push_back(p);
        a.findings=Analyzer::AnalyzeProbe(p);return a;
    }
}

int main(int argc,char** argv)
{
    try
    {
        auto audit=Fixture();auto context=AITriageEngine::BuildContext(audit);
        auto mock=std::make_shared<MockAIProvider>();auto cancel=std::make_shared<std::atomic_bool>(false);
        auto json=mock->Generate(AITriageEngine::SerializeContext(context),2000,cancel).json;
        std::vector<TriageItem> items;std::string error;
        auto validate=[&](const std::string& s){return AITriageEngine::Validate(s,context,items,error);};
        Check(validate(json)&&!items.empty(),"valid structured Mock output");
        Check(!validate("not json"),"invalid provider JSON");
        Check(!validate("{}"),"missing required fields");
        Check(!validate(Replace(json,"\"priority\":", "\"execute\":true,\"priority\":")),"unexpected execution field");
        Check(!validate(Replace(json,"\"priority\":", "\"priority\":0,\"priority\":")),"duplicate JSON key");
        Check(!validate(Replace(json,"\"assessment\":\"candidate\"","\"assessment\":\"confirmed\"")),"AI cannot confirm vulnerability");
        Check(!validate(Replace(json,"evidence-1","evidence-does-not-exist")),"nonexistent evidence ID");
        Check(!validate(Replace(json,"[\"evidence-1\"]","[]")),"risk claim without evidence");
        Check(!validate(Replace(json,"fact-1","fact-does-not-exist")),"nonexistent fact ID");
        Check(!validate(Replace(json,"endpoint-1","subject-does-not-exist")),"nonexistent subject ID");
        Check(!validate(Replace(json,"\"priority\":"+std::to_string(audit.endpoints[0].riskScore.value),"\"priority\":101")),"out of range priority");
        Check(!validate(Replace(json,"\"method\":\"NONE\"","\"method\":\"POST\"")),"unsafe method rejected");
        Check(!validate(Replace(json,"\"url\":\""+audit.targetUrl+"\"","\"url\":\"https://outside.invalid/\"")),"hallucinated action target rejected");
        Check(!validate(Replace(json,"\"reason\":","\"payload\":\"delete everything\",\"reason\":")),"malicious command payload rejected");
        auto depth=std::string(30,'[')+"0"+std::string(30,']');Check(!validate(depth),"JSON depth bound");
        Check(!validate(std::string(1024*1024+1,' ')),"provider size bound");
        Check(!validate(Replace(json,"Mock:",std::string(1,char(0xff)))),"invalid UTF8 rejected");
        Check(!validate(Replace(json,"Mock:","\\ud800")),"invalid surrogate rejected");
        auto triage=AITriageEngine::Run(audit,mock);
        auto misleading=Replace(json,"Mock:","<script>execute_model_command()</script>Mock:");
        Check(validate(misleading),"model instructions remain inert hypothesis text");
        auto inert=triage;inert.items=items;
        auto inertAudit=audit;inertAudit.aiTriage=inert;
        auto inertHtml=HTMLReporter::Serialize("inert",1,"now",JsonReporter::Serialize(inertAudit));
        Check(inertHtml.find("<script>execute_model_command()") == std::string::npos && inertHtml.find("\\u003cscript\\u003eexecute_model_command()")!=std::string::npos,"malicious model markup escaped inside HTML data");
        Check(triage.status=="completed"&&triage.isMock,"Mock flow completed and explicitly labeled");
        Check(std::all_of(triage.items.begin(),triage.items.end(),[](const TriageItem& t){return !t.nextAction.executed&&t.nextAction.requiresApproval;}),"no recommendation executes");
        Check(audit.findings[0].status!=FindingStatus::Confirmed,"AI leaves finding certainty unchanged");
        auto otherSubject=context;otherSubject.subjects[0].evidenceIds={"evidence-other"};
        Check(!AITriageEngine::Validate(json,otherSubject,items,error),"real evidence ID from wrong subject rejected");
        auto fake=std::make_shared<FakeProvider>();fake->reply={AIProviderReply::Status::Ok,"invalid"};
        Check(AITriageEngine::Run(audit,fake).status=="invalid_output","invalid provider response retains audit");
        fake->reply.status=AIProviderReply::Status::Unavailable;Check(AITriageEngine::Run(audit,fake).status=="unavailable","unavailable provider");
        fake->reply.status=AIProviderReply::Status::Timeout;Check(AITriageEngine::Run(audit,fake).status=="timeout","provider-reported timeout");
        fake->delay=100;auto start=std::chrono::steady_clock::now();
        Check(AITriageEngine::Run(audit,fake,5).status=="timeout"&&std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100),"provider deadline enforced");
        fake=std::make_shared<FakeProvider>();fake->throws=true;auto throwing=AITriageEngine::Run(audit,fake);
        Check(throwing.status=="provider_error"&&throwing.error.find("secret-provider-error")==std::string::npos,"provider exception sanitized");
        Check(AITriageEngine::Run(audit,nullptr).status=="unavailable","no credentials/provider required");
        Check(AITriageEngine::Run(AuditResult{},mock).status=="no_evidence","no evidence yields no risk assertion");
        TriageAction action;action.kind="baseline_get";action.method="GET";action.url="https://outside.invalid/";
        AITriageEngine::CheckAction(action,audit);Check(action.policyDecision=="blocked_scope","action scope gate");
        action.url=audit.targetUrl;audit.requestBudget.remainingRequests=0;
        AITriageEngine::CheckAction(action,audit);Check(action.policyDecision=="blocked_budget","action budget gate");
        audit.requestBudget.remainingRequests=1;action.method="DELETE";AITriageEngine::CheckAction(action,audit);
        Check(action.policyDecision=="blocked_method","action method gate");
        action.method="GET";AITriageEngine::CheckAction(action,audit);
        Check(!action.executed&&action.policyDecision.find("requires_user_approval")==0,"in-scope GET still requires approval and send-time gates");
        RequestBudget zero(0);Check(!zero.TryBeginRequest()&&zero.Snapshot().budgetRejectedRequests==1,"zero budget boundary");
        RequestBudget one(1);Check(one.TryBeginRequest()&&!one.TryBeginRequest()&&one.Snapshot().attemptedRequests==1,"one request budget boundary");
        ScopePolicy pathScope=audit.scopePolicy;pathScope.allowedPaths={"/api"};
        Check(!ScopePolicyEngine::Check(pathScope,"http://127.0.0.1:8787/api-other").allowed,"scope segment boundary");
        Check(!ScopePolicyEngine::Check(pathScope,"http://127.0.0.1:8787/api/%2e%2e/outside").allowed,"scope encoded traversal rejected");
        ScopePolicy domainScope;domainScope.allowedDomains={"example.invalid"};
        Check(!ScopePolicyEngine::Check(domainScope,"https://example.invalid.attacker.invalid/api").allowed,"scope domain suffix boundary");
        RequestRateLimiter limiter(30);limiter.WaitForSlot();start=std::chrono::steady_clock::now();limiter.WaitForSlot();
        Check(std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(28),"rate limiter minimum gap");
        auto e=audit.evidenceCatalog[0];auto other=e;other.observation="different probe label";
        Check(SameEvidenceSnapshot(e,other),"same response deduplicates independently of probe label");
        other.bodySha256="different";Check(!SameEvidenceSnapshot(e,other),"different bodies preserve distinct evidence");
        HttpResponse r;r.body="a";r.headers.emplace("Set-Cookie","x=one");auto e1=CaptureEvidence(r);r.headers.clear();r.headers.emplace("Set-Cookie","x=two");auto e2=CaptureEvidence(r);
        Check(!SameEvidenceSnapshot(e1,e2),"different sensitive headers retain distinct fingerprints");
        Check(e1.bodySha256.size()==64,"complete body SHA256 captured");
        auto input=AITriageEngine::SerializeContext(context);
        Check(input.find("private-body")==std::string::npos&&input.find("private-cookie")==std::string::npos,"provider context excludes raw body and credentials");
        Check(ParameterIntelligence::RedactSensitiveValues("https://user:pass@example.invalid/api?token=secret#private").find("pass")==std::string::npos,"URL userinfo redacted");
        r.headers.clear();r.headers.emplace("Content-Type","application/json");auto headers=SecurityChecks::CheckSecurityHeaders(r);
        Check(std::none_of(headers.begin(),headers.end(),[](const Finding& f){return f.ruleId=="headers.csp.missing"||f.ruleId=="headers.frame-options.missing";}),"JSON avoids HTML-only header candidates");
        r.headers.clear();r.headers.emplace("Content-Type","text/html");r.headers.emplace("Content-Security-Policy","default-src 'self'; frame-ancestors 'none'");headers=SecurityChecks::CheckSecurityHeaders(r);
        Check(std::none_of(headers.begin(),headers.end(),[](const Finding& f){return f.ruleId=="headers.frame-options.missing";}),"CSP frame-ancestors satisfies frame protection");
        r.headers.clear();r.headers.emplace("Set-Cookie","a=samesite=lax; Secure; HttpOnly");
        auto cookies=SecurityChecks::CheckCookies(r);Check(std::any_of(cookies.begin(),cookies.end(),[](const Finding& f){return f.ruleId=="cookie.samesite.missing";}),"cookie value cannot impersonate SameSite attribute");
        auto cookieProbe=audit.probes[0];cookieProbe.response.headers.clear();cookieProbe.response.headers.emplace("Set-Cookie","one=value");cookieProbe.response.headers.emplace("Set-Cookie","two=value");
        auto cookieMerged=Analyzer::AnalyzeProbe(cookieProbe);
        Check(std::any_of(cookieMerged.begin(),cookieMerged.end(),[](const Finding& f){return f.ruleId=="cookie.secure.missing"&&f.evidence.find("one")!=std::string::npos&&f.evidence.find("two")!=std::string::npos;}),"merged cookie rule retains both observations");
        Endpoint ep;ep.url="https://example.invalid/api/users?redirect=/oauth/token";EndpointRiskEngine::Analyze(ep);
        Check(ep.classification==EndpointClass::Resource,"query values do not change path classification");
        ep.url="https://example.invalid/unclassified";EndpointRiskEngine::Analyze(ep);Check(ep.classification==EndpointClass::Unknown&&ep.riskHints.empty(),"risk analysis resets stale classification");
        ProbeResult partial=audit.probes[0];partial.response.disposition=HttpRequestDisposition::Failed;
        Check(Analyzer::AnalyzeProbe(partial).empty(),"incomplete response produces no finding");
        audit.aiTriage=triage;auto report=JsonReporter::Serialize(audit);auto parsed=StrictJson::Parse(report);
        Check(parsed.At("aiTriage").At("isMock").scalar=="true"&&parsed.At("evidenceCatalog").array.size()==1,"report schema contains triage and evidence catalog");
        Check(report.find("private-cookie")==std::string::npos&&report.find("private-body")==std::string::npos,"report secrets omitted");
        auto html=HTMLReporter::Serialize("<script>bad</script>",1,"now",report);
        Check(html.find("AI-assisted security triage")!=std::string::npos&&html.find("&lt;script&gt;bad")!=std::string::npos,"HTML triage and escaped metadata");
        auto oldHtml=HTMLReporter::Serialize("old",1,"now","{\"findings\":[],\"endpoints\":[]}");
        Check(oldHtml.find("status:\"disabled\"")!=std::string::npos,"legacy report fallback without AI fields");
        audit.configuration.reportOptions.includeEvidence=false;parsed=StrictJson::Parse(JsonReporter::Serialize(audit));
        Check(parsed.At("evidenceCatalog").array[0].At("detailsOmitted").scalar=="true","exclude-evidence retains resolvable IDs");

        if(argc>1)
        {
            const std::string base=argv[1];AuditConfiguration c;c.targetUrl=base+"/discovery";c.minIntervalMs=0;c.timeoutMs=1000;c.useLegacyTimeoutProfile=false;
            ScopePolicyEngine::CreateDefault(c.targetUrl,c.scope,error);c.maxRequests=1;c.aiTriage=true;
            auto run=Pipeline::Run(c);Check(run.requestBudget.attemptedRequests==1&&run.requestBudget.budgetRejectedRequests>0,"HTTP probes cannot exceed budget");
            Check(run.aiTriage.isMock&&run.aiTriage.status=="completed","actual local audit to Mock triage");
            c.maxRequests=20;auto noAI=c;noAI.aiTriage=false;auto with=Pipeline::Run(c);auto without=Pipeline::Run(noAI);
            Check(with.requestBudget.attemptedRequests==without.requestBudget.attemptedRequests,"AI adds zero target requests");
            Check(with.requestBudget.attemptedRequests==3,"multiple plans share one baseline request per endpoint");
            RequestBudget budget(5);RequestRateLimiter fast(0);
            auto denied=HttpClient::Get("https://outside.invalid/",c,budget,fast);
            Check(denied.disposition==HttpRequestDisposition::ScopeRejected&&budget.Snapshot().attemptedRequests==0,"scope denial does not consume network budget");
            auto redirected=HttpClient::Get(base+"/redirect-1",c,budget,fast);
            Check(redirected.statusCode==200&&budget.Snapshot().attemptedRequests==3,"each redirect hop consumes budget");
            RequestBudget limited(1);redirected=HttpClient::Get(base+"/redirect-1",c,limited,fast);
            Check(redirected.disposition==HttpRequestDisposition::BudgetRejected&&limited.Snapshot().attemptedRequests==1,"redirect budget rejection structured");
            Check(redirected.completedHopEvidence.size()==1,"completed redirect evidence survives exhausted budget");
            RequestBudget scopeBudget(5);auto redirectDenied=HttpClient::Get(base+"/scope-start",c,scopeBudget,fast);
            Check(redirectDenied.disposition==HttpRequestDisposition::ScopeRejected&&scopeBudget.Snapshot().attemptedRequests==1&&redirectDenied.completedHopEvidence.size()==1,"redirect scope gate retains evidence and blocks outside hop");
            auto unsafe=MakeProbeTask(with.probePlans[0]);unsafe.method="POST";RequestBudget methodBudget(1);
            auto rejected=ProbeExecutor::Execute({unsafe},c,methodBudget,fast)[0];
            Check(!rejected.executed&&methodBudget.Snapshot().attemptedRequests==0,"executor cannot silently convert unsafe methods to GET");
            auto cookieResponse=HttpClient::Get(base+"/cookies",c,budget,fast);
            Check(cookieResponse.headers.count("Set-Cookie")==2,"HTTP repeated Set-Cookie retained");
            auto cookieFindings=SecurityChecks::CheckCookies(cookieResponse);
            Check(std::any_of(cookieFindings.begin(),cookieFindings.end(),[](const Finding& f){return f.evidence.find("second")!=std::string::npos;}),"second cookie analyzed");
            c.timeoutMs=1000;RequestBudget slowBudget(1);auto slow=HttpClient::Get(base+"/slow",c,slowBudget,fast);
            if(slow.disposition!=HttpRequestDisposition::Failed || slow.error.find("timeout")==std::string::npos)
            {
                std::cerr << "TIMEOUT DETAIL: " << slow.statusCode << ' ' << HttpRequestDispositionToString(slow.disposition) << ' ' << slow.error << '\n';
                for(const auto& h:slow.headers)std::cerr << h.first << ": " << h.second << '\n';
            }
            Check(slow.disposition==HttpRequestDisposition::Failed&&slow.error.find("timeout")!=std::string::npos&&slowBudget.Snapshot().failedRequests==1,"HTTP timeout counted as failure");
            std::this_thread::sleep_for(std::chrono::milliseconds(5100));c.timeoutMs=1000;
            RequestBudget partialBudget(1);auto broken=HttpClient::Get(base+"/partial",c,partialBudget,fast);
            if(broken.disposition!=HttpRequestDisposition::Failed)std::cerr << "PARTIAL DETAIL: " << broken.statusCode << ' ' << broken.body.size() << ' ' << broken.error << '\n';
            Check(broken.disposition==HttpRequestDisposition::Failed&&partialBudget.Snapshot().failedRequests==1,"truncated body is failed HTTP evidence");
            RequestBudget largeBudget(1);auto large=HttpClient::Get(base+"/large",c,largeBudget,fast);
            Check(large.disposition==HttpRequestDisposition::Failed&&large.error.find("8 MiB")!=std::string::npos,"oversized HTTP body rejected without analysis");
            RequestBudget authBudget(1);auto auth=HttpClient::Get(base+"/auth",c,authBudget,fast);
            Check(auth.statusCode==401&&CaptureEvidence(auth).observation.find("Authentication is required")!=std::string::npos,"HTTP authentication failure explained as observation");
            std::filesystem::path path=std::filesystem::u8path(base.empty()?"unused":"x64/client-test.project.json");
            auto project=ProjectStore::Create(c.targetUrl,"test");project.defaultConfiguration=c;ProjectStore::AddAuditRun(project,JsonReporter::Serialize(with),with.requestBudget);
            Check(ProjectStore::Save(path.u8string(),project,error),"project save with triage");
            Project loaded;Check(ProjectStore::Load(path.u8string(),loaded,error)&&loaded.defaultConfiguration.aiTriage,"project default AI switch roundtrip");
            Check(loaded.runs[0].reportJson.find("evidence-1")!=std::string::npos,"project replay preserves evidence references");
            std::ifstream projectFile(path);std::string projectText((std::istreambuf_iterator<char>(projectFile)),{});projectFile.close();
            auto projectJson=StrictJson::Parse(projectText);
            auto write=[&](const std::string& text){std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<text;};
            for(int version=1;version<=4;++version)
            {
                auto legacy=projectJson;legacy.object["projectSchemaVersion"].scalar=std::to_string(version);
                if(version<4)legacy.object.erase("defaultConfiguration");
                else {legacy.object["defaultConfiguration"].object.erase("aiTriage");legacy.object["defaultConfiguration"].object.erase("aiTimeoutMs");}
                if(version==1)legacy.object.erase("scopePolicy");
                if(version==3){StrictJson::Value n;n.kind=StrictJson::Value::Kind::Number;n.scalar="50";legacy.object["defaultMaxRequestsPerRun"]=n;legacy.object.erase("defaultMinRequestIntervalMs");legacy.object["runs"].array[0].object["requestBudget"].object.erase("minRequestIntervalMs");}
                if(version<3)legacy.object["runs"].array[0].object.erase("requestBudget");
                write(StrictJson::Serialize(legacy));Project legacyLoaded;
                const std::string name="legacy project v"+std::to_string(version)+" compatibility";
                Check(ProjectStore::Load(path.u8string(),legacyLoaded,error)&&!legacyLoaded.defaultConfiguration.aiTriage,name.c_str());
            }
            write(Replace(projectText,"\"projectSchemaVersion\": 4","\"projectSchemaVersion\": 4,\"projectSchemaVersion\": 1"));
            Check(!ProjectStore::Load(path.u8string(),loaded,error),"duplicate project fields rejected");
            std::filesystem::remove(path);
        }
    }
    catch(const std::exception& e){++failed;std::cerr<<"TEST EXCEPTION "<<e.what()<<'\n';}
    std::cout<<"RESULT "<<passed<<" passed, "<<failed<<" failed\n";
    return failed?1:0;
}
