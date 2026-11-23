/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpConfig.h for what this is.
 */

#include "PvpConfig.h"

#include "Config.h"

#include <algorithm>

namespace PvpConfig
{
bool LeaderEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.Leader.Enable", true);
    return enabled;
}

bool CalloutReaderEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.CalloutReader.Enable", true);
    return enabled;
}

PvpObs::Limits ReaderLimits()
{
    static PvpObs::Limits const limits = []
    {
        PvpObs::Limits l;
        // Bounds keep a bad setting from turning the store off or making it unbounded.
        int32 const ttlSeconds = std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.Pvp.CalloutReader.TtlSeconds", 15));
        int32 const maxPerSide = std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.Pvp.CalloutReader.MaxPerSide", 32));
        int32 const senderGap = std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.Pvp.CalloutReader.SenderGapMs", 2000));
        l.ttlMs = std::min<int32>(std::max<int32>(ttlSeconds, 3), 120) * 1000;
        l.maxPerSide = std::min<int32>(std::max<int32>(maxPerSide, 4), 256);
        l.senderGapMs = std::min<int32>(senderGap, 30000);
        return l;
    }();
    return limits;
}

bool AwarenessEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.Awareness.Enable", true);
    return enabled;
}

bool InterruptRulesEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.Interrupts.Enable", true);
    return enabled;
}

bool DefensiveRulesEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.Defensives.Enable", true);
    return enabled;
}

bool FakeCastEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.FakeCast.Enable", true);
    return enabled;
}

std::uint32_t FakeCastChancePercent()
{
    static std::uint32_t const percent = static_cast<std::uint32_t>(
        std::min<int32>(std::max<int32>(sConfigMgr->GetIntDefault("AiPlayerbot.Pvp.FakeCast.ChancePercent", 65), 0), 100));
    return percent;
}

bool BurstRulesEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.Bursts.Enable", true);
    return enabled;
}

bool TeamPlayEnabled()
{
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Pvp.TeamPlay.Enable", true);
    return enabled;
}
}  // namespace PvpConfig
