#include "parameter_intelligence.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
    std::string NormalizeName(
        std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                const char lower =
                    static_cast<char>(std::tolower(c));

                if (lower == '-' || lower == '.')
                    return '_';

                return lower;
            });

        while (!value.empty() &&
            (value.back() == ']' || value.back() == '['))
        {
            value.pop_back();
        }

        return value;
    }

    bool IsOneOf(
        const std::string& value,
        std::initializer_list<const char*> options)
    {
        for (const char* option : options)
        {
            if (value == option)
                return true;
        }

        return false;
    }

    bool EndsWith(
        const std::string& value,
        const std::string& suffix)
    {
        return value.size() >= suffix.size() &&
            value.compare(
                value.size() - suffix.size(),
                suffix.size(),
                suffix) == 0;
    }

    ParameterRisk MakeRisk(
        ParameterRiskType type,
        ParameterRiskLevel level,
        ParameterConfidence confidence,
        const std::string& reason)
    {
        ParameterRisk risk;
        risk.type = type;
        risk.level = level;
        risk.confidence = confidence;
        risk.reason = reason;
        return risk;
    }

    std::string UrlDecode(
        const std::string& value)
    {
        std::string result;

        for (size_t i = 0;
            i < value.size();
            ++i)
        {
            if (value[i] == '%' &&
                i + 2 < value.size())
            {
                auto HexValue =
                    [](char c) -> int
                    {
                        if (c >= '0' && c <= '9')
                            return c - '0';

                        if (c >= 'a' && c <= 'f')
                            return c - 'a' + 10;

                        if (c >= 'A' && c <= 'F')
                            return c - 'A' + 10;

                        return -1;
                    };

                const int highValue =
                    HexValue(value[i + 1]);

                const int lowValue =
                    HexValue(value[i + 2]);

                if (highValue >= 0 &&
                    lowValue >= 0)
                {
                    result += static_cast<char>(
                        (highValue << 4) |
                        lowValue);

                    i += 2;
                    continue;
                }
            }

            if (value[i] == '+')
            {
                result += ' ';
                continue;
            }

            result += value[i];
        }

        return result;
    }

    void ParseQuery(
        const std::string& query,
        std::vector<Parameter>& results)
    {
        size_t start = 0;

        while (start <= query.size())
        {
            const size_t end =
                query.find('&', start);

            const std::string item =
                end == std::string::npos
                ? query.substr(start)
                : query.substr(
                    start,
                    end - start);

            if (!item.empty())
            {
                const size_t equals =
                    item.find('=');

                const std::string rawName =
                    equals == std::string::npos
                    ? item
                    : item.substr(0, equals);

                const std::string rawValue =
                    equals == std::string::npos
                    ? std::string()
                    : item.substr(equals + 1);

                Parameter parameter;
                parameter.name = UrlDecode(rawName);
                parameter.value = UrlDecode(rawValue);
                parameter.location = "query";

                if (!parameter.name.empty())
                {
                    results.push_back(parameter);
                }
            }

            if (end == std::string::npos)
                break;

            start = end + 1;
        }
    }
}

