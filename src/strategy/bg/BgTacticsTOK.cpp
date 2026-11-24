/*
 * This file is part of the mod-playerbots module. See AUTHORS file for Copyright information;
 * released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 *
 * Temple of Kotmogu (map 998, BATTLEGROUND_VOP) bot tactics - issue #577, epic #580.
 * AC has no spec for this map; this extends AC's WSG/EotS carrier pattern
 * (AC BattleGroundTactics.cpp: WSG selectObjective 2144-2311, atFlag pick-up 3561-3965,
 * moveToStart 1719-1840).
 *
 * The current deterministic policy uses visible enemy observations and public orb availability.
 * Scoring zones and approaches still require live validation.
 */

#include "BattleGroundTactics.h"
#include "BgRealType.h"
#include "BotCompat_C.h"
#include "AbPlanLogic.h"
#include "TokPlanLogic.h"
#include "BgLeaderPolicy.h"
#include "BgCalloutPolicy.h"
#include "BotSkill.h"
#include "BotVoice.h"
#include "PvpMatchState.h"
#include "PvpConfig.h"
#include "WorldSession.h"
#include <algorithm>

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "Battleground.h"
#include "BattlegroundVOP.h"
#include "Creature.h"
#include "Config.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "GameObject.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ServerFacade.h"
#include "Timer.h"
#include <cmath>
#include <map>
#include <mutex>

namespace
{
    // Orb stand positions: BattlegroundTOK.h m_orbPositions (index = entry - 212091:
    // 0 blue, 1 purple, 2 green, 3 orange). Ground z is the MoP navmesh surface under the stand
    // (stand z 13.57 is above the floor; the mesh edge is ~1.3 yd from the stand).
    constexpr uint8 BG_TOK_MAX_ORBS = 4;
    float const TOK_ORB_X[BG_TOK_MAX_ORBS] = {1716.83f, 1850.26f, 1716.78f, 1850.29f};
    float const TOK_ORB_Y[BG_TOK_MAX_ORBS] = {1249.93f, 1416.77f, 1416.64f, 1250.31f};
    float const TOK_ORB_GROUND_Z = 10.774f;

    // Temple centre: midpoint of the four orb stands. BattlegroundTOK::UpdateScore awards 3 / 4 / 5
    // points per 5 s tick for zone OUT / IN / MIDDLE, so the middle is the best place to hold.
    // Navmesh surface at the centre is z=5.708 (the pit).
    float const TOK_CENTRE_X = 1783.5f;
    float const TOK_CENTRE_Y = 1333.0f;
    float const TOK_CENTRE_Z = 5.708f;

    // Start rooms (#577, build 14). The room is the narrow raised ramp BEHIND each door: team start locations are
    // WorldSafeLocs 4059 (Alliance 1784.27,1077.63,28.88) and 4060 (Horde 1780.42,1598.72,33.78), the doors sit at
    // y 1100.66 / 1570.22 (core m_doorPositions) and the ramp floor falls from the start location to the door
    // (mmtile_probe, map 998: Alliance z 27.0 / 26.1 / 24.5 / 22.9 at y 1084 / 1088 / 1092 / 1096; Horde z 30.0 / 31.1 /
    // 32.1 / 33.2 at y 1584 / 1588 / 1592 / 1596), about 12 yd wide. The old spots (y 1120 / 1550) were on the TEMPLE side of
    // the doors and 43 / 49 yd from the start location, while battleground_template.StartMaxDist for map 998 is 30: the
    // WAIT_JOIN guard in BGTactics::Execute therefore pulled every bot back (the #227 "pull-back teleport") each time it
    // walked through the closed door. These are 13 / 11 yd from the start location, 10 / 18 yd short of the door.
    Position const TOK_WAIT_ALLIANCE = {1784.3f, 1090.0f, 25.3f, 0.0f};
    Position const TOK_WAIT_HORDE = {1780.4f, 1588.0f, 31.1f, 0.0f};

    uint32 const TOK_ORB_ENTRY_FIRST = BG_VOP_OBJECT_ORB_1_ENTRY;

