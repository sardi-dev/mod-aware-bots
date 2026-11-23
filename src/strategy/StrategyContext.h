#ifndef _PLAYERBOT_STRATEGYCONTEXT_H
#define _PLAYERBOT_STRATEGYCONTEXT_H


#include "CustomStrategy.h"

#include "NamedObjectContext.h"

#include "AttackEnemyPlayersStrategy.h"
#include "CastTimeStrategy.h"
#include "ChatCommandHandlerStrategy.h"
#include "ConserveManaStrategy.h"
#include "CombatStrategy.h"  // #628 (AvoidAoeStrategy; AC StrategyContext.h:15)
#include "ForceRebuff.h"  // #628 (AC StrategyContext.h:26)
#include "DeadStrategy.h"
#include "DuelStrategy.h"
#include "EmoteStrategy.h"
#include "DpsAssistStrategy.h"
#include "DpsSheetStrategy.h"
#include "FleeStrategy.h"
#include "FocusHealTargetsStrategy.h"
#include "FollowMasterStrategy.h"
#include "MarkRtiStrategy.h"
#include "RacialsStrategy.h"
#include "UseFoodStrategy.h"
#include "GuardStrategy.h"
#include "GroupStrategy.h"
#include "LootNonCombatStrategy.h"
#include "TrainStrategy.h"
#include "ShopStrategy.h"
#include "RepairStrategy.h"
#include "SellTripStrategy.h"
#include "PvpTalentsStrategy.h"
#include "CraftStrategy.h"
#include "MarkPullStrategy.h"
#include "MeleeCombatStrategy.h"
#include "MoveFromGroupStrategy.h"
#include "RejoinGroup.h"
#include "TalentSpells.h"
#include "TankPackStart.h"
#include "TankDanger.h"
#include "NewRpgStrategy.h"
#include "RpgStrategy.h"     // #272 (AC StrategyContext.h:48)
#include "TravelStrategy.h"  // #269 (AC StrategyContext.h:54)
#include "DebugStrategy.h"   // #611 (AC StrategyContext.h:25)
#include "NonCombatStrategy.h"
#include "CollisionStrategy.h"  // #271 (AC NonCombatStrategy.h:23-31)
#include "PullStrategy.h"
#include "QuestStrategies.h"
#include "TankAssistStrategy.h"
#include "PvpKiteStrategy.h"
#include "OneVsOneStrategy.h"
#include "PvpCastStrategy.h"
#include "PvpDefenseStrategy.h"
#include "PvpTeamStrategy.h"
#include "ThreatStrategy.h"
#include "BattlegroundStrategy.h"
#include "BgTacticsTB.h"
#include "HelloStrategy.h"
#include "LfgStrategy.h"
#include "DungeonChatStrategy.h"
#include "LfmLeadStrategy.h"
#include "BreakStrategy.h"
#include "RunawayStrategy.h"
#include "RangedCombatStrategy.h"
#include "StayStrategy.h"
#include "AttackTaggedStrategy.h"  // #230 D2
#include "RTSCStrategy.h"  // #648
#include "UsePotionsStrategy.h"
#include "WaitForAttackStrategy.h"
#include "WorldPacketHandlerStrategy.h"

