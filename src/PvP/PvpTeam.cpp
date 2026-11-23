/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpTeam.h for what this is.
 */

#include "PvpTeam.h"

#include "Battleground.h"
#include "BotSkill.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAfflictionPolicy.h"
#include "PvpAwareness.h"
#include "PvpConfig.h"
#include "PvpMatchState.h"
#include "PvpTeamPolicy.h"
#include "PvpUnitFacts.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PvpTeamGlue
{
namespace
{
using PvpSpells::DrCategory;

constexpr float CLUSTER_YARDS = 40.0f;       // the enemy healer and the partners count within this of the kill target
constexpr float PARTNER_YARDS = 45.0f;
constexpr float SPLASH_YARDS = 8.0f;          // the reach of the area controls of the kit (Shadowfury)
constexpr std::uint32_t PICK_CACHE_MS = 150;
constexpr float HEALER_FREE_HP_PERCENT = 65.0f;
constexpr std::size_t PICK_MAX = 128;
constexpr std::uint32_t LOG_GAP_MS = 4000;

// The big offensive cooldowns a partner shows as an aura on itself (ids checked against the module's own class sheets): Berserk,
// Incarnation, Arcane Power, Alter Time, Pillar of Frost, Metamorphosis, the three Dark Souls, Rapid Fire, Bestial Wrath, Stampede,
// Combustion, Icy Veins, Avenging Wrath, Adrenaline Rush, Shadow Blades, Vendetta, Recklessness, Bloodlust, Heroism, Time Warp,
// Ascendance, Bladestorm, Avatar, Power Infusion, Summon Gargoyle, Tigereye Brew.
constexpr uint32 BURST_AURAS[] = { 106951, 102543, 12042, 108978, 51271, 103958, 113858, 113860, 113861, 3045, 19574, 121818, 11129,
    12472, 31884, 13750, 121471, 79140, 1719, 2825, 32182, 80353, 114049, 46924, 107574, 10060, 49206, 116740 };

// The control kit by class: spells that hold the enemy healer (hard control or a silence), with the diminishing returns category
// they use and whether a hit breaks them. The bot casts a spell it knows; the core's own cast check (range, sight, resource,
// facing, a spell that needs a frozen target) decides whether it can go now. Ground and totem controls (traps, Solar Beam,
// Capacitor Totem) need a placement, and the controls that strike everyone around the caster (Psychic Scream, Leg Sweep, Howl of
// Terror) would wake the team's own sheep, so none of them is in the kit; Ring of Frost is the one ground link (armMs).
struct KitSpell
{
    uint8 cls;
    uint32 spellId;
    DrCategory category;
    bool breakable;
    bool hard;   // holds the target (against a silence, which only stops the healing)
    std::uint32_t armMs;   // a ground control: lands this long after the cast (0: at once)
    bool splash;           // strikes everyone within SPLASH_YARDS of the target: not cast beside an enemy that a hit would free
};
constexpr KitSpell KIT[] = {
    { CLASS_MAGE, 118, DrCategory::INCAPACITATE, true, true, 0, false },          // Polymorph
    { CLASS_MAGE, 44572, DrCategory::STUN, false, true, 0, false },               // Deep Freeze
    { CLASS_MAGE, 102051, DrCategory::SILENCE, false, false, 0, false },          // Frostjaw
    // BT-650: Ring of Frost under a healer that a stun already holds (it arms 2 s after the cast and shares the incapacitate
    // category with Polymorph). A ground control, so it is an armed chain link of an expert only (PvpTeam::ScoreControl).
    { CLASS_MAGE, 113724, DrCategory::INCAPACITATE, false, false, 2000, false },  // Ring of Frost
    { CLASS_ROGUE, 2094, DrCategory::FEAR, true, true, 0, false },                // Blind
    { CLASS_ROGUE, 408, DrCategory::STUN, false, true, 0, false },                // Kidney Shot
    { CLASS_ROGUE, 1776, DrCategory::INCAPACITATE, true, true, 0, false },        // Gouge
    { CLASS_PRIEST, 88625, DrCategory::DISORIENT, true, true, 0, false },         // Holy Word: Chastise
    { CLASS_PRIEST, 64044, DrCategory::HORROR, false, true, 0, false },           // Psychic Horror
    { CLASS_PRIEST, 15487, DrCategory::SILENCE, false, false, 0, false },         // Silence
    { CLASS_HUNTER, 19503, DrCategory::DISORIENT, true, true, 0, false },         // Scatter Shot
    { CLASS_HUNTER, 19386, DrCategory::INCAPACITATE, true, true, 0, false },      // Wyvern Sting
    { CLASS_DRUID, 33786, DrCategory::CYCLONE, false, true, 0, false },           // Cyclone
    { CLASS_DRUID, 5211, DrCategory::STUN, false, true, 0, false },               // Mighty Bash
    // BT-663: Shadowfury, the area stun of the ladder's tier 30 pick, cast under the healer; Fear follows as the next link.
    { CLASS_WARLOCK, 30283, DrCategory::STUN, false, true, 0, true },              // Shadowfury
    { CLASS_WARLOCK, 5782, DrCategory::FEAR, true, true, 0, false },              // Fear
    { CLASS_WARLOCK, 6789, DrCategory::HORROR, false, true, 0, false },           // Mortal Coil
    { CLASS_PALADIN, 853, DrCategory::STUN, false, true, 0, false },              // Hammer of Justice
    { CLASS_PALADIN, 20066, DrCategory::INCAPACITATE, true, true, 0, false },     // Repentance
    { CLASS_SHAMAN, 51514, DrCategory::INCAPACITATE, true, true, 0, false },      // Hex
    { CLASS_MONK, 115078, DrCategory::INCAPACITATE, true, true, 0, false },       // Paralysis
    { CLASS_DEATH_KNIGHT, 108194, DrCategory::STUN, false, true, 0, false },      // Asphyxiate
    { CLASS_DEATH_KNIGHT, 47476, DrCategory::SILENCE, false, false, 0, false },   // Strangulate
    { CLASS_WARRIOR, 107570, DrCategory::STUN, false, true, 0, false },           // Storm Bolt
    { CLASS_WARRIOR, 5246, DrCategory::FEAR, true, true, 0, false },              // Intimidating Shout
};

struct Pick
{
    bool valid = false;
    bool executor = false;
    uint64 target = 0;
    uint32 spellId = 0;
    DrCategory category = DrCategory::NONE;
    bool hard = true;
    std::uint32_t castMs = 0;
    std::int32_t score = 0;
    std::uint32_t atMs = 0;
    std::uint32_t lastLogMs = 0;
};

std::mutex g_lock;
std::unordered_map<std::uint32_t, Pick> g_picks;

struct Ctx
{
    Battleground* bg = nullptr;
    PvpMatch::Handle match;
    PvpTeam::TeamState* team = nullptr;
    BotSkillMath::Tier tier = BotSkillMath::Tier::GOOD;
    std::uint32_t now = 0;
};

// A bot of a battleground or an arena in progress; the team table is the one of its side.
bool Gather(Player* bot, Ctx& c)
{
    if (!PvpConfig::TeamPlayEnabled() || !bot || !bot->IsInWorld() || !bot->IsAlive() || !bot->InBattleground())
        return false;
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS || bg->GetStartDelayTime() > 0)
        return false;

    c.bg = bg;
    c.now = getMSTime();
    c.tier = BotSkill::TierOf(bot);
    c.match = PvpMatch::Touch(bg->GetInstanceID(), bg->GetMapId(), bg->GetElapsedTime(), false, c.now);
    if (!c.match)
        return false;
    c.team = &c.match->Side(Battleground::GetTeamIndexByTeamId(bot->GetBGTeam())).team;
    return true;
}

struct Lock
{
    std::uint32_t ms = 0;
    DrCategory category = DrCategory::NONE;
    bool breakable = false;
};

// The longest hard control or silence on the unit, read off its auras (public state: a unit frame shows the same).
Lock ReadLock(Unit* unit)
{
    Lock best;
    for (auto const& entry : unit->GetAppliedAuras())
    {
        AuraApplication const* app = entry.second;
        Aura const* aura = app ? app->GetBase() : nullptr;
        SpellInfo const* info = aura ? aura->GetSpellInfo() : nullptr;
        if (!info || app->IsPositive())
            continue;
        std::uint32_t const mechanics = info->GetAllEffectsMechanicMask();
        if (!(PvpSpells::CcKindsOfMechanics(mechanics) & (PvpSpells::CC_HARD | PvpSpells::CC_SILENCE)))
            continue;
        int32 const left = aura->GetDuration();
        if (left <= 0 || std::uint32_t(left) <= best.ms)
            continue;
        best.ms = std::uint32_t(left);
        best.category = PvpSpells::DrCategoryFor(info->Id, mechanics);
        best.breakable = (mechanics & PvpUnitFacts::BREAKS_ON_DAMAGE_MECHANICS) != 0;
    }
    return best;
}

// How long the longest breakable control on the unit still lasts.
std::uint32_t BreakableLeftMs(Unit* unit)
{
    std::uint32_t best = 0;
    for (auto const& entry : unit->GetAppliedAuras())
    {
        AuraApplication const* app = entry.second;
        Aura const* aura = app ? app->GetBase() : nullptr;
        SpellInfo const* info = aura ? aura->GetSpellInfo() : nullptr;
        if (!info || app->IsPositive() || !(info->GetAllEffectsMechanicMask() & PvpUnitFacts::BREAKS_ON_DAMAGE_MECHANICS))
            continue;
        if (aura->GetDuration() > 0)
            best = std::max(best, std::uint32_t(aura->GetDuration()));
    }
    return best;
}

// The enemy players the bot can see within `radius` of `center`.
std::vector<Player*> EnemiesNear(PlayerbotAI* botAI, Player* bot, Unit* center, float radius)
{
    std::vector<Player*> out;
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (enemy && enemy->IsAlive() && bot->IsValidAttackTarget(enemy) && bot->CanSeeOrDetect(enemy) &&
            (!center || enemy->GetExactDist(center) <= radius))
            out.push_back(enemy);
    }
    return out;
}

