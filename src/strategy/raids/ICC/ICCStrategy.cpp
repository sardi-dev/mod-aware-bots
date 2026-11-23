/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCStrategy.h"
#include "ICCMultipliers.h"

void RaidIccStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Never fires; it keeps the shared encounter state current (ICCScripts.cpp). A node needs a handler, so it names
    // the matching no-op action.
    triggers.push_back(new TriggerNode("icc watch", NextAction::array(0, new NextAction("icc watch", ACTION_RAID), nullptr)));

    //Lord Marrogwar
    triggers.push_back(new TriggerNode("icc lm",
        NextAction::array(0, new NextAction("icc lm tank position", ACTION_RAID + 5), new NextAction("icc spike", ACTION_RAID + 3), nullptr)));

    //Lady Deathwhisper
    triggers.push_back(new TriggerNode("icc dark reckoning",
        NextAction::array(0, new NextAction("icc dark reckoning", ACTION_MOVE + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc lady deathwhisper",
        NextAction::array(0, new NextAction("icc ranged position lady deathwhisper", ACTION_MOVE + 2), new NextAction("icc adds lady deathwhisper", ACTION_RAID + 3), new NextAction("icc shade lady deathwhisper", ACTION_RAID + 4), nullptr)));

    //Gunship Battle
    triggers.push_back(new TriggerNode("icc rotting frost giant tank position",
        NextAction::array(0, new NextAction("icc rotting frost giant tank position", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc gunship cannon near",
        NextAction::array(0, new NextAction("icc gunship enter cannon", ACTION_RAID + 6), nullptr)));

    triggers.push_back( new TriggerNode("icc in cannon",
        NextAction::array(0, new NextAction("icc cannon fire", ACTION_RAID+5), nullptr)));

    triggers.push_back(new TriggerNode("icc gunship rocket jump",
        NextAction::array(0, new NextAction("icc gunship rocket jump", ACTION_RAID + 4), nullptr)));

    triggers.push_back(new TriggerNode("icc gunship rocket pack setup",
        NextAction::array(0, new NextAction("icc gunship rocket pack setup", ACTION_RAID + 2), nullptr)));

    //DBS
    triggers.push_back(new TriggerNode("icc dbs",
        NextAction::array(0, new NextAction("icc dbs tank position", ACTION_RAID + 3), new NextAction("icc adds dbs", ACTION_RAID + 5), nullptr)));

    // Boss taunt on Rune of Blood is handled inside icc dbs tank position action

    triggers.push_back(new TriggerNode("icc dogs",
        NextAction::array(0, new NextAction("icc dogs tank position", ACTION_RAID + 3), nullptr)));

    //FESTERGUT
    triggers.push_back(new TriggerNode("icc festergut group position",
        NextAction::array(0, new NextAction("icc festergut group position", ACTION_MOVE + 4), nullptr)));

    triggers.push_back(new TriggerNode("icc festergut spore",
        NextAction::array(0, new NextAction("icc festergut spore", ACTION_MOVE + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc festergut avoid malleable goo",
        NextAction::array(0, new NextAction("icc festergut avoid malleable goo", ACTION_RAID + 7), nullptr)));

    //ROTFACE
    triggers.push_back(new TriggerNode("icc rotface tank position",
        NextAction::array(0, new NextAction("icc rotface tank position", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc rotface group position",
        NextAction::array(0, new NextAction("icc rotface group position", ACTION_RAID + 6), nullptr)));

    triggers.push_back(new TriggerNode("icc rotface move away from explosion",
        NextAction::array(0, new NextAction("icc rotface move away from explosion", ACTION_RAID +7), nullptr)));

    triggers.push_back(new TriggerNode("icc rotface avoid vile gas",
        NextAction::array(0, new NextAction("icc rotface avoid vile gas", ACTION_RAID + 8), nullptr)));

    //PP
    triggers.push_back(new TriggerNode("icc putricide volatile ooze",
        NextAction::array(0, new NextAction("icc putricide volatile ooze", ACTION_RAID + 4), nullptr)));

    triggers.push_back(new TriggerNode("icc putricide gas cloud",
        NextAction::array(0, new NextAction("icc putricide gas cloud", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc putricide growing ooze puddle",
        NextAction::array(0, new NextAction("icc putricide growing ooze puddle", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("icc putricide mutated plague",
        NextAction::array(0, new NextAction("icc putricide mutated plague", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("icc putricide malleable goo",
        NextAction::array(0, new NextAction("icc putricide avoid malleable goo", ACTION_RAID + 6), nullptr)));

    triggers.push_back(new TriggerNode("icc putricide abomination",
        NextAction::array(0, new NextAction("icc putricide abomination", ACTION_RAID + 7), nullptr)));

    //BPC
    triggers.push_back(new TriggerNode("icc bpc keleseth tank",
        NextAction::array(0, new NextAction("icc bpc keleseth tank", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("icc bpc main tank",
        NextAction::array(0, new NextAction("icc bpc main tank", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("icc bpc empowered vortex",
        NextAction::array(0, new NextAction("icc bpc empowered vortex", ACTION_RAID + 4), nullptr)));

    triggers.push_back(new TriggerNode("icc bpc kinetic bomb",
        NextAction::array(0, new NextAction("icc bpc kinetic bomb", ACTION_RAID + 6), nullptr)));

    triggers.push_back(new TriggerNode("icc bpc ball of flame",
                        NextAction::array(0, new NextAction("icc bpc ball of flame", ACTION_RAID + 7), nullptr)));

    //BQL
    triggers.push_back(new TriggerNode("icc bql group position",
        NextAction::array(0, new NextAction("icc bql group position", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("icc bql pact of darkfallen",
        NextAction::array(0, new NextAction("icc bql pact of darkfallen", ACTION_RAID +1), nullptr)));

    triggers.push_back(new TriggerNode("icc bql vampiric bite",
        NextAction::array(0, new NextAction("icc bql vampiric bite", ACTION_EMERGENCY + 5), nullptr)));

    //Sister Svalna
    triggers.push_back(new TriggerNode("icc valkyre spear",
        NextAction::array(0, new NextAction("icc valkyre spear", ACTION_EMERGENCY + 5), nullptr)));
    //VDW
    triggers.push_back(new TriggerNode("icc valithria group",
        NextAction::array(0, new NextAction("icc valithria group", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("icc valithria zombie kite",
        NextAction::array(0, new NextAction("icc valithria zombie kite", ACTION_EMERGENCY + 9), nullptr)));

    triggers.push_back(new TriggerNode("icc valithria portal",
        NextAction::array(0, new NextAction("icc valithria portal", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc valithria heal",
        NextAction::array(0, new NextAction("icc valithria heal", ACTION_RAID+2), nullptr)));

    triggers.push_back(new TriggerNode("icc valithria dream cloud",
        NextAction::array(0, new NextAction("icc valithria dream cloud", ACTION_RAID + 4), nullptr)));

    //SINDRAGOSA
    triggers.push_back(new TriggerNode("icc sindragosa group position",
        NextAction::array(0, new NextAction("icc sindragosa group position", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa frost beacon",
        NextAction::array(0, new NextAction("icc sindragosa frost beacon", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa hot",
        NextAction::array(0, new NextAction("icc sindragosa hot", ACTION_RAID + 6), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa blistering cold",
        NextAction::array(0, new NextAction("icc sindragosa blistering cold", ACTION_EMERGENCY + 4), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa unchained magic",
        NextAction::array(0, new NextAction("icc sindragosa unchained magic", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa chilled to the bone",
        NextAction::array(0, new NextAction("icc sindragosa chilled to the bone", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa mystic buffet",
        NextAction::array(0, new NextAction("icc sindragosa mystic buffet", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("icc sindragosa frost bomb",
        NextAction::array(0, new NextAction("icc sindragosa frost bomb", ACTION_RAID + 7), nullptr)));

    //LICH KING
    triggers.push_back(new TriggerNode("icc lich king shadow trap",
        NextAction::array(0, new NextAction("icc lich king shadow trap", ACTION_RAID + 6), nullptr)));

    triggers.push_back(new TriggerNode("icc lich king necrotic plague",
        NextAction::array(0, new NextAction("icc lich king necrotic plague", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode("icc lich king winter",
        NextAction::array(0, new NextAction("icc lich king winter", ACTION_RAID +5), nullptr)));

    triggers.push_back(new TriggerNode("icc lich king adds",
        NextAction::array(0, new NextAction("icc lich king adds", ACTION_RAID +2), nullptr)));

    triggers.push_back(new TriggerNode("icc lich king spirit bomb",
        NextAction::array(0, new NextAction("icc lich king spirit bomb", ACTION_RAID +7), nullptr)));
}

void RaidIccStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IccLadyDeathwhisperMultiplier(botAI));
    multipliers.push_back(new IccAddsDbsMultiplier(botAI));
    multipliers.push_back(new IccDogsMultiplier(botAI));
    multipliers.push_back(new IccFestergutMultiplier(botAI));
    multipliers.push_back(new IccRotfaceMultiplier(botAI));
    multipliers.push_back(new IccAddsPutricideMultiplier(botAI));
    multipliers.push_back(new IccBpcAssistMultiplier(botAI));
    multipliers.push_back(new IccBqlMultiplier(botAI));
    multipliers.push_back(new IccValithriaDreamCloudMultiplier(botAI));
    multipliers.push_back(new IccSindragosaMultiplier(botAI));
    multipliers.push_back(new IccLichKingAddsMultiplier(botAI));
    multipliers.push_back(new IccLichKingSpiritBombMultiplier(botAI));
    multipliers.push_back(new IccGunshipMultiplier(botAI));
}
