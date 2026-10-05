#include "cli.h"

#include <cstdlib>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "console_ui.h"
#include "html_reporter.h"
#include "json_reporter.h"
#include "project_store.h"
#include "pipeline.h"
#include "scope_policy.h"
#include "audit_configuration.h"
#include "config_loader.h"

namespace
{
    struct CliOptions
    {
        bool json = false;
        bool html = false;
        bool text = false;
        bool replay = false;
        bool resume = false;
        bool help = false;
        bool maxRequestsSpecified = false;
        size_t maxRequests = DefaultMaxRequestsPerRun;
        bool minIntervalSpecified = false;
        size_t minRequestIntervalMs = DefaultMinRequestIntervalMs;
        bool timeoutSpecified = false;
        int timeoutMs = DefaultTimeoutMs;
        std::optional<bool> followRedirects;
        std::optional<bool> includeEvidence;
        std::optional<bool> includeProbeHistory;
        std::optional<bool> persistProject;

        std::string outputPath;
        bool outputPathSpecified = false;
        std::string configPath;
        std::string projectPath;
        std::string newProjectPath;
        std::string projectName;
        std::string url;
        std::vector<std::string> scopeHosts;
        std::vector<std::string> scopeDomains;
        std::vector<std::string> scopePaths;
    };

    std::filesystem::path NormalizePath(
        const std::string& value)
    {
        std::error_code error;
        auto path = std::filesystem::absolute(
            std::filesystem::u8path(value),
            error);

        if (error)
            path = std::filesystem::u8path(value);

        error.clear();
        auto canonical = std::filesystem::weakly_canonical(
            path,
            error);

        return (error ? path : canonical).lexically_normal();
    }

    bool PathsReferToSameFile(
        const std::string& left,
        const std::string& right)
    {
        if (left.empty() || right.empty())
            return false;

        const auto leftPath = NormalizePath(left);
        const auto rightPath = NormalizePath(right);

        std::error_code error;

        if (std::filesystem::exists(leftPath, error) && !error)
        {
            error.clear();

            if (std::filesystem::exists(rightPath, error) && !error)
            {
                error.clear();

                if (std::filesystem::equivalent(
                    leftPath,
                    rightPath,
                    error) && !error)
                {
                    return true;
                }
            }
        }

        const std::wstring leftNative = leftPath.native();
        const std::wstring rightNative = rightPath.native();

        return CompareStringOrdinal(
            leftNative.c_str(),
            static_cast<int>(leftNative.size()),
            rightNative.c_str(),
            static_cast<int>(rightNative.size()),
            TRUE) == CSTR_EQUAL;
    }

    bool HasHostScopeOverrides(
        const CliOptions& options)
    {
        return !options.scopeHosts.empty() ||
            !options.scopeDomains.empty();
    }

    bool HasScopeOverrides(
        const CliOptions& options)
    {
        return HasHostScopeOverrides(options) ||
            !options.scopePaths.empty();
    }

    bool ApplyScopeOptions(
        const std::string& targetUrl,
        const CliOptions& options,
        ScopePolicy& policy,
        bool startFromExisting,
        std::string& error)
    {
        error.clear();

        if (!startFromExisting &&
            !ScopePolicyEngine::CreateDefault(
                targetUrl,
                policy,
                error))
        {
            return false;
        }

        if (HasHostScopeOverrides(options))
        {
            policy.allowedHosts.clear();
            policy.allowedDomains.clear();

            for (const auto& host : options.scopeHosts)
            {
                if (!ScopePolicyEngine::AddHost(
                    policy,
                    host,
                    error))
                {
                    return false;
                }
            }

            for (const auto& domain : options.scopeDomains)
            {
                if (!ScopePolicyEngine::AddDomain(
                    policy,
                    domain,
                    error))
                {
                    return false;
                }
            }
        }

        if (!options.scopePaths.empty())
        {
            policy.allowedPaths.clear();

            for (const auto& path : options.scopePaths)
            {
                if (!ScopePolicyEngine::AddPath(
                    policy,
                    path,
                    error))
                {
                    return false;
                }
            }
        }

        if (!ScopePolicyEngine::Validate(policy, error))
            return false;

        const ScopeDecision targetDecision =
            ScopePolicyEngine::Check(policy, targetUrl);

        if (!targetDecision.allowed)
        {
            error = "Audit target is outside the configured scope: " +
                targetDecision.reason;
            return false;
        }

        return true;
    }