    BattlegroundVOP* AsTok(Battleground* bg)
    {
        if (!bg)
            return nullptr;
        BattlegroundTypeId t = RealBgTypeId(bg); // #896
        return t == BATTLEGROUND_VOP ? static_cast<BattlegroundVOP*>(bg) : nullptr;
    }

    bool IsOrbEntry(uint32 entry)
    {
        return entry >= TOK_ORB_ENTRY_FIRST && entry < TOK_ORB_ENTRY_FIRST + BG_TOK_MAX_ORBS;
    }

    // ---- Scoring zones (the "centre bonus") -------------------------------------------------------------
    // BattlegroundTOK::UpdateScore pays a keeper 3 / 4 / 5 points per 5 s for being OUT of the temple / IN it /
    // in the MIDDLE, and the zone is only ever changed by BattlegroundTOK::HandleAreaTrigger: area trigger 7734
    // (the temple box) toggles OUT <-> IN and 7735 (the pit box) toggles IN <-> MIDDLE. A real client sends those
    // triggers as CMSG_AREATRIGGER when it crosses a box edge; a bot has no client, so (like the Warsong capture
    // trigger and the Deepwind cart trigger, see the note at the top of BattleGroundTactics.cpp) nothing ever called
    // them for a bot and every orb it held scored the OUT rate. TokSyncZone sends them for the bot.
    uint32 const TOK_TRIGGER_TEMPLE = 7734;  // AreaTrigger.dbc: box 145 x 178 x 40 around the four orb stands
    uint32 const TOK_TRIGGER_PIT = 7735;     // AreaTrigger.dbc: box 66 x 91 x 5 round the centre pit floor

    enum TokShadowZone : uint8
    {
        TOK_SHADOW_OUT = 0,
        TOK_SHADOW_IN = 1,
        TOK_SHADOW_MIDDLE = 2
    };

    // The core keeps the zone in a private map, so each bot keeps a mirror of it. It starts OUT, as
    // BattlegroundTOK::AddPlayer sets it, and only changes by the same two toggles the core applies.
    // #577 (2026-10-12): the mirror was thread_local. Bot AI runs inside Player::Update (mod_playerbots.cpp:322-326,
    // OnAfterUpdate), the test server runs MapUpdate.Threads = 4, and MapUpdater hands each map update to whichever worker
    // pops it (MapUpdater.cpp:84/111), so a bot's ticks move between threads and each thread kept its own stale copy.
    // HandleAreaTrigger is a blind toggle (BattlegroundTOK.cpp:265-273: 7734 flips OUT <-> IN, 7735 flips IN <-> MIDDLE),
    // so a stale copy flipped the core zone the WRONG way (a carrier in the pit dropped to OUT: 3 points instead of 5).
    // One process-wide mirror under a lock; the bots of one instance share one map update at a time, so the lock is
    // uncontended in practice. (The other thread_local helpers in these files only cache choices; a stale copy there
    // costs a re-pick, not a wrong core state.)
    struct TokZoneMirror
    {
        uint32 instanceId = 0;
        uint8 zone = TOK_SHADOW_OUT;
    };
    std::mutex s_tokZoneLock;
    std::map<BotGuid, TokZoneMirror> s_tokZone;

    char const* const TOK_SHADOW_NAMES[3] = {"out", "in", "middle"};