ParameterType ParameterClassifier::Classify(
    const std::string& name)
{
    const std::string normalized =
        NormalizeName(name);

    if (IsOneOf(
        normalized,
        {
            "token", "access_token", "refresh_token",
            "id_token", "api_key", "apikey", "api_token",
            "access_key", "x_api_key", "bearer", "bearer_token",
            "jwt", "session", "session_id", "sessionid",
            "credential", "credentials", "secret", "client_secret",
            "password", "passwd", "pwd", "authorization",
            "auth_code", "code", "code_verifier", "private_key",
            "signature"
        }))
    {
        return ParameterType::Credential;
    }

    if (IsOneOf(
        normalized,
        {
            "redirect_uri", "client_id", "response_type",
            "response_mode", "state", "nonce", "scope",
            "grant_type", "code_challenge", "code_challenge_method"
        }))
    {
        return ParameterType::OAuthControl;
    }

    if (IsOneOf(
        normalized,
        {
            "id", "uid", "uuid", "userid", "accountid",
            "objectid", "resourceid", "recordid", "itemid",
            "user", "account"
        }) ||
        EndsWith(normalized, "_id") ||
        EndsWith(normalized, "_uuid"))
    {
        return ParameterType::Identifier;
    }

    if (IsOneOf(
        normalized,
        {
            "q", "query", "search", "keyword", "term",
            "text", "message", "comment", "title", "description",
            "content", "input"
        }))
    {
        return ParameterType::SearchInput;
    }

    if (IsOneOf(
        normalized,
        {
            "page", "page_size", "pagesize", "limit", "offset",
            "per_page", "perpage", "cursor", "after", "before"
        }))
    {
        return ParameterType::Pagination;
    }

    if (IsOneOf(
        normalized,
        {
            "filter", "sort", "order", "order_by", "where",
            "field", "fields", "group_by"
        }))
    {
        return ParameterType::Filter;
    }

    if (IsOneOf(
        normalized,
        {
            "redirect", "redirect_url", "return", "return_url",
            "returnto", "next", "continue", "back", "success_url"
        }))
    {
        return ParameterType::RedirectTarget;
    }

    if (IsOneOf(
        normalized,
        {
            "callback", "callback_url", "cb", "jsonp"
        }))
    {
        return ParameterType::Callback;
    }

    if (IsOneOf(
        normalized,
        {
            "url", "uri", "target", "target_url", "remote_url",
            "destination", "destination_url", "host", "hostname",
            "image_url", "webhook", "webhook_url", "feed_url",
            "endpoint", "proxy_url", "source_url"
        }))
    {
        return ParameterType::UrlOrHost;
    }

    if (IsOneOf(
        normalized,
        {
            "file", "filename", "path", "filepath", "directory",
            "folder", "document"
        }))
    {
        return ParameterType::FilePath;
    }

    return ParameterType::Generic;
}

bool ParameterClassifier::ShouldRedactValue(
    const std::string& name)
{
    const std::string normalized =
        NormalizeName(name);

    return Classify(normalized) == ParameterType::Credential ||
        IsOneOf(
            normalized,
            {
                "state", "nonce", "code_challenge"
            });
}

std::vector<ParameterRisk> ParameterClassifier::ClassifyRisks(
    const Parameter& parameter)
{
    std::vector<ParameterRisk> risks;
    const std::string name =
        NormalizeName(parameter.name);

    switch (parameter.type)
    {
    case ParameterType::Identifier:
    {
        const bool exactIdentifier =
            IsOneOf(
                name,
                {
                    "id", "uid", "uuid", "userid", "accountid",
                    "user_id", "account_id", "object_id",
                    "resource_id", "record_id", "item_id",
                    "objectid", "resourceid", "recordid", "itemid"
                });

        const bool ambiguousObjectName =
            name == "user" ||
            name == "account";

        risks.push_back(MakeRisk(
            ParameterRiskType::IDORCandidate,
            exactIdentifier
                ? ParameterRiskLevel::High
                : ParameterRiskLevel::Medium,
            exactIdentifier
                ? ParameterConfidence::High
                : ambiguousObjectName
                ? ParameterConfidence::Low
                : ParameterConfidence::Medium,
            "The parameter name indicates an object identifier; verify object-level authorization using an authorized test account."));
        break;
    }

    case ParameterType::SearchInput:
        risks.push_back(MakeRisk(
            ParameterRiskType::SQLInjectionCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "Search-like input may reach query construction; server-side behavior has not been verified."));

        risks.push_back(MakeRisk(
            ParameterRiskType::XSSCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "Search-like input may be reflected or rendered; verify output encoding in the relevant response context."));
        break;

    case ParameterType::Filter:
        risks.push_back(MakeRisk(
            ParameterRiskType::SQLInjectionCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Low,
            "Filter or sort controls can influence query structure; inspect allowed fields and server-side query handling."));
        break;

    case ParameterType::RedirectTarget:
        risks.push_back(MakeRisk(
            ParameterRiskType::OpenRedirectCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "The parameter name suggests a redirect destination; verify destination allowlisting and scheme validation."));
        break;

    case ParameterType::Callback:
        risks.push_back(MakeRisk(
            ParameterRiskType::OpenRedirectCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Low,
            "Callback destinations should be checked for origin validation and unsafe redirect behavior."));

        risks.push_back(MakeRisk(
            ParameterRiskType::OAuthReviewCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "Callback-like parameter may participate in an authentication flow; review registration and state binding."));
        break;

    case ParameterType::OAuthControl:
        risks.push_back(MakeRisk(
            ParameterRiskType::OAuthReviewCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::High,
            "OAuth-related control parameter detected; review redirect validation, state/nonce binding, and flow-specific checks."));

        if (name == "redirect_uri")
        {
            risks.push_back(MakeRisk(
                ParameterRiskType::OpenRedirectCandidate,
                ParameterRiskLevel::Medium,
                ParameterConfidence::High,
                "OAuth redirect URI should be matched against an exact registered allowlist."));
        }
        break;

    case ParameterType::UrlOrHost:
        risks.push_back(MakeRisk(
            ParameterRiskType::SSRFCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "The parameter accepts a URL or host-like value; verify outbound request controls and private-network protections."));
        break;

    case ParameterType::FilePath:
        risks.push_back(MakeRisk(
            ParameterRiskType::PathTraversalCandidate,
            ParameterRiskLevel::Medium,
            ParameterConfidence::Medium,
            "The parameter may select a file or path; verify canonicalization and confinement to the intended storage area."));
        break;

    case ParameterType::Credential:
    {
        const bool hasValue =
            !parameter.value.empty();

        risks.push_back(MakeRisk(
            ParameterRiskType::SensitiveValueExposureCandidate,
            hasValue
                ? ParameterRiskLevel::High
                : ParameterRiskLevel::Medium,
            ParameterConfidence::High,
            hasValue
                ? "A credential-like value is present in the URL and may leak through logs, browser history, or referrer data."
                : "A credential-like query parameter is accepted; verify that secrets are not transmitted in URLs."));

        if (name == "token" ||
            name == "access_token" ||
            name == "refresh_token" ||
            name == "id_token" ||
            name == "client_secret" ||
            name == "code" ||
            name == "auth_code")
        {
            risks.push_back(MakeRisk(
                ParameterRiskType::OAuthReviewCandidate,
                ParameterRiskLevel::Medium,
                ParameterConfidence::High,
                "Token or authorization-code parameter detected; review transport, storage, and single-use or expiry controls."));
        }
        break;
    }

    case ParameterType::Pagination:
    case ParameterType::Generic:
    default:
        break;
    }

    return risks;
}

