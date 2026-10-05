#include "scope_policy.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

namespace
{
    struct UrlParts
    {
        std::string host;
        std::string path;
        INTERNET_SCHEME scheme = INTERNET_SCHEME_HTTP;
        INTERNET_PORT port = 0;
    };

    std::wstring ToWide(
        const std::string& value)
    {
        if (value.empty())
            return {};

        const int required = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

        if (required <= 0)
            return {};

        std::wstring result(
            static_cast<size_t>(required),
            L'\0');

        if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required) <= 0)
        {
            return {};
        }

        return result;
    }

    std::string ToUtf8(
        const std::wstring& value)
    {
        if (value.empty())
            return {};

        const int required = WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (required <= 0)
            return {};

        std::string result(
            static_cast<size_t>(required),
            '\0');

        if (WideCharToMultiByte(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required,
            nullptr,
            nullptr) <= 0)
        {
            return {};
        }

        return result;
    }

    std::string LowerAscii(
        std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        return value;
    }

    std::string StripIpv6Brackets(
        std::string host)
    {
        if (host.size() >= 2 &&
            host.front() == '[' &&
            host.back() == ']')
        {
            host = host.substr(1, host.size() - 2);
        }

        return host;
    }

    bool CrackUrl(
        const std::string& url,
        UrlParts& parts)
    {
        const std::wstring wideUrl = ToWide(url);

        if (wideUrl.empty())
            return false;

        std::vector<wchar_t> host(1024, L'\0');
        std::vector<wchar_t> path(32768, L'\0');

        URL_COMPONENTS components{};
        components.dwStructSize = sizeof(components);
        components.lpszHostName = host.data();
        components.dwHostNameLength = static_cast<DWORD>(host.size());
        components.lpszUrlPath = path.data();
        components.dwUrlPathLength = static_cast<DWORD>(path.size());

        if (!WinHttpCrackUrl(
            wideUrl.c_str(),
            static_cast<DWORD>(wideUrl.size()),
            0,
            &components))
        {
            return false;
        }

        if (components.dwHostNameLength == 0)
            return false;

        parts.host = LowerAscii(StripIpv6Brackets(
            ToUtf8(std::wstring(
                components.lpszHostName,
                components.dwHostNameLength))));

        parts.path = components.dwUrlPathLength == 0
            ? "/"
            : ToUtf8(std::wstring(
                components.lpszUrlPath,
                components.dwUrlPathLength));

        const size_t queryOrFragment = parts.path.find_first_of("?#");
        if (queryOrFragment != std::string::npos)
            parts.path.erase(queryOrFragment);

        parts.scheme = components.nScheme;
        parts.port = components.nPort;

        return !parts.host.empty();
    }

    std::string CanonicalHostRule(
        const std::string& host,
        INTERNET_PORT port,
        bool explicitPort)
    {
        std::string result = host.find(':') == std::string::npos
            ? host
            : "[" + host + "]";

        if (explicitPort)
        {
            result += ":" + std::to_string(port);
        }

        return result;
    }

    bool HasExplicitPort(
        const std::string& authority)
    {
        if (authority.empty())
            return false;

        if (authority.front() == '[')
        {
            const size_t close = authority.find(']');
            return close != std::string::npos &&
                close + 1 < authority.size() &&
                authority[close + 1] == ':';
        }

        const size_t firstColon = authority.find(':');
        return firstColon != std::string::npos &&
            authority.find(':', firstColon + 1) == std::string::npos;
    }

    bool ParseHostRule(
        const std::string& rule,
        UrlParts& parts,
        bool& explicitPort)
    {
        if (rule.empty() ||
            rule.find_first_of("/@?#\\ \t\r\n") != std::string::npos)
        {
            return false;
        }

        explicitPort = HasExplicitPort(rule);

        if (explicitPort)
        {
            const size_t colon = rule.rfind(':');

            if (colon == std::string::npos || colon + 1 >= rule.size())
                return false;

            unsigned int port = 0;

            for (size_t i = colon + 1; i < rule.size(); ++i)
            {
                if (!std::isdigit(static_cast<unsigned char>(rule[i])))
                    return false;

                port = port * 10 +
                    static_cast<unsigned int>(rule[i] - '0');

                if (port > 65535)
                    return false;
            }

            if (port == 0)
                return false;
        }

        return CrackUrl(
            "http://" + rule + "/",
            parts) &&
            parts.scheme == INTERNET_SCHEME_HTTP;
    }

    int HexValue(
        char value)
    {
        if (value >= '0' && value <= '9')
            return value - '0';

        if (value >= 'a' && value <= 'f')
            return value - 'a' + 10;

        if (value >= 'A' && value <= 'F')
            return value - 'A' + 10;

        return -1;
    }

    bool NormalizePath(
        const std::string& input,
        std::string& output)
    {
        if (input.empty() || input.front() != '/' ||
            input.find_first_of("?#\\") != std::string::npos)
        {
            return false;
        }

        std::string decoded = input;

        for (size_t pass = 0; pass < 8; ++pass)
        {
            std::string next;
            next.reserve(decoded.size());
            bool changed = false;

            for (size_t i = 0; i < decoded.size(); ++i)
            {
                if (decoded[i] == '%')
                {
                    if (i + 2 >= decoded.size())
                        return false;

                    const int high = HexValue(decoded[i + 1]);
                    const int low = HexValue(decoded[i + 2]);

                    if (high < 0 || low < 0)
                        return false;

                    const char value = static_cast<char>(
                        (high << 4) | low);

                    if (value == '\\' ||
                        static_cast<unsigned char>(value) < 0x20 ||
                        value == 0x7f)
                    {
                        return false;
                    }

                    next += value;
                    i += 2;
                    changed = true;
                    continue;
                }

                const unsigned char value =
                    static_cast<unsigned char>(decoded[i]);

                if (value < 0x20 || value == 0x7f)
                    return false;

                next += decoded[i];
            }

            decoded = std::move(next);

            if (!changed)
                break;

            if (pass == 7)
                return false;
        }

        std::vector<std::string> segments;
        size_t position = 1;

        while (position <= decoded.size())
        {
            const size_t slash = decoded.find('/', position);
            const size_t end = slash == std::string::npos
                ? decoded.size()
                : slash;
            const std::string segment = decoded.substr(
                position,
                end - position);

            if (segment == "..")
            {
                if (!segments.empty())
                    segments.pop_back();
            }
            else if (!segment.empty() && segment != ".")
            {
                segments.push_back(segment);
            }

            if (slash == std::string::npos)
                break;

            position = slash + 1;
        }

        output = "/";

        for (size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0)
                output += '/';

            output += segments[i];
        }

        if (output.size() > 1 && decoded.back() == '/')
            output += '/';

        return true;
    }

    bool NormalizeDomain(
        const std::string& input,
        std::string& output)
    {
        if (input.empty() ||
            input.find_first_of("/:@?#\\ \t\r\n[]") != std::string::npos)
        {
            return false;
        }

        output = LowerAscii(input);

        while (!output.empty() && output.back() == '.')
            output.pop_back();

        if (output.empty() || output.find(':') != std::string::npos)
            return false;

        size_t labelStart = 0;

        while (labelStart <= output.size())
        {
            const size_t dot = output.find('.', labelStart);
            const size_t labelEnd = dot == std::string::npos
                ? output.size()
                : dot;

            if (labelEnd == labelStart ||
                output[labelStart] == '-' ||
                output[labelEnd - 1] == '-')
            {
                return false;
            }

            for (size_t i = labelStart; i < labelEnd; ++i)
            {
                const unsigned char ch =
                    static_cast<unsigned char>(output[i]);

                if (!std::isalnum(ch) && ch != '-')
                    return false;
            }

            if (dot == std::string::npos)
                break;

            labelStart = dot + 1;
        }

        bool onlyIpv4Characters = true;
        size_t dotCount = 0;

        for (const unsigned char ch : output)
        {
            if (ch == '.')
            {
                ++dotCount;
            }
            else if (!std::isdigit(ch))
            {
                onlyIpv4Characters = false;
                break;
            }
        }

        return !(onlyIpv4Characters && dotCount == 3);
    }

    bool HostMatches(
        const std::string& host,
        INTERNET_PORT port,
        const std::string& rule)
    {
        UrlParts ruleParts;
        bool explicitPort = false;

        if (!ParseHostRule(rule, ruleParts, explicitPort))
            return false;

        return host == ruleParts.host &&
            (!explicitPort || port == ruleParts.port);
    }

    bool DomainMatches(
        const std::string& host,
        const std::string& domainRule)
    {
        if (host.find(':') != std::string::npos)
            return false;

        bool numericHost = true;
        size_t hostDots = 0;

        for (const unsigned char ch : host)
        {
            if (ch == '.')
                ++hostDots;
            else if (!std::isdigit(ch))
                numericHost = false;
        }

        if (numericHost && hostDots == 3)
            return false;

        std::string domain;

        if (!NormalizeDomain(domainRule, domain))
            return false;

        if (host == domain)
            return true;

        if (host.size() <= domain.size())
            return false;

        const size_t suffix = host.size() - domain.size();
        return host.compare(suffix, domain.size(), domain) == 0 &&
            host[suffix - 1] == '.';
    }

    bool PathMatches(
        const std::string& requestPath,
        const std::string& pathRule)
    {
        std::string normalizedRequest;
        std::string normalizedRule;

        if (!NormalizePath(requestPath, normalizedRequest) ||
            !NormalizePath(pathRule, normalizedRule))
        {
            return false;
        }

        while (normalizedRule.size() > 1 &&
            normalizedRule.back() == '/')
        {
            normalizedRule.pop_back();
        }

        if (normalizedRule == "/" ||
            normalizedRequest == normalizedRule)
        {
            return true;
        }

        return normalizedRequest.size() > normalizedRule.size() &&
            normalizedRequest.compare(
                0,
                normalizedRule.size(),
                normalizedRule) == 0 &&
            normalizedRequest[normalizedRule.size()] == '/';
    }
}