    void PrintHelp()
    {
        std::cout << R"(
RABBITX - API Security Intelligence

Usage:

  RABBITX.exe
      Start interactive mode.

  RABBITX.exe <URL>
      Audit one URL and exit.

  RABBITX.exe --url <URL>
      Audit one URL and exit.

  RABBITX.exe --json <URL>
      Audit one URL and output JSON.

  RABBITX.exe --json --output <FILE> <URL>
      Audit one URL and save JSON.

  RABBITX.exe --html --output <FILE> <URL>
      Audit one URL and save a standalone HTML report.

  RABBITX.exe <URL> [--scope-host <HOST[:PORT]>]
      Restrict requests to explicitly allowed hosts. Repeatable.

  RABBITX.exe <URL> [--scope-domain <DOMAIN>]
      Allow a domain and its subdomains. Repeatable.

  RABBITX.exe <URL> [--scope-path <PREFIX>]
      Restrict requests to path prefixes. Repeatable.

  RABBITX.exe <URL> [--max-requests <COUNT>]
      Limit actual HTTP requests in this audit run (default: 50).

  RABBITX.exe <URL> [--min-interval-ms <MILLISECONDS>]
      Set the minimum gap between actual requests (default: 200 ms).

  RABBITX.exe <URL> --config <FILE>
      Load JSON defaults; explicit CLI options override that file.
      Unknown JSON properties are ignored.

  Config keys: targetUrl, projectName, maxRequests, minIntervalMs, timeoutMs,
      followRedirects, scope.hosts/domains/paths, outputFormat, outputPath,
      reportOptions.includeEvidence/includeProbeHistory, persistProject.

  Runtime options include --timeout-ms <MILLISECONDS>, --follow-redirects,
  --no-follow-redirects, --include-evidence, --exclude-evidence,
  --include-probe-history, --exclude-probe-history, --save-project, and
  --no-save-project.

  RABBITX.exe --new-project <FILE> <URL> [--name <NAME>]
      Audit a URL and save a new project with its report.

  RABBITX.exe --project <FILE>
      Open the project and output its latest saved report (offline).

  RABBITX.exe --project <FILE> --resume
      Re-run the saved target and append an audit snapshot.
      Config and CLI overrides apply to this run only; saved project defaults
      stay unchanged. Each resume starts fresh run limits.

  RABBITX.exe --project <FILE> --replay
      Output the latest saved report without sending requests.

  RABBITX.exe --project <FILE> --replay --json --output <FILE>
      Export the latest saved report as JSON.

  RABBITX.exe --project <FILE> --html --output <FILE>
      Export the latest saved report as standalone HTML.

  Project files retain the exact target URL for reproducible audits.
  New projects save their scope rules; legacy projects default to the target origin.
  Without scope flags, only the target host and port are allowed.
  Protect them if the URL contains credentials.

  RABBITX.exe --help
      Show this help.

Interactive commands:

  <URL>       Audit the specified URL.
  help        Show commands.
  clear       Clear the console.
  exit        Exit RABBITX.
  quit        Exit RABBITX.

Examples:

  RABBITX.exe https://example.com

  RABBITX.exe --url https://example.com

  RABBITX.exe --json https://example.com

  RABBITX.exe --json --output result.json https://example.com

  RABBITX.exe --html --output report.html https://example.com

  RABBITX.exe --new-project project.json https://example.com

  RABBITX.exe --project project.json

  RABBITX.exe --project project.json --resume

  RABBITX.exe https://example.com --max-requests 50

  RABBITX.exe https://example.com --min-interval-ms 250

  RABBITX.exe https://example.com --config rabbitx.json --max-requests 20
)" << '\n';
    }

    void ClearScreen()
    {
        std::system("cls");
    }

    void PrintInteractiveHelp()
    {
        std::cout << R"(
Commands:

  <URL>   Run an audit.
  help    Show this help.
  clear   Clear the screen.
  exit    Exit.
  quit    Exit.
)" << '\n';
    }

    int RunTarget(
        const AuditConfiguration& configuration,
        Project* project = nullptr,
        const std::string& projectPath = {})
    {
        if (configuration.targetUrl.empty())
        {
            if (configuration.outputFormat == AuditOutputFormat::Console)
                ConsoleUI::PrintInvalidUrl();

            return 1;
        }

        const AuditResult result =
            Pipeline::Run(configuration);

        const std::string jsonText =
            JsonReporter::Serialize(result);

        if (project && configuration.persistProject)
        {
            ProjectStore::AddAuditRun(
                *project,
                jsonText,
                result.requestBudget);

            std::string error;

            if (!ProjectStore::Save(
                projectPath,
                *project,
                error))
            {
                std::cerr
                    << "[-] Unable to save project: "
                    << error
                    << '\n';

                return 1;
            }
        }

        bool success = false;

        for (const auto& probe :
            result.probes)
        {
            if (probe.executed)
            {
                success = true;
                break;
            }
        }

        const bool completed = success ||
            result.requestBudget.budgetRejectedRequests > 0;

        const size_t runNumber = project
            ? project->runs.size() + (configuration.persistProject ? 0 : 1)
            : 1;
        const std::string reportTime = project && configuration.persistProject
            ? project->updatedAtUtc
            : "Generated during this audit";

        if (configuration.outputFormat == AuditOutputFormat::Html)
        {
            std::string error;

            if (!HTMLReporter::WriteToFile(
                project ? project->name : "Standalone audit",
                runNumber,
                reportTime,
                jsonText,
                configuration.outputPath,
                error))
            {
                std::cerr
                    << "[-] "
                    << error
                    << '\n';

                return 1;
            }

            std::cout
                << "[+] HTML report written to: "
                << configuration.outputPath
                << '\n';

            return completed ? 0 : 1;
        }

        if (configuration.outputFormat == AuditOutputFormat::Json)
        {
            if (configuration.outputPath.empty())
            {
                std::cout
                    << jsonText;
            }
            else
            {
                std::string error;

                if (!JsonReporter::WriteToFile(
                    result,
                    configuration.outputPath,
                    error))
                {
                    std::cerr
                        << "[-] "
                        << error
                        << '\n';

                    return 1;
                }

                std::cout
                    << "[+] JSON written to: "
                    << configuration.outputPath
                    << '\n';
            }

            return completed ? 0 : 1;
        }

        ConsoleUI::PrintRequestStart();

        if (configuration.reportOptions.includeProbeHistory)
        {
            for (const auto& probe : result.probes)
            {
                ConsoleUI::PrintProbeStart(probe.probe);

                ConsoleUI::PrintProbeResult(probe);
            }
        }

        ConsoleUI::PrintFindings(
            result.findings);

        ConsoleUI::PrintEndpoints(
            result.endpoints);

        ConsoleUI::PrintProbePlans(
            result.probePlans);

        const auto PrintScopeValues = [](
            const char* label,
            const std::vector<std::string>& values,
            const char* emptyValue)
        {
            std::cout << "[+] " << label << ": ";
            if (values.empty())
            {
                std::cout << emptyValue;
            }
            else
            {
                for (size_t i = 0; i < values.size(); ++i)
                {
                    if (i > 0)
                        std::cout << ", ";
                    std::cout << values[i];
                }
            }
            std::cout << '\n';
        };

        std::cout << "\n[ Scope Policy ]\n";
        PrintScopeValues("Allowed hosts", result.scopePolicy.allowedHosts, "<none>");
        PrintScopeValues("Allowed domains", result.scopePolicy.allowedDomains, "<none>");
        PrintScopeValues("Allowed paths", result.scopePolicy.allowedPaths, "<all>");

        std::cout
            << "\n[ Effective Configuration ]\n"
            << "[+] Target: " << configuration.targetUrl << '\n'
            << "[+] Request limit: " << configuration.maxRequests << '\n'
            << "[+] Minimum interval: " << configuration.minIntervalMs << " ms\n"
            << "[+] Timeout: " << configuration.timeoutMs << " ms"
            << (configuration.useLegacyTimeoutProfile ? " (legacy phase profile)\n" : "\n")
            << "[+] Follow redirects: " << (configuration.followRedirects ? "yes" : "no") << '\n'
            << "[+] Output format: " << AuditOutputFormatToString(configuration.outputFormat) << '\n'
            << "[+] Output path: " << (configuration.outputPath.empty() ? "<stdout>" : configuration.outputPath) << '\n'
            << "[+] Persist project: " << (configuration.persistProject ? "yes" : "no") << '\n'
            << "[+] Evidence in report: " << (configuration.reportOptions.includeEvidence ? "yes" : "no") << '\n'
            << "[+] Probe history in report: " << (configuration.reportOptions.includeProbeHistory ? "yes" : "no") << '\n';

        const auto& budget = result.requestBudget;
        std::cout
            << "\n[ Request Budget ]\n"
            << "[+] Limit: " << budget.maxRequestsPerRun << '\n'
            << "[+] Minimum interval: " << budget.minRequestIntervalMs << " ms\n"
            << "[+] Attempted: " << budget.attemptedRequests << '\n'
            << "[+] Successful: " << budget.successfulRequests << '\n'
            << "[+] Failed: " << budget.failedRequests << '\n'
            << "[+] Scope rejected: " << budget.scopeRejectedRequests << '\n'
            << "[+] Budget rejected: " << budget.budgetRejectedRequests << '\n'
            << "[+] Remaining: " << budget.remainingRequests << '\n';

        if (budget.budgetRejectedRequests > 0)
        {
            std::cout
                << "[!] Request budget exhausted; remaining in-scope requests were not sent.\n";
        }

        if (project && configuration.persistProject)
        {
            std::cout
                << "[+] Project saved: "
                << projectPath
                << " (audit run "
                << project->runs.size()
                << ")\n";
        }
        else if (project)
        {
            std::cout << "[i] Project defaults and history were not changed.\n";
        }

        return completed ? 0 : 1;
    }

    bool ParseOptions(
        int argc,
        char* argv[],
        CliOptions& options)
    {
        for (int i = 1;
            i < argc;
            ++i)
        {
            const std::string arg =
                argv[i];

            if (arg == "--json")
            {
                options.json = true;
                continue;
            }

            if (arg == "--html")
            {
                options.html = true;
                continue;
            }

            if (arg == "--text")
            {
                options.text = true;
                continue;
            }

            if (arg == "--config")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[-] Missing file path after --config.\n";
                    return false;
                }
                options.configPath = argv[++i];
                continue;
            }

            if (arg == "--scope-host" ||
                arg == "--scope-domain" ||
                arg == "--scope-path")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing scope value after "
                        << arg
                        << ".\n";

                    return false;
                }

                const std::string value = argv[++i];

                if (arg == "--scope-host")
                    options.scopeHosts.push_back(value);
                else if (arg == "--scope-domain")
                    options.scopeDomains.push_back(value);
                else
                    options.scopePaths.push_back(value);

                continue;
            }

            if (arg == "--max-requests")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[-] Missing request count after --max-requests.\n";
                    return false;
                }

                const std::string value = argv[++i];
                size_t parsed = 0;
                const char* begin = value.data();
                const char* end = begin + value.size();
                const auto conversion = std::from_chars(begin, end, parsed);

                if (value.empty() || conversion.ec != std::errc{} ||
                    conversion.ptr != end)
                {
                    std::cerr << "[-] --max-requests requires a non-negative integer.\n";
                    return false;
                }

                options.maxRequests = parsed;
                options.maxRequestsSpecified = true;
                continue;
            }

            if (arg == "--min-interval-ms")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[-] Missing interval after --min-interval-ms.\n";
                    return false;
                }

                const std::string value = argv[++i];
                size_t parsed = 0;
                const char* begin = value.data();
                const char* end = begin + value.size();
                const auto conversion = std::from_chars(begin, end, parsed);

                if (value.empty() || conversion.ec != std::errc{} ||
                    conversion.ptr != end || parsed > 86400000)
                {
                    std::cerr << "[-] --min-interval-ms requires an integer from 0 to 86400000.\n";
                    return false;
                }

                options.minRequestIntervalMs = parsed;
                options.minIntervalSpecified = true;
                continue;
            }

            if (arg == "--timeout-ms")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "[-] Missing timeout after --timeout-ms.\n";
                    return false;
                }

                const std::string value = argv[++i];
                size_t parsed = 0;
                const auto conversion = std::from_chars(
                    value.data(),
                    value.data() + value.size(),
                    parsed);
                if (value.empty() || conversion.ec != std::errc{} ||
                    conversion.ptr != value.data() + value.size() ||
                    parsed == 0 || parsed > MaximumTimeoutMs)
                {
                    std::cerr << "[-] --timeout-ms requires an integer from 1 to "
                        << MaximumTimeoutMs << ".\n";
                    return false;
                }
                options.timeoutMs = static_cast<int>(parsed);
                options.timeoutSpecified = true;
                continue;
            }

            if (arg == "--follow-redirects" || arg == "--no-follow-redirects")
            {
                options.followRedirects = arg == "--follow-redirects";
                continue;
            }

            if (arg == "--include-evidence" || arg == "--exclude-evidence")
            {
                options.includeEvidence = arg == "--include-evidence";
                continue;
            }

            if (arg == "--include-probe-history" || arg == "--exclude-probe-history")
            {
                options.includeProbeHistory = arg == "--include-probe-history";
                continue;
            }

            if (arg == "--save-project" || arg == "--no-save-project")
            {
                options.persistProject = arg == "--save-project";
                continue;
            }

            if (arg == "--output")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing file path after --output.\n";

                    return false;
                }

                options.outputPath =
                    argv[++i];
                options.outputPathSpecified = true;

                continue;
            }

            if (arg == "--project")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing file path after --project.\n";

                    return false;
                }

                options.projectPath =
                    argv[++i];

                continue;
            }

            if (arg == "--new-project")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing file path after --new-project.\n";

                    return false;
                }

                options.newProjectPath =
                    argv[++i];

                continue;
            }

            if (arg == "--name")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing project name after --name.\n";

                    return false;
                }

                options.projectName =
                    argv[++i];

                continue;
            }

            if (arg == "--replay")
            {
                options.replay = true;
                continue;
            }

            if (arg == "--resume")
            {
                options.resume = true;
                continue;
            }

            if (arg == "--url")
            {
                if (i + 1 >= argc)
                {
                    std::cerr
                        << "[-] Missing URL after --url.\n";

                    return false;
                }

                options.url =
                    argv[++i];

                continue;
            }

            if (arg == "--help" ||
                arg == "-h")
            {
                PrintHelp();
                options.help = true;
                return true;
            }

            if (!options.url.empty())
            {
                std::cerr
                    << "[-] Unexpected argument: "
                    << arg
                    << '\n';

                return false;
            }

            options.url = arg;
        }

        return true;
    }

    bool HasRunOverrides(const CliOptions& options)
    {
        return options.maxRequestsSpecified ||
            options.minIntervalSpecified ||
            options.timeoutSpecified ||
            options.followRedirects.has_value() ||
            options.includeEvidence.has_value() ||
            options.includeProbeHistory.has_value() ||
            options.persistProject.has_value() ||
            HasScopeOverrides(options);
    }

    bool ApplyCliOverrides(
        const CliOptions& options,
        AuditConfiguration& configuration,
        std::string& error)
    {
        if (!options.url.empty())
            configuration.targetUrl = options.url;
        if (!options.projectName.empty())
            configuration.projectName = options.projectName;
        if (options.maxRequestsSpecified)
            configuration.maxRequests = options.maxRequests;
        if (options.minIntervalSpecified)
            configuration.minIntervalMs = options.minRequestIntervalMs;
        if (options.timeoutSpecified)
        {
            configuration.timeoutMs = options.timeoutMs;
            configuration.useLegacyTimeoutProfile = false;
        }
        if (options.followRedirects)
            configuration.followRedirects = *options.followRedirects;
        if (options.includeEvidence)
            configuration.reportOptions.includeEvidence = *options.includeEvidence;
        if (options.includeProbeHistory)
            configuration.reportOptions.includeProbeHistory = *options.includeProbeHistory;
        if (options.persistProject)
            configuration.persistProject = *options.persistProject;
        if (options.json)
            configuration.outputFormat = AuditOutputFormat::Json;
        else if (options.html)
            configuration.outputFormat = AuditOutputFormat::Html;
        else if (options.text)
            configuration.outputFormat = AuditOutputFormat::Console;
        if (options.outputPathSpecified)
            configuration.outputPath = options.outputPath;

        return ApplyScopeOptions(
            configuration.targetUrl,
            options,
            configuration.scope,
            true,
            error);
    }

    bool ValidateOutputConfiguration(
        const AuditConfiguration& configuration,
        std::string& error)
    {
        error.clear();
        if (configuration.outputFormat == AuditOutputFormat::Html &&
            configuration.outputPath.empty())
        {
            error = "HTML output requires an output path.";
            return false;
        }
        if (configuration.outputFormat == AuditOutputFormat::Console &&
            !configuration.outputPath.empty())
        {
            error = "An output path requires JSON or HTML output.";
            return false;
        }
        return true;
    }

    bool LoadConfigurationFile(
        const CliOptions& options,
        AuditConfigurationOverrides& overrides,
        std::string& error)
    {
        if (options.configPath.empty())
        {
            overrides = {};
            error.clear();
            return true;
        }
        return ConfigurationFile::Load(options.configPath, overrides, error);
    }

    int RunInteractive()
    {
        ConsoleUI::PrintBanner();

        std::cout
            << "Interactive mode\n"
            << "Type 'help' for commands.\n\n";

        while (true)
        {
            std::cout
                << "RABBITX> ";

            std::string input;

            if (!std::getline(
                std::cin,
                input))
            {
                break;
            }

            if (input.empty())
                continue;

            if (input == "exit" ||
                input == "quit")
            {
                break;
            }

            if (input == "help")
            {
                PrintInteractiveHelp();
                continue;
            }

            if (input == "clear")
            {
                ClearScreen();
                continue;
            }

            AuditConfiguration configuration;
            std::string scopeError;

            if (!ScopePolicyEngine::CreateDefault(
                input,
                configuration.scope,
                scopeError))
            {
                std::cerr
                    << "[-] "
                    << scopeError
                    << '\n';

                continue;
            }

            configuration.targetUrl = input;
            RunTarget(configuration);

            std::cout << '\n';
        }

        return 0;
    }
}