// The enemy healer the control goes to: the nearest healer of the kit's reach that the bot sees; ties to the lower guid.
Player* PickHealer(Player* bot, std::vector<Player*> const& enemies)
{
    Player* best = nullptr;
    for (Player* enemy : enemies)
    {
        if (!PlayerBotSpec::IsHeal(enemy, true))
            continue;
        if (!best || bot->GetExactDist(enemy) < bot->GetExactDist(best) ||
            (bot->GetExactDist(enemy) == bot->GetExactDist(best) && enemy->GetGUIDLow() < best->GetGUIDLow()))
            best = enemy;
    }
    return best;
}

// How many enemies within `radius` of `center` are held in a breakable control (the area controls would free them).
std::uint32_t BreakableHeldNear(std::vector<Player*> const& enemies, Unit* center, float radius)
{
    std::uint32_t count = 0;
    for (Player* enemy : enemies)
        if (enemy != center && enemy->GetExactDist(center) <= radius && PvpUnitFacts::HasBreakableCc(enemy))
            ++count;
    return count;
}

bool PartnerBurstOn(Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            member->GetExactDist(target) > PARTNER_YARDS)
            continue;
        for (uint32 spell : BURST_AURAS)
        {
            Aura const* aura = member->GetAura(spell);
            if (!aura || aura->GetMaxDuration() <= 0 || aura->GetDuration() < 0)
                continue;
            std::uint32_t const age = std::uint32_t(aura->GetMaxDuration() - aura->GetDuration());
            if (PvpTeam::PartnerBurstCounts(age))
                return true;
        }
    }
    return false;
}

