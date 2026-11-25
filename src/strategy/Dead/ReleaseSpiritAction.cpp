/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ReleaseSpiritAction.h"

#include "BotVoice.h"

#include "DatabaseEnvFwd.h"
#include "Config.h"
#include "DungeonReleasePolicy.h"
#include "Group.h"
#include "Battleground.h"
#include "Event.h"
#include "ObjectDefines.h"
#include "BotGuid.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Corpse.h"
#include "NearestNpcsValue.h"
#include "WorldPosition.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

#include <algorithm>
#include <chrono>
#include <sstream>

bool ReleaseSpiritAction::Execute(Event event)
{
    if (bot->IsAlive())
    {
        // Outside a battleground only. Inside one the "-follow" half used to be overwritten with
        // "+follow" on the very next tick by the master check in PlayerbotAI, so the bot cycled
        // between "+stay" and "+follow" and stuttered on the spot. Upstream AC guards this site for
        // exactly that reason (its comment: "+stay in bg causes stuttering effect as bot is cycled
        // between +stay and +follow each tick"). See issue #53.
        if (!bot->InBattleground())
        {
            botAI->TellMasterNoFacing("I am not dead, will wait here");
            botAI->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);
        }

        return false;
    }

    if (bot->GetCorpse() && bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        botAI->TellMasterNoFacing("I am already a spirit");
        return false;
    }

    WorldPacket& p = event.getPacket();
    if (!p.empty() && p.GetOpcode() == CMSG_REPOP_REQUEST)
        botAI->TellMasterNoFacing("Releasing...");
    else
        botAI->TellMasterNoFacing("Meet me at the graveyard");

    BotVoice::NoteDeath(bot);

    // Death Count to prevent skeleton piles
    Player* master = GetMaster();
    if (!master || (master && GET_PLAYERBOT_AI(master)))
    {
        uint32 dCount = AI_VALUE(uint32, "death count");
        context->GetValue<uint32>("death count")->Set(dCount + 1);
    }

    // #BT-015: AC repairs the death's durability loss for free here; bots pay a repair NPC instead (BotRepair.h).

    SF_LOG_INFO("playerbots", "Bot %s %s:%u <%s> released", BotGuid(bot->GetGUID()).ToString().c_str(),
             bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());

    WorldPacket packet(CMSG_REPOP_REQUEST);
    packet << uint8(0);
    bot->GetSession()->HandleRepopRequestOpcode(packet);

    // // add waiting for ress aura
    // if (bot->InBattleground() && !botAI->HasAura(SPELL_WAITING_FOR_RESURRECT, bot) && !bot->IsAlive())
    // {
    //     // cast Waiting for Resurrect
    //     GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    //     BotGuid guid;
    //     Unit* unit;
    //     for (GuidVector::iterator i = npcs.begin(); i != npcs.end(); i++)
    //     {
    //         unit = botAI->GetUnit(*i);
    //         if (unit && unit->IsSpiritService())
    //         {
    //             guid = unit->GetGUID();
    //             break;
    //         }
    //     }
    //     if (!guid) {
    //         return true;
    //     }
    //     if (bot->GetDistance(unit) >= INTERACTION_DISTANCE) {
    //         bot->GetMotionMaster()->MoveChase(unit);
    //     } else {
    //         WorldPacket packet(CMSG_GOSSIP_HELLO);
    //         packet << guid;
    //         bot->GetSession()->HandleGossipHelloOpcode(packet);
    //     }
    // }

    return true;
}

