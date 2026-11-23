/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef PLAYERBOTS_TOCTRIGGERS_H
#define PLAYERBOTS_TOCTRIGGERS_H

#include <vector>

#include "ToCLogic.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;
class Unit;

// Trial of the Crusader, written from the SkyFire 5.4.8 scripts (src/server/scripts/Northrend/CrusadersColiseum/
// TrialOfTheCrusader/); mod-playerbots has no strategy for this raid. Every id was checked against Spell.dbc,
// creature_template and the scripts. Kept in a namespace because the other raid headers share one translation unit
// (RaidContexts.cpp) and name the same creatures.
namespace ToC
{
uint32 constexpr MAP_ID = 649;

// Northrend Beasts (boss_northrend_beasts.cpp)
uint32 constexpr NPC_GORMOK = 34796;
uint32 constexpr NPC_DREADSCALE = 34799;
uint32 constexpr NPC_ACIDMAW = 35144;
uint32 constexpr NPC_ICEHOWL = 34797;
uint32 constexpr NPC_SNOBOLD_VASSAL = 34800;

// Lord Jaraxxus (boss_lord_jaraxxus.cpp)
uint32 constexpr NPC_JARAXXUS = 34780;
uint32 constexpr NPC_INFERNAL_VOLCANO = 34813;
uint32 constexpr NPC_FELFLAME_INFERNAL = 34815;
uint32 constexpr NPC_NETHER_PORTAL = 34825;
uint32 constexpr NPC_MISTRESS_OF_PAIN = 34826;
uint32 constexpr SPELL_LEGION_FLAME = 66197;
uint32 constexpr SPELL_INCINERATE_FLESH = 66237;

// Faction Champions (boss_faction_champions.cpp): a champion that is immune to damage is skipped until it is not
uint32 constexpr SPELL_DIVINE_SHIELD = 66010;
uint32 constexpr SPELL_ICE_BLOCK = 65802;
uint32 constexpr SPELL_DETERRENCE = 65871;

// Twin Val'kyr (boss_twin_valkyr.cpp). The essences are clickable creatures (npc_spellclick_spells 34567 -> 65684 and
// 34568 -> 65686); the orbs are Concentrated Darkness / Light.
uint32 constexpr NPC_DARKBANE = 34496;  // Eydis, dark
uint32 constexpr NPC_LIGHTBANE = 34497;  // Fjola, light
uint32 constexpr NPC_DARK_ESSENCE = 34567;
uint32 constexpr NPC_LIGHT_ESSENCE = 34568;
uint32 constexpr NPC_CONCENTRATED_DARKNESS = 34628;
uint32 constexpr NPC_CONCENTRATED_LIGHT = 34630;
uint32 constexpr SPELL_DARK_ESSENCE = 65684;
uint32 constexpr SPELL_LIGHT_ESSENCE = 65686;
uint32 constexpr SPELL_DARK_TWIN_PACT = 65875;
uint32 constexpr SPELL_LIGHT_TWIN_PACT = 65876;

// Anub'arak (boss_anubarak_trial.cpp). The spike has the boss's name, so it is told apart by entry only.
uint32 constexpr NPC_ANUBARAK = 34564;
uint32 constexpr NPC_SWARM_SCARAB = 34605;
uint32 constexpr NPC_FROST_SPHERE = 34606;
uint32 constexpr NPC_NERUBIAN_BURROWER = 34607;
uint32 constexpr NPC_ANUBARAK_SPIKE = 34660;
uint32 constexpr SPELL_PURSUED_BY_ANUBARAK = 67574;

uint8 constexpr SKULL_ICON = 7;

// What a bot sees of the fight, from one pass over the creatures near it. Lists hold only living creatures.
struct Scene
{
    Player* bot = nullptr;
    Unit* gormok = nullptr;
    Unit* dreadscale = nullptr;
    Unit* acidmaw = nullptr;
    Unit* icehowl = nullptr;
    Unit* jaraxxus = nullptr;
    Unit* fjola = nullptr;
    Unit* eydis = nullptr;
    Unit* anubarak = nullptr;
    Unit* spike = nullptr;
    std::vector<Unit*> snobolds;
    std::vector<Unit*> portals;
    std::vector<Unit*> volcanoes;
    std::vector<Unit*> mistresses;
    std::vector<Unit*> infernals;
    std::vector<Unit*> champions;
    std::vector<Unit*> burrowers;
    std::vector<Unit*> scarabs;
    std::vector<Unit*> spheres;
    std::vector<Unit*> hazards;
    std::vector<Unit*> darkOrbs;
    std::vector<Unit*> lightOrbs;
    std::vector<Unit*> darkEssences;
    std::vector<Unit*> lightEssences;