namespace ScopePolicyEngine
{
    bool CreateDefault(
        const std::string& targetUrl,
        ScopePolicy& policy,
        std::string& error)
    {
        error.clear();
        policy = {};

        UrlParts parts;

        if (!CrackUrl(targetUrl, parts) ||
            (parts.scheme != INTERNET_SCHEME_HTTP &&
                parts.scheme != INTERNET_SCHEME_HTTPS))
        {
            error = "The target must be a valid HTTP or HTTPS URL.";
            return false;
        }

        policy.allowedHosts.push_back(
            CanonicalHostRule(
                parts.host,
                parts.port,
                true));

        return true;
    }

    bool AddHost(
        ScopePolicy& policy,
        const std::string& host,
        std::string& error)
    {
        error.clear();
        UrlParts parts;
        bool explicitPort = false;

        if (!ParseHostRule(host, parts, explicitPort))
        {
            error = "Invalid scope host. Use a hostname, IPv6 address, or optional :port.";
            return false;
        }

        policy.allowedHosts.push_back(
            CanonicalHostRule(
                parts.host,
                parts.port,
                explicitPort));

        return true;
    }

    bool AddDomain(
        ScopePolicy& policy,
        const std::string& domain,
        std::string& error)
    {
        error.clear();
        std::string normalized;

        if (!NormalizeDomain(domain, normalized))
        {
            error = "Invalid scope domain.";
            return false;
        }

        policy.allowedDomains.push_back(normalized);
        return true;
    }