bool AutoReleaseSpiritAction::Execute(Event event)
{
    if (dungeonRelease)
        SF_LOG_DEBUG("playerbots", "[DEAD] bot=%s waited=%us reason=%s",
                     bot->GetName().c_str(), dungeonWaited, dungeonReason);
    dungeonRelease = false;
    BotVoice::NoteDeath(bot);

    // Death Count to prevent skeleton piles
    Player* master = GetMaster();
    if (!master || (master && GET_PLAYERBOT_AI(master)))
    {
        uint32 dCount = AI_VALUE(uint32, "death count");
        context->GetValue<uint32>("death count")->Set(dCount + 1);
    }

    // #BT-015: AC repairs the death's durability loss for free here; bots pay a repair NPC instead (BotRepair.h).

    SF_LOG_DEBUG("playerbots", "Bot %s %s:%u <%s> auto released", BotGuid(bot->GetGUID()).ToString().c_str(),
              bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());

    WorldPacket packet(CMSG_REPOP_REQUEST);
    packet << uint8(0);
    bot->GetSession()->HandleRepopRequestOpcode(packet);

    SF_LOG_DEBUG("playerbots", "Bot %s %s:%u <%s> releases spirit", BotGuid(bot->GetGUID()).ToString().c_str(),
              bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());

    if (bot->InBattleground())
    {
        SF_LOG_DEBUG("playerbots", "[BG] Bot %s %s:%u <%s> released in battleground, running spirit healer handler",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str());

        // HandleRepopRequestOpcode above releases the bot to a graveyard, which for a battleground
        // death is a teleport (Player::RepopAtGraveyard -> Battleground::GetClosestGraveYard ->
        // TeleportTo). The healer handler searches the grid around the bot, so it must not run
        // while that teleport is in flight or the bot is otherwise off its map.
        if (bot->IsBeingTeleported() || !bot->IsInWorld() || !bot->FindMap())
        {
            botAI->SetNextCheckDelay(1000);
            return true;
        }

        bool handled = HandleBattlegroundSpiritHealer();
        botAI->SetNextCheckDelay(1000);
        return handled;
    }

    botAI->SetNextCheckDelay(1000);
    return true;
}

// Ported from AC mod-playerbots ReleaseSpiritAction.cpp:119-167 (HandleBattlegroundSpiritHealer).
// This replaces the previous inline "nearest npcs" block that was unreachable and unsafe.
bool AutoReleaseSpiritAction::HandleBattlegroundSpiritHealer()
{
    constexpr uint32_t RESURRECT_DELAY = 15;
    const time_t now = time(nullptr);

    // Recently gossiped a healer and the "waiting for resurrect" aura is up: give the core time
    // to finish the resurrection before poking the healer again.
    if ((now - bg_gossip_time < RESURRECT_DELAY) && bot->HasAura(SPELL_WAITING_FOR_RESURRECT))
    {
        SF_LOG_DEBUG("playerbots", "[BG] Bot %s %s:%u <%s> waiting for resurrect aura, healer gossip on cooldown",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str());
        return false;
    }

    // Scan wider than sight range: in IoC the resurrection point is far from the bot's base.
    float bgRange = 2000.0f;
    GuidVector npcs = NearestNpcsValue(botAI, bgRange);
    Unit* spiritHealer = nullptr;

    for (GuidVector::iterator i = npcs.begin(); i != npcs.end(); i++)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (unit && unit->IsFriendlyTo(bot) && unit->IsSpiritService() &&
            (!spiritHealer || bot->GetDistance(unit) < bot->GetDistance(spiritHealer)))
            spiritHealer = unit;  // nearest: the list is not sorted
    }

    if (!spiritHealer)
    {
        // #724 DIAGNOSTIC ONLY (behaviour stays AC's): NearestNpcsValue drops units out of line of sight (AC
        // NearestUnitsValue.h:20), and a ToK ghost ~113 yd from its alive, friendly guide logged this line 1377 times.
        // List every spirit guide in range with alive / friendly / los plus the ghost's position, so the next run shows
        // why the ghost is out of sight of its guide. Debug logging only (the scan is skipped otherwise).
        if (!sLog->ShouldLog("playerbots", LogLevel::LOG_LEVEL_DEBUG))
            return false;

        std::list<Unit*> units;
        Skyfire::AnyUnitInObjectRangeCheck u_check(bot, bgRange);
        Skyfire::UnitListSearcher<Skyfire::AnyUnitInObjectRangeCheck> searcher(bot, units, u_check);
        bot->VisitNearbyObject(bgRange, searcher);

        std::ostringstream seen;
        for (Unit* unit : units)
        {
            if (!unit || unit->GetTypeId() == TypeID::TYPEID_PLAYER || !unit->IsSpiritService())
                continue;
            seen << " [" << unit->GetEntry() << ":" << unit->GetGUIDLow() << " d=" << uint32(bot->GetDistance(unit))
                 << " alive=" << unit->IsAlive() << " friendly=" << unit->IsFriendlyTo(bot)
                 << " los=" << bot->IsWithinLOSInMap(unit) << "]";
        }

        Battleground* bg = bot->GetBattleground();
        SF_LOG_DEBUG("playerbots",
                  "[BG] Bot %s %s:%u <%s> found no friendly spirit healer within %f (pos %.1f %.1f %.1f map %u area %u"
                  " bgStatus %u bgTeam %u faction %u) guides:%s",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str(), bgRange, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                  bot->GetMapId(), bot->GetAreaId(), bg ? uint32(bg->GetStatus()) : 0u, uint32(bot->GetBGTeam()),
                  bot->getFaction(), seen.str().empty() ? " none" : seen.str().c_str());
        return false;
    }

    if (bot->GetDistance(spiritHealer) >= INTERACTION_DISTANCE)
    {
        // Bot needs to actually click spirit-healer in BG to get res timer going
        // and in IOC it's not within clicking range when they res in own base

        // Teleport to nearest friendly Spirit Healer when not currently in range of one.
        SF_LOG_DEBUG("playerbots", "[BG] Bot %s %s:%u <%s> teleporting to spirit healer %s to get in click range",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str(), BotGuid(spiritHealer->GetGUID()).ToString().c_str());
        bot->RemoveAurasWithInterruptFlags(
            SpellAuraInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP));
        bot->TeleportTo(bot->GetMapId(), spiritHealer->GetPositionX(), spiritHealer->GetPositionY(),
                        spiritHealer->GetPositionZ(), 0.f);
        RESET_AI_VALUE(bool, "combat::self target");
        RESET_AI_VALUE(WorldPosition, "current position");
    }
    else if (!botAI->IsRealPlayer()) // below doesnt work properly on realplayer, but its also not needed
    {
        bg_gossip_time = now;
        SF_LOG_DEBUG("playerbots", "[BG] Bot %s %s:%u <%s> gossips spirit healer %s to queue resurrection",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str(), BotGuid(spiritHealer->GetGUID()).ToString().c_str());
        // AC sends CMSG_GOSSIP_HELLO with a plain guid and AC's handler queues a
        // spirit guide directly (AC NPCHandler.cpp:141-144, :177-183). The 5.4.8 handler reads a bit-packed guid and
        // requires the gossip npcflag, which the BG spirit guides do not have, so no MoP bot was ever resurrected in
        // a battleground. Queue it on every click as AC's core does (the waiting aura 2584 survives death and
        // resurrection, so it cannot mark "already queued": gating on it revived each bot only once).
        // Drop any older entry first so the queue holds no duplicates.
        if (Battleground* bg = bot->GetBattleground())
        {
            bg->RemovePlayerFromResurrectQueue(bot->GetGUID());
            bg->AddPlayerToResurrectQueue(spiritHealer->GetGUID(), bot->GetGUID());
        }
    }

    return true;
}

