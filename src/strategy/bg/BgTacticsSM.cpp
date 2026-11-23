/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// Silvershard Mines (map 727, BATTLEGROUND_SM) bot tactics - BT-611, docs/bg-sheets/silvershard-mines.md. The plan, the jobs and the
// scores live in SmPlan.cpp and Utils/SmPlanLogic.h; this file is the part that needs the tactics class: it walks the bot to the cart it
// was dealt, keeps it inside the circle, jumps off the start platform, holds and clicks a track switch the way a player does (the spell
// click, a cast that damage interrupts), and brings a bot back that a fight pulled out of the circle.
//
// Core truth (BattlegroundSM::UpdateCartControl): every alive, not stealthed player within 22 yd of a cart moves its bar one step a second
// towards his team, the side of 50 the bar is on keeps the cart and scores a point a second, and a docked cart pays 150 to its keeper.
// There is no click on a cart. A cart moves at about 2 yd/s, so the bot targets the point of the rail it reaches when the cart does
// (SmLogic::InterceptOf), not the place the cart is standing at now.
//
// Navmesh of map 727 (sheet section 18): the played area lies inside one complete tile; the two start platforms are islands (a bot jumps
// down), and every rail runs within 10 yd of walkable ground, so a bot always finds a standing place inside the 22 yd circle.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "Battleground.h"
#include "BattlegroundSM.h"
#include "BotSkill.h"
#include "Creature.h"
#include "Map.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ServerFacade.h"
#include "SmPlan.h"
#include "SmPlanLogic.h"
#include <cmath>
#include <map>
#include <mutex>
#include <vector>

namespace Sm = SmLogic;

namespace
{
// A bot at its cart stays within this many yards of it (the circle is 22 yd; the middle of it keeps the bar moving when the cart rolls on);
// closer than this to the cart a bot follows it at a gap.
constexpr float SM_STAY_RADIUS = 8.0f;
constexpr float SM_FOLLOW_GAP = 4.0f;
// Inside this a bot walks to the cart itself (MoveNear), beyond it to the point of the rail it will meet the cart at.
constexpr float SM_NEAR_ENGAGE = 40.0f;
// A bot gets off its mount this close to the cart: a mount does not count out of the circle but it cannot fight from it.
constexpr float SM_DISMOUNT_RADIUS = 30.0f;
// The click on a switch: the NPC lies 1.7 to 1.9 yd from the nearest mesh vertex, so a bot at the stand is within this of it.
constexpr float SM_SWITCH_CLICK_RANGE = 4.5f;
constexpr uint32 SM_SWITCH_CLICK_GAP_MS = 4000;   // after a click (the core's own lock is 3 s)
constexpr uint32 SM_LOG_REPEAT_MS = 10000;

// Gate-side start spots, on the map-727 navmesh (the platforms are islands at z about 329): the spot is picked by a bot's place in its
// team's roster so that the team does not stand in one heap.
Position const SM_START_ALLIANCE[3] = { { 846.0f, 144.0f, 329.0f, 0.0f }, { 852.0f, 150.0f, 329.0f, 0.0f }, { 858.0f, 156.0f, 328.9f, 0.0f } };
Position const SM_START_HORDE[3] = { { 633.0f, 224.0f, 329.0f, 0.0f }, { 639.0f, 230.0f, 329.0f, 0.0f }, { 645.0f, 236.0f, 329.1f, 0.0f } };
// Where to wait while no cart has spawned yet (the start slots).
Position const SM_CART_PIT = { 744.5f, 183.5f, 319.7f, 0.0f };

// A BgCreatures slot as a creature, or null. Looked up through the map rather than Battleground::GetBGCreature, which logs an error
// whenever the slot is empty (a cart slot is empty for a moment after the depot capture).
Creature* SmSlot(Battleground* bg, uint32 slot)
{
    if (slot >= bg->BgCreatures.size() || !bg->BgCreatures[slot])
        return nullptr;
    Creature* c = bg->GetBgMap()->GetCreature(bg->BgCreatures[slot]);
    return c && c->IsAlive() ? c : nullptr;
}

// ---- Start platforms (#578, #748) -------------------------------------------------------------------------------------
// Each start area is a platform about 9 yd above the mine floor, and on the map-727 navmesh it is a separate island (24 polygons for the
// Alliance, 26 for the Horde, re-read from today's tile on 2026-11-19: sheet section 18). No path leaves either island, so every bot
// walks to a take-off point on the lip and jumps down, as on the Warsong Gulch and Eye of the Storm ledges. Take-off and landing lie
// within 1 yd of the mesh of the main component, which holds the pit, every rail, all four depots and both switches.
struct SmLedge
{
    float minX, maxX, minY, maxY;  // island box (+1 yd)
    float takeoff[3];
    float landing[3];
    char const* name;
};
SmLedge const SM_LEDGES[2] = {
    { 819.0f, 876.0f, 112.0f, 172.0f, { 823.0f, 144.7f, 329.0f }, { 817.0f, 145.5f, 320.7f }, "alliance" },
    { 605.0f, 663.0f, 194.0f, 254.0f, { 659.8f, 229.8f, 329.0f }, { 665.5f, 229.0f, 320.7f }, "horde" }
};
// The islands sit at z 328.7..330.4; the main mesh inside the same boxes is at z 319.7..322 (below the overhang) and at z 366..367 (a ledge north
// of the Alliance box), so a z band picks the island alone.
constexpr float SM_LEDGE_MIN_Z = 325.5f;
constexpr float SM_LEDGE_MAX_Z = 334.0f;
constexpr float SM_TAKEOFF_RADIUS = 3.0f;

SmLedge const* SmLedgeOf(Player* bot)
{
    float const z = bot->GetPositionZ();
    if (z < SM_LEDGE_MIN_Z || z > SM_LEDGE_MAX_Z)
        return nullptr;
    for (SmLedge const& l : SM_LEDGES)
        if (bot->GetPositionX() >= l.minX && bot->GetPositionX() <= l.maxX && bot->GetPositionY() >= l.minY && bot->GetPositionY() <= l.maxY)
            return &l;
    return nullptr;
}

// Per-bot bookkeeping of the tick, behind one lock: the last switch click and when a bot left its cart's leash.
std::mutex s_smLock;
std::map<uint32, uint32> s_lastClickMs;
std::map<uint32, uint32> s_outsideSinceMs;
std::map<uint32, uint32> s_lastLeashLogMs;
}  // namespace