    bool IsTwinFight() const { return fjola || eydis; }
};

// One pass per bot and millisecond: the triggers of a bot run together in one engine tick and would each repeat it.
// The reference stays valid until the next Scan call for another bot on the same thread.
Scene const& Scan(PlayerbotAI* botAI);

// Alive, hostile to the bot and not flagged untouchable (the script keeps Nether Portals and Infernal Volcanoes
// untouchable on normal difficulty, and the champions immune to players until they are released).
bool Attackable(Player* bot, Unit* unit);

// The unit a tank holds and the one the assist tank holds, or null while there is none
Unit* MainTankUnit(Scene const& scene);
Unit* AssistTankUnit(Scene const& scene);

// The unit a damage dealer works on: `current` is the bot's own target (kept while two choices are close), `bot` is
// null for the shared choice a tank marks.
Unit* PriorityTarget(Scene const& scene, Player* bot, Unit* current);

// The skull's unit in the bot's group, or null
Unit* SkullUnit(PlayerbotAI* botAI);

// With a real player as main tank the first bot assist tank marks, else the main tank bot does
bool IsMarkerBot(Player* bot, Unit* mainTank);

// Which essence the bot holds: sets `colour` and returns true when it holds one
bool HoldsEssence(Player* bot, ToCLogic::Twin& colour);
// The twin the bot attacks while neither twin is shielded: the one opposite its essence, else the assigned one
ToCLogic::Twin TwinFor(Player* bot);
Unit* UnitOfTwin(Scene const& scene, ToCLogic::Twin twin);

// What a bot's duty is right now: the unit to mark, to attack as a damage dealer, to hold as the main or assist tank.
// Each is null when the bot has no such duty or is on it already.
Unit* MarkTarget(PlayerbotAI* botAI);
Unit* DpsTarget(PlayerbotAI* botAI);
Unit* TankTarget(PlayerbotAI* botAI, bool main);

// A ground effect the bot stands too close to
Unit* HazardToAvoid(PlayerbotAI* botAI);
// Where the bot steps for Icehowl's charge (Icehowl faces its victim for 4 s before the hop); false when it need not
bool ChargeEscape(PlayerbotAI* botAI, ToCLogic::Point& dest);
// A group member that carries Incinerate Flesh
Player* IncineratedMember(PlayerbotAI* botAI);
// The essence creature the bot should click, or null
Unit* EssenceToTake(PlayerbotAI* botAI);
// Where the bot steps for the twins' orbs; false when it stays
bool OrbMove(PlayerbotAI* botAI, ToCLogic::Point& dest);
// Where the bot marked by Anub'arak's spike runs; false without a spike
bool SpikeEscape(PlayerbotAI* botAI, ToCLogic::Point& dest);
}  // namespace ToC

// The checks the triggers run, one per trigger. Each starts with its cheap role test so the creature scan only runs for
// the bots it concerns.
namespace ToCChecks
{
bool MarkPriority(PlayerbotAI* botAI);
bool AttackPriority(PlayerbotAI* botAI);
bool MainTankTarget(PlayerbotAI* botAI);
bool AssistTankTarget(PlayerbotAI* botAI);
bool GroundHazard(PlayerbotAI* botAI);
bool IcehowlCharge(PlayerbotAI* botAI);
bool LegionFlame(PlayerbotAI* botAI);
bool IncinerateFlesh(PlayerbotAI* botAI);
bool NeedEssence(PlayerbotAI* botAI);
bool TwinOrbs(PlayerbotAI* botAI);
bool SpikePursuit(PlayerbotAI* botAI);
bool BurrowerNearBoss(PlayerbotAI* botAI);
}

// One trigger class for all the checks: they differ only in the function they run
class ToCTrigger : public Trigger
{
public:
    using Check = bool (*)(PlayerbotAI*);

    ToCTrigger(PlayerbotAI* botAI, std::string const name, Check check) : Trigger(botAI, name), _check(check) {}
    bool IsActive() override { return _check(botAI); }

private:
    Check _check;
};

#endif
