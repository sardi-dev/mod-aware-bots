/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "Playerbots.h"
#include "ScriptMgr.h"

#include <atomic>
#include <ctime>

// BT-202: the time a bot last took damage, from the core's own damage hook (UnitScript::OnDamage, called by
// Unit::DealDamage before absorbs, so a hit that was absorbed or healed back at once still counts). The rejoin walk
// (RejoinGroup.cpp) drops a stale combat state only when no damage came for 30 s; the health between two samples cannot
// tell, because a hit that was healed back before the next reading leaves no trace.
// The same hook adds up the damage a bot and its pet deal, for the [DPSFIGHT] line (DpsMeter.cpp). The amount is the hit
// before absorbs and overkill, the same for every specialisation, so it compares them and does not match a combat log.
namespace
{
class BotDamageWatchScript : public UnitScript
{
public:
    BotDamageWatchScript() : UnitScript("BotDamageWatchScript") {}

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!damage || !victim || attacker == victim)
            return;

        // Cheap tests first: this runs for every damage event of the server.
        Player* player = victim->ToPlayer();
        if (player && player->GetSession() && player->GetSession()->IsBot())
            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(player))
                botAI->lastDamageTakenAt.store(time(nullptr), std::memory_order_relaxed);

        if (attacker)
            CountDealt(attacker, damage);
    }

private:
    // A bot's own hit, or a hit of a pet or guardian it owns. A creature with no player owner stops at one compare.
    static void CountDealt(Unit* attacker, uint32 damage)
    {
        Player* dealer = attacker->ToPlayer();
        bool fromPet = false;
        if (!dealer)
        {
            uint64 const ownerGuid = attacker->GetOwnerGUID();
            if (!ownerGuid || !IS_PLAYER_GUID(ownerGuid))
                return;
            dealer = ObjectAccessor::GetPlayer(*attacker, ownerGuid);
            fromPet = true;
        }
        if (!dealer || !dealer->GetSession() || !dealer->GetSession()->IsBot())
            return;

        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(dealer))
            (fromPet ? botAI->petDamageTotal : botAI->damageDealtTotal).fetch_add(damage, std::memory_order_relaxed);
    }
};
}  // namespace

void AddSC_bot_damage_watch()
{
    new BotDamageWatchScript();
}
