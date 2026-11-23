/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "WarlockTriggers.h"
#include "WarlockActions.h"
#include "BotSkill.h"
#include "DpsRotationMath.h"

// 5.4.8 include-order: DynamicObject.h derives from GridObject<DynamicObject> but only includes
// Object.h, and Map.h does NOT pull GridObject.h either - GridObject.h is reached only via Creature.h
// (Creature.h:28) and GameObject.h. So DynamicObject.h does not compile unless one of those came first
// (C7568/C3770 "argument list missing after assumed function template 'GridObject'"). Matches the core's
// own precedent, e.g. ObjectAccessor.cpp includes Creature.h before DynamicObject.h.
#include "Creature.h"
#include "DynamicObject.h"
#include "Player.h"
#include "Group.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpBurst.h"
#include "PvpBurstPolicy.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "GenericTriggers.h"

// Warlock sheets: use the long cooldown on a boss or a pack of elites.
bool WarlockMajorDemonTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature || !creature->IsAlive() || !bot->HasSpell(18540))
        return false;
    if (creature->isWorldBoss())
        return true;
    if (!creature->isElite())
        return false;
    unsigned elites = 0;
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* enemy = unit ? unit->ToCreature() : nullptr;
        if (enemy && enemy->IsAlive() && enemy->isElite() && enemy->IsWithinDistInMap(target, 8.0f))
            ++elites;
    }
    return elites >= 3;
}

bool WarlockImpSwarmTrigger::IsActive()
{
    if (!bot->HasSpell(104316))
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    return creature && (creature->isWorldBoss() || AoeTrigger(botAI, 3, 8.0f).IsActive());
}

bool ImmolationAuraActiveTrigger::IsActive()
{
    if (!HasAuraTrigger::IsActive())
        return false;
    Group* group = bot->GetGroup();
    if (!group)
        return true;
    // Demonology sheet: a bot tank with the mark-pull strategy leads the pull.
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap() || !PlayerBotSpec::IsTank(member))
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(member);
        if (ai && ai->HasStrategy("mark pull", BOT_STATE_NON_COMBAT))
            return false;
    }
    return true;
}

bool WarlockNoPetTrigger::IsActive()
{
    return !bot->HasAura(108503) && NoPetTrigger::IsActive();
}

bool WarlockSacrificeTrigger::IsActive()
{
    return bot->GetTalentSpecialization() == SPEC_WARLOCK_DESTRUCTION && bot->HasSpell(108503) && bot->GetPet() && !bot->HasAura(108503);
}

bool CurseOfElementsTrigger::IsActive()
{
    return bot->HasSpell(1490) && DebuffTrigger::IsActive() && !botAI->HasAura("curse of the elements", GetTarget()) &&
        !botAI->HasAura("curse of enfeeblement", GetTarget(), false, true);
}

bool CurseOfEnfeeblementTrigger::IsActive()
{
    return DebuffTrigger::IsActive() && !botAI->HasAura("curse of enfeeblement", GetTarget()) &&
        !botAI->HasAura("curse of the elements", GetTarget(), false, true);
}

bool OutOfSoulShardsTrigger::IsActive()
{
    // On 5.4.8 soul shards are a capped power resource, not item 6265 in the bags; the WotLK
    // inventory-count condition has no valid equivalent here and must not manufacture a count.
    return false;
}

bool TooManySoulShardsTrigger::IsActive()
{
    // On 5.4.8 soul shards are a capped power resource, not item 6265 in the bags; the WotLK
    // inventory-count condition has no valid equivalent here and must not manufacture a count.
    return false;
}

bool DemonArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !botAI->HasAura("fel armor", target);
}

bool UnendingBreathTrigger::IsActive()
{
    return BuffTrigger::IsActive() && AI_VALUE2(bool, "swimming", "self target");
}

bool UnendingBreathOnPartyTrigger::IsActive()
{
    return BuffOnPartyTrigger::IsActive() && AI_VALUE2(bool, "swimming", "self target");
}

