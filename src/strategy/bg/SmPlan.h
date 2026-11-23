/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_SMPLAN_H
#define _PLAYERBOT_SMPLAN_H

// BT-611: the Silvershard Mines team plan (docs/bg-sheets/silvershard-mines.md) between the pure rules (Utils/SmPlanLogic.h) and the
// game. One record per match holds the three carts (position, keeper, the seconds still to ride, whether the junction is ahead,
// the modelled bar), the two switches, and for each team the roster, the leader, the stance, the main cart and the job of every
// bot. A bot reads it through Decide(); the record is rebuilt from the battleground at most every two seconds by whichever bot
// asks first, on the map thread of the match, so there is no extra scan per tick. Map 727 only.

#include "Define.h"
#include "SmPlanLogic.h"

class Battleground;
class Player;
class PlayerbotAI;

namespace SmPlan
{
    // What a bot does and where.
    enum class Job : uint8
    {
        NONE = 0,
        MAIN,       // the main group at the main cart
        OFF,        // one bot at an off cart
        SWITCH      // holds a track switch for its team
    };

    struct View
    {
        Job job = Job::NONE;
        uint8 cart = SmLogic::CART_NONE;        // the bot's cart (a switch holder keeps the cart it was dealt)
        // Where to walk to meet the cart: the point on its rail the bot reaches when the cart does.
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool canMeet = false;                   // false: no point on the rail can be reached in time
        // The cart now.
        float cartX = 0.0f;
        float cartY = 0.0f;
        float cartZ = 0.0f;
        // The switch duty.
        uint8 switchTrack = SmLogic::TRACK_NONE;
        SmLogic::SwitchAct switchAct = SmLogic::SwitchAct::NONE;
        float secondsToJunction = 0.0f;
        bool switchOpened = false;              // the aura now
        bool switchWrong = false;               // the state is not the team's preferred one
        uint8 stance = 0;                       // BgLeader::Stance
    };

    // Map 727 (Silvershard Mines).
    bool Enabled(Battleground* bg);

    // The bot's job and place. False when the plan cannot say (the caller then plays the generic objectives).
    bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, View& out);

    // The plan state changed since this bot last chose, and the bot's tier lag has passed (core rule: follow the plan state when it
    // changes, late for casual bots).
    bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde);

    // The bot's cart and how far from it (2D) the bot is, for the leash. False when it has none or the cart is not live.
    bool CartDistance(Battleground* bg, Player* bot, uint8& cart, float& yards);

    // Said by the bot when its turn comes (after the tier's delay, the team's limiter allowing it); true when a line went out.
    bool DeliverCalls(PlayerbotAI* botAI);

    // A bot's own start spot while the gates are closed (0 to 2, by its place in the team's roster).
    uint32 StartSpot(Battleground* bg, Player* bot);

    // The match is over: drop its record.
    void Forget(uint32 instanceId);
}

#endif
