#include "AutoMaintenanceOnLevelupAction.h"

#include "BotFactory.h"
#include "BroadcastHelper.h"
#include "Config.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <sstream>

namespace
{
// #533: AC AutoMaintenanceOnLevelupAction::FormatSpell (Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp:148-159).
// 5.4.8 SpellInfo has one SpellName string and no Rank[] array, so the rank suffix is dropped.
std::string FormatSpell(SpellInfo const* sInfo)
{
    std::ostringstream out;
    out << "|cffffffff|Hspell:" << sInfo->Id << "|h[" << (sInfo->SpellName ? sInfo->SpellName : "") << "]|h|r";
    return out.str();
}

// #533: the trainer spell and pet sweep remains controlled by AutoLearnTrainerSpells.
void LearnTrainerSpells(Player* bot)
{
    BotFactory factory(bot, bot->getLevel());
    factory.InitClassSpells();
    factory.InitPet();
}

// #533: AC AutoMaintenanceOnLevelupAction::LearnQuestSpells (AC :87-146). 5.4.8 quest API: AC
// GetRequiredClasses -> GetAllowableClasses, AC GetRewSpellCast (the spell the quest casts) -> GetRewSpell,
// AC GetRewSpell (the displayed spell) -> GetRewDisplaySpell (Player::RewardQuest casts/uses them the same
// way, Player.cpp:16780-16783).
void LearnQuestSpells(Player* bot, std::ostringstream* out)
{
    ObjectMgr::QuestMap const& questTemplates = sObjectMgr->GetQuestTemplates();
    for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i)
    {
        Quest const* quest = i->second;

        if (!quest->GetRequiredClasses() || quest->IsRepeatable() || quest->GetMinLevel() < 10 ||
            quest->GetMinLevel() > int32(bot->getLevel()))
            continue;

        if (!bot->SatisfyQuestClass(quest, false) || !bot->SatisfyQuestRace(quest, false) ||
            !bot->SatisfyQuestSkill(quest, false))
            continue;

        int32 spellId = quest->GetRewSpell();
        if (!spellId)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        bool found = false;
        for (uint8 e = 0; e < MAX_SPELL_EFFECTS; ++e)
        {
            if (spellInfo->Effects[e].Effect == SPELL_EFFECT_LEARN_SPELL && spellInfo->Effects[e].TriggerSpell &&
                !bot->HasSpell(spellInfo->Effects[e].TriggerSpell))
            {
                if (SpellInfo const* triggeredInfo = sSpellMgr->GetSpellInfo(spellInfo->Effects[e].TriggerSpell))
                    if (triggeredInfo->Effects[0].Effect == SPELL_EFFECT_TRADE_SKILL)
                        break;

                found = true;
                break;
            }
        }

        if (!found)
            continue;

        bot->CastSpell(bot, spellId, true);

        // PORT: SkyFire does not expose LoA's reward display spell; use its reward spell for the message.
        uint32 rewSpellId = quest->GetRewSpell();
        if (rewSpellId)
        {
            if (SpellInfo const* rewSpellInfo = sSpellMgr->GetSpellInfo(rewSpellId))
            {
                *out << FormatSpell(rewSpellInfo) << ", ";
                continue;
            }
        }

        *out << FormatSpell(spellInfo) << ", ";
    }
}

// AC PlayerbotAIConfig.cpp reads AiPlayerbot.AutoLearnQuestSpells; this fork's PlayerbotAIConfig.h has no
// field for it, so it is read here (AC default 0).
bool AutoLearnQuestSpells()
{
    static bool const value = sConfigMgr->GetBoolDefault("AiPlayerbot.AutoLearnQuestSpells", false);
    return value;
}
}  // namespace