bool FirestoneTrigger::IsActive()
{
    // Firestone was removed before MoP; there is no weapon-imbue aura or action to maintain.
    return false;
}

bool SpellstoneTrigger::IsActive()
{
    // Spellstone was removed before MoP; there is no weapon-imbue aura or action to maintain.
    return false;
}

bool CorruptionOnAttackerTrigger::IsActive()
{
    return DebuffOnAttackerTrigger::IsActive() && !botAI->HasAura("seed of corruption", GetTarget(), false, true);
}

bool ImmolateOnAttackerTrigger::IsActive()
{
    return DebuffOnAttackerTrigger::IsActive();
}

bool UnstableAfflictionOnAttackerTrigger::IsActive()
{
    return DebuffOnAttackerTrigger::IsActive();
}

bool CurseOfAgonyOnAttackerTrigger::IsActive()
{
    return bot->HasSpell(980) && DebuffOnAttackerTrigger::IsActive();
}

bool CurseOfExhaustionTrigger::IsActive()
{
    return DebuffTrigger::IsActive();
}

bool CurseOfTonguesTrigger::IsActive()
{
    return DebuffTrigger::IsActive();
}

bool CurseOfWeaknessTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    static const uint32 exclusiveDebuffs[] = {
        702, 1108, 6205, 7646, 11707, 11708, 27224, 30909, 50511,
        99, 1735, 9490, 9747, 9898, 26998, 48559, 48560,
        1160, 6190, 11554, 11555, 11556, 25202, 25203, 47437,
        67, 26017
    };
    for (uint32 spellId : exclusiveDebuffs)
        if (target->HasAura(spellId))
            return false;

    return DebuffTrigger::IsActive();
}

bool LifeTapTrigger::IsActive()
{
    if (AI_VALUE2(uint8, "health", "self target") <= sPlayerbotAIConfig->lowHealth ||
        !AI_VALUE2(bool, "has mana", "self target") || AI_VALUE2(uint8, "mana", "self target") >= 85)
        return false;
    // BT-172: between pulls the tap costs health the healer has to give back; only a warlock with room to spare taps.
    // In combat the old low-health rule stays (the tap is a mana emergency there).
    constexpr uint8 LIFE_TAP_MIN_HEALTH_OUT_OF_COMBAT = 60;
    if (!bot->IsInCombat() && AI_VALUE2(uint8, "health", "self target") < LIFE_TAP_MIN_HEALTH_OUT_OF_COMBAT)
        return false;
    return true;
}

bool DemonicEmpowermentTrigger::IsActive()
{
    Pet* pet = bot->GetPet();
    return pet && !botAI->HasAura("demonic empowerment", pet);
}

bool DecimationTrigger::IsActive()
{
    Aura* aura = botAI->GetAura(getName(), GetTarget(), false, true);
    return aura && aura->GetDuration() > 3000;
}