class StrategyContext : public NamedObjectContext<Strategy>
{
public:
    StrategyContext()
    {
        creators["default"] = &StrategyContext::world_packet;
        creators["custom"] = &StrategyContext::custom;
        creators["rtsc"] = &StrategyContext::rtsc;  // #648 (AC StrategyContext.h:115)
        // #628 (AC StrategyContext.h:76): the rebuff-before-ready-reply strategy (AI/ForceRebuff.h).
        creators["force rebuff"] = &StrategyContext::force_rebuff;
        // #628 (AC StrategyContext.h:126): AiFactory.cpp:260 already adds "avoid aoe" by name; it was never registered.
        creators["avoid aoe"] = &StrategyContext::avoid_aoe;

        creators["save mana"] = &StrategyContext::auto_save_mana;
        creators["close"] = &StrategyContext::close;
        creators["ranged"] = &StrategyContext::ranged;
        creators["dead"] = &StrategyContext::dead;
        creators["group"] = &StrategyContext::group;
        // #163 S8 (AC parity, AiFactory.cpp:288): healthstone/potion use at critical health/medium mana.
        creators["potions"] = &StrategyContext::potions;
        // #163 S8 (AC parity): de-prioritize a long cast that won't land before the target dies.
        creators["cast time"] = &StrategyContext::cast_time;
        // #163 S8 (AC parity): accept a duel request, attack the duel opponent.
        creators["duel"] = &StrategyContext::duel;
        // #410 (AC StrategyContext.h:80): empty marker strategy, see DuelStrategy.h.
        creators["start duel"] = &StrategyContext::start_duel;
        // #163 S8 (AC parity): marker strategy for the "focus heal targets" chat command.
        creators["focus heal targets"] = &StrategyContext::focus_heal_targets;
        // #163 S8 (AC parity): auto-assign a raid target icon when none is set (opt-in, like
        // "boost"/"save mana" - AC doesn't default it on either).
        creators["mark rti"] = &StrategyContext::mark_rti;
        // #163 S8 (AC parity): AiFactory.cpp already adds "racials" by name to every engine; this was
        // the missing registration.
        creators["racials"] = &StrategyContext::racials;
        // #163 S8 (AC parity): AiFactory.cpp already adds "food" by name to every non-BG non-combat
        // engine (:594); this was the missing registration.
        creators["food"] = &StrategyContext::food;
        // #163 S8 (AC parity): AiFactory.cpp already adds "emote" by name to every non-BG non-combat
        // engine (:594); this was the missing registration.
        creators["emote"] = &StrategyContext::emote;
        // #173: turns AiFactory's existing (previously silent no-op) "+chat" default strategy entry
        // into a live strategy - see strategy/generic/ChatCommandHandlerStrategy.h.
        creators["chat"] = &StrategyContext::chat;

        creators["formation"] = &StrategyContext::combat_formation;
        creators["move from group"] = &StrategyContext::move_from_group;
        creators["rejoin group"] = &StrategyContext::rejoin_group;  // BT-202
        creators["tank danger"] = &StrategyContext::tank_danger;  // BT-292
        creators["talent spells"] = &StrategyContext::talent_spells;  // BT-322
        creators["tank pack start"] = &StrategyContext::tank_pack_start;
        creators["healer triage"] = &StrategyContext::healer_triage;  // BT-295

        creators["tank face"] = &StrategyContext::tank_face;
        creators["threat"] = &StrategyContext::threat;
        creators["dps cooldown"] = &StrategyContext::dps_cooldown;
        creators["dps sheet"] = &StrategyContext::dps_sheet;
        creators["dps emergency offheal"] = &StrategyContext::dps_emergency_offheal;
        creators["pvp kite"] = &StrategyContext::pvp_kite;
        creators["1v1"] = &StrategyContext::one_vs_one;
        creators["pvp defense"] = &StrategyContext::pvp_defense;
        creators["pvp cast"] = &StrategyContext::pvp_cast;
        creators["pvp team"] = &StrategyContext::pvp_team;
        // AC parity (#141 item 5): "wait for attack" - opt-in, toggled the same way any
        // other named strategy is (ChangeStrategyAction). No bot has it on by default.
        creators["wait for attack"] = &StrategyContext::wait_for_attack;
        // world PvP: engage hostile players (issue #60)
        creators["pvp"] = &StrategyContext::pvp;
        // -- temp
        creators["say hello"] = &StrategyContext::say_hello;
        // new rpg (runtime roaming)
        // #269/#272 (AC StrategyContext.h:93-98, owner rule 2026-10-05: 1:1 incl. dormant code). Registered
        // as in AC; AiFactory adds none of them by default (AC AiFactory.cpp:616-622, mirrored in
        // Factory/AiFactory.cpp), so they only run if someone adds them by hand ("nc +rpg").
        creators["rpg"] = &StrategyContext::rpg;
        creators["new rpg"] = &StrategyContext::new_rpg;
        creators["travel"] = &StrategyContext::travel;
        // #611 (AC StrategyContext.h:117-121): debug family; registered only, nothing enables them by default.
        creators["debug"] = &StrategyContext::debug;
        creators["debug move"] = &StrategyContext::debug_move;
        creators["debug rpg"] = &StrategyContext::debug_rpg;
        creators["debug spell"] = &StrategyContext::debug_spell;
        creators["debug quest"] = &StrategyContext::debug_quest;
        creators["explore"] = &StrategyContext::explore;
        creators["map"] = &StrategyContext::map;
        creators["map full"] = &StrategyContext::map_full;
        // mount (issue #62)
        creators["mount"] = &StrategyContext::mount;
        creators["attack tagged"] = &StrategyContext::attack_tagged;  // #230 D2 (AC StrategyContext.h:116)
        // loot (issue #62, extended #151): LootNonCombatStrategy (strategy/generic/
        // LootNonCombatStrategy.h), not the old NonCombatStrategy.h "LootStrategy" (see that
        // header's class comment).
        creators["loot"] = &StrategyContext::loot;
        creators["train"] = &StrategyContext::train;
        creators["shop"] = &StrategyContext::shop;
        creators["repair"] = &StrategyContext::repair;
        creators["sell trip"] = &StrategyContext::sell_trip;
        creators["pvp talents"] = &StrategyContext::pvp_talents;  // BT-320
        creators["craft"] = &StrategyContext::craft;
        creators["mark pull"] = &StrategyContext::mark_pull;
        // gather (issue #62)
        creators["gather"] = &StrategyContext::gather;
        // #412 (AC StrategyContext.h:132-133): fishing.
        creators["use bobber"] = &StrategyContext::bobber_strategy;
        // #377 (AC StrategyContext.h:91)
        creators["reveal"] = &StrategyContext::reveal;
        creators["collision"] = &StrategyContext::collision;  // #271 AC StrategyContext.h:92 (dormant, opt-in)
        creators["master fishing"] = &StrategyContext::master_fishing;
        // grind / move random (issue #61, AC parity)
        creators["grind"] = &StrategyContext::grind;
        creators["move random"] = &StrategyContext::move_random;

        // battlegrounds (issue #33)
        creators["bg"] = &StrategyContext::bg;
        // In-battleground strategies (#33 slice B): the shared objective layer plus one per map.
        creators["battleground"] = &StrategyContext::battleground;
        creators["warsong"] = &StrategyContext::warsong;
        creators["arathi"] = &StrategyContext::arathi;
        creators["eye"] = &StrategyContext::eye;
        creators["alterac"] = &StrategyContext::alterac;
        // #346 (AC StrategyContext.h:113): arena positioning replaces the battleground objective
        // layer - an arena has no flags, nodes or towers, so there is nothing for "battleground" to do.
        creators["arena"] = &StrategyContext::arena;
        // #393 (AC StrategyContext.h:112): Isle of Conquest vehicles + node play.
        creators["isle"] = &StrategyContext::isle;
        // #717 (beyond AC, no AC spec): Strand of the Ancients.
        creators["strand"] = &StrategyContext::strand;
        // #576-#579: the Cataclysm/MoP-only battlegrounds (no AC spec; extend AC's per-map pattern).
        creators["gilneas"] = &StrategyContext::gilneas;
        creators["kotmogu"] = &StrategyContext::kotmogu;
        creators["silvershard"] = &StrategyContext::silvershard;
        creators["deepwind"] = &StrategyContext::deepwind;
        // #901 (beyond AC): Tol Barad battlefield tactics.
        creators["tolbarad"] = &StrategyContext::tolbarad;

        // #210 S10 G11 registration (AC parity): not added by AiFactory yet (#142 owns the
        // random-bot self-queue "lfg join"/"lfg leave" half) - see LfgStrategy.h.
        creators["lfg"] = &StrategyContext::lfg;
        creators["lfg combat leave"] = &StrategyContext::lfg_combat_leave;  // BT-206
        creators["lfm lead"] = &StrategyContext::lfm_lead;
        creators["dungeon chat"] = &StrategyContext::dungeon_chat;
        creators["break"] = &StrategyContext::break_strategy;

        // #257 slice H registration (AC StrategyContext.h:249-261, the AC QuestStrategyContext).
        // AiFactory.cpp:608 has been adding "quest" by name to every non-BG non-combat engine
        // since the quest layer landed, but with no StrategyContext entry it was a silent no-op.
        creators["quest"] = &StrategyContext::quest;
        creators["accept all quests"] = &StrategyContext::accept_all_quests;

        // #408 (AC StrategyContext.h): "pull back" is the tank leash strategy PullBackStrategy triggers
        // "return to pull position" from; "adds" is the PossibleAddsStrategy support layer. "pull" itself is
        // registered per class (Dk/Druid/Paladin/Warrior, exactly as AC does), never in the base context.
        creators["pull back"] = &StrategyContext::pull_back;
        creators["adds"] = &StrategyContext::possible_adds;
    }
private:
    // -- temp
    static Strategy* say_hello(PlayerbotAI* botAI) { return new HelloStrategy(botAI); }
    // --
    
