/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotSkill.h"

#include "ArenaLadder.h"
#include "Config.h"
#include "Player.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"

#include <algorithm>

namespace
{
    struct Settings
    {
        bool enabled;
        BotSkillMath::Shares shares;
        uint32 minMs[3];
        uint32 maxMs[3];
    };

    uint32 Value(char const* key, int32 fallback)
    {
        return uint32(std::max(0, sConfigMgr->GetIntDefault(key, fallback)));
    }

    // Read once on first use, like the other local settings (MarkTriggers.cpp).
    Settings const& Cfg()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.SkillTiers.Enable", true);
            s.shares.expertPercent = std::min(100u, Value("AiPlayerbot.SkillTiers.ExpertPercent", 10));
            s.shares.goodPercent = std::min(100u - s.shares.expertPercent, Value("AiPlayerbot.SkillTiers.GoodPercent", 60));
            s.minMs[0] = Value("AiPlayerbot.SkillTiers.ExpertDelayMinMs", 0);
            s.maxMs[0] = std::max(s.minMs[0], Value("AiPlayerbot.SkillTiers.ExpertDelayMaxMs", 0));
            s.minMs[1] = Value("AiPlayerbot.SkillTiers.GoodDelayMinMs", 150);
            s.maxMs[1] = std::max(s.minMs[1], Value("AiPlayerbot.SkillTiers.GoodDelayMaxMs", 400));
            s.minMs[2] = Value("AiPlayerbot.SkillTiers.CasualDelayMinMs", 400);
            s.maxMs[2] = std::max(s.minMs[2], Value("AiPlayerbot.SkillTiers.CasualDelayMaxMs", 900));
            return s;
        }();
        return settings;
    }

    // BT-153 (owner, 2026-11-12): battlegrounds play the tier mix too (10 / 60 / 30), because the battleground playbooks
    // mark their rules core, good or expert. Arenas stay as they were: the arena work decides later. InArena() is the
    // test, not InBattleground(): an arena match is a battleground to the core.
    bool HasTier(Player* bot)
    {
        return Cfg().enabled && bot && !bot->InArena() && sRandomPlayerbotMgr->IsRandomBot(bot);
    }

    // BT-154 (owner, 2026-11-12): inside a ladder arena match a bot plays at its team's tier, the weakest member's.
    // Battlegrounds and unranked arena play keep the rules above.
    bool ArenaTier(Player* bot, BotSkillMath::Tier& tier)
    {
        return Cfg().enabled && bot && bot->InArena() && ArenaLadder::TeamTierOf(bot->GetGUIDLow(), tier);
    }
}  // namespace

namespace BotSkill
{
    BotSkillMath::Tier TierOf(Player* bot)
    {
        BotSkillMath::Tier arenaTier;
        if (ArenaTier(bot, arenaTier))
            return arenaTier;

        if (!HasTier(bot))
            return BotSkillMath::Tier::EXPERT;

        return BotSkillMath::TierOf(bot->GetGUIDLow(), Cfg().shares);
    }

    bool Uses(Player* bot, BotSkillMath::Tier line)
    {
        return BotSkillMath::Uses(TierOf(bot), line);
    }

    uint32 ExtraReactionMs(Player* bot)
    {
        BotSkillMath::Tier arenaTier;
        bool const inLadderMatch = ArenaTier(bot, arenaTier);
        if (!inLadderMatch && !HasTier(bot))
            return 0;

        uint32 const tier = uint32(inLadderMatch ? arenaTier : BotSkillMath::TierOf(bot->GetGUIDLow(), Cfg().shares));
        uint32 const low = Cfg().minMs[tier];
        uint32 const high = Cfg().maxMs[tier];
        return high > low ? urand(low, high) : low;
    }
}
