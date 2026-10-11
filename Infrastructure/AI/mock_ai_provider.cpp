#include "ai_provider.h"
#include "Core/Models/strict_json.h"
#include <sstream>

AIProviderReply MockAIProvider::Generate(const std::string& contextJson, int,
    const std::shared_ptr<std::atomic_bool>& cancelled)
{
    if(cancelled->load()) return {AIProviderReply::Status::Timeout,{}};
    auto context = StrictJson::Parse(contextJson);
    std::ostringstream out;
    out << "{\"schemaVersion\":\"1.0\",\"items\":[";
    bool first = true;
    for(const auto& subject : context.At("subjects").array)
    {
        if(!first)out << ',';first=false;
        auto id=subject.At("id").String();
        auto url=subject.At("url").String();
        const auto description=subject.At("description").String();
        const std::string assessment=subject.At("kind").String()=="finding" && subject.At("riskScore").Integer()==10 ? "informational" : "candidate";
        std::string risk="Mock: review "+description+"; observed configuration alone does not establish exploitability.";
        std::string missing="Human validation of impact and application context.";
        std::string next="Inspect the cited response and rule before planning another request.";
        if(description=="RESOURCE") {risk="Mock: object reference surface may warrant authorization review; IDOR is unverified.";missing="Two authorized accounts, expected object ownership and a comparable response.";next="Review identifiers and expected object ownership using existing evidence.";}
        else if(description=="SEARCH") {risk="Mock: user input surface may warrant reflection and query handling review; SQLi and XSS are unverified.";missing="Input handling context, response semantics and safe validation plan.";next="Review parameter types and response context before selecting one minimal test.";}
        else if(description=="GRAPHQL") {risk="Mock: GraphQL surface identified; authorization and operation exposure are unverified.";missing="Authorized operations and expected access boundaries.";next="Review the authorized operation inventory; no introspection request is generated.";}
        else if(description=="AUTH") {risk="Mock: authentication surface identified; OAuth or session weakness is unverified.";missing="Expected token flow, client configuration and authorized accounts.";next="Review the documented authentication flow against the endpoint reference.";}
        auto array=[&](const char* key) {out << '[';bool f=true;for(const auto& v:subject.At(key).array){if(!f)out<<',';f=false;out<<StrictJson::Quote(v.String());}out<<']';};
        out << "{\"subjectId\":" << StrictJson::Quote(id)
            << ",\"priority\":" << subject.At("riskScore").Integer()
            << ",\"assessment\":" << StrictJson::Quote(assessment) << ",\"evidenceIds\":";
        array("evidenceIds");out << ",\"factIds\":";array("factIds");
        out << ",\"possibleRisk\":" << StrictJson::Quote(risk)
            << ",\"uncertainty\":\"Mock heuristic output. Authorization, exploitability and impact are unknown.\""
            << ",\"missingValidation\":" << StrictJson::Quote(missing)
            << ",\"nextAction\":{\"kind\":\"manual_review\",\"method\":\"NONE\",\"url\":" << StrictJson::Quote(url)
            << ",\"reason\":" << StrictJson::Quote(next) << "}}";
    }
    out << "]}";
    return {AIProviderReply::Status::Ok,out.str()};
}
