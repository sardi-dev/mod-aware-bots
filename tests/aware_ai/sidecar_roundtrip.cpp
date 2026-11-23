/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

// Drives the real client and protocol code against a running aware-ai sidecar: pushes settings that ask for far more
// than the ceiling, reads the status back and checks the answer. Usage: sidecar_roundtrip <url> <expected mode>
// Built and run by tests/aware_ai/roundtrip.sh.

#include "AwareAiClient.h"
#include "AwareAiProtocol.h"
#include <cstdio>
#include <future>
#include <string>

namespace
{
    AwareAiResponse Call(AwareAiHttpMethod method, std::string target, std::string body)
    {
        auto promise = std::make_shared<std::promise<AwareAiResponse>>();
        std::future<AwareAiResponse> future = promise->get_future();
        if (!sAwareAiClient.Send(method, std::move(target), std::move(body), 2s,
                [promise](AwareAiResponse response) { promise->set_value(std::move(response)); }))
            return AwareAiResponse{ false, 0, {}, "not sent" };

        return future.get();
    }

    int g_failures = 0;

    void Check(bool ok, char const* what)
    {
        std::printf("%s %s\n", ok ? "ok    " : "FAILED", what);
        g_failures += ok ? 0 : 1;
    }
}

int main(int argc, char** argv)
{
    std::string const url = argc > 1 ? argv[1] : "http://127.0.0.1:8090";
    std::string const expectedMode = argc > 2 ? argv[2] : "fake";
    if (!sAwareAiClient.Start(url))
        return 2;

    AwareAiSettings settings;
    settings.Enabled = true;
    settings.MonthlyUsd = 5000.0;
    settings.DailyUsd = 4000.0;
    AwareAiResponse const push = Call(AwareAiHttpMethod::Put, "/v1/settings", BuildSettingsBody(settings));
    Check(push.Ok, "PUT /v1/settings accepted");

    AwareAiResponse const poll = Call(AwareAiHttpMethod::Get, "/v1/status", "");
    Check(poll.Ok, "GET /v1/status answered");
    auto const status = ParseStatusBody(poll.Body);
    Check(status.has_value(), "status body parsed");
    if (status)
    {
        std::printf("       mode=%s version=%s enabled=%d has_api_key=%d breaker=%s month_cap=%.2f day_cap=%.2f "
            "ceiling=%.2f clamped=%d\n", status->Mode.c_str(), status->Version.c_str(), status->Enabled,
            status->HasApiKey, status->Breaker.c_str(), status->MonthCapUsd, status->DayCapUsd, status->CeilingUsd,
            status->CapClamped);
        Check(status->Mode == expectedMode, "mode is the expected one");
        Check(status->Enabled, "sidecar reports the layer enabled");
        Check(!status->HasApiKey || expectedMode == "live", "no key in use unless live");
        Check(status->MonthCapUsd == status->CeilingUsd, "monthly cap clamped to the ceiling");
        Check(status->DayCapUsd <= status->MonthCapUsd, "daily cap not above the monthly cap");
        Check(status->CapClamped, "clamp reported");
    }

    sAwareAiClient.Stop();
    return g_failures == 0 ? 0 : 1;
}