std::vector<Parameter> ParameterAnalyzer::Analyze(
    const std::string& url)
{
    std::vector<Parameter> results;

    const size_t queryStart =
        url.find('?');

    if (queryStart == std::string::npos)
        return results;

    const size_t pathFragment =
        url.find('#');

    if (pathFragment != std::string::npos &&
        pathFragment < queryStart)
    {
        return results;
    }

    const size_t fragmentStart =
        url.find('#', queryStart + 1);

    const std::string query =
        fragmentStart == std::string::npos
        ? url.substr(queryStart + 1)
        : url.substr(
            queryStart + 1,
            fragmentStart - queryStart - 1);

    ParseQuery(query, results);

    for (auto& parameter : results)
    {
        parameter.type =
            ParameterClassifier::Classify(parameter.name);

        parameter.risks =
            ParameterClassifier::ClassifyRisks(parameter);
    }

    return results;
}

namespace ParameterIntelligence
{
    std::vector<Parameter> Analyze(
        const std::string& url)
    {
        return ParameterAnalyzer::Analyze(url);
    }

    std::string RedactSensitiveValues(
        const std::string& url)
    {
        const size_t queryStart =
            url.find('?');

        if (queryStart == std::string::npos)
            return url;

        const size_t pathFragment =
            url.find('#');

        if (pathFragment != std::string::npos &&
            pathFragment < queryStart)
        {
            return url;
        }

        const size_t fragmentStart =
            url.find('#', queryStart + 1);

        const size_t queryEnd =
            fragmentStart == std::string::npos
            ? url.size()
            : fragmentStart;

        std::string result =
            url.substr(0, queryStart + 1);

        size_t start = queryStart + 1;

        while (start <= queryEnd)
        {
            const size_t end =
                url.find('&', start);

            const size_t itemEnd =
                end == std::string::npos || end > queryEnd
                ? queryEnd
                : end;

            const std::string item =
                url.substr(start, itemEnd - start);

            const size_t equals =
                item.find('=');

            const std::string rawName =
                equals == std::string::npos
                ? item
                : item.substr(0, equals);

            if (equals != std::string::npos &&
                ParameterClassifier::ShouldRedactValue(
                    UrlDecode(rawName)))
            {
                result += rawName + "=[REDACTED]";
            }
            else
            {
                result += item;
            }

            if (itemEnd == queryEnd)
                break;

            result += '&';
            start = itemEnd + 1;
        }

        if (fragmentStart != std::string::npos)
        {
            result += url.substr(fragmentStart);
        }

        return result;
    }
}
