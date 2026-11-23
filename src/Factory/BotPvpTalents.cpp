/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotPvpTalents.h"

#include "BotFactory.h"
#include "BotSalePolicy.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Spell.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <sstream>
#include <vector>

using namespace PvpTalentSetPolicy;

static_assert(PlayerbotAIConfig::PREMADE_SPEC_NO_CHOICE == PvpTalentSetPolicy::NO_CHOICE,
              "the config and the policy must agree on 'no preference'");

namespace
{
// The bot's class talents by tier and column, and the column it has in each tier of the active group.
struct TalentState
{
    TalentEntry const* grid[TALENT_TIERS][TALENT_COLUMNS] = {};
    std::vector<uint8> current;
    uint32 unlocked = 0;
};

TalentState ReadTalents(Player* bot)
{
    TalentState state;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(i);
        if (talent && talent->playerClass == bot->getClass() && talent->Row < TALENT_TIERS && talent->Col < TALENT_COLUMNS)
            state.grid[talent->Row][talent->Col] = talent;
    }

    uint8 const activeSpec = bot->GetActiveSpec();
    state.current.assign(TALENT_TIERS, NO_TALENT);
    for (uint32 tier = 0; tier < TALENT_TIERS; ++tier)
        for (uint32 column = 0; column < TALENT_COLUMNS; ++column)
            if (TalentEntry const* talent = state.grid[tier][column])
                if (bot->HasTalent(talent->SpellId, activeSpec))
                    state.current[tier] = uint8(column);

    state.unlocked = std::min<uint32>(bot->CalculateTalentsPoints(), TALENT_TIERS);
    return state;
}

std::vector<GlyphSlot> ReadGlyphSlots(Player* bot)
{
    uint32 const enabled = bot->GetUInt32Value(PLAYER_FIELD_GLYPH_SLOTS_ENABLED);
    uint8 const activeSpec = bot->GetActiveSpec();
    std::vector<GlyphSlot> slots;
    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        GlyphSlotEntry const* gs = sGlyphSlotStore.LookupEntry(bot->GetGlyphSlot(slot));
        GlyphSlot s;
        s.type = gs ? gs->TypeFlags : 0xFFFFFFFF;
        s.enabled = gs && (enabled & (1 << slot)) && bot->getLevel() >= 25;
        s.glyph = bot->GetGlyph(activeSpec, slot);
        slots.push_back(s);
    }
    return slots;
}

// A configured glyph list as the planner wants it, without the glyphs this bot cannot use yet.
struct ResolvedGlyphs
{
    std::vector<WantedGlyph> wanted;
    std::vector<uint32> applySpells;  // in step with `wanted`
};

ResolvedGlyphs ResolveGlyphs(Player* bot, std::vector<uint32> const& items)
{
    ResolvedGlyphs out;
    for (uint32 itemId : items)
    {
        BotFactory::GlyphItem info;
        if (!BotFactory::ResolveGlyph(itemId, bot->getClass(), info))
            continue;
        if (info.requiredLevel > bot->getLevel() ||
            (info.allowableRace != -1 && !(info.allowableRace & int32(bot->getRaceMask()))))
            continue;
        out.wanted.push_back({ info.glyphId, info.typeFlags });
        out.applySpells.push_back(info.applySpell);
    }
    return out;
}

// The PvP set belongs to the primary group's specialization: the secondary group mirrors it.
uint32 PvpIndexOf(Player* bot)
{
    return BotFactory::PremadeSpecIndexOf(bot, PVE_GROUP);
}

bool HasPvpSet(Player* bot, uint32 index)
{
    uint8 const cls = bot->getClass();
    return index < MAX_SPECIALIZATIONS - 1 && cls < MAX_CLASSES &&
           (!sPlayerbotAIConfig->premadeSpecLinkPvp[cls][index].empty() ||
            !sPlayerbotAIConfig->premadeSpecGlyphPvp[cls][index].empty());
}