int CLI::Run(
    int argc,
    char* argv[])
{
    if (argc == 1)
        return RunInteractive();

    CliOptions options;
    if (!ParseOptions(argc, argv, options))
        return 1;
    if (options.help)
        return 0;

    if ((options.json && options.html) ||
        (options.text && (options.json || options.html)))
    {
        std::cerr << "[-] Choose one output format: --text, --json, or --html.\n";
        return 1;
    }

    std::string error;

    if (!options.projectPath.empty())
    {
        if (!options.newProjectPath.empty() ||
            !options.projectName.empty() ||
            (options.url.size() > 0 && !options.resume) ||
            (options.resume && options.replay))
        {
            std::cerr << "[-] Invalid combination of project, URL, replay, and creation options.\n";
            return 1;
        }

        Project project;
        if (!ProjectStore::Load(options.projectPath, project, error))
        {
            std::cerr << "[-] Unable to load project: " << error << '\n';
            return 1;
        }

        if (!options.resume)
        {
            if (!options.configPath.empty() || HasRunOverrides(options) || options.text)
            {
                std::cerr << "[-] Audit configuration options require --resume; project replay is offline.\n";
                return 1;
            }
            if (options.html && options.outputPath.empty())
            {
                std::cerr << "[-] --html requires --output <FILE>.\n";
                return 1;
            }
            if (options.outputPathSpecified && !options.json && !options.html)
            {
                std::cerr << "[-] --output requires --json or --html.\n";
                return 1;
            }
            if (PathsReferToSameFile(options.outputPath, options.projectPath))
            {
                std::cerr << "[-] Report output path must differ from the project file.\n";
                return 1;
            }
            if (project.runs.empty())
            {
                std::cerr << "[-] Project has no saved audit report.\n";
                return 1;
            }

            if (options.outputPath.empty())
            {
                std::cout << project.runs.back().reportJson << '\n';
                return 0;
            }

            const auto& latestRun = project.runs.back();
            const bool exported = options.html
                ? HTMLReporter::WriteToFile(
                    project.name,
                    project.runs.size(),
                    latestRun.savedAtUtc,
                    latestRun.reportJson,
                    options.outputPath,
                    error)
                : ProjectStore::WriteLatestReport(project, options.outputPath, error);

            if (!exported)
            {
                std::cerr << "[-] Unable to export saved report: " << error << '\n';
                return 1;
            }

            std::cout << "[+] Saved report exported: " << options.outputPath << '\n';
            return 0;
        }

        AuditConfiguration configuration = project.defaultConfiguration;
        configuration.targetUrl = project.targetUrl;
        configuration.projectName = project.name;
        configuration.scope = project.scopePolicy;

        AuditConfigurationOverrides fileOverrides;
        if (!LoadConfigurationFile(options, fileOverrides, error))
        {
            std::cerr << "[-] Invalid configuration: " << error << '\n';
            return 1;
        }
        ConfigurationFile::Apply(fileOverrides, configuration);

        if (!ApplyCliOverrides(options, configuration, error))
        {
            std::cerr << "[-] " << error << '\n';
            return 1;
        }
        if (!ValidateOutputConfiguration(configuration, error))
        {
            std::cerr << "[-] " << error << '\n';
            return 1;
        }
        if (PathsReferToSameFile(configuration.outputPath, options.projectPath))
        {
            std::cerr << "[-] Report output path must differ from the project file.\n";
            return 1;
        }

        return RunTarget(configuration, &project, options.projectPath);
    }

    if (options.replay || options.resume)
    {
        std::cerr << "[-] --replay and --resume require --project.\n";
        return 1;
    }

    AuditConfigurationOverrides fileOverrides;
    if (!LoadConfigurationFile(options, fileOverrides, error))
    {
        std::cerr << "[-] Invalid configuration: " << error << '\n';
        return 1;
    }

    AuditConfiguration configuration;
    ConfigurationFile::Apply(fileOverrides, configuration);

    // Establish a safe target-origin scope unless the configuration file supplied one.
    if (!fileOverrides.scope)
    {
        if (options.url.empty() && configuration.targetUrl.empty())
        {
            PrintHelp();
            return 1;
        }
        if (!options.url.empty())
            configuration.targetUrl = options.url;

        if (!ScopePolicyEngine::CreateDefault(
            configuration.targetUrl,
            configuration.scope,
            error))
        {
            std::cerr << "[-] " << error << '\n';
            return 1;
        }
    }

    if (!ApplyCliOverrides(options, configuration, error))
    {
        std::cerr << "[-] " << error << '\n';
        return 1;
    }
    if (configuration.targetUrl.empty())
    {
        std::cerr << "[-] A target URL is required in the command line or config file.\n";
        return 1;
    }
    if (!ValidateOutputConfiguration(configuration, error))
    {
        std::cerr << "[-] " << error << '\n';
        return 1;
    }

    if (!options.newProjectPath.empty())
    {
        if (options.projectName.empty() && !fileOverrides.projectName)
            configuration.projectName = {};
        if (!configuration.persistProject)
        {
            std::cerr << "[-] New projects must be persisted; remove persistProject:false or override with --save-project.\n";
            return 1;
        }
        if (PathsReferToSameFile(configuration.outputPath, options.newProjectPath))
        {
            std::cerr << "[-] Report output path must differ from the project file.\n";
            return 1;
        }

        std::error_code pathError;
        if (std::filesystem::exists(
            std::filesystem::u8path(options.newProjectPath),
            pathError))
        {
            std::cerr << "[-] Project file already exists. Use --project to add an audit run.\n";
            return 1;
        }
        if (pathError)
        {
            std::cerr << "[-] Unable to check project path: " << pathError.message() << '\n';
            return 1;
        }

        Project project = ProjectStore::Create(
            configuration.targetUrl,
            configuration.projectName);
        project.scopePolicy = configuration.scope;
        project.defaultConfiguration = configuration;
        configuration.projectName = project.name;
        return RunTarget(configuration, &project, options.newProjectPath);
    }

    if (!options.projectName.empty())
    {
        std::cerr << "[-] --name requires --new-project.\n";
        return 1;
    }

    return RunTarget(configuration);
}
