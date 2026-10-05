#pragma once

#include <string>
#include <vector>

#include "endpoint.h"
#include "finding.h"
#include "probe.h"


struct AuditContext
{
    std::string target;


    std::vector<Endpoint>
        endpoints;


    std::vector<ProbeResult>
        probeResults;


    std::vector<Finding>
        findings;
};