/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TALENTSPELLS_H
#define _PLAYERBOT_TALENTSPELLS_H

#include "GenericSpellActions.h"
#include "Strategy.h"
#include "TalentSpellPolicy.h"
#include "Trigger.h"

#include <string>
#include <utility>
#include <vector>

class PlayerbotAI;

// BT-322 (rules: TalentSpellPolicy.h): one trigger per active talent no class strategy casts. It fires only for a bot
// that has the talent, so a bot with another pick of the tier pays one spell lookup.
class TalentSpellTrigger : public Trigger
{
public:
    TalentSpellTrigger(PlayerbotAI* botAI, TalentSpellPolicy::Talent talent)
        : Trigger(botAI, TalentSpellPolicy::Of(talent).name, 1), talent(talent)
    {
    }

    bool IsActive() override;

private:
    TalentSpellPolicy::Talent const talent;
};

// Casts the talent's spell on the target its rule names (itself, its current target, the member to heal, the main
// tank). Range, cooldown, power and line of sight stay with CastSpellAction.
class TalentSpellAction : public CastSpellAction
{
public:
    TalentSpellAction(PlayerbotAI* botAI, TalentSpellPolicy::Talent talent)
        : CastSpellAction(botAI, TalentSpellPolicy::Of(talent).spell), talent(talent)
    {
    }

    std::string const getName() override { return TalentSpellPolicy::Of(talent).name; }
    std::string const GetTargetName() override;
    ActionThreatType getThreatType() override { return ActionThreatType::None; }

private:
    TalentSpellPolicy::Talent const talent;
};

namespace TalentSpells
{
template <TalentSpellPolicy::Talent T>
Trigger* CreateTrigger(PlayerbotAI* botAI)
{
    return new TalentSpellTrigger(botAI, T);
}

template <TalentSpellPolicy::Talent T>
Action* CreateAction(PlayerbotAI* botAI)
{
    return new TalentSpellAction(botAI, T);
}

// Fills a trigger or action context's creators with one entry per talent, under the talent's name.
template <typename Creators, std::size_t... I>
void RegisterTriggers(Creators& creators, std::index_sequence<I...>)
{
    ((creators[TalentSpellPolicy::TABLE[I].name] = &CreateTrigger<static_cast<TalentSpellPolicy::Talent>(I)>), ...);
}

template <typename Creators, std::size_t... I>
void RegisterActions(Creators& creators, std::index_sequence<I...>)
{
    ((creators[TalentSpellPolicy::TABLE[I].name] = &CreateAction<static_cast<TalentSpellPolicy::Talent>(I)>), ...);
}

template <typename Creators>
void RegisterTriggers(Creators& creators)
{
    RegisterTriggers(creators, std::make_index_sequence<TalentSpellPolicy::TABLE.size()>{});
}

template <typename Creators>
void RegisterActions(Creators& creators)
{
    RegisterActions(creators, std::make_index_sequence<TalentSpellPolicy::TABLE.size()>{});
}
}  // namespace TalentSpells

// Added to every bot's combat engine; InitTriggers registers only the talents of the bot's class.
class TalentSpellsStrategy : public Strategy
{
public:
    TalentSpellsStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "talent spells"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
