#pragma once

#include <string>

#include "probe.h"
#include "http_client.h"
#include "evidence.h"


struct ProbeResult
{
    Probe probe;

    HttpResponse response;

    Evidence evidence;

    bool executed = false;

    std::string error;
};