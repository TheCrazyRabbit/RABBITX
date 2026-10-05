#include "http_client.h"

#include <windows.h>
#include <winhttp.h>

#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <set>

#pragma comment(lib, "winhttp.lib")

namespace
{
    std::wstring ToWide(const std::string& input)
    {
        if (input.empty())
            return {};

        const int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            input.c_str(),
            static_cast<int>(input.size()),
            nullptr,
            0);

        if (size <= 0)
            return {};

        std::vector<wchar_t> buffer(size + 1, L'\0');

        if (MultiByteToWideChar(
            CP_UTF8,
            0,
            input.c_str(),
            static_cast<int>(input.size()),
            buffer.data(),
            size) <= 0)
        {
            return {};
        }

        return std::wstring(buffer.data(), size);
    }

    std::string WideToUtf8(const std::wstring& input)
    {
        if (input.empty())
            return {};

        const int size = WideCharToMultiByte(
            CP_UTF8,
            0,
            input.c_str(),
            static_cast<int>(input.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (size <= 0)
            return {};

        std::vector<char> buffer(size + 1, '\0');

        if (WideCharToMultiByte(
            CP_UTF8,
            0,
            input.c_str(),
            static_cast<int>(input.size()),
            buffer.data(),
            size,
            nullptr,
            nullptr) <= 0)
        {
            return {};
        }

        return std::string(buffer.data(), size);
    }

    std::string Win32Error(DWORD error)
    {
        if (error == ERROR_SUCCESS)
            return {};

        LPWSTR messageBuffer = nullptr;

        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            reinterpret_cast<LPWSTR>(&messageBuffer),
            0,
            nullptr);

        std::string result;

        if (length > 0 && messageBuffer)
        {
            result = WideToUtf8(
                std::wstring(
                    messageBuffer,
                    length));
        }

        if (messageBuffer)
            LocalFree(messageBuffer);

        while (!result.empty() &&
            (result.back() == '\r' ||
                result.back() == '\n'))
        {
            result.pop_back();
        }

        return result;
    }

    bool CrackUrl(
        const std::wstring& url,
        URL_COMPONENTS& components,
        std::vector<wchar_t>& hostBuffer,
        std::vector<wchar_t>& pathBuffer,
        std::vector<wchar_t>& extraBuffer)
    {
        hostBuffer.resize(512);
        pathBuffer.resize(8192);
        extraBuffer.resize(8192);

        ZeroMemory(
            &components,
            sizeof(components));

        components.dwStructSize =
            sizeof(components);

        components.lpszHostName =
            hostBuffer.data();

        components.dwHostNameLength =
            static_cast<DWORD>(
                hostBuffer.size());

        components.lpszUrlPath =
            pathBuffer.data();

        components.dwUrlPathLength =
            static_cast<DWORD>(
                pathBuffer.size());

        components.lpszExtraInfo =
            extraBuffer.data();

        components.dwExtraInfoLength =
            static_cast<DWORD>(
                extraBuffer.size());

        return WinHttpCrackUrl(
            url.c_str(),
            0,
            0,
            &components) == TRUE;
    }

    std::string BuildRequestPath(
        const URL_COMPONENTS& components)
    {
        std::wstring path;

        if (components.dwUrlPathLength > 0)
        {
            path.assign(
                components.lpszUrlPath,
                components.dwUrlPathLength);
        }
        else
        {
            path = L"/";
        }

        if (components.dwExtraInfoLength > 0)
        {
            path.append(
                components.lpszExtraInfo,
                components.dwExtraInfoLength);
        }

        return WideToUtf8(path);
    }

    std::string ReadResponseBody(
        HINTERNET request)
    {
        std::string body;

        DWORD available = 0;

        while (WinHttpQueryDataAvailable(
            request,
            &available))
        {
            if (available == 0)
                break;

            std::vector<BYTE> buffer(
                available);

            DWORD downloaded = 0;

            if (!WinHttpReadData(
                request,
                buffer.data(),
                available,
                &downloaded))
            {
                break;
            }

            if (downloaded == 0)
                break;

            body.append(
                reinterpret_cast<const char*>(
                    buffer.data()),
                downloaded);
        }

        return body;
    }

    void ReadHeaders(
        HINTERNET request,
        HttpResponse& response)
    {
        DWORD headerSize = 0;

        WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            nullptr,
            &headerSize,
            WINHTTP_NO_HEADER_INDEX);

        if (GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER)
        {
            return;
        }

        std::vector<wchar_t> buffer(
            headerSize / sizeof(wchar_t) + 1,
            L'\0');

        if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            buffer.data(),
            &headerSize,
            WINHTTP_NO_HEADER_INDEX))
        {
            return;
        }

        std::wstring rawHeaders(
            buffer.data(),
            headerSize / sizeof(wchar_t));

        std::wistringstream stream(
            rawHeaders);

        std::wstring line;

        while (std::getline(stream, line))
        {
            if (!line.empty() &&
                line.back() == L'\r')
            {
                line.pop_back();
            }

            const size_t colon =
                line.find(L':');

            if (colon == std::wstring::npos)
                continue;

            const std::wstring key =
                line.substr(0, colon);

            std::wstring value =
                line.substr(colon + 1);

            while (!value.empty() &&
                (value.front() == L' ' ||
                    value.front() == L'\t'))
            {
                value.erase(
                    value.begin());
            }

            response.headers[
                WideToUtf8(key)] =
                WideToUtf8(value);
        }
    }

    bool IsRedirectStatus(
        int statusCode)
    {
        return statusCode == 301 ||
            statusCode == 302 ||
            statusCode == 303 ||
            statusCode == 307 ||
            statusCode == 308;
    }

    std::string FindHeader(
        const HttpResponse& response,
        const std::string& name)
    {
        for (const auto& header : response.headers)
        {
            if (header.first.size() != name.size())
                continue;

            bool equal = true;

            for (size_t i = 0; i < name.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(header.first[i])) !=
                    std::tolower(static_cast<unsigned char>(name[i])))
                {
                    equal = false;
                    break;
                }
            }

            if (equal)
                return header.second;
        }

        return {};
    }

    std::string ResolveRedirect(
        const std::string& baseUrl,
        const std::string& location)
    {
        const size_t schemeEnd = baseUrl.find("://");

        if (schemeEnd == std::string::npos || location.empty())
            return {};

        const std::string scheme = baseUrl.substr(0, schemeEnd);
        const size_t authorityStart = schemeEnd + 3;
        const size_t authorityEnd = baseUrl.find_first_of(
            "/?#",
            authorityStart);
        const std::string origin = baseUrl.substr(
            0,
            authorityEnd == std::string::npos
                ? baseUrl.size()
                : authorityEnd);

        std::string basePathAndQuery = authorityEnd == std::string::npos
            ? "/"
            : baseUrl.substr(authorityEnd);

        const size_t baseFragment = basePathAndQuery.find('#');
        if (baseFragment != std::string::npos)
            basePathAndQuery.erase(baseFragment);

        if (basePathAndQuery.empty() ||
            basePathAndQuery.front() == '?')
        {
            basePathAndQuery = "/" + basePathAndQuery;
        }

        std::string cleanLocation = location;
        const size_t fragment = cleanLocation.find('#');
        if (fragment != std::string::npos)
            cleanLocation.erase(fragment);

        if (cleanLocation.empty())
            return origin + basePathAndQuery;

        if (cleanLocation.find("://") != std::string::npos)
            return cleanLocation;

        if (cleanLocation.rfind("//", 0) == 0)
            return scheme + ":" + cleanLocation;

        if (cleanLocation.front() == '?')
        {
            const size_t query = basePathAndQuery.find('?');
            const std::string basePath = query == std::string::npos
                ? basePathAndQuery
                : basePathAndQuery.substr(0, query);
            return origin + basePath + cleanLocation;
        }

        if (cleanLocation.front() == '/')
            return origin + cleanLocation;

        const size_t query = basePathAndQuery.find('?');
        const std::string basePath = query == std::string::npos
            ? basePathAndQuery
            : basePathAndQuery.substr(0, query);
        const size_t lastSlash = basePath.rfind('/');
        const std::string directory = lastSlash == std::string::npos
            ? "/"
            : basePath.substr(0, lastSlash + 1);

        std::string combinedPath = directory + cleanLocation;
        const size_t combinedQuery = combinedPath.find('?');
        const std::string queryPart = combinedQuery == std::string::npos
            ? std::string{}
            : combinedPath.substr(combinedQuery);
        const std::string pathPart = combinedQuery == std::string::npos
            ? combinedPath
            : combinedPath.substr(0, combinedQuery);

        std::vector<std::string> segments;
        size_t position = 1;

        while (position <= pathPart.size())
        {
            const size_t slash = pathPart.find('/', position);
            const size_t end = slash == std::string::npos
                ? pathPart.size()
                : slash;
            const std::string segment = pathPart.substr(
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

        std::string normalizedPath = "/";

        for (size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0)
                normalizedPath += '/';

            normalizedPath += segments[i];
        }

        if (pathPart.size() > 1 && pathPart.back() == '/')
            normalizedPath += '/';

        return origin + normalizedPath + queryPart;
    }
}

