#pragma once

#include <string>

struct Evidence
{
    std::string method;

    std::string requestedUrl;
    std::string finalUrl;

    int statusCode = 0;

    std::string requestHeaders;
    std::string responseHeaders;

    std::string bodyExcerpt;

    std::string observation;
};