bool IsCandidate(Player* bot)
{
    return BotPvpTalents::Enabled() && sRandomPlayerbotMgr->IsRandomBot(bot);
}

Situation SituationOf(Player* bot)
{
    Situation s;
    s.enabled = IsCandidate(bot);
    s.hasPvpSet = HasPvpSet(bot, PvpIndexOf(bot));
    s.hasDualSpec = bot->GetSpecsCount() > 1;
    s.activeGroup = bot->GetActiveSpec();
    s.inPvpMatch = bot->InBattleground() && bot->GetBattleground();
    s.inArena = s.inPvpMatch && bot->InArena();
    s.queuedForPvp = bot->InBattlegroundQueue();
    s.preparation = bot->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PREPARATION);
    s.inCombat = bot->IsInCombat();
    s.alive = bot->IsAlive();
    Map* map = bot->GetMap();
    s.pveOpportunity = !bot->IsInFlight() && (bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ||
                                              (map && map->IsDungeon() && !s.inPvpMatch));
    return s;
}

// Everything Evaluate and Apply share.
struct Plan
{
    Situation situation;
    PvpGroupState group;
    Step step = Step::NONE;
    uint32 index = MAX_SPECIALIZATIONS;
    TalentState talents;
    ResolvedGlyphs glyphs;
    std::vector<GlyphPut> puts;
};

Plan MakePlan(Player* bot)
{
    Plan plan;
    plan.situation = SituationOf(bot);
    if (!plan.situation.enabled || !plan.situation.hasPvpSet || !plan.situation.hasDualSpec)
        return plan;

    plan.index = PvpIndexOf(bot);
    if (plan.situation.activeGroup == PVP_GROUP)
    {
        uint32 const pvpSpec = bot->GetTalentSpecialization(PVP_GROUP);
        plan.group.specMissing = pvpSpec == 0;
        plan.group.specDiffers = pvpSpec != 0 && pvpSpec != bot->GetTalentSpecialization(PVE_GROUP);
        plan.talents = ReadTalents(bot);
        plan.group.tiers =
            PlanTiers(plan.talents.current, sPlayerbotAIConfig->premadeSpecLinkPvp[bot->getClass()][plan.index],
                      plan.talents.unlocked);
        plan.glyphs = ResolveGlyphs(bot, sPlayerbotAIConfig->premadeSpecGlyphPvp[bot->getClass()][plan.index]);
        plan.puts = PlanGlyphs(ReadGlyphSlots(bot), plan.glyphs.wanted);
        plan.group.glyphsDiffer = !plan.puts.empty();
    }

    plan.step = Decide(plan.situation, plan.group);
    return plan;
}

// Spell::EffectApplyGlyph without the spell: the old glyph's aura goes, the new glyph is stored and its aura cast.
// (Player::SetGlyph alone only stores the id; the aura would wait for the next login, Player::_LoadGlyphAuras.)
void PutGlyph(Player* bot, uint8 slot, uint32 glyphId, uint32 applySpell)
{
    // A player knows a glyph once it used the item; the bot learns the item's spell the same way InitGlyphs does.
    if (applySpell && !bot->HasSpell(applySpell))
        bot->learnSpell(applySpell, false);

    BotFactory::PutGlyph(bot, slot, glyphId);
}

// The Activate Spec cast a player starts from the talent pane: stand still, cast, wait for it.
bool StartActivation(PlayerbotAI* botAI, Player* bot, uint32 spellId)
{
    if (bot->IsNonMeleeSpellCasted(false))
        return false;
    if (bot->isMoving())
        bot->StopMoving();
    if (!botAI->CastSpell(spellId, bot))
        return false;
    botAI->WaitForSpellCast(bot->GetCurrentSpell(CURRENT_GENERIC_SPELL));
    SF_LOG_INFO("playerbots", "[PVPSPEC] bot=%s class=%u level=%u activates its %s group", bot->GetName().c_str(),
                uint32(bot->getClass()), bot->getLevel(), spellId == SPELL_ACTIVATE_SECONDARY_SPEC ? "pvp" : "pve");
    return true;
}

