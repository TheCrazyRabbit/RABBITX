#include "config_loader.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "scope_policy.h"

namespace
{
    void AppendUtf8(
        std::string& output,
        unsigned int codePoint)
    {
        if (codePoint <= 0x7F)
            output += static_cast<char>(codePoint);
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

    int HexValue(char value)
    {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    class JsonReader
    {
    public:
        explicit JsonReader(const std::string& text)
            : text_(text)
        {
        }

        bool ReadConfiguration(
            AuditConfigurationOverrides& overrides,
            std::string& error)
        {
            SkipWhitespace();
            if (!Consume('{'))
                return Fail(error, "Configuration root must be a JSON object.");

            std::set<std::string> keys;
            SkipWhitespace();

            if (!Consume('}'))
            {
                while (true)
                {
                    std::string key;
                    if (!ReadString(key))
                        return Fail(error, "Expected a configuration property name.");
                    if (!keys.insert(key).second)
                        return Fail(error, "Duplicate configuration property: " + key);
                    SkipWhitespace();
                    if (!Consume(':'))
                        return Fail(error, "Expected ':' after configuration property " + key);
                    SkipWhitespace();

                    if (!ReadRootProperty(key, overrides, error))
                        return false;

                    SkipWhitespace();
                    if (Consume('}'))
                        break;
                    if (!Consume(','))
                        return Fail(error, "Expected ',' or '}' in configuration object.");
                    SkipWhitespace();
                }
            }

            SkipWhitespace();
            if (position_ != text_.size())
                return Fail(error, "Unexpected data after configuration object.");

            if (overrides.scope)
            {
                std::string scopeError;
                if (!ScopePolicyEngine::Validate(*overrides.scope, scopeError))
                    return Fail(error, "Invalid scope configuration: " + scopeError);
            }

            error.clear();
            return true;
        }

    private:
        const std::string& text_;
        size_t position_ = 0;

        bool Fail(std::string& error, const std::string& message) const
        {
            error = message + " (byte " + std::to_string(position_) + ")";
            return false;
        }

        void SkipWhitespace()
        {
            while (position_ < text_.size() &&
                (text_[position_] == ' ' || text_[position_] == '\t' ||
                    text_[position_] == '\r' || text_[position_] == '\n'))
            {
                ++position_;
            }
        }

        bool Consume(char expected)
        {
            if (position_ >= text_.size() || text_[position_] != expected)
                return false;
            ++position_;
            return true;
        }

        bool ReadHex4(unsigned int& value)
        {
            if (position_ + 4 > text_.size())
                return false;

            value = 0;
            for (size_t i = 0; i < 4; ++i)
            {
                const int digit = HexValue(text_[position_++]);
                if (digit < 0)
                    return false;
                value = (value << 4) | static_cast<unsigned int>(digit);
            }
            return true;
        }

        bool ReadString(std::string& value)
        {
            if (!Consume('"'))
                return false;

            value.clear();
            while (position_ < text_.size())
            {
                const unsigned char ch = static_cast<unsigned char>(text_[position_++]);
                if (ch == '"')
                    return true;
                if (ch < 0x20)
                    return false;
                if (ch != '\\')
                {
                    value += static_cast<char>(ch);
                    continue;
                }

                if (position_ >= text_.size())
                    return false;

                switch (text_[position_++])
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
                    if (!ReadHex4(codePoint))
                        return false;

                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
                    {
                        if (position_ + 2 > text_.size() ||
                            text_[position_] != '\\' ||
                            text_[position_ + 1] != 'u')
                        {
                            return false;
                        }
                        position_ += 2;
                        unsigned int low = 0;
                        if (!ReadHex4(low) || low < 0xDC00 || low > 0xDFFF)
                            return false;
                        codePoint = 0x10000 +
                            ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
                    {
                        return false;
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

        bool ReadUnsigned(size_t& value)
        {
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9')
                return false;

            const char* begin = text_.data() + position_;
            const char* end = text_.data() + text_.size();
            size_t parsed = 0;
            const auto result = std::from_chars(begin, end, parsed);
            if (result.ec != std::errc{})
                return false;

            position_ = static_cast<size_t>(result.ptr - text_.data());
            if (position_ < text_.size() &&
                (text_[position_] == '.' || text_[position_] == 'e' || text_[position_] == 'E'))
            {
                return false;
            }

            value = parsed;
            return true;
        }

        bool ReadBoolean(bool& value)
        {
            if (text_.compare(position_, 4, "true") == 0)
            {
                position_ += 4;
                value = true;
                return true;
            }
            if (text_.compare(position_, 5, "false") == 0)
            {
                position_ += 5;
                value = false;
                return true;
            }
            return false;
        }

        bool ReadStringArray(
            const std::string& property,
            std::vector<std::string>& values,
            ScopePolicy& scope)
        {
            if (!Consume('['))
                return false;
            SkipWhitespace();
            if (Consume(']'))
                return true;

            while (true)
            {
                std::string value;
                if (!ReadString(value))
                    return false;
                std::string error;
                bool added = false;
                if (property == "hosts")
                    added = ScopePolicyEngine::AddHost(scope, value, error);
                else if (property == "domains")
                    added = ScopePolicyEngine::AddDomain(scope, value, error);
                else
                    added = ScopePolicyEngine::AddPath(scope, value, error);
                if (!added)
                    return false;

                values.push_back(value);
                SkipWhitespace();
                if (Consume(']'))
                    return true;
                if (!Consume(','))
                    return false;
                SkipWhitespace();
            }
        }

        bool ReadScope(ScopePolicy& scope)
        {
            if (!Consume('{'))
                return false;
            std::set<std::string> keys;
            SkipWhitespace();
            if (Consume('}'))
                return true;

            while (true)
            {
                std::string key;
                if (!ReadString(key) || !keys.insert(key).second)
                    return false;
                SkipWhitespace();
                if (!Consume(':'))
                    return false;
                SkipWhitespace();

                std::vector<std::string> ignored;
                if (key == "hosts" || key == "domains" || key == "paths")
                {
                    if (!ReadStringArray(key, ignored, scope))
                        return false;
                }
                else if (!SkipValue())
                {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}'))
                    return true;
                if (!Consume(','))
                    return false;
                SkipWhitespace();
            }
        }

        bool ReadReportOptions(AuditConfigurationOverrides& overrides)
        {
            if (!Consume('{'))
                return false;
            std::set<std::string> keys;
            SkipWhitespace();
            if (Consume('}'))
                return true;

            while (true)
            {
                std::string key;
                if (!ReadString(key) || !keys.insert(key).second)
                    return false;
                SkipWhitespace();
                if (!Consume(':'))
                    return false;
                SkipWhitespace();

                if (key == "includeEvidence" || key == "includeProbeHistory")
                {
                    bool value = false;
                    if (!ReadBoolean(value))
                        return false;
                    if (key == "includeEvidence")
                        overrides.includeEvidence = value;
                    else
                        overrides.includeProbeHistory = value;
                }
                else if (!SkipValue())
                {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}'))
                    return true;
                if (!Consume(','))
                    return false;
                SkipWhitespace();
            }
        }

        bool SkipNumber()
        {
            const size_t start = position_;
            if (position_ < text_.size() && text_[position_] == '-')
                ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9')
                return false;
            if (text_[position_] == '0')
                ++position_;
            else
            {
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
            }
            if (position_ < text_.size() && text_[position_] == '.')
            {
                ++position_;
                const size_t fractionStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
                if (position_ == fractionStart)
                    return false;
            }
            if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E'))
            {
                ++position_;
                if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
                    ++position_;
                const size_t exponentStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
                if (position_ == exponentStart)
                    return false;
            }
            return position_ > start;
        }

        bool SkipValue()
        {
            SkipWhitespace();
            if (position_ >= text_.size())
                return false;
            if (text_[position_] == '"')
            {
                std::string ignored;
                return ReadString(ignored);
            }
            if (text_[position_] == '{')
            {
                ++position_;
                SkipWhitespace();
                if (Consume('}'))
                    return true;
                while (true)
                {
                    std::string ignored;
                    if (!ReadString(ignored)) return false;
                    SkipWhitespace();
                    if (!Consume(':')) return false;
                    if (!SkipValue()) return false;
                    SkipWhitespace();
                    if (Consume('}')) return true;
                    if (!Consume(',')) return false;
                    SkipWhitespace();
                }
            }
            if (text_[position_] == '[')
            {
                ++position_;
                SkipWhitespace();
                if (Consume(']'))
                    return true;
                while (true)
                {
                    if (!SkipValue()) return false;
                    SkipWhitespace();
                    if (Consume(']')) return true;
                    if (!Consume(',')) return false;
                    SkipWhitespace();
                }
            }
            if (text_.compare(position_, 4, "true") == 0)
            {
                position_ += 4;
                return true;
            }
            if (text_.compare(position_, 5, "false") == 0)
            {
                position_ += 5;
                return true;
            }
            if (text_.compare(position_, 4, "null") == 0)
            {
                position_ += 4;
                return true;
            }
            return SkipNumber();
        }

        bool ReadRootProperty(
            const std::string& key,
            AuditConfigurationOverrides& overrides,
            std::string& error)
        {
            if (key == "targetUrl" || key == "projectName" || key == "outputPath")
            {
                std::string value;
                if (!ReadString(value))
                    return Fail(error, "Expected a string for configuration property " + key);
                if (key == "targetUrl") overrides.targetUrl = value;
                else if (key == "projectName") overrides.projectName = value;
                else overrides.outputPath = value;
                return true;
            }

            if (key == "maxRequests" || key == "minIntervalMs" || key == "timeoutMs")
            {
                size_t value = 0;
                if (!ReadUnsigned(value))
                    return Fail(error, "Expected a non-negative integer for configuration property " + key);
                if (key == "maxRequests") overrides.maxRequests = value;
                else if (key == "minIntervalMs")
                {
                    if (value > 86400000)
                        return Fail(error, "minIntervalMs must be between 0 and 86400000.");
                    overrides.minIntervalMs = value;
                }
                else
                {
                    if (value == 0 || value > MaximumTimeoutMs)
                        return Fail(error, "timeoutMs must be between 1 and 600000.");
                    overrides.timeoutMs = static_cast<int>(value);
                }
                return true;
            }

            if (key == "followRedirects" || key == "persistProject")
            {
                bool value = false;
                if (!ReadBoolean(value))
                    return Fail(error, "Expected a boolean for configuration property " + key);
                if (key == "followRedirects") overrides.followRedirects = value;
                else overrides.persistProject = value;
                return true;
            }

            if (key == "outputFormat")
            {
                std::string value;
                AuditOutputFormat format;
                if (!ReadString(value) || !AuditOutputFormatFromString(value, format))
                    return Fail(error, "outputFormat must be 'console', 'json', or 'html'.");
                overrides.outputFormat = format;
                return true;
            }

            if (key == "scope")
            {
                ScopePolicy scope;
                if (!ReadScope(scope))
                    return Fail(error, "scope must contain string arrays named hosts, domains, or paths.");
                overrides.scope = scope;
                return true;
            }

            if (key == "reportOptions")
            {
                if (!ReadReportOptions(overrides))
                    return Fail(error, "reportOptions contains an invalid value.");
                return true;
            }

            // Unknown configuration properties are safely ignored, including nested values.
            if (!SkipValue())
                return Fail(error, "Invalid value for unknown configuration property " + key);
            return true;
        }
    };
}

namespace ConfigurationFile
{
    bool Load(
        const std::string& path,
        AuditConfigurationOverrides& overrides,
        std::string& error)
    {
        error.clear();
        std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
        if (!file.is_open())
        {
            error = "Unable to open configuration file: " + path;
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (!file.good() && !file.eof())
        {
            error = "Failed to read configuration file: " + path;
            return false;
        }

        std::string contents = buffer.str();
        if (contents.size() >= 3 &&
            static_cast<unsigned char>(contents[0]) == 0xEF &&
            static_cast<unsigned char>(contents[1]) == 0xBB &&
            static_cast<unsigned char>(contents[2]) == 0xBF)
        {
            contents.erase(0, 3);
        }

        JsonReader reader(contents);
        return reader.ReadConfiguration(overrides, error);
    }

    void Apply(
        const AuditConfigurationOverrides& overrides,
        AuditConfiguration& configuration)
    {
        if (overrides.targetUrl) configuration.targetUrl = *overrides.targetUrl;
        if (overrides.projectName) configuration.projectName = *overrides.projectName;
        if (overrides.maxRequests) configuration.maxRequests = *overrides.maxRequests;
        if (overrides.minIntervalMs) configuration.minIntervalMs = *overrides.minIntervalMs;
        if (overrides.timeoutMs)
        {
            configuration.timeoutMs = *overrides.timeoutMs;
            configuration.useLegacyTimeoutProfile = false;
        }
        if (overrides.followRedirects) configuration.followRedirects = *overrides.followRedirects;
        if (overrides.scope) configuration.scope = *overrides.scope;
        if (overrides.outputFormat) configuration.outputFormat = *overrides.outputFormat;
        if (overrides.outputPath) configuration.outputPath = *overrides.outputPath;
        if (overrides.persistProject) configuration.persistProject = *overrides.persistProject;
        if (overrides.includeEvidence)
            configuration.reportOptions.includeEvidence = *overrides.includeEvidence;
        if (overrides.includeProbeHistory)
            configuration.reportOptions.includeProbeHistory = *overrides.includeProbeHistory;
    }
}