bool BGTactics::moveToStartSM(Battleground* bg)
{
    Position const* spots = BgTeamIdOf(bot) == TEAM_HORDE ? SM_START_HORDE : SM_START_ALLIANCE;
    Position const& p = spots[SmPlan::StartSpot(bg, bot)];
    return MoveTo(bg->GetMapId(), p.GetPositionX() + frand(-3.0f, 3.0f), p.GetPositionY() + frand(-3.0f, 3.0f), p.GetPositionZ());
}

bool BGTactics::selectObjectiveSM(Battleground* bg)
{
    if (!SmPlan::Enabled(bg))
        return false;

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];

    SmPlan::View view;
    if (SmPlan::Decide(bg, bot, GetBotStrategyForTeam(bg, TEAM_ALLIANCE), GetBotStrategyForTeam(bg, TEAM_HORDE), view))
    {
        if (view.switchTrack < Sm::TRACK_COUNT)
            pos.Set(Sm::SWITCH_STAND[view.switchTrack].x, Sm::SWITCH_STAND[view.switchTrack].y, Sm::SWITCH_STAND[view.switchTrack].z, bot->GetMapId());
        else
            pos.Set(view.x, view.y, view.z, bot->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    }

    // No cart yet (the carts spawn when the gates open) or the plan cannot say: wait at the pit.
    pos.Set(SM_CART_PIT.GetPositionX() + frand(-6.0f, 6.0f), SM_CART_PIT.GetPositionY() + frand(-6.0f, 6.0f), SM_CART_PIT.GetPositionZ(),
            bot->GetMapId());
    posMap["bg objective"] = pos;
    return true;
}