    static Strategy* world_packet(PlayerbotAI* botAI) { return new WorldPacketHandlerStrategy(botAI); }
    static Strategy* custom(PlayerbotAI* botAI) { return new CustomStrategy(botAI); }
    static Strategy* rtsc(PlayerbotAI* botAI) { return new RTSCStrategy(botAI); }  // #648

    static Strategy* follow_master(PlayerbotAI* botAI) { return new FollowMasterStrategy(botAI); }
    static Strategy* auto_save_mana(PlayerbotAI* botAI) { return new HealerAutoSaveManaStrategy(botAI); }
    static Strategy* close(PlayerbotAI* botAI) { return new MeleeCombatStrategy(botAI); }
    static Strategy* ranged(PlayerbotAI* botAI) { return new RangedCombatStrategy(botAI); }
    static Strategy* dead(PlayerbotAI* botAI) { return new DeadStrategy(botAI); }
    static Strategy* group(PlayerbotAI* botAI) { return new GroupStrategy(botAI); }
    static Strategy* potions(PlayerbotAI* botAI) { return new UsePotionsStrategy(botAI); }
    static Strategy* cast_time(PlayerbotAI* botAI) { return new CastTimeStrategy(botAI); }
    static Strategy* duel(PlayerbotAI* botAI) { return new DuelStrategy(botAI); }
    static Strategy* start_duel(PlayerbotAI* botAI) { return new StartDuelStrategy(botAI); }
    static Strategy* focus_heal_targets(PlayerbotAI* botAI) { return new FocusHealTargetsStrategy(botAI); }
    static Strategy* mark_rti(PlayerbotAI* botAI) { return new MarkRtiStrategy(botAI); }
    static Strategy* racials(PlayerbotAI* botAI) { return new RacialsStrategy(botAI); }
    static Strategy* food(PlayerbotAI* botAI) { return new UseFoodStrategy(botAI); }
    static Strategy* force_rebuff(PlayerbotAI* botAI) { return new ForceRebuffStrategy(botAI); }
    static Strategy* avoid_aoe(PlayerbotAI* botAI) { return new AvoidAoeStrategy(botAI); }
    static Strategy* emote(PlayerbotAI* botAI) { return new EmoteStrategy(botAI); }
    static Strategy* chat(PlayerbotAI* botAI) { return new ChatCommandHandlerStrategy(botAI); }
    static Strategy* combat_formation(PlayerbotAI* ai) { return new CombatFormationStrategy(ai); }
    static Strategy* move_from_group(PlayerbotAI* botAI) { return new MoveFromGroupStrategy(botAI); }
    static Strategy* rejoin_group(PlayerbotAI* botAI) { return new RejoinGroupStrategy(botAI); }
    static Strategy* tank_danger(PlayerbotAI* botAI) { return new TankDangerStrategy(botAI); }
    static Strategy* talent_spells(PlayerbotAI* botAI) { return new TalentSpellsStrategy(botAI); }
    static Strategy* tank_pack_start(PlayerbotAI* botAI) { return new TankPackStartStrategy(botAI); }
    static Strategy* healer_triage(PlayerbotAI* botAI) { return new HealerTriageStrategy(botAI); }

