#include "audit_configuration.h"

const char* AuditOutputFormatToString(
    AuditOutputFormat format)
{
    switch (format)
    {
    case AuditOutputFormat::Json:
        return "json";
    case AuditOutputFormat::Html:
        return "html";
    case AuditOutputFormat::Console:
    default:
        return "console";
    }
}

bool AuditOutputFormatFromString(
    const std::string& value,
    AuditOutputFormat& format)
{
    if (value == "console" || value == "text")
    {
        format = AuditOutputFormat::Console;
        return true;
    }

    if (value == "json")
    {
        format = AuditOutputFormat::Json;
        return true;
    }

    if (value == "html")
    {
        format = AuditOutputFormat::Html;
        return true;
    }

    return false;
}
