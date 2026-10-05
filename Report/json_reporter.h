#pragma once

#include <string>

#include "pipeline.h"

namespace JsonReporter
{
    std::string Serialize(
        const AuditResult& result);

    bool WriteToFile(
        const AuditResult& result,
        const std::string& path,
        std::string& error);
}