    // Bring the core's zone for this bot in line with where it actually stands. Two toggles at most.
    // PORT: SkyFire VOP does not implement the LoA scoring-zone triggers.
    void TokSyncZone(Player* bot, Battleground* bg, bool keeper)
    {
#if 0
        // HandleAreaTrigger ignores everything outside IN_PROGRESS, so the mirror must not move then either.
        if (bg->GetStatus() != STATUS_IN_PROGRESS)
            return;

        AreaTriggerEntry const* temple = sAreaTriggerStore.LookupEntry(TOK_TRIGGER_TEMPLE);
        AreaTriggerEntry const* pit = sAreaTriggerStore.LookupEntry(TOK_TRIGGER_PIT);
        if (!temple || !pit)
            return;

        std::lock_guard<std::mutex> guard(s_tokZoneLock);
        TokZoneMirror& mirror = s_tokZone[bot->GetGUID()];
        if (mirror.instanceId != bg->GetInstanceID())
        {
            mirror.instanceId = bg->GetInstanceID();
            mirror.zone = TOK_SHADOW_OUT;
        }

        // The same box test the core's CMSG_AREATRIGGER handler applies (Player::IsInAreaTrigger).
        uint8 wanted = TOK_SHADOW_OUT;
        if (bot->IsInAreaTrigger(temple))
            wanted = bot->IsInAreaTrigger(pit) ? TOK_SHADOW_MIDDLE : TOK_SHADOW_IN;

        // Evidence (#577): an orb keeper's scoring zone as the core now sees it (UpdateScore pays 3 / 4 / 5 per 5 s).
        if (keeper && mirror.zone != wanted)
            SF_LOG_DEBUG("playerbots", "[BGTOK] keeper-zone bot=%s bgTeam=%u zone=%s->%s at=%.1f,%.1f,%.1f score=%u-%u",
                         bot->GetName().c_str(), uint32(bot->GetBGTeam()), TOK_SHADOW_NAMES[mirror.zone],
                         TOK_SHADOW_NAMES[wanted], bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                         bg->GetTeamScore(TEAM_ALLIANCE), bg->GetTeamScore(TEAM_HORDE));

        for (uint32 step = 0; step < 3 && mirror.zone != wanted; ++step)
        {
            if (mirror.zone == TOK_SHADOW_OUT)  // 7734: OUT -> IN
            {
                bg->HandleAreaTrigger(bot, TOK_TRIGGER_TEMPLE, true);
                mirror.zone = TOK_SHADOW_IN;
            }
            else if (mirror.zone == TOK_SHADOW_IN)
            {
                if (wanted == TOK_SHADOW_MIDDLE)  // 7735: IN -> MIDDLE
                {
                    bg->HandleAreaTrigger(bot, TOK_TRIGGER_PIT, true);
                    mirror.zone = TOK_SHADOW_MIDDLE;
                }
                else  // 7734: IN -> OUT
                {
                    bg->HandleAreaTrigger(bot, TOK_TRIGGER_TEMPLE, false);
                    mirror.zone = TOK_SHADOW_OUT;
                }
            }
            else  // 7735: MIDDLE -> IN
            {
                bg->HandleAreaTrigger(bot, TOK_TRIGGER_PIT, false);
                mirror.zone = TOK_SHADOW_IN;
            }
        }
#endif
    }

    // Each bot's opening role, set where the objective is chosen and read at the orb (both on the bot's map thread,
    // but different bots of one match can run on different threads).
    std::mutex s_tokRoleLock;
    std::map<BotGuid, TokPlan::Role> s_tokRole;

    void RememberTokRole(BotGuid guid, TokPlan::Role role)
    {
        std::lock_guard<std::mutex> guard(s_tokRoleLock);
        s_tokRole[guid] = role;
    }

    bool IsTokPickup(BotGuid guid)
    {
        std::lock_guard<std::mutex> guard(s_tokRoleLock);
        auto const found = s_tokRole.find(guid);
        return found != s_tokRole.end() && found->second == TokPlan::Role::PICKUP;
    }
}

bool BGTactics::isCarryingTOK(Battleground* bg)
{
    BattlegroundVOP* tok = AsTok(bg);
    if (!tok)
        return false;

    // PORT: SkyFire keeps VOP orb holders private; use the pickup auras it applies.
    return bot->HasAura(BG_VOP_SPELL_ORB_PICKED_UP_1) || bot->HasAura(BG_VOP_SPELL_ORB_PICKED_UP_2) ||
           bot->HasAura(BG_VOP_SPELL_ORB_PICKED_UP_3) || bot->HasAura(BG_VOP_SPELL_ORB_PICKED_UP_4);
}