    bool AddPath(
        ScopePolicy& policy,
        const std::string& pathPrefix,
        std::string& error)
    {
        error.clear();
        std::string normalized;

        if (!NormalizePath(pathPrefix, normalized))
        {
            error = "Invalid scope path. Use an absolute path prefix without query or fragment.";
            return false;
        }

        while (normalized.size() > 1 &&
            normalized.back() == '/')
        {
            normalized.pop_back();
        }

        policy.allowedPaths.push_back(normalized);
        return true;
    }

    bool Validate(
        const ScopePolicy& policy,
        std::string& error)
    {
        error.clear();

        if (policy.allowedHosts.empty() &&
            policy.allowedDomains.empty())
        {
            error = "Scope policy must contain at least one allowed host or domain.";
            return false;
        }

        for (const auto& host : policy.allowedHosts)
        {
            UrlParts parts;
            bool explicitPort = false;

            if (!ParseHostRule(host, parts, explicitPort))
            {
                error = "Scope policy contains an invalid host rule.";
                return false;
            }
        }

        for (const auto& domain : policy.allowedDomains)
        {
            std::string normalized;

            if (!NormalizeDomain(domain, normalized))
            {
                error = "Scope policy contains an invalid domain rule.";
                return false;
            }
        }

        for (const auto& path : policy.allowedPaths)
        {
            std::string normalized;

            if (!NormalizePath(path, normalized))
            {
                error = "Scope policy contains an invalid path rule.";
                return false;
            }
        }

        return true;
    }

    ScopeDecision Check(
        const ScopePolicy& policy,
        const std::string& url)
    {
        std::string validationError;

        if (!Validate(policy, validationError))
        {
            return { false, validationError };
        }

        UrlParts parts;

        if (!CrackUrl(url, parts) ||
            (parts.scheme != INTERNET_SCHEME_HTTP &&
                parts.scheme != INTERNET_SCHEME_HTTPS))
        {
            return { false, "Only valid HTTP and HTTPS URLs are allowed by scope policy." };
        }

        bool hostAllowed = false;

        for (const auto& rule : policy.allowedHosts)
        {
            if (HostMatches(parts.host, parts.port, rule))
            {
                hostAllowed = true;
                break;
            }
        }

        if (!hostAllowed)
        {
            for (const auto& rule : policy.allowedDomains)
            {
                if (DomainMatches(parts.host, rule))
                {
                    hostAllowed = true;
                    break;
                }
            }
        }

        if (!hostAllowed)
        {
            return { false, "Target host is not in the allowed host/domain list." };
        }

        if (!policy.allowedPaths.empty())
        {
            bool pathAllowed = false;

            for (const auto& rule : policy.allowedPaths)
            {
                if (PathMatches(parts.path, rule))
                {
                    pathAllowed = true;
                    break;
                }
            }

            if (!pathAllowed)
            {
                return { false, "Request path is outside the allowed path prefixes." };
            }
        }

        return { true, {} };
    }
}