bool IsHardCategory(DrCategory category)
{
    return category != DrCategory::SILENCE && category != DrCategory::ROOT && category != DrCategory::DISARM && category != DrCategory::NONE;
}

// The own book's reading of a stopper the enemy team could use against a cast (an expert weighs instants).
bool EnemyKickReady(Player* bot, std::vector<Player*> const& enemies, std::uint32_t now)
{
    std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
    for (Player* enemy : enemies)
    {
        if (!PvpAware::Book::HasStopper(enemy->getClass()))
            continue;
        if (!book || book->KickReading(enemy->GetGUID(), enemy->getClass(), now).state != PvpAware::State::DOWN)
            return true;
    }
    return false;
}

// A healer controls only while nobody it should heal is hurt: the cast would take the global cooldown of a heal.
bool TeamNeedsHealing(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetMap() == bot->GetMap() && member->GetHealthPct() < HEALER_FREE_HP_PERCENT)
            return true;
    }
    return false;
}

Pick ComputePick(PlayerbotAI* botAI, Player* bot, Ctx const& c)
{
    Pick pick;
    if (c.tier == BotSkillMath::Tier::CASUAL)
        return pick;
    if (PlayerBotSpec::IsHeal(bot, true) && TeamNeedsHealing(bot))
        return pick;

    std::vector<Player*> const enemies = EnemiesNear(botAI, bot, nullptr, 60.0f);
    Player* target = PickHealer(bot, enemies);
    if (!target)
        return pick;   // an off damage dealer is the arena plan's business once the plan names one (play v3)

    std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
    Lock const lock = ReadLock(target);
    PvpTeam::ControlTarget state;
    state.lockedMs = lock.ms;
    state.lockCategory = lock.category;
    state.enemyKickReady = c.tier == BotSkillMath::Tier::EXPERT && EnemyKickReady(bot, enemies, c.now);

    bool const immune = PvpUnitFacts::IsImmune(target);
    if (immune)
        return pick;

    std::int32_t bestScore = PvpTeam::NOT_DUE;
    KitSpell const* best = nullptr;
    std::uint32_t bestCastMs = 0;
    for (KitSpell const& row : KIT)
    {
        if (row.cls != bot->getClass() || !bot->HasSpell(row.spellId))
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(row.spellId);
        if (!info || !botAI->CanCastSpell(row.spellId, target, false))
            continue;
        if (c.team->control.Blocked(bot->GetGUIDLow(), target->GetGUIDLow(), row.category, row.hard, c.now))
            continue;
        // BT-663: an area control is not cast beside an enemy that a teammate holds in a breakable control (the hit frees it).
        if (row.splash && !PvpAffliction::SplashAllowed(BreakableHeldNear(enemies, target, SPLASH_YARDS)))
            continue;

        PvpTeam::ControlCandidate cand;
        cand.member = bot->GetGUIDLow();
        cand.spellId = row.spellId;
        cand.category = row.category;
        cand.baseMs = info->GetDuration() > 0 ? std::uint32_t(info->GetDuration()) : 0;
        cand.castMs = info->CalcCastTime(bot->getLevel()) > 0 ? std::uint32_t(info->CalcCastTime(bot->getLevel())) : 0;
        cand.breakable = row.breakable;
        cand.armMs = row.armMs;
        cand.drStep = book ? book->DrStep(target->GetGUID(), row.category, c.now) : 0;

        std::int32_t const score = PvpTeam::ScoreControl(cand, state, c.tier);
        if (score > bestScore || (score == bestScore && best && row.spellId < best->spellId))
        {
            bestScore = score;
            best = &row;
            bestCastMs = cand.castMs;
        }
    }

    if (!best || bestScore < 0)
    {
        c.team->control.Withdraw(bot->GetGUIDLow());
        return pick;
    }

    pick.valid = true;
    pick.target = target->GetGUID();
    pick.spellId = best->spellId;
    pick.category = best->category;
    pick.hard = best->hard;
    pick.castMs = bestCastMs;
    pick.score = bestScore;
    pick.atMs = c.now;
    PvpTeam::Offer offer;
    offer.member = bot->GetGUIDLow();
    offer.targetLow = target->GetGUIDLow();
    offer.spellId = best->spellId;
    offer.category = best->category;
    offer.score = bestScore;
    offer.atMs = c.now;
    c.team->control.Publish(offer, c.now);
    pick.executor = c.team->control.IsExecutor(bot->GetGUIDLow(), target->GetGUIDLow(), c.now);
    return pick;
}