bool WrongPetTrigger::IsActive()
{
    // #521 (AC WarlockTriggers.cpp:178-): fires when exactly one pet strategy is enabled and the active pet is
    // not that strategy's demon. Changes vs the old MoP port: (1) both key sets are accepted - AC's
    // imp/felhunter/felguard and this module's bhealth/bmana/bdps; (2) voidwalker/succubus are covered
    // (AC has them in its pets[] table); (3) the compared summon is the BASE spell (summon imp 688 etc.,
    // learnable) - the Grimoire of Supremacy override summons (Fel Imp 112866, Voidlord 112867,
    // Shivarra 112868, Observer 112869, Wrathguard 112870 - DBC ids via cls.py 2026-10-04) are
    // NOT-LEARNABLE, so "spell id" never resolves them (SpellIdValue.cpp scans the spellbook only) and the
    // trigger could never fire. Their pet entries are accepted as correct too.
    struct PetChoice
    {
        char const* strategy;
        char const* altStrategy;  // this module's spec-slot key, or nullptr
        char const* summonSpell;
        uint32 overrideSpellId;
    };
    static const PetChoice choices[] = {
        {"imp", "bhealth", "summon imp", 112866},
        {"voidwalker", nullptr, "summon voidwalker", 112867},
        {"succubus", nullptr, "summon succubus", 112868},
        {"felhunter", "bmana", "summon felhunter", 112869},
        {"felguard", "bdps", "summon felguard", 112870}
    };

    Player* player = botAI->GetBot();
    Pet* pet = player->GetPet();
    PetChoice const* selected = nullptr;
    uint32 selectedCount = 0;
    for (PetChoice const& choice : choices)
        if (botAI->HasStrategy(choice.strategy, BOT_STATE_NON_COMBAT) ||
            (choice.altStrategy && botAI->HasStrategy(choice.altStrategy, BOT_STATE_NON_COMBAT)))
        {
            selected = &choice;
            ++selectedCount;
        }

    if (selectedCount != 1 || !pet || player->HasAura(108503))
        return false;

    auto summonedEntry = [](SpellInfo const* info) -> uint32
    {
        if (!info)
            return 0;
        for (SpellEffectInfo const& effect : info->Effects)
            if (effect.Effect == SPELL_EFFECT_SUMMON_PET && effect.MiscValue > 0)
                return static_cast<uint32>(effect.MiscValue);
        return 0;
    };

    // The base summon must be learned before resolving its name.
    uint32 required[] = {688, 697, 712, 691, 30146};
    size_t choiceIndex = selected - choices;
    if (!player->HasSpell(required[choiceIndex]))
        return false;
    uint32 summonSpellId = AI_VALUE2(uint32, "spell id", selected->summonSpell);
    uint32 baseEntry = summonedEntry(summonSpellId ? sSpellMgr->GetSpellInfo(summonSpellId) : nullptr);
    if (!baseEntry)
        return false;

    uint32 overrideEntry = summonedEntry(sSpellMgr->GetSpellInfo(selected->overrideSpellId));
    return pet->GetEntry() != baseEntry && (!overrideEntry || pet->GetEntry() != overrideEntry);
}

const std::set<uint32> RainOfFireChannelCheckTrigger::RAIN_OF_FIRE_SPELL_IDS = {
    5740, 6219, 11677, 11678, 27212, 47819, 47820
};

bool RainOfFireChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        if (spell->m_spellInfo && RAIN_OF_FIRE_SPELL_IDS.count(spell->m_spellInfo->Id))
            return AI_VALUE(uint8, "attacker count") < minEnemies;
    return false;
}

bool LifeTapGlyphBuffTrigger::IsActive()
{
    constexpr uint32 SPELL_LIFE_TAP_GLYPH = 63320;
    return bot->HasAura(SPELL_LIFE_TAP_GLYPH) && BuffTrigger::IsActive();
}

bool WarlockConjuredItemTrigger::IsActive()
{
    // 5.4.8: AC also required a spendable "soul shard" inventory item here, but Cataclysm turned Soul
    // Shards into a capped resource (not a bag item) and Create Healthstone/Firestone/Spellstone no
    // longer cost one on this core (see the commented-out "soul shard" item-count check this fork already
    // left in WarlockActions.cpp CastDrainSoulAction::isUseful) -- so only the item-count check applies.
    return ItemCountTrigger::IsActive();
}

bool DarkIntentTrigger::IsActive()
{
    constexpr uint32 SPELL_DARK_INTENT = 109773;
    if (!bot->HasSpell(SPELL_DARK_INTENT))
        return false;
    return !bot->HasAura(SPELL_DARK_INTENT, bot->GetGUID());
}

