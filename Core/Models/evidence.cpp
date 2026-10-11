#include "evidence.h"
#include "http_client.h"
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

Evidence CaptureEvidence(const HttpResponse& response)
{
    Evidence e;
    e.method=response.method;e.requestedUrl=response.requestedUrl;e.finalUrl=response.finalUrl;e.statusCode=response.statusCode;
    for(const auto& header:response.headers)
    {
        auto name=header.first;
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
        const bool secret=name=="set-cookie"||name=="cookie"||name=="authorization"||name=="proxy-authorization"||name=="x-api-key";
        e.responseHeaders += header.first + ": " + (secret ? "[REDACTED]" : header.second) + "\n";
    }
    // Store a complete-body fingerprint without copying arbitrary target secrets into exports.
    BYTE digest[32]{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(response.body.data())),
        static_cast<ULONG>(response.body.size()),digest,sizeof(digest))>=0)
    {
        const char* hex="0123456789abcdef";
        for(BYTE b:digest){e.bodySha256+=hex[b>>4];e.bodySha256+=hex[b&15];}
    }
    e.bodyExcerpt="[Body omitted; SHA-256 fingerprint recorded]";
    std::string snapshot=response.body;
    for(const auto& h:response.headers)snapshot += '\0' + h.first + ':' + h.second;
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(snapshot.data()),
        static_cast<ULONG>(snapshot.size()),digest,sizeof(digest))>=0)
    {
        const char* hex="0123456789abcdef";
        for(BYTE b:digest){e.responseSha256+=hex[b>>4];e.responseSha256+=hex[b&15];}
    }
    e.observation="HTTP response snapshot; sensitive headers and raw body omitted.";
    if(response.statusCode==401)e.observation+=" Authentication is required; no access-control vulnerability has been established.";
    if(response.statusCode==403)e.observation+=" Access was denied; application authorization context needs manual review.";
    return e;
}

bool SameEvidenceSnapshot(const Evidence& a,const Evidence& b)
{
    return a.method==b.method && a.requestedUrl==b.requestedUrl && a.finalUrl==b.finalUrl && a.statusCode==b.statusCode
        && a.requestHeaders==b.requestHeaders && a.responseHeaders==b.responseHeaders
        && a.bodySha256==b.bodySha256 && a.responseSha256==b.responseSha256 && a.bodyExcerpt==b.bodyExcerpt;
}