static HttpResponse GetOnce(
    const std::string& url,
    bool followRedirects,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter,
    int timeoutMs,
    bool useLegacyTimeoutProfile)
{
    HttpResponse response;

    response.method = "GET";
    response.requestedUrl = url;

    const std::wstring wideUrl =
        ToWide(url);

    if (wideUrl.empty())
    {
        response.error =
            "Invalid or empty URL.";

        return response;
    }

    URL_COMPONENTS components{};

    std::vector<wchar_t> hostBuffer;
    std::vector<wchar_t> pathBuffer;
    std::vector<wchar_t> extraBuffer;

    if (!CrackUrl(
        wideUrl,
        components,
        hostBuffer,
        pathBuffer,
        extraBuffer))
    {
        response.error =
            "WinHttpCrackUrl failed: " +
            Win32Error(GetLastError());

        return response;
    }

    response.isHttps =
        components.nScheme ==
        INTERNET_SCHEME_HTTPS;

    HINTERNET session =
        WinHttpOpen(
            L"RABBITX/0.4",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

    if (!session)
    {
        response.error =
            "WinHttpOpen failed: " +
            Win32Error(GetLastError());

        return response;
    }

    const int resolveTimeoutMs = useLegacyTimeoutProfile
        ? 10000
        : timeoutMs;
    const int connectTimeoutMs = useLegacyTimeoutProfile
        ? 10000
        : timeoutMs;
    const int sendTimeoutMs = useLegacyTimeoutProfile
        ? 15000
        : timeoutMs;
    const int receiveTimeoutMs = useLegacyTimeoutProfile
        ? 15000
        : timeoutMs;

    WinHttpSetTimeouts(
        session,
        resolveTimeoutMs,
        connectTimeoutMs,
        sendTimeoutMs,
        receiveTimeoutMs);

    HINTERNET connection =
        WinHttpConnect(
            session,
            components.lpszHostName,
            components.nPort,
            0);

    if (!connection)
    {
        response.error =
            "WinHttpConnect failed: " +
            Win32Error(GetLastError());

        WinHttpCloseHandle(session);

        return response;
    }

    DWORD flags = 0;

    if (response.isHttps)
        flags |= WINHTTP_FLAG_SECURE;

    const std::string requestPath =
        BuildRequestPath(components);

    const std::wstring widePath =
        ToWide(requestPath);

    HINTERNET request =
        WinHttpOpenRequest(
            connection,
            L"GET",
            widePath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);

    if (!request)
    {
        response.error =
            "WinHttpOpenRequest failed: " +
            Win32Error(GetLastError());

        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    DWORD redirectPolicy = followRedirects
        ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
        : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;

    if (!WinHttpSetOption(
        request,
        WINHTTP_OPTION_REDIRECT_POLICY,
        &redirectPolicy,
        sizeof(redirectPolicy)) && !followRedirects)
    {
        response.error =
            "Unable to disable automatic redirects for scoped request: " +
            Win32Error(GetLastError());

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    const wchar_t* headers =
        L"Accept: */*\r\n"
        L"User-Agent: RABBITX/0.4\r\n";

    if (!requestBudget.TryBeginRequest())
    {
        response.error = "Request budget exhausted before HTTP send.";
        response.disposition = HttpRequestDisposition::BudgetRejected;

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    requestRateLimiter.WaitForSlot();

    if (!WinHttpSendRequest(
        request,
        headers,
        static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0))
    {
        response.error =
            "WinHttpSendRequest failed: " +
            Win32Error(GetLastError());
        response.disposition = HttpRequestDisposition::Failed;

        requestBudget.RecordFailedRequest();

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    if (!WinHttpReceiveResponse(
        request,
        nullptr))
    {
        response.error =
            "WinHttpReceiveResponse failed: " +
            Win32Error(GetLastError());
        response.disposition = HttpRequestDisposition::Failed;

        requestBudget.RecordFailedRequest();

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize =
        sizeof(statusCode);

    if (!WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        response.error =
            "Failed to read HTTP status: " +
            Win32Error(GetLastError());
        response.disposition = HttpRequestDisposition::Failed;

        requestBudget.RecordFailedRequest();

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return response;
    }

    response.statusCode =
        static_cast<int>(statusCode);
    response.disposition = HttpRequestDisposition::Succeeded;

    requestBudget.RecordSuccessfulRequest();

    ReadHeaders(
        request,
        response);

    response.body =
        ReadResponseBody(request);

    DWORD urlSize = 0;

    WinHttpQueryOption(
        request,
        WINHTTP_OPTION_URL,
        nullptr,
        &urlSize);

    if (GetLastError() ==
        ERROR_INSUFFICIENT_BUFFER)
    {
        std::vector<wchar_t> finalUrlBuffer(
            urlSize / sizeof(wchar_t) + 1,
            L'\0');

        if (WinHttpQueryOption(
            request,
            WINHTTP_OPTION_URL,
            finalUrlBuffer.data(),
            &urlSize))
        {
            response.finalUrl =
                WideToUtf8(
                    std::wstring(
                        finalUrlBuffer.data()));
        }
    }

    if (response.finalUrl.empty())
        response.finalUrl = url;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return response;
}

HttpResponse HttpClient::Get(
    const std::string& url)
{
    RequestBudget requestBudget(DefaultMaxRequestsPerRun);
    RequestRateLimiter requestRateLimiter;
    ScopePolicy scopePolicy;
    std::string scopeError;

    if (!ScopePolicyEngine::CreateDefault(
        url,
        scopePolicy,
        scopeError))
    {
        HttpResponse response;
        response.method = "GET";
        response.requestedUrl = url;
        response.error = "Unable to create default request scope: " + scopeError;
        return response;
    }

    return Get(url, scopePolicy, requestBudget, requestRateLimiter);
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const ScopePolicy& scopePolicy)
{
    AuditConfiguration configuration;
    configuration.scope = scopePolicy;
    RequestBudget requestBudget(DefaultMaxRequestsPerRun);
    RequestRateLimiter requestRateLimiter;
    return Get(url, configuration, requestBudget, requestRateLimiter);
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const ScopePolicy& scopePolicy,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter)
{
    AuditConfiguration configuration;
    configuration.scope = scopePolicy;
    return Get(url, configuration, requestBudget, requestRateLimiter);
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const AuditConfiguration& configuration,
    RequestBudget& requestBudget,
    RequestRateLimiter& requestRateLimiter)
{
    HttpResponse response;
    response.method = "GET";
    response.requestedUrl = url;

    std::string currentUrl = url;
    std::set<std::string> visitedUrls;
    visitedUrls.insert(currentUrl);

    for (size_t redirectCount = 0; redirectCount <= 10; ++redirectCount)
    {
        const ScopeDecision decision =
            ScopePolicyEngine::Check(configuration.scope, currentUrl);

        if (!decision.allowed)
        {
            requestBudget.RecordScopeRejectedRequest();

            if (redirectCount == 0)
            {
                response.error = "Scope denied: " + decision.reason;
                response.disposition = HttpRequestDisposition::ScopeRejected;
                return response;
            }

            response.error = "Redirect blocked by scope: " + decision.reason;
            response.disposition = HttpRequestDisposition::ScopeRejected;
            return response;
        }

        HttpResponse previousResponse = response;
        HttpResponse hopResponse = GetOnce(
            currentUrl,
            false,
            requestBudget,
            requestRateLimiter,
            configuration.timeoutMs,
            configuration.useLegacyTimeoutProfile);
        hopResponse.requestedUrl = url;

        if (redirectCount > 0 && hopResponse.statusCode == 0)
        {
            response = previousResponse;
            response.error = hopResponse.error;
            response.disposition = hopResponse.disposition;
            return response;
        }

        response = hopResponse;

        if (response.statusCode == 0 ||
            response.disposition == HttpRequestDisposition::BudgetRejected ||
            response.disposition == HttpRequestDisposition::Failed)
            return response;

        if (!configuration.followRedirects)
            return response;

        const std::string location = FindHeader(response, "Location");

        if (!IsRedirectStatus(response.statusCode) || location.empty())
            return response;

        if (redirectCount == 10)
        {
            response.error = "Maximum scoped redirect count reached.";
            return response;
        }

        const std::string redirectUrl = ResolveRedirect(
            currentUrl,
            location);

        if (redirectUrl.empty())
        {
            response.error = "Unable to resolve redirect target URL.";
            return response;
        }

        const ScopeDecision redirectDecision =
            ScopePolicyEngine::Check(configuration.scope, redirectUrl);

        if (!redirectDecision.allowed)
        {
            requestBudget.RecordScopeRejectedRequest();
            response.error = "Redirect blocked by scope: " +
                redirectDecision.reason;
            response.disposition = HttpRequestDisposition::ScopeRejected;
            return response;
        }

        if (!visitedUrls.insert(redirectUrl).second)
        {
            response.error = "Redirect loop stopped within scope.";
            return response;
        }

        currentUrl = redirectUrl;
    }

    response.error = "Scoped redirect processing stopped unexpectedly.";
    return response;
}

const char* HttpRequestDispositionToString(
    HttpRequestDisposition disposition)
{
    switch (disposition)
    {
    case HttpRequestDisposition::Succeeded:
        return "succeeded";
    case HttpRequestDisposition::Failed:
        return "failed";
    case HttpRequestDisposition::ScopeRejected:
        return "scope_rejected";
    case HttpRequestDisposition::BudgetRejected:
        return "budget_rejected";
    default:
        return "not_attempted";
    }
}
