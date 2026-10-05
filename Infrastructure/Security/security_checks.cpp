#include "security_checks.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
    std::string Lower(std::string value)
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

    std::string Trim(
        const std::string& value)
    {
        const auto first =
            value.find_first_not_of(" \t");

        if (first == std::string::npos)
            return {};

        const auto last =
            value.find_last_not_of(" \t");

        return value.substr(
            first,
            last - first + 1);
    }

    std::string NormalizeUrlForComparison(
        const std::string& url)
    {
        if (url.empty())
            return url;

        std::string result = url;

        const size_t query =
            result.find('?');

        const size_t fragment =
            result.find('#');

        size_t end =
            result.size();

        if (query != std::string::npos)
            end = query;

        if (fragment != std::string::npos &&
            fragment < end)
        {
            end = fragment;
        }

        while (end > 0 &&
            result[end - 1] == '/')
        {
            --end;
        }

        result.resize(end);

        return result;
    }

    bool IsHttps(
        const std::string& url)
    {
        return Lower(url).rfind(
            "https://",
            0) == 0;
    }

    bool IsHttp(
        const std::string& url)
    {
        return Lower(url).rfind(
            "http://",
            0) == 0;
    }

    std::vector<std::string> HeaderValues(
        const HttpResponse& response,
        const std::string& name)
    {
        std::vector<std::string> values;

        const std::string target =
            Lower(name);

        for (const auto& [key, value] :
            response.headers)
        {
            if (Lower(key) == target)
                values.push_back(value);
        }

        return values;
    }

    bool HasToken(
        const std::string& value,
        const std::string& token)
    {
        const std::string lower =
            Lower(value);

        const std::string target =
            Lower(token);

        size_t start = 0;

        while (start <= lower.size())
        {
            const size_t end =
                lower.find(';', start);

            const std::string part =
                Trim(
                    lower.substr(
                        start,
                        end == std::string::npos
                        ? end
                        : end - start));

            if (part == target)
                return true;

            if (end == std::string::npos)
                break;

            start = end + 1;
        }

        return false;
    }

    void AddMissingHeader(
        std::vector<Finding>& findings,
        const HttpResponse& response,
        const char* header,
        const char* ruleId,
        const char* title,
        const char* description)
    {
        if (HeaderValues(
            response,
            header).empty())
        {
            findings.push_back(
                {
                    ruleId,
                    "security-header",
                    Severity::Low,
                    Confidence::High,
                    title,
                    description,
                    std::string(header) +
                        ": missing"
                });
        }
    }
}

namespace SecurityChecks
{
    std::vector<Finding> CheckSecurityHeaders(
        const HttpResponse& response)
    {
        std::vector<Finding> findings;

        if (response.isHttps)
        {
            AddMissingHeader(
                findings,
                response,
                "Strict-Transport-Security",
                "headers.hsts.missing",
                "Missing HSTS",
                "HTTPS responses should use HSTS to instruct browsers to keep using HTTPS.");
        }

        AddMissingHeader(
            findings,
            response,
            "Content-Security-Policy",
            "headers.csp.missing",
            "Missing Content Security Policy",
            "A CSP can reduce the impact of content injection by restricting allowed resources.");

        AddMissingHeader(
            findings,
            response,
            "X-Content-Type-Options",
            "headers.content-type-options.missing",
            "Missing X-Content-Type-Options",
            "Set nosniff to prevent browsers from guessing a different content type.");

        AddMissingHeader(
            findings,
            response,
            "X-Frame-Options",
            "headers.frame-options.missing",
            "Missing clickjacking protection",
            "Use X-Frame-Options or a CSP frame-ancestors directive to control embedding.");

        AddMissingHeader(
            findings,
            response,
            "Referrer-Policy",
            "headers.referrer-policy.missing",
            "Missing Referrer-Policy",
            "Set a policy to control how much referrer information browsers send.");

        return findings;
    }

    std::vector<Finding> CheckCors(
        const HttpResponse& response)
    {
        std::vector<Finding> findings;

        const auto origins =
            HeaderValues(
                response,
                "Access-Control-Allow-Origin");

        const auto credentials =
            HeaderValues(
                response,
                "Access-Control-Allow-Credentials");

        bool wildcard = false;
        bool credentialed = false;

        for (const auto& value : origins)
        {
            if (Trim(value) == "*")
            {
                wildcard = true;
                break;
            }
        }

        for (const auto& value : credentials)
        {
            if (Lower(Trim(value)) == "true")
            {
                credentialed = true;
                break;
            }
        }

        /*
         * Credentialed wildcard CORS is the more specific
         * condition, so do not also report the generic
         * wildcard finding.
         */
        if (wildcard && credentialed)
        {
            findings.push_back(
                {
                    "cors.wildcard.credentials",
                    "cors",
                    Severity::Low,
                    Confidence::High,
                    "Invalid credentialed CORS wildcard",
                    "Credentialed CORS must not use a wildcard origin; review origin validation and proxy behavior.",
                    "Access-Control-Allow-Origin: *; Access-Control-Allow-Credentials: true"
                });

            return findings;
        }

        if (wildcard)
        {
            findings.push_back(
                {
                    "cors.wildcard",
                    "cors",
                    Severity::Low,
                    Confidence::Medium,
                    "CORS allows every origin",
                    "A wildcard origin allows cross-origin reads for responses that are otherwise eligible for CORS.",
                    "Access-Control-Allow-Origin: *"
                });
        }

        return findings;
    }