bool MetamorphosisTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    // BT-620: against an enemy player the pack and boss gate never opened. The guide bursts in Metamorphosis from 300 Fury
    // inside the kill window and keeps the Hand of Gul'dan charges for it, so the PvE rules for the charges, the pack and the
    // 500 Fury floor do not apply; the form is also kept down to a few casts of Fury (the PvE drop at 300 would undo the entry).
    if (PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
    {
        uint32 const fury = bot->GetPower(Powers::POWER_DEMONIC_FURY);
        bool const inForm = botAI->HasAura("metamorphosis", bot);
        if (inForm && PvpBurst::MetamorphosisLeave(fury, true, 0))
            botAI->RemoveAura("metamorphosis");
        return bot->getLevel() >= 36 && bot->HasSpell(103958) &&
               PvpBurst::MetamorphosisEnterVsPlayer(PvpBurstGlue::Window(botAI, bot, target), fury, inForm) && BuffTrigger::IsActive();
    }

    if (botAI->HasAura("metamorphosis", bot) && bot->GetPower(Powers::POWER_DEMONIC_FURY) <= 300)
    {
        botAI->RemoveAura("metamorphosis");
    }
    // #864 ("cast Metamorphosis 103958 ... in combat, demonic fury >= 500"): the
    // fork needed 800 and a 40 s dummy fight never got there (wave 73/74: 0 casts).
    SpellInfo const* hand = sSpellMgr->GetSpellInfo(105174);
    SpellCategoriesEntry const* categories = hand ? hand->GetSpellCategories() : nullptr;
    return bot->getLevel() >= 36 && bot->HasSpell(103958) && categories && categories->ChargesCategory &&
        !bot->HasSpellCharge(categories->ChargesCategory) &&
        (AoeTrigger(botAI, 3, 8.0f).IsActive() || (target && target->ToCreature() && target->ToCreature()->isWorldBoss())) &&
        BuffTrigger::IsActive() && bot->GetPower(Powers::POWER_DEMONIC_FURY) >= 500;
}

bool CorruptionTrigger::IsActive()
{
    if (botAI->HasAura("metamorphosis", bot))
    {
        spell = "doom";
        return DebuffTrigger::IsActive();
    }

    spell = "corruption";
    return DebuffTrigger::IsActive() && !botAI->HasAura("doom", GetTarget(), false, true);
}

bool FireAndBrimeStoneTrigger::IsActive()
{
    bool const pack = AoeTrigger(botAI, 4, 8.0f).IsActive();
    if (botAI->HasAura("fire and brimstone", bot) && (bot->GetPower(Powers::POWER_BURNING_EMBERS) <= 0 || !pack))
    {
        botAI->RemoveAura("fire and brimstone");
    }
    return bot->HasSpell(108683) && pack &&
        BuffTrigger::IsActive() && bot->GetPower(Powers::POWER_BURNING_EMBERS) >= 10;
}

bool SoulstoneTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", "soulstone");
    return spellId && bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

bool WarlockPetLowHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < 40 && !AI_VALUE2(bool, "dead", "pet target") /*&& !AI_VALUE2(bool, "mounted", "self target")*/;
}
bool WarlockPetMediumHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < sPlayerbotAIConfig->mediumHealth /* && !AI_VALUE2(bool, "dead", "pet target") && !AI_VALUE2(bool, "mounted", "self target")*/;
}

// ---- #864 / epic #854 (rotation task 9) ----
bool WarlockHavocTrigger::IsActive()
{
    // Destruction sheet: prepare Havoc only when Chaos Bolt can follow.
    uint32 const embers = bot->GetPower(Powers::POWER_BURNING_EMBERS);
    return bot->HasSpell(80240) && bot->HasSpell(116858) &&
        (embers >= 30 || (embers >= 10 && bot->HasAura(113858))) &&
        !bot->HasSpellCooldown(116858) && WarlockHavocTarget(botAI);
}

bool WarlockSummonAbyssalTrigger::IsActive()
{
    if (!bot->HasSpell(112921))
        return false;
    uint32 spellId = AI_VALUE2(uint32, "spell id", "summon abyssal");
    if (!spellId || !bot->HasSpell(spellId) || bot->HasSpellCooldown(spellId))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->IsAlive();
}

bool WarlockShadowBoltFillerTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return !bot->HasSpell(103103) && target && target->IsAlive() && (rand() % 3) == 0;
}

