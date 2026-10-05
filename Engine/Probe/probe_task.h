#pragma once

#include <string>
#include <vector>

#include "probe.h"
#include "probe_plan.h"


struct ProbeTask
{
    ProbePlanType type;

    std::string url;

    std::string method;

    std::string reason;

    std::vector<std::string> parameters;
};


inline ProbeTask MakeProbeTask(
    const ProbePlan& plan)
{
    ProbeTask task;

    task.type =
        plan.type;

    task.url =
        plan.endpointUrl;

    task.method =
        plan.method;

    task.reason =
        plan.reason;

    task.parameters =
        plan.parameters;

    return task;
}