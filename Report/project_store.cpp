#include "project_store.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>

#include "scope_policy.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
    std::string EscapeJson(
        const std::string& value)
    {
        std::ostringstream out;

        for (const unsigned char ch : value)
        {
            switch (ch)
            {
            case '"':
                out << "\\\"";
                break;

            case '\\':
                out << "\\\\";
                break;

            case '\b':
                out << "\\b";
                break;

            case '\f':
                out << "\\f";
                break;

            case '\n':
                out << "\\n";
                break;

            case '\r':
                out << "\\r";
                break;

            case '\t':
                out << "\\t";
                break;

            default:
                if (ch < 0x20)
                {
                    const char* hex =
                        "0123456789abcdef";

                    out << "\\u00"
                        << hex[(ch >> 4) & 0x0F]
                        << hex[ch & 0x0F];
                }
                else
                {
                    out << static_cast<char>(ch);
                }
                break;
            }
        }

        return out.str();
    }

    std::string Quote(
        const std::string& value)
    {
        return "\"" + EscapeJson(value) + "\"";
    }

    std::string UtcNow()
    {
        const std::time_t now =
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());

        std::tm utc{};

        if (gmtime_s(&utc, &now) != 0)
            return {};

        char buffer[32]{};

        if (std::strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%dT%H:%M:%SZ",
            &utc) == 0)
        {
            return {};
        }

        return buffer;
    }

    std::string GenerateId()
    {
        std::ostringstream out;
        out << std::hex << std::setfill('0');

        try
        {
            std::random_device random;

            for (int i = 0; i < 4; ++i)
            {
                out << std::setw(8)
                    << static_cast<unsigned int>(random());
            }
        }
        catch (...)
        {
            const auto ticks =
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count();

            out << std::setw(16)
                << static_cast<unsigned long long>(ticks);
        }

        return out.str();
    }

    std::string DefaultName(
        const std::string& targetUrl)
    {
        size_t start = targetUrl.find("://");

        start = start == std::string::npos
            ? 0
            : start + 3;

        size_t end = targetUrl.find_first_of(
            "/?#",
            start);

        if (end == std::string::npos)
            end = targetUrl.size();

        if (end <= start)
            return "RABBITX Project";

        const size_t credentials = targetUrl.rfind('@', end);
        if (credentials != std::string::npos && credentials >= start)
            start = credentials + 1;

        if (start >= end)
            return "RABBITX Project";

        return targetUrl.substr(start, end - start);
    }

    size_t FindValueStart(
        const std::string& json,
        const std::string& key,
        size_t searchFrom = 0)
    {
        const std::string field =
            "\"" + key + "\"";

        size_t position = json.find(field, searchFrom);

        while (position != std::string::npos)
        {
            position += field.size();

            while (position < json.size() &&
                std::isspace(
                    static_cast<unsigned char>(json[position])))
            {
                ++position;
            }

            if (position < json.size() && json[position] == ':')
            {
                ++position;

                while (position < json.size() &&
                    std::isspace(
                        static_cast<unsigned char>(json[position])))
                {
                    ++position;
                }

                return position;
            }

            position = json.find(field, position);
        }

        return std::string::npos;
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

    bool ReadHexCodePoint(
        const std::string& json,
        size_t start,
        unsigned int& codePoint)
    {
        if (start + 4 > json.size())
            return false;

        codePoint = 0;

        for (size_t i = 0; i < 4; ++i)
        {
            const int digit = HexValue(json[start + i]);

            if (digit < 0)
                return false;

            codePoint = (codePoint << 4) |
                static_cast<unsigned int>(digit);
        }

        return true;
    }

    void AppendUtf8(
        std::string& output,
        unsigned int codePoint)
    {
        if (codePoint <= 0x7F)
        {
            output += static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF)
        {
            output += static_cast<char>(0xC0 | (codePoint >> 6));
            output += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint <= 0xFFFF)
        {
            output += static_cast<char>(0xE0 | (codePoint >> 12));
            output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else
        {
            output += static_cast<char>(0xF0 | (codePoint >> 18));
            output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            output += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }

    bool ReadString(
        const std::string& json,
        size_t start,
        std::string& value,
        size_t& end)
    {
        if (start >= json.size() || json[start] != '"')
            return false;

        value.clear();

        for (size_t i = start + 1; i < json.size(); ++i)
        {
            const char ch = json[i];

            if (ch == '"')
            {
                end = i + 1;
                return true;
            }

            if (ch != '\\')
            {
                value += ch;
                continue;
            }

            if (++i >= json.size())
                return false;

            switch (json[i])
            {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;

            case 'u':
            {
                unsigned int codePoint = 0;

                if (!ReadHexCodePoint(
                    json,
                    i + 1,
                    codePoint))
                {
                    return false;
                }

                i += 4;

                if (codePoint >= 0xD800 &&
                    codePoint <= 0xDBFF &&
                    i + 6 < json.size() &&
                    json[i + 1] == '\\' &&
                    json[i + 2] == 'u')
                {
                    unsigned int low = 0;

                    if (ReadHexCodePoint(
                        json,
                        i + 3,
                        low) &&
                        low >= 0xDC00 &&
                        low <= 0xDFFF)
                    {
                        codePoint = 0x10000 +
                            ((codePoint - 0xD800) << 10) +
                            (low - 0xDC00);

                        i += 6;
                    }
                }

                AppendUtf8(value, codePoint);
                break;
            }

            default:
                return false;
            }
        }

        return false;
    }

    bool ReadStringProperty(
        const std::string& json,
        const std::string& key,
        std::string& value,
        size_t searchFrom = 0)
    {
        const size_t start =
            FindValueStart(json, key, searchFrom);

        if (start == std::string::npos)
            return false;

        size_t end = 0;
        return ReadString(json, start, value, end);
    }

    bool ReadStringArrayProperty(
        const std::string& json,
        const std::string& key,
        std::vector<std::string>& values,
        size_t searchFrom = 0)
    {
        const size_t start = FindValueStart(json, key, searchFrom);

        if (start == std::string::npos ||
            start >= json.size() ||
            json[start] != '[')
        {
            return false;
        }

        values.clear();
        size_t position = start + 1;

        while (position < json.size())
        {
            while (position < json.size() &&
                (std::isspace(static_cast<unsigned char>(json[position])) ||
                    json[position] == ','))
            {
                ++position;
            }

            if (position >= json.size())
                return false;

            if (json[position] == ']')
                return true;

            std::string value;
            size_t end = 0;

            if (!ReadString(json, position, value, end))
                return false;

            values.push_back(value);
            position = end;
        }

        return false;
    }

    bool ExtractObject(
        const std::string& json,
        size_t start,
        std::string& object,
        size_t& end)
    {
        if (start >= json.size() || json[start] != '{')
            return false;

        int depth = 0;
        bool inString = false;
        bool escaped = false;

        for (size_t i = start; i < json.size(); ++i)
        {
            const char ch = json[i];

            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                    continue;
                }

                if (ch == '\\')
                {
                    escaped = true;
                    continue;
                }

                if (ch == '"')
                    inString = false;

                continue;
            }

            if (ch == '"')
            {
                inString = true;
            }
            else if (ch == '{')
            {
                ++depth;
            }
            else if (ch == '}')
            {
                --depth;

                if (depth == 0)
                {
                    end = i + 1;
                    object = json.substr(
                        start,
                        end - start);

                    return true;
                }
            }
        }

        return false;
    }

    bool ExtractObjectProperty(
        const std::string& json,
        const std::string& key,
        std::string& object,
        size_t searchFrom = 0)
    {
        const size_t start =
            FindValueStart(json, key, searchFrom);

        if (start == std::string::npos)
            return false;

        size_t end = 0;
        return ExtractObject(json, start, object, end);
    }

    bool IsJsonObject(
        const std::string& json)
    {
        size_t start = 0;

        while (start < json.size() &&
            std::isspace(
                static_cast<unsigned char>(json[start])))
        {
            ++start;
        }

        std::string object;
        size_t end = 0;

        if (!ExtractObject(json, start, object, end))
            return false;

        while (end < json.size() &&
            std::isspace(
                static_cast<unsigned char>(json[end])))
        {
            ++end;
        }

        return end == json.size();
    }

    bool ReadVersion(
        const std::string& json,
        int& version)
    {
        const size_t start =
            FindValueStart(json, "projectSchemaVersion");

        if (start == std::string::npos ||
            start >= json.size() ||
            !std::isdigit(
                static_cast<unsigned char>(json[start])))
        {
            return false;
        }

        version = 0;
        size_t i = start;

        while (i < json.size() &&
            std::isdigit(
                static_cast<unsigned char>(json[i])))
        {
            version = version * 10 +
                json[i] - '0';
            ++i;
        }

        return true;
    }

    bool ReadSizeProperty(
        const std::string& json,
        const std::string& key,
        size_t& value)
    {
        const size_t start = FindValueStart(json, key);

        if (start == std::string::npos ||
            start >= json.size() ||
            !std::isdigit(static_cast<unsigned char>(json[start])))
        {
            return false;
        }

        size_t parsed = 0;
        size_t position = start;

        while (position < json.size() &&
            std::isdigit(static_cast<unsigned char>(json[position])))
        {
            const size_t digit = static_cast<size_t>(json[position] - '0');
            if (parsed > ((std::numeric_limits<size_t>::max)() - digit) / 10)
                return false;

            parsed = parsed * 10 + digit;
            ++position;
        }

        while (position < json.size() &&
            std::isspace(static_cast<unsigned char>(json[position])))
        {
            ++position;
        }

        if (position < json.size() &&
            json[position] != ',' &&
            json[position] != '}' &&
            json[position] != ']')
        {
            return false;
        }

        value = parsed;
        return true;
    }

    bool ReadBoolProperty(
        const std::string& json,
        const std::string& key,
        bool& value)
    {
        const size_t start = FindValueStart(json, key);
        if (start == std::string::npos)
            return false;

        if (json.compare(start, 4, "true") == 0)
        {
            value = true;
            return true;
        }

        if (json.compare(start, 5, "false") == 0)
        {
            value = false;
            return true;
        }

        return false;
    }

    bool ReadProjectDefaults(
        const std::string& json,
        AuditConfiguration& configuration)
    {
        size_t timeoutMs = 0;
        std::string outputFormat;
        std::string reportOptions;

        if (!ReadSizeProperty(json, "maxRequests", configuration.maxRequests) ||
            !ReadSizeProperty(json, "minIntervalMs", configuration.minIntervalMs) ||
            configuration.minIntervalMs > 86400000 ||
            !ReadSizeProperty(json, "timeoutMs", timeoutMs) ||
            timeoutMs == 0 || timeoutMs > MaximumTimeoutMs ||
            !ReadBoolProperty(json, "useLegacyTimeoutProfile", configuration.useLegacyTimeoutProfile) ||
            !ReadBoolProperty(json, "followRedirects", configuration.followRedirects) ||
            !ReadStringProperty(json, "outputFormat", outputFormat) ||
            !AuditOutputFormatFromString(outputFormat, configuration.outputFormat) ||
            !ReadStringProperty(json, "outputPath", configuration.outputPath) ||
            !ReadBoolProperty(json, "persistProject", configuration.persistProject) ||
            !ExtractObjectProperty(json, "reportOptions", reportOptions) ||
            !ReadBoolProperty(reportOptions, "includeEvidence", configuration.reportOptions.includeEvidence) ||
            !ReadBoolProperty(reportOptions, "includeProbeHistory", configuration.reportOptions.includeProbeHistory))
        {
            return false;
        }

        configuration.timeoutMs = static_cast<int>(timeoutMs);
        return true;
    }

    bool ReadRequestBudgetStats(
        const std::string& json,
        RequestBudgetStats& stats,
        bool requireInterval)
    {
        const bool hasInterval = ReadSizeProperty(
            json,
            "minRequestIntervalMs",
            stats.minRequestIntervalMs);

        return ReadSizeProperty(json, "maxRequestsPerRun", stats.maxRequestsPerRun) &&
            (hasInterval || !requireInterval) &&
            ReadSizeProperty(json, "attemptedRequests", stats.attemptedRequests) &&
            ReadSizeProperty(json, "successfulRequests", stats.successfulRequests) &&
            ReadSizeProperty(json, "failedRequests", stats.failedRequests) &&
            ReadSizeProperty(json, "scopeRejectedRequests", stats.scopeRejectedRequests) &&
            ReadSizeProperty(json, "budgetRejectedRequests", stats.budgetRejectedRequests) &&
            ReadSizeProperty(json, "remainingRequests", stats.remainingRequests) &&
            stats.attemptedRequests <= stats.maxRequestsPerRun &&
            stats.remainingRequests == stats.maxRequestsPerRun - stats.attemptedRequests &&
            stats.successfulRequests <= stats.attemptedRequests &&
            stats.failedRequests == stats.attemptedRequests - stats.successfulRequests;
    }

    bool ExtractRuns(
        const std::string& json,
        std::vector<ProjectAuditRun>& runs,
        bool requireRequestBudget,
        bool requireBudgetInterval)
    {
        const size_t arrayStart =
            FindValueStart(json, "runs");

        if (arrayStart == std::string::npos ||
            arrayStart >= json.size() ||
            json[arrayStart] != '[')
        {
            return false;
        }

        size_t position = arrayStart + 1;

        while (position < json.size())
        {
            while (position < json.size() &&
                (std::isspace(
                    static_cast<unsigned char>(json[position])) ||
                    json[position] == ','))
            {
                ++position;
            }

            if (position >= json.size())
                return false;

            if (json[position] == ']')
                return true;

            std::string runObject;
            size_t runEnd = 0;

            if (!ExtractObject(
                json,
                position,
                runObject,
                runEnd))
            {
                return false;
            }

            ProjectAuditRun run;

            if (!ReadStringProperty(
                runObject,
                "savedAtUtc",
                run.savedAtUtc) ||
                !ExtractObjectProperty(
                    runObject,
                    "report",
                    run.reportJson) ||
                !IsJsonObject(run.reportJson))
            {
                return false;
            }

            std::string budgetJson;
            if (ExtractObjectProperty(runObject, "requestBudget", budgetJson))
            {
                if (!ReadRequestBudgetStats(
                    budgetJson,
                    run.requestBudget,
                    requireBudgetInterval))
                    return false;
            }
            else if (requireRequestBudget)
            {
                return false;
            }

            runs.push_back(run);
            position = runEnd;
        }

        return false;
    }

    bool ReadWholeFile(
        const std::string& path,
        std::string& contents,
        std::string& error)
    {
        std::ifstream file(
            std::filesystem::u8path(path),
            std::ios::binary);

        if (!file.is_open())
        {
            error = "Unable to open project file: " + path;
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        if (!file.good() && !file.eof())
        {
            error = "Failed to read project file: " + path;
            return false;
        }

        contents = buffer.str();
        return true;
    }

    bool WriteWholeFile(
        const std::string& path,
        const std::string& contents,
        std::string& error)
    {
        const std::string temporaryPath =
            path + ".tmp." + GenerateId();

        const std::filesystem::path temporaryFile =
            std::filesystem::u8path(temporaryPath);

        std::ofstream file(
            temporaryFile,
            std::ios::binary | std::ios::trunc);

        if (!file.is_open())
        {
            error = "Unable to create file: " + path;
            return false;
        }

        file.write(
            contents.data(),
            static_cast<std::streamsize>(contents.size()));

        file.close();

        if (!file.good())
        {
            std::error_code ignored;
            std::filesystem::remove(temporaryFile, ignored);
            error = "Failed to write file: " + path;
            return false;
        }

        const std::filesystem::path destination =
            std::filesystem::u8path(path);

        if (!MoveFileExW(
            temporaryFile.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING |
            MOVEFILE_WRITE_THROUGH))
        {
            const DWORD code = GetLastError();
            std::error_code ignored;
            std::filesystem::remove(temporaryFile, ignored);

            error = "Unable to replace file: " + path +
                " (Windows error " +
                std::to_string(code) + ")";

            return false;
        }

        return true;
    }
}

Project ProjectStore::Create(
    const std::string& targetUrl,
    const std::string& name)
{
    Project project;
    project.id = GenerateId();
    project.name = name.empty()
        ? DefaultName(targetUrl)
        : name;
    project.targetUrl = targetUrl;
    project.createdAtUtc = UtcNow();
    project.updatedAtUtc = project.createdAtUtc;

    std::string scopeError;
    ScopePolicyEngine::CreateDefault(
        targetUrl,
        project.scopePolicy,
        scopeError);

    return project;
}

bool ProjectStore::Load(
    const std::string& path,
    Project& project,
    std::string& error)
{
    error.clear();

    std::string json;

    if (!ReadWholeFile(path, json, error))
        return false;

    int version = 0;

    if (!ReadVersion(json, version) ||
        (version != 1 && version != 2 && version != 3 && version != 4))
    {
        error = "Unsupported or invalid project schema version.";
        return false;
    }

    Project loaded;

    if (version == 3 &&
        !ReadSizeProperty(
            json,
            "defaultMaxRequestsPerRun",
            loaded.defaultConfiguration.maxRequests))
    {
        error = "Project file contains an invalid default request budget.";
        return false;
    }

    if (version == 3 &&
        FindValueStart(json, "defaultMinRequestIntervalMs") != std::string::npos &&
        !ReadSizeProperty(
            json,
            "defaultMinRequestIntervalMs",
            loaded.defaultConfiguration.minIntervalMs))
    {
        error = "Project file contains an invalid default request interval.";
        return false;
    }

    if (version == 4)
    {
        std::string defaultsJson;
        if (!ExtractObjectProperty(json, "defaultConfiguration", defaultsJson) ||
            !ReadProjectDefaults(defaultsJson, loaded.defaultConfiguration))
        {
            error = "Project file contains invalid default configuration.";
            return false;
        }
    }

    if (!ReadStringProperty(json, "projectId", loaded.id) ||
        !ReadStringProperty(json, "projectName", loaded.name) ||
        !ReadStringProperty(json, "targetUrl", loaded.targetUrl) ||
        !ReadStringProperty(json, "createdAtUtc", loaded.createdAtUtc) ||
        !ReadStringProperty(json, "updatedAtUtc", loaded.updatedAtUtc) ||
        loaded.targetUrl.empty())
    {
        error = "Project file is missing required metadata.";
        return false;
    }

    if (version == 1)
    {
        if (!ScopePolicyEngine::CreateDefault(
            loaded.targetUrl,
            loaded.scopePolicy,
            error))
        {
            error = "Legacy project target cannot produce a safe default scope: " + error;
            return false;
        }
    }
    else
    {
        std::string scopeJson;

        if (!ExtractObjectProperty(json, "scopePolicy", scopeJson) ||
            !ReadStringArrayProperty(
                scopeJson,
                "allowedHosts",
                loaded.scopePolicy.allowedHosts) ||
            !ReadStringArrayProperty(
                scopeJson,
                "allowedDomains",
                loaded.scopePolicy.allowedDomains) ||
            !ReadStringArrayProperty(
                scopeJson,
                "allowedPaths",
                loaded.scopePolicy.allowedPaths))
        {
            error = "Project file contains an invalid scope policy.";
            return false;
        }
    }

    if (loaded.defaultConfiguration.minIntervalMs > 86400000 ||
        loaded.defaultConfiguration.timeoutMs <= 0 ||
        loaded.defaultConfiguration.timeoutMs > MaximumTimeoutMs)
    {
        error = "Project default request limits are outside supported ranges.";
        return false;
    }

    if (!ScopePolicyEngine::Validate(
        loaded.scopePolicy,
        error))
    {
        error = "Project scope policy is invalid: " + error;
        return false;
    }

    const ScopeDecision loadedTargetDecision =
        ScopePolicyEngine::Check(
            loaded.scopePolicy,
            loaded.targetUrl);

    if (!loadedTargetDecision.allowed)
    {
        error = "Project target is outside its saved scope: " +
            loadedTargetDecision.reason;
        return false;
    }

    if (!ExtractRuns(json, loaded.runs, version >= 3, version >= 4) || loaded.runs.empty())
    {
        error = "Project file contains no valid audit runs.";
        return false;
    }

    project = loaded;
    return true;
}

bool ProjectStore::Save(
    const std::string& path,
    const Project& project,
    std::string& error)
{
    error.clear();

    if (project.id.empty() ||
        project.name.empty() ||
        project.targetUrl.empty() ||
        project.createdAtUtc.empty() ||
        project.updatedAtUtc.empty())
    {
        error = "Project metadata is incomplete.";
        return false;
    }

    if (project.defaultConfiguration.minIntervalMs > 86400000 ||
        project.defaultConfiguration.timeoutMs <= 0 ||
        project.defaultConfiguration.timeoutMs > MaximumTimeoutMs)
    {
        error = "Project default request limits are outside supported ranges.";
        return false;
    }

    for (const auto& run : project.runs)
    {
        const auto& budget = run.requestBudget;
        if (budget.attemptedRequests > budget.maxRequestsPerRun ||
            budget.remainingRequests !=
                budget.maxRequestsPerRun - budget.attemptedRequests ||
            budget.successfulRequests > budget.attemptedRequests ||
            budget.failedRequests !=
                budget.attemptedRequests - budget.successfulRequests)
        {
            error = "Project contains inconsistent request budget statistics.";
            return false;
        }
    }

    if (!ScopePolicyEngine::Validate(
        project.scopePolicy,
        error))
    {
        error = "Project scope policy is invalid: " + error;
        return false;
    }

    const ScopeDecision targetDecision =
        ScopePolicyEngine::Check(
            project.scopePolicy,
            project.targetUrl);

    if (!targetDecision.allowed)
    {
        error = "Project target is outside its scope: " +
            targetDecision.reason;
        return false;
    }

    std::ostringstream out;
    out << "{\n"
        << "  \"projectSchemaVersion\": 4,\n"
        << "  \"projectId\": " << Quote(project.id) << ",\n"
        << "  \"projectName\": " << Quote(project.name) << ",\n"
        << "  \"targetUrl\": " << Quote(project.targetUrl) << ",\n"
        << "  \"createdAtUtc\": " << Quote(project.createdAtUtc) << ",\n"
        << "  \"updatedAtUtc\": " << Quote(project.updatedAtUtc) << ",\n"
        << "  \"defaultConfiguration\": {\n"
        << "    \"maxRequests\": " << project.defaultConfiguration.maxRequests << ",\n"
        << "    \"minIntervalMs\": " << project.defaultConfiguration.minIntervalMs << ",\n"
        << "    \"timeoutMs\": " << project.defaultConfiguration.timeoutMs << ",\n"
        << "    \"useLegacyTimeoutProfile\": "
        << (project.defaultConfiguration.useLegacyTimeoutProfile ? "true" : "false") << ",\n"
        << "    \"followRedirects\": "
        << (project.defaultConfiguration.followRedirects ? "true" : "false") << ",\n"
        << "    \"outputFormat\": "
        << Quote(AuditOutputFormatToString(project.defaultConfiguration.outputFormat)) << ",\n"
        << "    \"outputPath\": " << Quote(project.defaultConfiguration.outputPath) << ",\n"
        << "    \"persistProject\": "
        << (project.defaultConfiguration.persistProject ? "true" : "false") << ",\n"
        << "    \"reportOptions\": {\n"
        << "      \"includeEvidence\": "
        << (project.defaultConfiguration.reportOptions.includeEvidence ? "true" : "false") << ",\n"
        << "      \"includeProbeHistory\": "
        << (project.defaultConfiguration.reportOptions.includeProbeHistory ? "true" : "false") << "\n"
        << "    }\n"
        << "  },\n"
        << "  \"scopePolicy\": {\n";

    const auto WriteArray = [&out](
        const char* key,
        const std::vector<std::string>& values,
        bool trailingComma)
    {
        out << "    \"" << key << "\": [";

        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                out << ", ";

            out << Quote(values[i]);
        }

        out << "]" << (trailingComma ? ",\n" : "\n");
    };

    WriteArray("allowedHosts", project.scopePolicy.allowedHosts, true);
    WriteArray("allowedDomains", project.scopePolicy.allowedDomains, true);
    WriteArray("allowedPaths", project.scopePolicy.allowedPaths, false);

    out << "  },\n"
        << "  \"runCount\": " << project.runs.size() << ",\n"
        << "  \"runs\": [\n";

    for (size_t i = 0; i < project.runs.size(); ++i)
    {
        const auto& run = project.runs[i];

        if (!IsJsonObject(run.reportJson))
        {
            error = "Project run contains invalid report JSON.";
            return false;
        }

        out << "    {\n"
            << "      \"runNumber\": " << i + 1 << ",\n"
            << "      \"savedAtUtc\": " << Quote(run.savedAtUtc) << ",\n"
            << "      \"requestBudget\": {\n"
            << "        \"maxRequestsPerRun\": " << run.requestBudget.maxRequestsPerRun << ",\n"
            << "        \"minRequestIntervalMs\": " << run.requestBudget.minRequestIntervalMs << ",\n"
            << "        \"attemptedRequests\": " << run.requestBudget.attemptedRequests << ",\n"
            << "        \"successfulRequests\": " << run.requestBudget.successfulRequests << ",\n"
            << "        \"failedRequests\": " << run.requestBudget.failedRequests << ",\n"
            << "        \"scopeRejectedRequests\": " << run.requestBudget.scopeRejectedRequests << ",\n"
            << "        \"budgetRejectedRequests\": " << run.requestBudget.budgetRejectedRequests << ",\n"
            << "        \"remainingRequests\": " << run.requestBudget.remainingRequests << "\n"
            << "      },\n"
            << "      \"report\": " << run.reportJson << "\n"
            << "    }";

        if (i + 1 < project.runs.size())
            out << ',';

        out << '\n';
    }

    out << "  ]\n"
        << "}\n";

    return WriteWholeFile(
        path,
        out.str(),
        error);
}

void ProjectStore::AddAuditRun(
    Project& project,
    const std::string& reportJson,
    const RequestBudgetStats& requestBudget)
{
    ProjectAuditRun run;
    run.savedAtUtc = UtcNow();
    run.reportJson = reportJson;
    run.requestBudget = requestBudget;

    project.runs.push_back(run);
    project.updatedAtUtc = run.savedAtUtc;
}

bool ProjectStore::WriteLatestReport(
    const Project& project,
    const std::string& path,
    std::string& error)
{
    error.clear();

    if (project.runs.empty())
    {
        error = "Project has no saved audit report.";
        return false;
    }

    return WriteWholeFile(
        path,
        project.runs.back().reportJson + "\n",
        error);
}
