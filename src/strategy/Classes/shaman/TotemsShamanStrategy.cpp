/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "TotemsShamanStrategy.h"

#include "Playerbots.h"

TotemsShamanStrategy::TotemsShamanStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}

void TotemsShamanStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);

    // #382: the unified strategy's "no air totem -> wrath of air totem" edge is removed. Wrath of Air Totem (and
    // Windfury, Mana Spring, Strength of Earth below) were removed in 5.0.4 (cls.py shaman: NOT-LEARNABLE in
    // 5.4.8); the only air totems left are the utility ones (Grounding, Capacitor, Stormlash, Windwalk), none of
    // which is a standing buff totem, so nothing replaces it (use the "grounding" strategy to keep Grounding up).
    // Water slot: Mana Spring is gone, Healing Stream (5394) is the remaining baseline water totem.
    triggers.push_back(
        new TriggerNode("no water totem", NextAction::array(0, new NextAction("healing stream totem", 7.0f), nullptr)));

    // AC anchor: "no fire totem" trigger. Not previously wired up in this generic totem strategy (the
    // combat-spec strategies already have their own spec-appropriate "no fire totem" consumers).
    triggers.push_back(
        new TriggerNode("no fire totem", NextAction::array(0, new NextAction("searing totem", 7.0f),
                                                           new NextAction("magma totem", 6.0f), nullptr)));

    // #382: the "strength of earth totem" edge is removed - Strength of Earth Totem is NOT-LEARNABLE in 5.4.8
    // (removed 5.0.4) and the earth slot has no same-role successor (Wowhead mop-classic lists only Earthbind,
    // Tremor, Stone Bulwark, Earth Elemental there).
}


// #431 (AC parity, AC Strategy/TotemsShamanStrategy.cpp:27-183): per-totem combat strategies. Each one puts the
// totem on "set <totem>" (relevance 60) and casts it when its element slot is empty (relevance 55). AC keeps the
// totem bar for the first; MoP 5.4.8 has no totem bar (MAX_ACTION_BUTTONS = 132, Player.h:~414, so AC's bar slots
// 132-135 do not exist), so the "set" action recasts the totem into the element's summon slot instead
// (ShamanActions.cpp, SetTotemAction).
// Totem of Wrath, Cleansing, Wrath of Air and Windfury are learned late (or, on MoP, may not exist in the spellbook);
// until the bot knows them the strategy assigns the AC fallback totem so the trigger cannot fire continuously.

EarthTotemStrategy::EarthTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void EarthTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set tremor totem", NextAction::array(0, new NextAction("set tremor totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no earth totem", NextAction::array(0, new NextAction("tremor totem", 55.0f), nullptr)));
}

EarthbindTotemStrategy::EarthbindTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void EarthbindTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set earthbind totem", NextAction::array(0, new NextAction("set earthbind totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no earth totem", NextAction::array(0, new NextAction("earthbind totem", 55.0f), nullptr)));
}


// Fire Totems
SearingTotemStrategy::SearingTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void SearingTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set searing totem", NextAction::array(0, new NextAction("set searing totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no fire totem", NextAction::array(0, new NextAction("searing totem", 55.0f), nullptr)));
}

MagmaTotemStrategy::MagmaTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void MagmaTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set magma totem", NextAction::array(0, new NextAction("set magma totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no fire totem", NextAction::array(0, new NextAction("magma totem", 55.0f), nullptr)));
}


// Water Totems
HealingStreamTotemStrategy::HealingStreamTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void HealingStreamTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set healing stream totem", NextAction::array(0, new NextAction("set healing stream totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no water totem", NextAction::array(0, new NextAction("healing stream totem", 55.0f), nullptr)));
}


GroundingTotemStrategy::GroundingTotemStrategy(PlayerbotAI* botAI) : GenericShamanStrategy(botAI) {}
void GroundingTotemStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode("set grounding totem", NextAction::array(0, new NextAction("set grounding totem", 60.0f), nullptr)));
    triggers.push_back(new TriggerNode("no air totem", NextAction::array(0, new NextAction("grounding totem", 55.0f), nullptr)));
}