// Every tick of "move to objective": a changed plan state (after the bot's tier lag) and the leash.
void BGTactics::smTick(Battleground* bg)
{
    if (!SmPlan::Enabled(bg) || !bot->IsAlive())
        return;

    // A call-out that is due (the bot's own delay passed, the team's limiter allows it).
    SmPlan::DeliverCalls(botAI);

    uint8 const strategyAlliance = GetBotStrategyForTeam(bg, TEAM_ALLIANCE);
    uint8 const strategyHorde = GetBotStrategyForTeam(bg, TEAM_HORDE);

    // A changed plan state (a dock, a keeper, a switch, a job): choose again, after the bot's tier lag.
    if (SmPlan::PlanChanged(bg, bot, strategyAlliance, strategyHorde))
    {
        resetObjective();
        return;
    }

    // The order of targets in a fight at a cart, by tier (owner decision 2026-11-18, shared with the node maps).
    FightOrder(bg);

    // The leash (core rule, sheet section 6): a bot in a fight that has been pulled more than 30 yd from its cart comes back; an expert at
    // once, a good bot after 3 s, a casual bot only about two times in three.
    uint32 const guid = bot->GetGUIDLow();
    uint32 const now = getMSTime();
    uint8 cart = Sm::CART_NONE;
    float yards = 0.0f;
    if (!bot->IsInCombat() || !SmPlan::CartDistance(bg, bot, cart, yards) || yards <= Sm::LEASH_YARDS)
    {
        std::lock_guard<std::mutex> guard(s_smLock);
        s_outsideSinceMs.erase(guid);
        return;
    }

    uint32 outside;
    {
        std::lock_guard<std::mutex> guard(s_smLock);
        uint32& since = s_outsideSinceMs[guid];
        if (!since)
            since = now ? now : 1;
        outside = now - since;
    }
    if (!Sm::LeashReturns(BotSkill::TierOf(bot), yards, outside, guid, now / 10000))
        return;

    Creature* cartUnit = SmSlot(bg, NPC_SM_MINE_CART_1 + cart);
    if (!cartUnit)
        return;

    bot->AttackStop();
    MoveNear(cartUnit, SM_FOLLOW_GAP + 2.0f);
    {
        std::lock_guard<std::mutex> guard(s_smLock);
        uint32& last = s_lastLeashLogMs[guid];
        if (!last || now - last >= SM_LOG_REPEAT_MS)
        {
            last = now ? now : 1;
            SF_LOG_DEBUG("playerbots", "[BGSM] leash %s tier=%u cart=%s %.0f yd out for %u ms: back to the cart", bot->GetName().c_str(),
                         uint32(BotSkill::TierOf(bot)), Sm::CartWord(cart), yards, outside);
        }
    }
}

bool BGTactics::atObjectiveSM(Battleground* bg)
{
    if (!bot->IsAlive())
        return false;

    // Start platform (#578, #748): get down to the main mesh first - see SM_LEDGES. Only once the gates are open (before that the bot keeps
    // its moveToStartSM wait spot on the same platform), and also after every resurrection, since the graveyard is on the platform too.
    if (bg->GetStatus() == STATUS_IN_PROGRESS && !bot->GetVehicle())
    {
        // Mid-jump (MotionMaster::MoveJump runs an EffectMovementGenerator): the arc passes through the island box.
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE)
            return true;

        if (SmLedge const* ledge = SmLedgeOf(bot))
        {
            if (bot->GetExactDist2d(ledge->takeoff[0], ledge->takeoff[1]) < SM_TAKEOFF_RADIUS)
            {
                if (JumpTo(bg->GetMapId(), ledge->landing[0], ledge->landing[1], ledge->landing[2]))
                    SF_LOG_DEBUG("playerbots", "[BGSM] jump-down bot=%s ledge=%s from=%.1f,%.1f,%.1f to=%.1f,%.1f,%.1f", bot->GetName().c_str(),
                                 ledge->name, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), ledge->landing[0], ledge->landing[1],
                                 ledge->landing[2]);
                return true;
            }

            MoveTo(bg->GetMapId(), ledge->takeoff[0], ledge->takeoff[1], ledge->takeoff[2]);
            return true;
        }
    }

    // Before the gates open the bots wait on their platforms (moveToStartSM); the plan is read from the first tick of the match.
    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return false;

    SmPlan::View view;
    if (!SmPlan::Decide(bg, bot, GetBotStrategyForTeam(bg, TEAM_ALLIANCE), GetBotStrategyForTeam(bg, TEAM_HORDE), view))
        return false;

    if (view.switchTrack < Sm::TRACK_COUNT && atSwitchSM(bg, view))
        return true;

    Creature* cart = SmSlot(bg, NPC_SM_MINE_CART_1 + view.cart);
    if (!cart)
        return false;

    // Fight what comes (the combat strategies do): the circle is 22 yd, the leash in smTick keeps the fight near it.
    if (bot->IsInCombat())
        return false;

    float const distance = sServerFacade->GetDistance2d(bot, cart->GetPositionX(), cart->GetPositionY());

    // A stealthed player does not count (BattlegroundSM::UpdateCartControl): the bot leaves stealth to stand in the circle.
    if (distance <= Sm::CONTROL_RADIUS && bot->HasStealthAura())
        bot->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
    if (distance <= SM_DISMOUNT_RADIUS && bot->IsMounted())
        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);

    // In the circle: stay (the bar moves while the bot stands there), follow the cart when it rolls away.
    if (distance <= SM_STAY_RADIUS)
        return true;

    if (distance <= SM_NEAR_ENGAGE)
    {
        MoveNear(cart, SM_FOLLOW_GAP);
        return true;
    }

    // Far: the point of the rail the bot reaches together with the cart; when no such point exists the cart itself.
    if (MoveTo(bg->GetMapId(), view.x, view.y, view.z))
        return true;
    return MoveNear(cart, SM_FOLLOW_GAP);
}