    std::vector<Finding> CheckCookies(
        const HttpResponse& response)
    {
        std::vector<Finding> findings;

        const auto cookies =
            HeaderValues(
                response,
                "Set-Cookie");

        for (const auto& value : cookies)
        {
            const std::string cookie =
                Trim(value);

            const size_t separator =
                cookie.find('=');

            const std::string name =
                separator == std::string::npos
                ? cookie
                : cookie.substr(
                    0,
                    separator);

            const bool secure =
                HasToken(cookie, "secure");

            const bool httpOnly =
                HasToken(cookie, "httponly");

            const bool sameSite =
                Lower(cookie).find(
                    "samesite=") !=
                std::string::npos;

            if (!secure)
            {
                findings.push_back(
                    {
                        "cookie.secure.missing",
                        "cookie",
                        Severity::Low,
                        Confidence::High,
                        "Cookie missing Secure",
                        "Cookies should use Secure so browsers send them only over HTTPS.",
                        "Set-Cookie: " +
                            name +
                            " (Secure missing)"
                    });
            }

            if (!httpOnly)
            {
                findings.push_back(
                    {
                        "cookie.httponly.missing",
                        "cookie",
                        Severity::Low,
                        Confidence::High,
                        "Cookie missing HttpOnly",
                        "Use HttpOnly for cookies that do not need JavaScript access.",
                        "Set-Cookie: " +
                            name +
                            " (HttpOnly missing)"
                    });
            }

            if (!sameSite)
            {
                findings.push_back(
                    {
                        "cookie.samesite.missing",
                        "cookie",
                        Severity::Low,
                        Confidence::High,
                        "Cookie missing SameSite",
                        "Set an appropriate SameSite attribute to reduce cross-site request exposure.",
                        "Set-Cookie: " +
                            name +
                            " (SameSite missing)"
                    });
            }
        }

        return findings;
    }

    std::vector<Finding> CheckInformationDisclosure(
        const HttpResponse& response)
    {
        std::vector<Finding> findings;

        for (const char* name :
            { "Server", "X-Powered-By" })
        {
            for (const auto& value :
                HeaderValues(response, name))
            {
                const std::string trimmed =
                    Trim(value);

                if (trimmed.empty())
                    continue;

                const std::string ruleId =
                    Lower(name) == "server"
                    ? "disclosure.server"
                    : "disclosure.x-powered-by";

                findings.push_back(
                    {
                        ruleId,
                        "information-disclosure",
                        Severity::Info,
                        Confidence::High,
                        "Server information disclosed",
                        "The response reveals server or framework information that may aid fingerprinting.",
                        std::string(name) +
                            ": " +
                            trimmed
                    });
            }
        }

        return findings;
    }

    std::vector<Finding>
        SecurityChecks::CheckRedirectSecurity(
            const HttpResponse& response)
    {
        std::vector<Finding> findings;

        if (response.requestedUrl.empty() ||
            response.finalUrl.empty())
        {
            return findings;
        }

        const std::string requested =
            NormalizeUrlForComparison(
                response.requestedUrl);

        const std::string final =
            NormalizeUrlForComparison(
                response.finalUrl);

        if (requested.empty() ||
            final.empty() ||
            requested == final)
        {
            return findings;
        }

        const bool requestedHttps =
            IsHttps(response.requestedUrl);

        const bool finalHttp =
            IsHttp(response.finalUrl);

        if (requestedHttps && finalHttp)
        {
            Finding finding;

            finding.ruleId =
                "redirect.https-downgrade";

            finding.category =
                "redirect";

            finding.severity =
                Severity::Low;

            finding.confidence =
                Confidence::High;

            finding.title =
                "HTTPS redirected to HTTP";

            finding.description =
                "The request was redirected from HTTPS to HTTP, "
                "which can expose the connection to downgrade risks.";

            finding.evidence =
                "Requested: " +
                response.requestedUrl +
                " -> Final: " +
                response.finalUrl;

            findings.push_back(
                finding);

            return findings;
        }

        Finding finding;

        finding.ruleId =
            "redirect.detected";

        finding.category =
            "redirect";

        finding.severity =
            Severity::Info;

        finding.confidence =
            Confidence::High;

        finding.title =
            "HTTP redirect detected";

        finding.description =
            "The final URL differs from the originally requested URL.";

        finding.evidence =
            "Requested: " +
            response.requestedUrl +
            " -> Final: " +
            response.finalUrl;

        findings.push_back(
            finding);

        return findings;
    }
}