bool BGTactics::moveToStartTOK(Battleground* bg)
{
    if (!AsTok(bg))
        return false;

    // AC moveToStart (WSG/AB branches): spread around the team's wait spot with frand - kept inside the ~12 yd wide ramp.
    Position const& wait = BgTeamIdOf(bot) == TEAM_HORDE ? TOK_WAIT_HORDE : TOK_WAIT_ALLIANCE;
    return MoveTo(bg->GetMapId(), wait.GetPositionX() + frand(-2.5f, 2.5f),
                  wait.GetPositionY() + frand(-3.0f, 3.0f), wait.GetPositionZ());
}

bool BGTactics::selectObjectiveTOK(Battleground* bg)
{
    BattlegroundVOP* tok = AsTok(bg);
    if (!tok)
        return false;

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];

    auto setObjective = [&](float x, float y, float z)
    {
        pos.Set(x, y, z, bot->GetMapId());
        posMap["bg objective"] = pos;
    };

    TeamId const myTeam = BgTeamIdOf(bot);
    std::vector<Player*> friends, enemies, bots;
    for (auto const& entry : bg->GetPlayers())
        if (Player* p = ObjectAccessor::FindPlayer(entry.first))
            if (p->IsInWorld() && p->IsAlive() && p->GetMapId() == bot->GetMapId())
            {
                if (BgTeamIdOf(p) == myTeam)
                {
                    friends.push_back(p);
                    if (p->GetSession() && p->GetSession()->IsBot()) bots.push_back(p);
                }
                else enemies.push_back(p);
            }
    auto seen = [&](Player* enemy)
    {
        return AbLogic::SeenByTeam(enemy->IsAlive(), friends,
            [](Player* p) { return p->IsAlive(); },
            [enemy](Player* p) { return AbLogic::WithinSight(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(),
                                                              p->GetPositionX(), p->GetPositionY(), p->GetPositionZ()); },
            [enemy](Player* p) { return p->CanSeeOrDetect(enemy); });
    };
    // Use the shared election policy: humans count in the side but cannot lead.
    std::vector<BgLeader::Candidate> candidates;
    for (Player* p : friends)
    {
        bool const isBot = p->GetSession() && p->GetSession()->IsBot();
        candidates.push_back({p->GetGUIDLow(), isBot ? BotSkill::TierOf(p) : BotSkillMath::Tier::EXPERT,
                              uint8(p->getLevel()), isBot, true, true});
    }
    uint32 const now = getMSTime();
    static bool const chatEnabled = sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Enable", true) &&
                                    sConfigMgr->GetBoolDefault("AiPlayerbot.BattlegroundChat.Replies", true);
    PvpMatch::Handle const match = PvpMatch::Touch(bg->GetInstanceID(), bg->GetMapId(), bg->GetElapsedTime(),
                                                   bg->GetStatus() == STATUS_WAIT_LEAVE, now);
    match->IngestInbox(now, PvpConfig::ReaderLimits());
    match->Side(uint32(myTeam)).observations.Sweep(now);
    struct SidePlan
    {
        uint32 generation = 0, sampleAt = 0, oldOurs = 0, oldTheirs = 0, startElapsed = 0;
        uint32 callAt[4] = {}, stanceCallAt = 0;
        std::map<uint32, uint32> senderCalls;
        BgLeader::Election election;
        TokPlan::Stance stance = TokPlan::Stance::BALANCED;
        bool announce = false, started = false;
    };
    static std::mutex leaderLock;
    static std::map<std::pair<uint32, uint32>, SidePlan> plans;
    TokPlan::Stance stance;
    bool announce = false;
    {
        std::lock_guard<std::mutex> guard(leaderLock);
        if (plans.size() > 128)
            for (auto it = plans.begin(); it != plans.end(); )
                if (it->first.first != bg->GetInstanceID() && !PvpMatch::Find(it->first.first)) it = plans.erase(it);
                else ++it;
        SidePlan& plan = plans[std::make_pair(bg->GetInstanceID(), uint32(myTeam))];
        if (plan.generation != match->Key().generation) { plan = SidePlan(); plan.generation = match->Key().generation; }
        bool const active = bg->GetStatus() == STATUS_IN_PROGRESS;
        BgLeader::Change const change = BgLeader::Elect(plan.election, candidates, now, active);
        if (change != BgLeader::Change::NONE)
            SF_LOG_DEBUG("playerbots", "[BGTOK-LEAD] team=%u leader=%u epoch=%u instance=%u",
                         uint32(myTeam), plan.election.leader, plan.election.epoch, bg->GetInstanceID());
        if (active && !plan.started) { plan.started = true; plan.startElapsed = bg->GetElapsedTime(); }
        if (active && plan.election.leader == bot->GetGUIDLow() &&
            (!plan.sampleAt || getMSTimeDiff(plan.sampleAt, now) >= TokPlan::DECISION_MS))
        {
            uint32 const ours = bg->GetTeamScore(uint32(myTeam));
            uint32 const theirs = bg->GetTeamScore(1 - uint32(myTeam));
            uint32 const played = bg->GetElapsedTime() - plan.startElapsed;
            uint32 const remaining = played < 1500000 ? 1500000 - played : 0;
            if (plan.sampleAt)
            {
                TokPlan::Projection const projection = TokPlan::Project(ours, theirs, plan.oldOurs, plan.oldTheirs,
                                                                         getMSTimeDiff(plan.sampleAt, now), remaining);
                TokPlan::Stance const next = TokPlan::ChooseStance(projection, ours, theirs, remaining, plan.stance);
                if (next != plan.stance) { plan.stance = next; plan.announce = true; }
                SF_LOG_DEBUG("playerbots", "[BGTOK-LEAD] instance=%u team=%u score=%u-%u projected=%.0f-%.0f stance=%u",
                             bg->GetInstanceID(), uint32(myTeam), ours, theirs, projection.ours, projection.theirs, unsigned(plan.stance));
            }
            plan.oldOurs = ours; plan.oldTheirs = theirs; plan.sampleAt = now ? now : 1;
        }
        stance = plan.stance;
        if (active && plan.announce && plan.election.leader == bot->GetGUIDLow() &&
            (!plan.stanceCallAt || getMSTimeDiff(plan.stanceCallAt, now) >= 20000))
            announce = true;
    }
    if (announce && chatEnabled)
    {
        char const* line = stance == TokPlan::Stance::HOLD ? "hold center" : stance == TokPlan::Stance::PUSH ? "push their carrier" :
                           stance == TokPlan::Stance::ALL_IN ? "all in, take orbs" : "regroup mid";
        if (BotVoice::Say(botAI, "tok.stance", line, {}, BotVoice::Channel::BATTLEGROUND, BotVoice::Priority::NOTICE, 100, nullptr, true))
        {
            std::lock_guard<std::mutex> guard(leaderLock);
            SidePlan& plan = plans[std::make_pair(bg->GetInstanceID(), uint32(myTeam))];
            if (plan.stance == stance) { plan.announce = false; plan.stanceCallAt = now ? now : 1; }
            SF_LOG_DEBUG("playerbots", "[BGTOK-CALL] %s says %s", bot->GetName().c_str(), line);
        }
    }
    unsigned support = 0;
    for (Player* p : friends)
        if (p != bot && bot->GetDistance(p) <= TokPlan::SUPPORT_RADIUS) ++support;
    unsigned const threats = TokPlan::CountSeen(enemies, [&](Player* p) { return seen(p) && bot->GetDistance(p) <= TokPlan::SUPPORT_RADIUS; });
    struct ZoneState { uint32 instance = 0; uint32 changed = 0; TokPlan::Zone zone = TokPlan::Zone::RING; };
    static std::mutex lock;
    static std::map<BotGuid, ZoneState> states;
    if (isCarryingTOK(bg))
    {
        std::lock_guard<std::mutex> guard(lock);
        uint32 const now = getMSTime();
        ZoneState& state = states[bot->GetGUID()];
        if (state.instance != bg->GetInstanceID()) { state = ZoneState(); state.instance = bg->GetInstanceID(); state.changed = now; }
        TokPlan::Zone const zone = TokPlan::CarrierZone(state.zone, bot->GetHealthPct(), support, threats,
                                                        support > 0, getMSTimeDiff(state.changed, now));
        if (zone != state.zone) { state.zone = zone; state.changed = now; }
        if (zone == TokPlan::Zone::CENTRE)
            setObjective(TOK_CENTRE_X, TOK_CENTRE_Y, TOK_CENTRE_Z);
        else
        {
            // Use an existing ground waypoint on the friendly approach; exact scoring boxes need live validation.
            uint8 const orb = myTeam == TEAM_ALLIANCE ? 0 : 2;
            setObjective(TOK_ORB_X[orb], TOK_ORB_Y[orb], TOK_ORB_GROUND_Z);
        }
        return true;
    }
    {
        std::lock_guard<std::mutex> guard(lock);
        states.erase(bot->GetGUID());
    }

    std::sort(bots.begin(), bots.end(), [](Player* a, Player* b) { return a->GetGUIDLow() < b->GetGUIDLow(); });
    unsigned index = 0;
    while (index < bots.size() && bots[index] != bot) ++index;
    TokPlan::Split const split = TokPlan::WithStance(TokPlan::Opening(friends.size()), stance);
    TokPlan::Role const role = TokPlan::RoleAt(split, index);
    RememberTokRole(bot->GetGUID(), role);
    bool orbFree[BG_TOK_MAX_ORBS] = {};
    for (uint8 i = 0; i < BG_TOK_MAX_ORBS; ++i)
        if (GameObject* go = bg->GetBGObject(BG_VOP_OBJECT_ORB_1 + i))
            orbFree[i] = go->isSpawned();

    // Assign distinct free stands greedily, with distance and GUID tie breaks.
    int assigned = -1;
    bool reserved[BG_TOK_MAX_ORBS] = {};
    unsigned assignedCount = 0;
    for (unsigned n = 0; n < bots.size() && n < split.pickup && assignedCount < split.pickup; ++n)
    {
        bool carrying = false;
        uint32 const auras[] = { BG_VOP_SPELL_ORB_PICKED_UP_1, BG_VOP_SPELL_ORB_PICKED_UP_2,
                                 BG_VOP_SPELL_ORB_PICKED_UP_3, BG_VOP_SPELL_ORB_PICKED_UP_4 };
        for (uint32 aura : auras) if (bots[n]->HasAura(aura)) carrying = true;
        if (carrying) continue;
        int best = -1; float distance = 1.0e9f;
        for (uint8 i = 0; i < BG_TOK_MAX_ORBS; ++i)
            if (orbFree[i] && !reserved[i])
            {
                float const d = sServerFacade->GetDistance2d(bots[n], TOK_ORB_X[i], TOK_ORB_Y[i]);
                if (d < distance) { best = i; distance = d; }
            }
        if (best >= 0) { reserved[best] = true; ++assignedCount; if (bots[n] == bot) assigned = best; }
    }
    Player* nearestFriend = nullptr;
    Player* nearestEnemy = nullptr;
    float friendDist = 1.0e9f, enemyDist = 1.0e9f;
    uint32 const orbAuras[BG_TOK_MAX_ORBS] = {BG_VOP_SPELL_ORB_PICKED_UP_1, BG_VOP_SPELL_ORB_PICKED_UP_2,
                                               BG_VOP_SPELL_ORB_PICKED_UP_3, BG_VOP_SPELL_ORB_PICKED_UP_4};
    for (Player* p : friends)
        if (p != bot)
            for (uint32 aura : orbAuras)
                if (p->HasAura(aura) && bot->GetDistance(p) < friendDist)
                    { nearestFriend = p; friendDist = bot->GetDistance(p); }
    for (Player* p : enemies)
        if (seen(p))
            for (uint32 aura : orbAuras)
                if (p->HasAura(aura) && bot->GetDistance(p) < enemyDist)
                    { nearestEnemy = p; enemyDist = bot->GetDistance(p); }
    // Only the calling bot's own sight may produce a report.
    static char const* const colours[] = {"blue", "purple", "green", "orange"};
    for (Player* enemy : enemies)
    {
        if (!chatEnabled) break;
        if (!AbLogic::WithinSight(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(),
                                  bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()) || !bot->CanSeeOrDetect(enemy)) continue;
        for (uint8 c = 0; c < BG_TOK_MAX_ORBS; ++c)
        {
            if (!enemy->HasAura(orbAuras[c])) continue;
            bool allowed;
            {
                std::lock_guard<std::mutex> guard(leaderLock);
                SidePlan& plan = plans[std::make_pair(bg->GetInstanceID(), uint32(myTeam))];
                allowed = TokPlan::CallAllowed(now, plan.callAt[c], plan.senderCalls[bot->GetGUIDLow()], unsigned(BotSkill::TierOf(bot)));
            }
            if (!allowed) continue;
            std::string const line = std::string("enemy ") + colours[c] + " carrier";
            if (BotVoice::Say(botAI, "tok.enemy_carrier", line, {}, BotVoice::Channel::BATTLEGROUND, BotVoice::Priority::NOTICE, 100, nullptr, true))
            {
                { std::lock_guard<std::mutex> guard(leaderLock);
                  SidePlan& plan = plans[std::make_pair(bg->GetInstanceID(), uint32(myTeam))];
                  plan.callAt[c] = plan.senderCalls[bot->GetGUIDLow()] = now ? now : 1; }
                PvpObs::Observation obs;
                obs.kind = PvpObs::Kind::ENEMY_CARRIER; obs.provenance = PvpObs::Provenance::BOT_CALL;
                obs.place = c; obs.senderLow = bot->GetGUIDLow();
                match->Side(uint32(myTeam)).observations.Add(obs, now, PvpConfig::ReaderLimits());
                SF_LOG_DEBUG("playerbots", "[BGTOK-CALL] %s says %s", bot->GetName().c_str(), line.c_str());
            }
        }
    }
    {
        static thread_local std::map<BotGuid, uint32> logged;
        uint32& last = logged[bot->GetGUID()];
        if (!last || getMSTimeDiff(last, getMSTime()) >= 30000)
        {
            last = getMSTime();
            SF_LOG_DEBUG("playerbots", "[BGTOK] plan bot=%s team=%u role=%u orb=%d instance=%u",
                         bot->GetName().c_str(), uint32(myTeam), unsigned(role), assigned, bg->GetInstanceID());
        }
    }
    if (assigned >= 0)
    {
        setObjective(TOK_ORB_X[assigned], TOK_ORB_Y[assigned], TOK_ORB_GROUND_Z);
        return true;
    }

    // A bot without a pickup reservation can take a free orb when no carrier needs it.
    int nearestOrb = -1;
    float orbDist = 1.0e9f;
    for (uint8 i = 0; i < BG_TOK_MAX_ORBS; ++i)
    {
        if (!orbFree[i])
            continue;
        float const d = sServerFacade->GetDistance2d(bot, TOK_ORB_X[i], TOK_ORB_Y[i]);
        if (d < orbDist)
        {
            orbDist = d;
            nearestOrb = i;
        }
    }

    // The centre group escorts an own carrier (carriers hold the centre) and otherwise holds the scoring pit itself;
    // takers without a free orb and hunters go for an enemy carrier first, then escort (AC WSG case: escort team FC /
    // hunt enemy FC, 2300-2311).
    if (role == TokPlan::Role::SUPPORT)
    {
        if (nearestFriend)
            setObjective(nearestFriend->GetPositionX(), nearestFriend->GetPositionY(), nearestFriend->GetPositionZ());
        else
            setObjective(TOK_CENTRE_X + frand(-6.0f, 6.0f), TOK_CENTRE_Y + frand(-6.0f, 6.0f), TOK_CENTRE_Z);
        return true;
    }

    if (Player* target = nearestEnemy ? nearestEnemy : nearestFriend)
    {
        setObjective(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
        return true;
    }

    // A colour-only report has no coordinates. Hunters search the central objective without
    // consulting the unseen carrier's live position.
    if (role == TokPlan::Role::HUNT)
        for (PvpObs::Observation const& obs : match->Side(uint32(myTeam)).observations.items)
            if (obs.kind == PvpObs::Kind::ENEMY_CARRIER && obs.place < BG_TOK_MAX_ORBS && TokPlan::Fresh(now, obs.receivedMs, obs.expiresMs - obs.receivedMs) &&
                BgCallout::Heeds(BotSkill::TierOf(bot), bot->GetGUIDLow(), obs.receivedMs))
            {
                setObjective(TOK_CENTRE_X, TOK_CENTRE_Y, TOK_CENTRE_Z);
                return true;
            }

    // No carrier on either side: take a free orb if any remain, else contest the centre.
    if (nearestOrb >= 0)
    {
        setObjective(TOK_ORB_X[nearestOrb], TOK_ORB_Y[nearestOrb], TOK_ORB_GROUND_Z);
        return true;
    }

    setObjective(TOK_CENTRE_X + frand(-4.0f, 4.0f), TOK_CENTRE_Y + frand(-4.0f, 4.0f), TOK_CENTRE_Z);
    return true;
}