// Puts the PvP set into the active secondary group: its specialization first (the primary group's, free the first
// time), then the empty tiers, the swapped tiers (only in the preparation phase, when taking a talent out is free),
// and the glyphs.
bool SetUpPvpGroup(Player* bot, Plan& plan)
{
    std::ostringstream log;
    if (plan.group.specDiffers)
    {
        // The primary group changed role since (the finder's role switch, a randomize). The secondary group follows,
        // with the same no-cost reset BotFactory::SwitchSpecialization uses for the primary group.
        bot->ResetTalents(true, true, true);
        log << " spec-reset";
    }
    if (plan.group.specMissing || plan.group.specDiffers)
    {
        uint32 const tab = BotFactory::SpecTabOf(bot, PVE_GROUP);
        if (tab >= MAX_SPECIALIZATIONS - 1)
            return false;
        WorldPacket p(CMSG_SET_PRIMARY_TALENT_TREE);
        p << tab;
        bot->GetSession()->HandeSetTalentSpecialization(p);
        log << " spec=" << bot->GetTalentSpecialization(PVP_GROUP);
        // The talents and glyphs come with the next check, read from the new state.
        SF_LOG_INFO("playerbots", "[PVPSPEC] bot=%s sets up its pvp group:%s", bot->GetName().c_str(), log.str().c_str());
        return true;
    }

    std::vector<uint16> const& link = sPlayerbotAIConfig->premadeSpecLinkPvp[bot->getClass()][plan.index];
    // LearnTalent gates on PLAYER_FIELD_MAX_TALENT_TIERS, as in BotFactory::InitTalentTiers.
    if (bot->GetUInt32Value(PLAYER_FIELD_MAX_TALENT_TIERS) < plan.talents.unlocked)
        bot->SetUInt32Value(PLAYER_FIELD_MAX_TALENT_TIERS, bot->CalculateTalentsPoints());

    if (plan.situation.preparation)
        for (uint8 tier : plan.group.tiers.replace)
        {
            TalentEntry const* old = plan.talents.grid[tier][plan.talents.current[tier]];
            TalentEntry const* pick = plan.talents.grid[tier][WantedColumn(link, tier)];
            if (!old || !pick)
                continue;
            // Player::RemoveTalent refuses a talent whose spell is cooling down, as the client does; the tier waits.
            if (!bot->RemoveTalent(old->TalentID))
            {
                log << " t" << uint32(tier) << "cooldown";
                continue;
            }
            bool const ok = bot->LearnTalent(uint16(pick->TalentID));
            log << " t" << uint32(tier) << "=" << pick->SpellId << (ok ? "" : "!");
        }

    for (uint8 tier : plan.group.tiers.learn)
        if (TalentEntry const* pick = plan.talents.grid[tier][WantedColumn(link, tier)])
        {
            bool const ok = bot->LearnTalent(uint16(pick->TalentID));
            log << " t" << uint32(tier) << "+" << pick->SpellId << (ok ? "" : "!");
        }

    for (GlyphPut const& put : plan.puts)
    {
        uint32 applySpell = 0;
        for (std::size_t i = 0; i < plan.glyphs.wanted.size(); ++i)
            if (plan.glyphs.wanted[i].glyph == put.glyph)
                applySpell = plan.glyphs.applySpells[i];
        PutGlyph(bot, put.slot, put.glyph, applySpell);
        log << " g" << uint32(put.slot) << "=" << put.glyph;
    }

    bot->SendTalentsInfoData();
    SF_LOG_INFO("playerbots", "[PVPSPEC] bot=%s class=%u level=%u sets up its pvp group:%s", bot->GetName().c_str(),
                uint32(bot->getClass()), bot->getLevel(), log.str().c_str());
    return !log.str().empty();
}
}

