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

    std::string id; // Run-local stable reference, preserved in saved reports.
    std::string bodySha256;
    std::string responseSha256;
};

struct HttpResponse;
Evidence CaptureEvidence(const HttpResponse& response);
bool SameEvidenceSnapshot(const Evidence& left, const Evidence& right);
