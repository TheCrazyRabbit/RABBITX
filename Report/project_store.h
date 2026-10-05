#pragma once

#include <string>

#include "project_context.h"

class ProjectStore
{
public:
    static Project Create(
        const std::string& targetUrl,
        const std::string& name = {});

    static bool Load(
        const std::string& path,
        Project& project,
        std::string& error);

    static bool Save(
        const std::string& path,
        const Project& project,
        std::string& error);

    static void AddAuditRun(
        Project& project,
        const std::string& reportJson,
        const RequestBudgetStats& requestBudget);

    static bool WriteLatestReport(
        const Project& project,
        const std::string& path,
        std::string& error);
};