bool BotPvpTalents::Enabled()
{
    return sPlayerbotAIConfig->pvpTalentSets;
}

Step BotPvpTalents::Evaluate(Player* bot)
{
    return MakePlan(bot).step;
}

bool BotPvpTalents::Apply(PlayerbotAI* botAI, Player* bot)
{
    Plan plan = MakePlan(bot);
    switch (plan.step)
    {
        case Step::ACTIVATE_PVP:
            return StartActivation(botAI, bot, SPELL_ACTIVATE_SECONDARY_SPEC);
        case Step::ACTIVATE_PVE:
            return StartActivation(botAI, bot, SPELL_ACTIVATE_PRIMARY_SPEC);
        case Step::SET_UP_PVP:
            return SetUpPvpGroup(bot, plan);
        default:
            return false;
    }
}

std::vector<uint16> const* BotPvpTalents::PvpLinkOfActiveGroup(Player* bot, uint32 premadeIndex)
{
    if (bot->GetActiveSpec() != PVP_GROUP || !IsCandidate(bot) || premadeIndex >= MAX_SPECIALIZATIONS - 1 ||
        bot->getClass() >= MAX_CLASSES)
        return nullptr;
    std::vector<uint16> const& link = sPlayerbotAIConfig->premadeSpecLinkPvp[bot->getClass()][premadeIndex];
    return link.empty() ? nullptr : &link;
}

std::vector<uint32> const* BotPvpTalents::PvpGlyphsOfActiveGroup(Player* bot, uint32 premadeIndex)
{
    if (bot->GetActiveSpec() != PVP_GROUP || !IsCandidate(bot) || premadeIndex >= MAX_SPECIALIZATIONS - 1 ||
        bot->getClass() >= MAX_CLASSES)
        return nullptr;
    std::vector<uint32> const& glyphs = sPlayerbotAIConfig->premadeSpecGlyphPvp[bot->getClass()][premadeIndex];
    return glyphs.empty() ? nullptr : &glyphs;
}

bool BotPvpTalents::WantsDualSpec(Player* bot)
{
    DualSpecOffer offer;
    offer.enabled = IsCandidate(bot);
    if (!offer.enabled)
        return false;
    offer.hasPvpSet = HasPvpSet(bot, PvpIndexOf(bot));
    offer.playsPvp = sPlayerbotAIConfig->randomBotJoinBG;
    offer.specsCount = bot->GetSpecsCount();
    offer.level = bot->getLevel();
    offer.minLevel = sWorld->getIntConfig(WorldIntConfigs::CONFIG_MIN_DUALSPEC_LEVEL);
    offer.money = bot->GetMoney();
    offer.reserve = BotSalePolicy::PurchaseReserve(bot->getLevel());
    return PvpTalentSetPolicy::WantsDualSpec(offer);
}

bool BotPvpTalents::BuyDualSpec(Player* bot, Creature* trainer)
{
    if (!trainer || !WantsDualSpec(bot) || !trainer->isCanTrainingAndResetTalentsOf(bot) || bot->IsInCombat())
        return false;

    // Player::OnGossipSelect, GOSSIP_OPTION_LEARNDUALSPEC: the two teaching spells, cast on the player as the
    // trainer's option does, and the option's price.
    bot->ModifyMoney(-int64(DUAL_SPEC_COST));
    bot->CastSpell(bot, SPELL_TEACH_SPEC_SWITCHES, true, nullptr, nullptr, bot->GetGUID());
    bot->CastSpell(bot, SPELL_LEARN_SECOND_SPEC, true, nullptr, nullptr, bot->GetGUID());
    SF_LOG_INFO("playerbots", "[PVPSPEC] bot=%s level=%u bought dual specialization from %s (specs=%u)",
                bot->GetName().c_str(), bot->getLevel(), trainer->GetName().c_str(), uint32(bot->GetSpecsCount()));
    return true;
}