bool WarlockPvpFearTrigger::IsActive()
{
    if (!AI_VALUE(bool, "in pvp instance"))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || target->GetTypeId() != TypeID::TYPEID_PLAYER || !target->IsAlive())
        return false;

    // BT-159: do not peel the target selected by a group tank.
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (PlayerBotSpec::IsTank(member) &&
                    (member->GetVictim() == target || member->GetTarget() == target->GetGUID()))
                    return false;

    return target->GetVictim() == bot && bot->IsWithinDistInMap(target, 5.0f) &&
           !AI_VALUE2(bool, "is cc", "current target") && !target->HasAuraType(SPELL_AURA_MOD_FEAR);
}

// Rotation fix task K (#868): see WarlockTriggers.h.
bool WarlockFleeCloseTrigger::IsActive()
{
    if (AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger(inner);
    return t && t->IsActive();
}

// Rotation fix task K (#868): see WarlockTriggers.h.
bool WarlockDuelRetargetTrigger::IsActive()
{
    if (!bot->duel || !bot->duel->opponent || bot->duel->startTime == 0)
        return false;

    return bot->duel->opponent->IsAlive() && !AI_VALUE(Unit*, "current target");
}

// BT-159: keep DoTs rolling before spending a global cooldown on a long channel.
bool WarlockDrainLifeHealthTrigger::IsActive()
{
    constexpr float drainLifeHealthPercent = 35.0f;
    return bot->GetHealthPct() < drainLifeHealthPercent;
}

bool WarlockExtraAttackerFearTrigger::IsActive()
{
    // BT-159: the shared CC target supplies cast and existing-CC checks; never
    // disturb a tank's pull, and hold only one extra attacker at a time.
    Group* group = bot->GetGroup();
    if (group)
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (PlayerBotSpec::IsTank(member))
                    return false;
    if (AI_VALUE2(Unit*, "current cc target", "fear"))
        return false;
    Unit* target = AI_VALUE2(Unit*, "cc target", "fear");
    if (!target || target == AI_VALUE(Unit*, "current target") || target->GetVictim() != bot)
        return false;
    unsigned attackers = 0;
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
        if (Unit* unit = botAI->GetUnit(guid))
            if (unit->IsAlive() && unit->GetVictim() == bot)
                ++attackers;
    constexpr unsigned minimumAttackers = 2;
    return attackers >= minimumAttackers;
}

// BT-159: the older marked-CC edge must respect the tank as well.
bool FearTrigger::IsActive()
{
    Unit* target = AI_VALUE2(Unit*, "cc target", "fear");
    Group* group = bot->GetGroup();
    if (group)
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (PlayerBotSpec::IsTank(member) &&
                    ((bot->GetMap() && bot->GetMap()->IsDungeon()) ||
                     (target && (member->GetVictim() == target || member->GetTarget() == target->GetGUID()))))
                    return false;
    return HasCcTargetTrigger::IsActive();
}

bool MoltenCoreTrigger::IsActive()
{
    constexpr uint32 SPELL_MOLTEN_CORE_BUFF = 122355;
    Aura const* core = bot->GetAura(SPELL_MOLTEN_CORE_BUFF);
    return DpsRotationMath::Warlock::SoulFireOnMoltenCore(BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT),
        core ? core->GetStackAmount() : 0, botAI->HasAura("metamorphosis", bot));
}

bool EmberTapTrigger::IsActive()
{
    constexpr uint32 SPELL_EMBER_TAP = 114635;
    constexpr uint32 SPELL_SACRIFICIAL_PACT = 108416;
    constexpr uint32 SPELL_UNENDING_RESOLVE = 104773;
    if (!bot->HasSpell(SPELL_EMBER_TAP))
        return false;
    bool const defensiveUp = bot->HasAura(SPELL_SACRIFICIAL_PACT) || bot->HasAura(SPELL_UNENDING_RESOLVE);
    return DpsRotationMath::Warlock::EmberTapUseful(BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT),
        bot->GetHealthPct(), bot->GetPower(Powers::POWER_BURNING_EMBERS), defensiveUp);
}

// BT-663 to BT-668: see WarlockTriggers.h.
bool PvpAfflictionTrigger::IsActive()
{
    return PvpAfflictionGlue::Due(botAI, bot, rule);
}
