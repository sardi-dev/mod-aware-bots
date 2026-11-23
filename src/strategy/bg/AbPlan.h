/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ABPLAN_H
#define _PLAYERBOT_ABPLAN_H

// BT-156: the Arathi Basin team plan (docs/bg-sheets/arathi-basin.md) between the pure rules (Utils/AbPlanLogic.h) and the
// game. One record per match holds each team's plan, regime, the job of every player, the node situations, the pending
// call-outs and the human calls. A bot reads it through Decide(); the record is rebuilt from the battleground at most
// every two seconds by whichever bot asks first, on the map thread of the match, so there is no extra scan per tick.
//
// Map 529 (Arathi Basin) uses this, and from BT-552 map 761 (Battle for Gilneas): the plan is generic over the node map
// (Utils/BgNodeMap.h). The file keeps its name. Any other map uses the older code; the older objective code of BGTactics::selectObjective stays the fallback when the plan cannot
// say (Enabled() is the map check).

#include "AbPlanLogic.h"
#include "Define.h"

#include <string>

class Battleground;
class Player;
class PlayerbotAI;

namespace AbPlan
{
    // What a bot walks to and why.
    struct Objective
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        AbLogic::Kind kind = AbLogic::Kind::HOLD_QUIET;
        AbLogic::Job job = AbLogic::Job::MOBILE;
        uint8 node = AbLogic::NODE_NONE;
        // A post: the bot stays when it arrives instead of choosing again.
        bool hold = false;
    };

    // The real Arathi Basin (map 529): the Random Battleground queue keeps its own type id, the map id is reliable.
    bool Enabled(Battleground* bg);

    // The bot's decision: the best option of AbLogic::Options that has a position. False when the plan cannot say (the
    // caller then runs its older code). strategyAlliance and strategyHorde are the teams' drawn plans (ABBotStrategy
    // numbers) from BGTactics::GetBotStrategyForTeam.
    bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, Objective& out);

    // The plan state changed since this bot last chose, and the bot's tier lag has passed (core rule: follow the plan
    // state when it changes, late for casual bots).
    bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde);

    // The bot stands at the post of its objective: it stays (G5). False while the bot is far from it.
    bool AtPost(Battleground* bg, Player* bot);

    // A friend casting the capture next to the bot does not send a bot with a job at that node away (G5).
    bool StaysWhileFriendCasts(Battleground* bg, Player* bot);

    // The capture is started only when the plan allows it: a raider with two defenders awake near the banner, or an
    // expert at a node the flip limiter blocks, does not start (good rule; the click rule itself is checked by the caller).
    bool MayCapture(Battleground* bg, Player* bot, uint8 node);

    // A guard picks up a buff only inside its leash, or when its node is quiet (G5).
    bool MayUseBuff(Battleground* bg, Player* bot, float buffX, float buffY);

    // The enemy player casting the capture within reach of one of our banners that this bot should hit first (G7), 0 for
    // none. Good rule: guards and the nearby bots hit the capper first.
    uint64 CapperGuid(Battleground* bg, Player* bot);

    // The raider's state (G9): a stealth class with a raid target. `lone` is the guid of the one defender near the banner
    // (exactly one: an expert saps or cheap shots it first), 0 for none or several.
    struct Raid
    {
        bool active = false;
        uint8 node = AbLogic::NODE_NONE;
        uint64 lone = 0;
        float distance = 0.0f;
        bool expert = false;
    };
    bool GetRaid(Battleground* bg, Player* bot, Raid& out);

    // Percent points a healer takes off a target's health when ordering heals: the player casting the capture first, and
    // the attacked player next to a banner of the healer's team (G7).
    uint32 HealPriority(Player* healer, Player* target);

    // Said by the bot when its turn comes (after the tier's delay); true when a line went out.
    bool DeliverCalls(PlayerbotAI* botAI);

    // BT-583 (Eye of the Storm, docs/bg-sheets/eye-of-the-storm.md section 6): what the flag layer reads of the plan's record. All of
    // it is the record's own data, rebuilt every two seconds: no scan here. Only for the presence map; false for any other.
    struct EyeView
    {
        bool valid = false;
        uint8 regime = 0;                      // AbLogic::Regime of the bot's team
        bool yields = false;                   // taking the fourth base or panicking: the flag waits
        unsigned ourBases = 0;
        unsigned theirBases = 0;
        bool held[4] = { false, false, false, false };
        int bar[4] = { 50, 50, 50, 50 };
        uint32 friends[4] = { 0, 0, 0, 0 };    // our alive players in the base's circle, the bot included
        uint32 enemies[4] = { 0, 0, 0, 0 };    // enemy players the team has seen in it
        uint32 heldForMs[4] = { 0, 0, 0, 0 };  // since the base's owner last changed
        bool free = false;                     // the bot is not a guard, a raider, a retreating or an opening squad member
        uint32 enemiesStand25 = 0;             // enemy players the team has seen within 25 yd of the centre stand (the pickup cast's reach)
        int oddsStand40 = 0;                   // our alive players minus the enemies seen within 40 yd of the stand
        // The decision layer's answers that still stand (D15 to D17), -1 for none: the rule plays then.
        int flagAction = -1;                   // EyFlag::Action
        int efcAction = -1;                    // EyFlag::EfcAction
        int carrierBase = -1;                  // a base id
    };
    bool GetEyeView(Battleground* bg, Player* bot, EyeView& out);

    // The bot's place among the free bots of its team by distance to a point (0 is the nearest), false when the bot is not free
    // (or not a healer / not an expert when asked). The free bots are the middle group and the reserve.
    bool FreeRank(Battleground* bg, Player* bot, float x, float y, bool healersOnly, bool expertsOnly, uint32& rank);

    // The enemies and the fighters near a flag carrier of the plan's team, from the record's positions (no scan): what the carrier
    // decision needs on a map that has no Warsong Gulch record.
    bool SurroundingsOf(Battleground* bg, Player* carrier, uint32& enemiesNear, uint32& escortsNear);

    // The match is over: drop its record.
    void Forget(uint32 instanceId);
}

#endif
