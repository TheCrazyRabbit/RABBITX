#pragma once

#include <string>
#include <vector>

#include "endpoint.h"
#include "http_client.h"

class EndpointIntelligence
{
public:
    static std::vector<Endpoint> Analyze(
        const HttpResponse& response);
};