bool BGTactics::atObjectiveTOK(Battleground* bg)
{
    BattlegroundVOP* tok = AsTok(bg);
    if (!tok)
        return false;

    // The centre bonus: tell the core which scoring zone this bot is in (every bot, every tick - this action
    // runs from "bg check flag" for the whole match, and a zone must be right before the bot ever takes an orb).
    TokSyncZone(bot, bg, isCarryingTOK(bg));

    // One orb per player (core EventPlayerClickedOnFlag HasOrb check).
    if (isCarryingTOK(bg) || !bot->IsAlive())
        return false;

    GuidVector closeObjects = *context->GetValue<GuidVector>("nearest game objects no los");
    if (closeObjects.empty())
        return false;

    GameObject* target = nullptr;
    float best = 1.0e9f;
    for (BotGuid const guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go || !IsOrbEntry(go->GetEntry()))
            continue;
        // A taken orb is despawned (TakeOrb: SpawnBGObject RESPAWN_ONE_DAY).
        if (!go->isSpawned() || go->GetGoState() != GO_STATE_READY)
            continue;

        float const d = bot->GetDistance(go);
        if (d < best)
        {
            best = d;
            target = go;
        }
    }
    if (!target)
        return false;

    if (best < INTERACTION_DISTANCE)
    {
        if (bot->IsMounted())
            bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        if (bot->IsInDisallowedMountForm())
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

        // See the note at the top of BattleGroundTactics.cpp: CMSG_GAME_OBJ_REPORT_USE is bit-packed here.
        SF_LOG_DEBUG("playerbots", "[BGTOK] %s (bgTeam %u) takes orb %u at %.1f yd",
                     bot->GetName().c_str(), uint32(bot->GetBGTeam()), target->GetEntry(), best);
        BotResetEmoteStateIfNeed(bot);
        // Lead (b12): the orb GO type was not confirmed (24/36 would route GameObject::Use to the BG; the
        // type-26 switch there does not list 212091-212094). So do exactly what GameObject::Use does for a
        // flag stand (GameObject.cpp:1765-1794) and hand the click straight to the battleground - the same
        // call either way, independent of the template type.
        if (!bot->CanUseBattlegroundObject(target) || bot->GetVehicle())
            return false;
        bot->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
        bot->RemoveAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        bg->EventPlayerClickedOnFlag(bot, target);
        resetObjective();
        return true;
    }

    // Walk the last stretch to an orb only for the opening's pickup bots; the rest keep their support or hunt
    // objective and pick one up only if they happen to stand next to it.
    if (!IsTokPickup(bot->GetGUID()) || best > 50.0f)
        return false;

    // AC atFlag AB branch: engage a nearby enemy player before going for the objective.
    Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target");
    if (enemyPlayer && enemyPlayer->IsAlive() && enemyPlayer->GetDistance(target) < 25.0f)
    {
        context->GetValue<Unit*>("current target")->Set(enemyPlayer);
        return false;
    }

    return MoveTo(bot->GetMapId(), target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
}