// Ported from AC mod-playerbots ReleaseSpiritAction.cpp:194-218 (ShouldDelayBattlegroundRelease).
bool AutoReleaseSpiritAction::ShouldDelayBattlegroundRelease()
{
    // The below delays release to spirit with 6 seconds.
    // This prevents currently casted (ranged) spells to be re-directed to the died bot's ghost.

    // If the bot already is a spirit, reset release time and return true
    if (bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        bgReleaseAttemptTime = 0;
        return true;
    }

    // Delay release to spirit.
    const time_t now = time(nullptr);
    constexpr time_t RELEASE_DELAY = 6;

    if (bgReleaseAttemptTime == 0)
    {
        bgReleaseAttemptTime = now;
        SF_LOG_DEBUG("playerbots", "[BG] Bot %s %s:%u <%s> delaying release %d s to avoid redirecting in-flight spells",
                  BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(),
                  bot->GetName().c_str(), static_cast<int>(RELEASE_DELAY));
    }

    if (now - bgReleaseAttemptTime < RELEASE_DELAY)
        return false;

    bgReleaseAttemptTime = 0;
    return true;
}

bool AutoReleaseSpiritAction::isUseful()
{
    dungeonRelease = false;
    if (!bot->isDead())
        return false;

    if (bot->InArena())
        return false;

    if (bot->InBattleground())
        return ShouldDelayBattlegroundRelease();

    if (bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        return false;

    if (!bot->GetGroup())
        return true;

    if (!botAI->GetGroupMaster())
        return true;

    if (botAI->GetGroupMaster() == bot)
        return true;

    if (!botAI->HasActivePlayerMaster())
        return true;

    if (botAI->HasActivePlayerMaster() && botAI->GetGroupMaster()->GetMapId() == bot->GetMapId() && bot->GetMap() &&
        (bot->GetMap()->IsRaid() || bot->GetMap()->IsDungeon()))
    {
        using Clock = std::chrono::steady_clock;
        Clock::time_point const now = Clock::now();
        if (dungeonDeathSerial != botAI->dungeonDeathSerial || combatEnded == Clock::time_point{})
        {
            dungeonDeathSerial = botAI->dungeonDeathSerial;
            combatEnded = botAI->dungeonDeathTime == Clock::time_point{} ? now : botAI->dungeonDeathTime;
            dungeonCombatObserved = false;
        }

        bool inCombat = false;
        bool resurrecter = false;
        for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            // Only members in this instance count; one elsewhere is updated by another map and fights its own fights.
            if (!member || !member->IsInWorld() || member->GetMap() != bot->GetMap() || !member->IsAlive())
                continue;
            inCombat = inCombat || member->IsInCombat();
            if ((member->HasSpell(2006) || member->HasSpell(7328) || member->HasSpell(2008) ||
                 member->HasSpell(50769) || member->HasSpell(20484) || member->HasSpell(115178)))
                resurrecter = true;
        }
        if (inCombat)
            dungeonCombatObserved = true;
        else if (dungeonCombatObserved)
        {
            combatEnded = now;
            dungeonCombatObserved = false;
        }
        auto const elapsed = [](Clock::time_point from, Clock::time_point to) -> uint64
        {
            return uint64(std::chrono::duration_cast<std::chrono::seconds>(to - from).count());
        };
        Clock::time_point const death = botAI->dungeonDeathTime == Clock::time_point{} ? now : botAI->dungeonDeathTime;
        uint64 const waited = elapsed(death, now);
        DungeonReleasePolicy::Facts const facts = {
            waited, elapsed(combatEnded, now),
            uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.DungeonReleaseWaitSeconds", 60))),
            uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.DungeonReleaseMaxWaitSeconds", 300))),
            inCombat, bot->IsRessurectRequested(), bot->GetUInt32Value(PLAYER_FIELD_SELF_RES_SPELL) != 0,
            resurrecter
        };
        auto const decision = DungeonReleasePolicy::Decide(facts);
        if (decision == DungeonReleasePolicy::Decision::Wait)
            return false;
        dungeonRelease = true;
        dungeonWaited = uint32(waited);
        dungeonReason = decision == DungeonReleasePolicy::Decision::NoResurrecter ? "no_resurrecter" :
                        decision == DungeonReleasePolicy::Decision::CombatWaitExpired ? "combat_wait_expired" :
                        "death_wait_expired";
        return true;
    }

    // #162: MoP-only "release when the master is dead" branch removed - AC's ShouldAutoRelease has none
    // (AC ReleaseSpiritAction.cpp): a grouped bot waits for its master's resurrection or the distance rule.

    if (sServerFacade->IsDistanceGreaterThan(AI_VALUE2(float, "distance", "master target"),
                                             sPlayerbotAIConfig->sightDistance))
        return true;

    return false;
}

