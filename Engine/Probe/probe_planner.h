#pragma once

#include <vector>

#include "endpoint.h"
#include "probe_plan.h"

class ProbePlanner
{
public:
    static std::vector<ProbePlan> BuildPlans(
        const std::vector<Endpoint>& endpoints);
};