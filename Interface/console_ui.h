#pragma once

#include <string>
#include <vector>

#include "endpoint.h"
#include "finding.h"
#include "http_client.h"
#include "pipeline.h"
#include "probe_plan.h"

namespace ConsoleUI
{
    void PrintBanner();

    std::string ReadTargetUrl();

    void PrintRequestStart();

    void PrintResponseSummary(
        const HttpResponse& response);

    void PrintRequestError(
        const HttpResponse& response);

    void PrintInvalidUrl();

    void PrintProbeStart(
        const Probe& probe);

    void PrintProbeResult(
        const ProbeResult& result);

    void PrintEndpoints(
        const std::vector<Endpoint>& endpoints);

    void PrintProbePlans(
        const std::vector<ProbePlan>& plans);

    void PrintFindings(
        const std::vector<Finding>& findings);

    void WaitForExit();
}