// The switch holder: walk to the stand next to the NPC, wait for the window an expert uses, click through the spell click (a cast that damage
// interrupts, as for a player), and cancel a cast the enemy has made useless (a click toggles). True when the bot is busy with the duty.
bool BGTactics::atSwitchSM(Battleground* bg, SmPlan::View const& view)
{
    Creature* npc = SmSlot(bg, SM_TRACK_SWITCH_EAST + view.switchTrack);
    if (!npc)
        return false;

    if (bot->IsNonMeleeSpellCasted(false))
    {
        Sm::SwitchFacts f;
        f.track = view.switchTrack;
        f.team = BgTeamIdOf(bot) == TEAM_ALLIANCE ? Sm::TEAM_IDX_ALLIANCE : Sm::TEAM_IDX_HORDE;
        f.opened = npc->HasAura(BG_SM_TRACK_SWITCH_OPENED);
        if (Sm::CancelCast(f))
        {
            bot->InterruptNonMeleeSpells(false);
            SF_LOG_DEBUG("playerbots", "[BGSM] switch-cancel %s track=%u: the enemy turned it the right way", bot->GetName().c_str(),
                         uint32(view.switchTrack));
        }
        return true;
    }

    if (!view.switchWrong || bot->IsInCombat())
        return false;

    Sm::Pt const& stand = Sm::SWITCH_STAND[view.switchTrack];
    float const toNpc = sServerFacade->GetDistance2d(bot, npc->GetPositionX(), npc->GetPositionY());
    if (toNpc > SM_SWITCH_CLICK_RANGE)
    {
        MoveTo(bg->GetMapId(), stand.x + frand(-0.6f, 0.6f), stand.y + frand(-0.6f, 0.6f), stand.z);
        return true;
    }

    // At the switch. An expert waits for the last window before the junction (the last click wins).
    if (!Sm::MayClickNow(view.switchAct, view.secondsToJunction) || npc->HasAura(BG_SM_PREVENTION_AURA))
    {
        bot->StopMoving();
        return true;
    }

    uint32 const now = getMSTime();
    {
        std::lock_guard<std::mutex> guard(s_smLock);
        uint32& last = s_lastClickMs[bot->GetGUIDLow()];
        if (last && now - last < SM_SWITCH_CLICK_GAP_MS)
            return true;
        last = now ? now : 1;
    }

    if (bot->IsMounted())
        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
    bot->StopMoving();
    bool const clicked = npc->HandleSpellClick(bot);
    SF_LOG_DEBUG("playerbots", "[BGSM] switch-click %s track=%s act=%u toJunction=%.0f now=%s clicked=%d", bot->GetName().c_str(),
                 view.switchTrack == Sm::TRACK_EAST ? "east" : "north", uint32(view.switchAct), view.secondsToJunction,
                 view.switchOpened ? "opened" : "closed", int(clicked));
    return true;
}