    static Strategy* tank_face(PlayerbotAI* botAI) { return new TankFaceStrategy(botAI); }
    static Strategy* dps_cooldown(PlayerbotAI* botAI) { return new DpsCooldownStrategy(botAI); }
    static Strategy* dps_sheet(PlayerbotAI* botAI) { return new DpsSheetStrategy(botAI); }
    static Strategy* dps_emergency_offheal(PlayerbotAI* botAI) { return new DpsOffhealStrategy(botAI); }
    static Strategy* threat(PlayerbotAI* botAI) { return new ThreatStrategy(botAI); }
    static Strategy* pvp_kite(PlayerbotAI* botAI) { return new PvpKiteStrategy(botAI); }
    static Strategy* one_vs_one(PlayerbotAI* botAI) { return new OneVsOneStrategy(botAI); }
    static Strategy* pvp_defense(PlayerbotAI* botAI) { return new PvpDefenseStrategy(botAI); }
    static Strategy* pvp_cast(PlayerbotAI* botAI) { return new PvpCastStrategy(botAI); }
    static Strategy* pvp_team(PlayerbotAI* botAI) { return new PvpTeamStrategy(botAI); }
    static Strategy* wait_for_attack(PlayerbotAI* botAI) { return new WaitForAttackStrategy(botAI); }
    static Strategy* pvp(PlayerbotAI* botAI) { return new AttackEnemyPlayersStrategy(botAI); }
    static Strategy* rpg(PlayerbotAI* botAI) { return new RpgStrategy(botAI); }
    static Strategy* new_rpg(PlayerbotAI* botAI) { return new NewRpgStrategy(botAI); }
    static Strategy* travel(PlayerbotAI* botAI) { return new TravelStrategy(botAI); }
    static Strategy* debug(PlayerbotAI* botAI) { return new DebugStrategy(botAI); }
    static Strategy* debug_move(PlayerbotAI* botAI) { return new DebugMoveStrategy(botAI); }
    static Strategy* debug_rpg(PlayerbotAI* botAI) { return new DebugRpgStrategy(botAI); }
    static Strategy* debug_spell(PlayerbotAI* botAI) { return new DebugSpellStrategy(botAI); }
    static Strategy* debug_quest(PlayerbotAI* botAI) { return new DebugQuestStrategy(botAI); }
    static Strategy* explore(PlayerbotAI* botAI) { return new ExploreStrategy(botAI); }
    static Strategy* map(PlayerbotAI* botAI) { return new MapStrategy(botAI); }
    static Strategy* map_full(PlayerbotAI* botAI) { return new MapFullStrategy(botAI); }
    static Strategy* mount(PlayerbotAI* botAI) { return new MountStrategy(botAI); }
    static Strategy* attack_tagged(PlayerbotAI* botAI) { return new AttackTaggedStrategy(botAI); }  // #230 D2
    static Strategy* loot(PlayerbotAI* botAI) { return new LootNonCombatStrategy(botAI); }
    static Strategy* train(PlayerbotAI* botAI) { return new TrainStrategy(botAI); }
    static Strategy* shop(PlayerbotAI* botAI) { return new ShopStrategy(botAI); }
    static Strategy* repair(PlayerbotAI* botAI) { return new RepairStrategy(botAI); }
    static Strategy* sell_trip(PlayerbotAI* botAI) { return new SellTripStrategy(botAI); }
    static Strategy* pvp_talents(PlayerbotAI* botAI) { return new PvpTalentsStrategy(botAI); }
    static Strategy* craft(PlayerbotAI* botAI) { return new CraftStrategy(botAI); }
    static Strategy* mark_pull(PlayerbotAI* botAI) { return new MarkPullStrategy(botAI); }
    static Strategy* gather(PlayerbotAI* botAI) { return new GatherStrategy(botAI); }
    static Strategy* bobber_strategy(PlayerbotAI* botAI) { return new UseBobberStrategy(botAI); }
    static Strategy* reveal(PlayerbotAI* botAI) { return new RevealStrategy(botAI); }  // AC StrategyContext.h:170
    static Strategy* collision(PlayerbotAI* botAI) { return new CollisionStrategy(botAI); }  // #271 AC :171
    static Strategy* master_fishing(PlayerbotAI* botAI) { return new MasterFishingStrategy(botAI); }
    static Strategy* grind(PlayerbotAI* botAI) { return new GrindingStrategy(botAI); }
    static Strategy* move_random(PlayerbotAI* botAI) { return new MoveRandomStrategy(botAI); }
    static Strategy* bg(PlayerbotAI* botAI) { return new BgStrategy(botAI); }
    static Strategy* battleground(PlayerbotAI* botAI) { return new BattlegroundStrategy(botAI); }
    static Strategy* warsong(PlayerbotAI* botAI) { return new WarsongStrategy(botAI); }
    static Strategy* arathi(PlayerbotAI* botAI) { return new ArathiStrategy(botAI); }
    static Strategy* eye(PlayerbotAI* botAI) { return new EyeStrategy(botAI); }
    static Strategy* alterac(PlayerbotAI* botAI) { return new AlteracStrategy(botAI); }
    static Strategy* arena(PlayerbotAI* botAI) { return new ArenaStrategy(botAI); }
    static Strategy* isle(PlayerbotAI* botAI) { return new IsleStrategy(botAI); }
    static Strategy* strand(PlayerbotAI* botAI) { return new StrandStrategy(botAI); }
    static Strategy* gilneas(PlayerbotAI* botAI) { return new GilneasStrategy(botAI); }
    static Strategy* kotmogu(PlayerbotAI* botAI) { return new KotmoguStrategy(botAI); }
    static Strategy* tolbarad(PlayerbotAI* botAI) { return new TolBaradStrategy(botAI); }
    static Strategy* silvershard(PlayerbotAI* botAI) { return new SilvershardStrategy(botAI); }
    static Strategy* deepwind(PlayerbotAI* botAI) { return new DeepwindStrategy(botAI); }
    static Strategy* lfg(PlayerbotAI* botAI) { return new LfgStrategy(botAI); }
    static Strategy* lfg_combat_leave(PlayerbotAI* botAI) { return new LfgCombatLeaveStrategy(botAI); }
    static Strategy* lfm_lead(PlayerbotAI* botAI) { return new LfmLeadStrategy(botAI); }
    static Strategy* dungeon_chat(PlayerbotAI* botAI) { return new DungeonChatStrategy(botAI); }
    static Strategy* break_strategy(PlayerbotAI* botAI) { return new BreakStrategy(botAI); }

