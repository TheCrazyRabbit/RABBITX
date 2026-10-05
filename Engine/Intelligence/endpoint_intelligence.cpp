#include "endpoint_intelligence.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <vector>

#include "endpoint_risk_engine.h"
#include "parameter_intelligence.h"

namespace
{
    std::string Lower(
        std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c));
            });

        return value;
    }

    bool StartsWith(
        const std::string& value,
        const std::string& prefix)
    {
        if (value.size() < prefix.size())
            return false;

        return value.compare(
            0,
            prefix.size(),
            prefix) == 0;
    }

    bool IsInterestingPath(
        const std::string& value)
    {
        const std::string lower =
            Lower(value);

        static const std::vector<std::string>
            keywords =
        {
            "/api/",
            "/api",
            "/graphql",
            "/rest/",
            "/v1/",
            "/v2/",
            "/v3/",
            "/oauth",
            "/auth",
            "/login",
            "/admin",
            "/user",
            "/users",
            "/account",
            "/accounts",
            "/upload",
            "/download",
            "/search",
            "/query"
        };

        for (const auto& keyword : keywords)
        {
            if (lower.find(keyword) !=
                std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    std::string BuildAbsoluteUrl(
        const std::string& baseUrl,
        const std::string& value)
    {
        if (value.empty())
            return {};

        if (StartsWith(value, "http://") ||
            StartsWith(value, "https://"))
        {
            return value;
        }

        if (StartsWith(value, "//"))
        {
            if (StartsWith(baseUrl, "https://"))
                return "https:" + value;

            return "http:" + value;
        }

        if (StartsWith(value, "/"))
        {
            const std::regex originRegex(
                R"(^(https?://[^/]+))",
                std::regex::icase);

            std::smatch match;

            if (std::regex_search(
                baseUrl,
                match,
                originRegex))
            {
                return match[1].str() + value;
            }
        }

        return {};
    }

    void AddEndpoint(
        std::vector<Endpoint>& results,
        std::set<std::string>& known,
        const std::string& url,
        const std::string& source)
    {
        if (url.empty())
            return;

        if (!known.insert(url).second)
            return;

        Endpoint endpoint;

        endpoint.url = url;
        endpoint.method = "GET";
        endpoint.source = source;

        endpoint.parameters =
            ParameterIntelligence::Analyze(url);

        if (!endpoint.parameters.empty())
        {
            endpoint.parameterHint =
                "Query parameters detected: " +
                std::to_string(
                    endpoint.parameters.size());
        }

        EndpointRiskEngine::Analyze(
            endpoint);

        results.push_back(
            endpoint);
    }
}

std::vector<Endpoint>
EndpointIntelligence::Analyze(
    const HttpResponse& response)
{
    std::vector<Endpoint> results;

    if (response.body.empty())
        return results;

    std::set<std::string> known;

    const std::regex absoluteUrlRegex(
        R"((https?://[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]+))",
        std::regex::icase);

    for (std::sregex_iterator it(
        response.body.begin(),
        response.body.end(),
        absoluteUrlRegex);
        it != std::sregex_iterator();
        ++it)
    {
        const std::string url =
            it->str();

        if (IsInterestingPath(url))
        {
            AddEndpoint(
                results,
                known,
                url,
                "absolute-url");
        }
    }

    const std::regex pathRegex(
        R"((["'])(/[A-Za-z0-9_./?=&%:+~!$'()*;,@#-]{2,})(\1))");

    for (std::sregex_iterator it(
        response.body.begin(),
        response.body.end(),
        pathRegex);
        it != std::sregex_iterator();
        ++it)
    {
        const std::string path =
            (*it)[2].str();

        if (!IsInterestingPath(path))
            continue;

        const std::string url =
            BuildAbsoluteUrl(
                response.finalUrl.empty()
                ? response.requestedUrl
                : response.finalUrl,
                path);

        AddEndpoint(
            results,
            known,
            url,
            "response-path");
    }

    return results;
}