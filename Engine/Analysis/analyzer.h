#pragma once

#include <vector>

#include "finding.h"
#include "http_client.h"
#include "probe.h"
#include "probe_result.h"

class Analyzer
{
public:
    static std::vector<Finding> Analyze(
        const HttpResponse& response);

    static std::vector<Finding> AnalyzeProbe(
        const ProbeResult& result);
};