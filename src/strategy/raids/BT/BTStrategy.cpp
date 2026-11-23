/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BTStrategy.h"
#include "BTMultipliers.h"

void RaidBlackTempleStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // General
    triggers.push_back(new TriggerNode("black temple bot is not in combat",
        NextAction::array(0, new NextAction("black temple erase timers and trackers", ACTION_EMERGENCY + 11), nullptr)));

    // High Warlord Naj'entus
    triggers.push_back(new TriggerNode("high warlord naj'entus pulling boss",
        NextAction::array(0, new NextAction("high warlord naj'entus misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("high warlord naj'entus boss engaged by tanks",
        NextAction::array(0, new NextAction("high warlord naj'entus tanks position boss", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("high warlord naj'entus casts needle spines",
        NextAction::array(0, new NextAction("high warlord naj'entus disperse ranged", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("high warlord naj'entus player is impaled",
        NextAction::array(0, new NextAction("high warlord naj'entus remove impaling spine", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("high warlord naj'entus boss has tidal shield",
        NextAction::array(0, new NextAction("high warlord naj'entus throw impaling spine", ACTION_RAID + 2), nullptr)));

    // Supremus
    triggers.push_back(new TriggerNode("supremus pulling boss or changing phase",
        NextAction::array(0, new NextAction("supremus misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("supremus boss engaged by ranged",
        NextAction::array(0, new NextAction("supremus disperse ranged", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("supremus boss is fixated on bot",
        NextAction::array(0, new NextAction("supremus kite boss", ACTION_EMERGENCY + 7), nullptr)));

    triggers.push_back(new TriggerNode("supremus volcano is nearby",
        NextAction::array(0, new NextAction("supremus move away from volcanos", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("supremus need to manage phase timer",
        NextAction::array(0, new NextAction("supremus manage phase timer", ACTION_EMERGENCY + 10), nullptr)));

    // Shade of Akama
    triggers.push_back(new TriggerNode("shade of akama killing channelers starts phase 2",
        NextAction::array(0, new NextAction("shade of akama melee dps prioritize channelers", ACTION_RAID + 1), nullptr)));

    // Teron Gorefiend
    triggers.push_back(new TriggerNode("teron gorefiend pulling boss",
        NextAction::array(0, new NextAction("teron gorefiend misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("teron gorefiend boss engaged by tanks",
        NextAction::array(0, new NextAction("teron gorefiend tanks position boss", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("teron gorefiend boss engaged by ranged",
        NextAction::array(0, new NextAction("teron gorefiend position ranged on balcony", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("teron gorefiend boss is casting shadow of death",
        NextAction::array(0, new NextAction("teron gorefiend avoid shadow of death", ACTION_EMERGENCY + 10), nullptr)));

    triggers.push_back(new TriggerNode("teron gorefiend bot has shadow of death",
        NextAction::array(0, new NextAction("teron gorefiend move to corner to die", ACTION_EMERGENCY + 10), nullptr)));

    triggers.push_back(new TriggerNode("teron gorefiend bot transformed into vengeful spirit",
        NextAction::array(0, new NextAction("teron gorefiend control and destroy shadowy constructs", ACTION_EMERGENCY + 10), nullptr)));

    // Gurtogg Bloodboil
    triggers.push_back(new TriggerNode("gurtogg bloodboil pulling boss",
        NextAction::array(0, new NextAction("gurtogg bloodboil misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("gurtogg bloodboil boss engaged by tanks",
        NextAction::array(0, new NextAction("gurtogg bloodboil tanks position boss", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("gurtogg bloodboil boss casts bloodboil",
        NextAction::array(0, new NextAction("gurtogg bloodboil rotate ranged groups", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("gurtogg bloodboil bot has fel rage",
        NextAction::array(0, new NextAction("gurtogg bloodboil ranged move away from enraged player", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("gurtogg bloodboil need to manage phase timer",
        NextAction::array(0, new NextAction("gurtogg bloodboil manage phase timer", ACTION_EMERGENCY + 10), nullptr)));

    // Reliquary of Souls
    triggers.push_back(new TriggerNode("reliquary of souls aggro resets upon phase change",
        NextAction::array(0, new NextAction("reliquary of souls misdirect boss to main tank", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("reliquary of souls essence of suffering fixates on closest target",
        NextAction::array(0, new NextAction("reliquary of souls adjust distance from suffering", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("reliquary of souls essence of suffering disables healing",
        NextAction::array(0, new NextAction("reliquary of souls healers dps suffering", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("reliquary of souls essence of desire has rune shield",
        NextAction::array(0, new NextAction("reliquary of souls spellsteal rune shield", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("reliquary of souls essence of desire casting deaden",
        NextAction::array(0, new NextAction("reliquary of souls spell reflect deaden", ACTION_EMERGENCY + 6), nullptr)));

    // Mother Shahraz
    triggers.push_back(new TriggerNode("mother shahraz pulling boss",
        NextAction::array(0, new NextAction("mother shahraz misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("mother shahraz boss engaged by tanks",
        NextAction::array(0, new NextAction("mother shahraz tanks position boss under pillar", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("mother shahraz tanks are positioning boss",
        NextAction::array(0, new NextAction("mother shahraz melee dps wait at safe position", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("mother shahraz sinister beam knocks back players",
        NextAction::array(0, new NextAction("mother shahraz position ranged under pillar", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("mother shahraz bots are linked by fatal attraction",
        NextAction::array(0, new NextAction("mother shahraz run away to break fatal attraction", ACTION_EMERGENCY + 10), nullptr)));

    // Illidari Council
    triggers.push_back(new TriggerNode("illidari council pulling bosses",
        NextAction::array(0, new NextAction("illidari council misdirect bosses to tanks", ACTION_RAID + 4), nullptr)));

    triggers.push_back(new TriggerNode("illidari council gathios engaged by main tank",
        NextAction::array(0, new NextAction("illidari council main tank position gathios", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council gathios casting judgement of command",
        NextAction::array(0, new NextAction("illidari council main tank reflect judgement of command", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council malande engaged by first assist tank",
        NextAction::array(0, new NextAction("illidari council first assist tank focus malande", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council darkshadow engaged by second assist tank",
        NextAction::array(0, new NextAction("illidari council second assist tank position darkshadow", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council zerevor engaged by mage tank",
        NextAction::array(0, new NextAction("illidari council mage tank position zerevor", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("illidari council mage tank needs dedicated healer",
        NextAction::array(0, new NextAction("illidari council position mage tank healer", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council zerevor casts dangerous aoes",
        NextAction::array(0, new NextAction("illidari council disperse ranged", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("illidari council pets screw up the pull",
        NextAction::array(0, new NextAction("illidari council command pets to attack gathios", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("illidari council determining dps assignments",
        NextAction::array(0, new NextAction("illidari council assign dps targets", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidari council need to manage dps timer",
        NextAction::array(0, new NextAction("illidari council manage dps timer", ACTION_EMERGENCY + 10), nullptr)));

    // Illidan Stormrage <The Betrayer>
    triggers.push_back(new TriggerNode("illidan stormrage tank needs aggro",
        NextAction::array(0, new NextAction("illidan stormrage misdirect to tank", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss casts flame crash in front of main tank",
        NextAction::array(0, new NextAction("illidan stormrage main tank reposition boss", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage bot has parasitic shadowfiend",
        NextAction::array(0, new NextAction("illidan stormrage isolate bot with parasite", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage parasitic shadowfiends run wild",
        NextAction::array(0, new NextAction("illidan stormrage set earthbind totem", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss summoned flames of azzinoth",
        NextAction::array(0, new NextAction("illidan stormrage assist tanks handle flames of azzinoth", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage pets die to fire",
        NextAction::array(0, new NextAction("illidan stormrage control pet aggression", ACTION_RAID + 4), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage grate is safe from flames",
        NextAction::array(0, new NextAction("illidan stormrage position above grate", ACTION_EMERGENCY + 2), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage bot struck by dark barrage",
        NextAction::array(0, new NextAction("illidan stormrage remove dark barrage", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss is preparing to land",
        NextAction::array(0, new NextAction("illidan stormrage move away from landing point", ACTION_EMERGENCY + 3), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss deals splash damage",
        NextAction::array(0, new NextAction("illidan stormrage disperse ranged", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage this expansion hates melee",
        NextAction::array(0, new NextAction("illidan stormrage melee go somewhere to not die", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss transforms into demon",
        NextAction::array(0, new NextAction("illidan stormrage warlock tank handle demon boss", ACTION_EMERGENCY + 9), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage boss spawns adds",
        NextAction::array(0, new NextAction("illidan stormrage dps prioritize adds", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage maiev placed shadow trap",
        NextAction::array(0, new NextAction("illidan stormrage use shadow trap", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("illidan stormrage need to manage dps timer and rti",
        NextAction::array(0, new NextAction("illidan stormrage manage dps timer and rti", ACTION_EMERGENCY + 11), nullptr)));
}

void RaidBlackTempleStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // General
    multipliers.push_back(new BlackTempleDelayDpsCooldownsMultiplier(botAI));

    // High Warlord Naj'entus
    multipliers.push_back(new HighWarlordNajentusDisableCombatFormationMoveMultiplier(botAI));

    // Supremus
    multipliers.push_back(new SupremusFocusOnAvoidanceInPhase2Multiplier(botAI));
    multipliers.push_back(new SupremusHitboxIsBuggedMultiplier(botAI));

    // Teron Gorefiend
    multipliers.push_back(new TeronGorefiendControlMovementMultiplier(botAI));
    multipliers.push_back(new TeronGorefiendMarkedBotOnlyMoveToDieMultiplier(botAI));
    multipliers.push_back(new TeronGorefiendSpiritsAttackOnlyShadowyConstructsMultiplier(botAI));
    multipliers.push_back(new TeronGorefiendDisableAttackingConstructsMultiplier(botAI));

    // Gurtogg Bloodboil
    multipliers.push_back(new GurtoggBloodboilControlMovementMultiplier(botAI));

    // Reliquary of Souls
    multipliers.push_back(new ReliquaryOfSoulsDontWasteHealingMultiplier(botAI));

    // Mother Shahraz
    multipliers.push_back(new MotherShahrazControlMovementMultiplier(botAI));
    multipliers.push_back(new MotherShahrazBotsWithFatalAttractionOnlyRunAwayMultiplier(botAI));

    // Illidari Council
    multipliers.push_back(new IllidariCouncilDisableTankActionsMultiplier(botAI));
    multipliers.push_back(new IllidariCouncilControlMovementMultiplier(botAI));
    multipliers.push_back(new IllidariCouncilControlMisdirectionMultiplier(botAI));
    multipliers.push_back(new IllidariCouncilDisableArcaneShotOnZerevorMultiplier(botAI));
    multipliers.push_back(new IllidariCouncilDisableIceBlockMultiplier(botAI));
    multipliers.push_back(new IllidariCouncilWaitForDpsMultiplier(botAI));

    // Illidan Stormrage <The Betrayer>
    multipliers.push_back(new IllidanStormrageDelayDpsCooldownsMultiplier(botAI));
    multipliers.push_back(new IllidanStormrageControlTankActionsMultiplier(botAI));
    multipliers.push_back(new IllidanStormrageDisableDefaultTargetingMultiplier(botAI));
    multipliers.push_back(new IllidanStormrageControlNonTankMovementMultiplier(botAI));
    multipliers.push_back(new IllidanStormrageUseEarthbindTotemMultiplier(botAI));
    multipliers.push_back(new IllidanStormrageWaitForDpsMultiplier(botAI));
}