bool RepopAction::Execute(Event event)
{
    SF_LOG_DEBUG("playerbots", "Bot %s %s:%u <%s> repops at graveyard", BotGuid(bot->GetGUID()).ToString().c_str(),
              bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());

    int64 deadTime;

    Corpse* corpse = bot->GetCorpse();
    if (corpse)
        deadTime = time(nullptr) - corpse->GetGhostTime();
    else if (bot->isDead())
        deadTime = 0;
    else
        deadTime = 60 * MINUTE;

    uint32 dCount = AI_VALUE(uint32, "death count");

    WorldSafeLocsEntry const* ClosestGrave = GetGrave(dCount > 10 || deadTime > 30 * MINUTE);
    if (!ClosestGrave)
        return false;

    auto ClosestGraveyard = BotGetClosestGraveYard(BotGetWorldLocation(bot), bot->GetTeam());
    if (!ClosestGraveyard)
        return false;

    bot->TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, 0.f);

    RESET_AI_VALUE(bool, "combat::self target");
    RESET_AI_VALUE(WorldPosition, "current position");

    return true;
}

bool RepopAction::isUseful()
{
    if (bot->InBattleground())
        return false;

    return true;
}

bool SelfResurrectAction::Execute(Event /*event*/)
{
    if (!bot->IsAlive() && bot->GetUInt32Value(PLAYER_FIELD_SELF_RES_SPELL))
    {
        WorldPacket packet(CMSG_SELF_RES);
        bot->GetSession()->HandleSelfResOpcode(packet);
        return true;
    }

    return false;
}

bool SelfResurrectAction::isUseful()
{
    return !bot->IsAlive() && bot->GetUInt32Value(PLAYER_FIELD_SELF_RES_SPELL) != 0;
}

bool AcceptResurrectAction::Execute(Event /*event*/)
{
    if (bot->IsAlive() || !bot->IsRessurectRequested())
        return false;

    bot->ResurectUsingRequestData();
    return true;
}

bool AcceptResurrectAction::isUseful() { return !bot->IsAlive() && bot->IsRessurectRequested(); }