    static Strategy* quest(PlayerbotAI* botAI) { return new DefaultQuestStrategy(botAI); }
    static Strategy* accept_all_quests(PlayerbotAI* botAI) { return new AcceptAllQuestsStrategy(botAI); }

    // #408 (AC StrategyContext.h)
    static Strategy* pull_back(PlayerbotAI* botAI) { return new PullBackStrategy(botAI); }
    static Strategy* possible_adds(PlayerbotAI* botAI) { return new PossibleAddsStrategy(botAI); }
};

class MovementStrategyContext : public NamedObjectContext<Strategy>
{
public:
    MovementStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["follow"] = &MovementStrategyContext::follow_master;
        creators["stay"] = &MovementStrategyContext::stay;
        creators["runaway"] = &MovementStrategyContext::runaway;
        creators["flee from adds"] = &MovementStrategyContext::flee_from_adds;
        creators["guard"] = &MovementStrategyContext::guard;
    }

private:
    static Strategy* guard(PlayerbotAI* botAI) { return new GuardStrategy(botAI); }
    static Strategy* follow_master(PlayerbotAI* botAI) { return new FollowMasterStrategy(botAI); }
    static Strategy* stay(PlayerbotAI* botAI) { return new StayStrategy(botAI); }
    static Strategy* runaway(PlayerbotAI* botAI) { return new RunawayStrategy(botAI); }
    static Strategy* flee_from_adds(PlayerbotAI* botAI) { return new FleeFromAddsStrategy(botAI); }
};

class AssistStrategyContext : public NamedObjectContext<Strategy>
{
public:
    AssistStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["dps assist"] = &AssistStrategyContext::dps_assist;
        creators["dps aoe"] = &AssistStrategyContext::dps_aoe;
        creators["tank assist"] = &AssistStrategyContext::tank_assist;
    }

private:
    static Strategy* dps_assist(PlayerbotAI* botAI) { return new DpsAssistStrategy(botAI); }
    static Strategy* dps_aoe(PlayerbotAI* botAI) { return new DpsAoeStrategy(botAI); }
    static Strategy* tank_assist(PlayerbotAI* botAI) { return new TankAssistStrategy(botAI); }
};

#endif
