#pragma once

#include <cstddef>
#include <string>

namespace HTMLReporter
{
    std::string Serialize(
        const std::string& projectName,
        size_t runNumber,
        const std::string& savedAtUtc,
        const std::string& reportJson);

    bool WriteToFile(
        const std::string& projectName,
        size_t runNumber,
        const std::string& savedAtUtc,
        const std::string& reportJson,
        const std::string& path,
        std::string& error);
}
