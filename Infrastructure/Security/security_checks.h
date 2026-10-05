#pragma once

#include <vector>

#include "finding.h"
#include "http_client.h"

namespace SecurityChecks
{
    std::vector<Finding> CheckSecurityHeaders(
        const HttpResponse& response);

    std::vector<Finding> CheckCors(
        const HttpResponse& response);

    std::vector<Finding> CheckCookies(
        const HttpResponse& response);

    std::vector<Finding> CheckInformationDisclosure(
        const HttpResponse& response);

    std::vector<Finding> CheckRedirectSecurity(
        const HttpResponse& response);
}