Pick PickFor(PlayerbotAI* botAI, Player* bot)
{
    Ctx c;
    if (!Gather(bot, c))
        return {};
    {
        std::lock_guard<std::mutex> guard(g_lock);
        auto const it = g_picks.find(bot->GetGUIDLow());
        if (it != g_picks.end() && it->second.atMs != 0 && getMSTimeDiff(it->second.atMs, c.now) < PICK_CACHE_MS)
            return it->second;
    }
    Pick pick = ComputePick(botAI, bot, c);
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_picks.size() > PICK_MAX)
        g_picks.clear();
    Pick& stored = g_picks[bot->GetGUIDLow()];
    pick.lastLogMs = stored.lastLogMs;
    if (pick.valid && pick.executor && getMSTimeDiff(stored.lastLogMs, c.now) > LOG_GAP_MS)
    {
        pick.lastLogMs = c.now;
        SF_LOG_DEBUG("playerbots", "[PVPTEAM] bot=%u tier=%u kind=CONTROL spell=%u target=%u score=%d", bot->GetGUIDLow(),
            uint32(c.tier), pick.spellId, uint32(GUID_LOPART(pick.target)), pick.score);
    }
    stored = pick;
    return pick;
}
}  // namespace

std::uint32_t LockedMs(Unit* unit)
{
    return unit ? ReadLock(unit).ms : 0;
}

bool HasBurstAura(Unit* unit)
{
    if (!unit)
        return false;
    for (uint32 spell : BURST_AURAS)
        if (unit->HasAura(spell))
            return true;
    return false;
}

bool EnemyStopperNotSpent(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;
    return EnemyKickReady(bot, EnemiesNear(botAI, bot, nullptr, 40.0f), getMSTime());
}

HealerFacts HealerNear(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    HealerFacts facts;
    if (!target || !bot)
        return facts;
    Player* healer = PickHealer(bot, EnemiesNear(botAI, bot, target, CLUSTER_YARDS));
    if (!healer)
        return facts;
    facts.present = true;
    facts.isTarget = healer == target;
    facts.lockedMs = ReadLock(healer).ms;
    return facts;
}