// #533 (AC AutoMaintenanceOnLevelupAction.cpp:15-23): AC's order - talents, spells, teleport, gear.
bool AutoMaintenanceOnLevelupAction::Execute(Event /*event*/)
{
    AutoPickTalents();

    // AC AutoLearnSpell (AC :50-66) / LearnSpells (AC :68-76), which starts with
    // BroadcastHelper::BroadcastLevelup (AC :70, #322).
    {
        BroadcastHelper::BroadcastLevelup(botAI, bot);
        std::ostringstream out;
        // BT-197: fishing and enchanting still reach value = cap = 5 x level on a level-up; these readings show
        // whether the core, the skill caps below or the trainer spells write it. Remove once the writer is fixed.
        uint16 const before[2] = {bot->GetPureSkillValue(SKILL_FISHING), bot->GetPureSkillValue(SKILL_ENCHANTING)};
        // BT-148, design decision 0016/0017: raise profession caps even when automatic spell learning is disabled.
        if (sRandomPlayerbotMgr->IsRandomBot(bot))
            BotFactory(bot, bot->getLevel()).InitSkills();
        uint16 const afterCaps[2] = {bot->GetPureSkillValue(SKILL_FISHING), bot->GetPureSkillValue(SKILL_ENCHANTING)};
        if (sPlayerbotAIConfig->autoLearnTrainerSpells && sRandomPlayerbotMgr->IsRandomBot(bot))
            LearnTrainerSpells(bot);
        uint16 const afterTrainer[2] = {bot->GetPureSkillValue(SKILL_FISHING), bot->GetPureSkillValue(SKILL_ENCHANTING)};
        if (afterCaps[0] != before[0] || afterCaps[1] != before[1] || afterTrainer[0] != afterCaps[0] ||
            afterTrainer[1] != afterCaps[1])
            SF_LOG_INFO("playerbots", "[SKILL] %s level %u fishing %u/%u/%u enchanting %u/%u/%u (on entry, after caps, after trainer)",
                        bot->GetName().c_str(), uint32(bot->getLevel()), before[0], afterCaps[0], afterTrainer[0], before[1],
                        afterCaps[1], afterTrainer[1]);

        if (AutoLearnQuestSpells() && sRandomPlayerbotMgr->IsRandomBot(bot))
            LearnQuestSpells(bot, &out);

        if (!out.str().empty())
        {
            std::string const temp = out.str();
            out.seekp(0);
            out << "Learned spells: ";
            out << temp;
            out.seekp(-2, out.cur);
            out << ".";
            botAI->TellMaster(out);
        }
    }

    AutoTeleportForLevel();
    AutoUpgradeEquip();
    return true;
}

// #219 / #533 (AC AutoMaintenanceOnLevelupAction.cpp:25-35): restored. AC's HasGameClientMaster() is this
// fork's HasRealPlayerMaster().
void AutoMaintenanceOnLevelupAction::AutoTeleportForLevel()
{
    if (!sPlayerbotAIConfig->autoTeleportForLevel || !sRandomPlayerbotMgr->IsRandomBot(bot))
        return;

    if (botAI->HasRealPlayerMaster())
        return;

    sRandomPlayerbotMgr->RandomTeleportForLevel(bot);
}

void AutoMaintenanceOnLevelupAction::AutoPickTalents()
{
    // Alt bots pick their spec at level 10, before they have a talent point, and their talents as they unlock
    // (design decision 0020).
    if (!sRandomPlayerbotMgr->IsRandomBot(bot))
    {
        if (BotFactory::AltBotsPickTalents())
            BotFactory(bot, bot->getLevel()).InitAltSpecAndTalents();
        return;
    }

    // #533 (AC AutoMaintenanceOnLevelupAction.cpp:37-48): gated by AiPlayerbot.AutoPickTalents as on AC.
    if (!sPlayerbotAIConfig->autoPickTalents)
        return;

    if ((bot->CalculateTalentsPoints() - bot->GetUsedTalentCount()) <= 0)
        return;

    // 5.4.8 talent tiers: BotFactory::InitTalentsTree replaces AC's InitTalentsTree(true, true, true) +
    // InitPetTalents (no 5.4.8 pet talent trees).
    BotFactory factory(bot, bot->getLevel());
    factory.InitTalentsTree(false);
}

void AutoMaintenanceOnLevelupAction::AutoUpgradeEquip()
{
    // BT-148, design decision 0016/0017: equip owned upgrades only; do not create supplies or gear.
    if (sRandomPlayerbotMgr->IsRandomBot(bot) && sPlayerbotAIConfig->autoUpgradeEquip)
        botAI->DoSpecificAction("equip upgrades packet action");
}