BurstFacts BurstFactsFor(PlayerbotAI* botAI, Player* bot, Player* target)
{
    BurstFacts facts;
    Ctx c;
    if (!target || !Gather(bot, c))
        return facts;

    // The enemy healer near the kill target (a battleground has many enemies; the healer of this fight is the one within reach).
    std::vector<Player*> const enemies = EnemiesNear(botAI, bot, target, CLUSTER_YARDS);
    Player* healer = PickHealer(bot, enemies);

    PvpTeam::WindowInputs in;
    in.healerPresent = healer != nullptr;
    in.targetImmune = PvpUnitFacts::IsImmune(target);
    if (healer)
    {
        in.healerLockedMs = ReadLock(healer).ms;
        if (std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot))
            in.healerTrinketDown = book->TrinketReading(healer->GetGUID(), c.now).state == PvpAware::State::DOWN;
    }

    PvpTeam::WindowVerdict const verdict = PvpTeam::EvaluateWindow(in, c.tier);
    if (verdict.open && PvpTeam::OpenGo(c.team->go, target->GetGUIDLow(), c.now, verdict.reason))
        SF_LOG_DEBUG("playerbots", "[PVPTEAM] bot=%u tier=%u kind=GO target=%u why=%s locked=%u", bot->GetGUIDLow(), uint32(c.tier),
            target->GetGUIDLow(), PvpTeam::WindowReasonName(verdict.reason), in.healerLockedMs);

    // Casual bots do not read the shared go (the policy returns closed for them), whoever opened it.
    facts.teamWindow = c.tier != BotSkillMath::Tier::CASUAL && PvpTeam::GoActive(c.team->go, target->GetGUIDLow(), c.now);
    facts.partnerBurst = c.tier != BotSkillMath::Tier::CASUAL && PartnerBurstOn(bot, target);
    return facts;
}

bool GoOpenOn(Player* bot, Unit* enemy)
{
    Ctx c;
    return enemy && Gather(bot, c) && PvpTeam::GoActive(c.team->go, enemy->GetGUIDLow(), c.now);
}

bool ControlDue(PlayerbotAI* botAI, Player* bot)
{
    Pick const pick = PickFor(botAI, bot);
    return pick.valid && pick.executor;
}

bool ControlPickIs(PlayerbotAI* botAI, Player* bot, uint32 spellId)
{
    Pick const pick = PickFor(botAI, bot);
    return pick.valid && pick.executor && pick.spellId == spellId;
}

bool ControlCast(PlayerbotAI* botAI, Player* bot)
{
    Pick const pick = PickFor(botAI, bot);
    if (!pick.valid || !pick.executor)
        return false;
    Player* target = ObjectAccessor::FindPlayer(pick.target);
    if (!target || !target->IsAlive() || !botAI->CastSpell(pick.spellId, target))
        return false;

    Ctx c;
    if (Gather(bot, c))
        c.team->control.NoteCast(bot->GetGUIDLow(), target->GetGUIDLow(), pick.category, pick.hard, c.now, pick.castMs);
    std::lock_guard<std::mutex> guard(g_lock);
    g_picks.erase(bot->GetGUIDLow());
    return true;
}

bool HoldsDamage(PlayerbotAI* botAI, Player* bot, Action* action)
{
    if (!PvpConfig::TeamPlayEnabled() || !bot->InBattleground())
        return false;
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast)
        return false;
    Unit* target = cast->GetTarget();
    Player* enemy = target ? target->ToPlayer() : nullptr;
    if (!enemy || !bot->IsValidAttackTarget(enemy) || !PvpUnitFacts::HasBreakableCc(enemy))
        return false;

    std::uint32_t const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", cast->getSpell())->Get();
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || PvpSpells::Find(spellId))
        return false;   // a control or a defensive of the catalog is not a damage cast

    bool damages = false;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS && !damages; ++i)
    {
        SpellEffectInfo const& effect = info->Effects[i];
        damages = effect.Effect == SPELL_EFFECT_SCHOOL_DAMAGE || effect.Effect == SPELL_EFFECT_WEAPON_DAMAGE ||
            effect.Effect == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL || effect.Effect == SPELL_EFFECT_NORMALIZED_WEAPON_DMG ||
            effect.Effect == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE ||
            (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE);
    }
    if (!damages)
        return false;

    std::uint32_t const left = BreakableLeftMs(enemy);
    return PvpTeam::HoldDamage(true, left, true, BotSkill::TierOf(bot));
}

void Forget(uint32 guidLow)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_picks.erase(guidLow);
}
}  // namespace PvpTeamGlue
