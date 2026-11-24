/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Bot-led dungeons (see LfgCrawlAction.h).
 */

#include "LfgCrawlAction.h"

#include "BotLfm.h"
#include "Config.h"
#include "Event.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "Group.h"
#include "Transport.h"
#include "InstanceScript.h"
#include "LFGMgr.h"
#include "LfgFill.h"
#include "LfgStallPolicy.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "PlayerbotOperations.h"
#include "PlayerbotSpec.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "GossipDef.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <tuple>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct Settings
    {
        bool enable;
        std::set<uint32> maps;
        float pullRange;          // yards: a pack this close ahead is pulled
        float bossPullRange;      // yards from a boss' spawn at which it is pulled
        float healthPercent;      // every member at least this healthy before a pull or a walk
        float manaPercent;        // every member with mana at least this full
        float groupRadius;        // every member within this many yards of the leader
        uint32 readyMaxWaitMs;    // waiting for the group never lasts longer than this
        uint32 stuckMs;           // no progress toward a boss for this long: the boss is skipped
        uint32 skipMs;            // how long a skipped boss is left alone
        bool eventSteps;          // the per-map event steps, anchors and doors (EventSteps() and below)
        uint32 eventWaitMs;       // how long the leader waits for an event-summoned boss to appear
        bool keys;                // fetch and demand the Dire Maul keys (owner rule), on top of the locks' own item keys
    };

    std::set<uint32> ParseIds(std::string const& list)
    {
        std::set<uint32> ids;
        std::istringstream in(list);
        std::string token;
        while (std::getline(in, token, ','))
            if (uint32 const id = uint32(std::strtoul(token.c_str(), nullptr, 10)))
                ids.insert(id);
        return ids;
    }

    Settings const& CrawlSettings()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgCrawl.Enable", true);
            s.maps = ParseIds(sConfigMgr->GetStringDefault("AiPlayerbot.LfgCrawl.Maps", "389,43,48,47,129,209,349,33,34,1001,36,90,429,269,560,755,657,595,644"));
            s.pullRange = sConfigMgr->GetFloatDefault("AiPlayerbot.LfgCrawl.PullRange", 30.0f);
            s.bossPullRange = sConfigMgr->GetFloatDefault("AiPlayerbot.LfgCrawl.BossPullRange", 45.0f);
            s.healthPercent = sConfigMgr->GetFloatDefault("AiPlayerbot.LfgCrawl.HealthPercent", 85.0f);
            s.manaPercent = sConfigMgr->GetFloatDefault("AiPlayerbot.LfgCrawl.ManaPercent", 70.0f);
            s.groupRadius = sConfigMgr->GetFloatDefault("AiPlayerbot.LfgCrawl.GroupRadius", 40.0f);
            s.readyMaxWaitMs = uint32(std::max(5, sConfigMgr->GetIntDefault("AiPlayerbot.LfgCrawl.ReadyMaxWaitSeconds", 75))) * IN_MILLISECONDS;
            s.stuckMs = uint32(std::max(10, sConfigMgr->GetIntDefault("AiPlayerbot.LfgCrawl.StuckSeconds", 40))) * IN_MILLISECONDS;
            s.skipMs = uint32(std::max(10, sConfigMgr->GetIntDefault("AiPlayerbot.LfgCrawl.SkipSeconds", 300))) * IN_MILLISECONDS;
            s.eventSteps = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgCrawl.EventSteps", true);
            s.eventWaitMs = uint32(std::max(5, sConfigMgr->GetIntDefault("AiPlayerbot.LfgCrawl.EventWaitSeconds", 60))) * IN_MILLISECONDS;
            s.keys = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgCrawl.DireMaulKeys", true);
            return s;
        }();
        return settings;
    }

    // A boss of a dungeon: from `instance_encounters` (creature kills) at its first creature spawn of this difficulty.
    struct Boss
    {
        uint32 entry = 0;
        uint32 encounterIndex = 0;
        bool final = false;     // the encounter that finishes the finder dungeon
        uint32 lastDungeon = 0; // the finder dungeon this encounter finishes (0: none)
        bool hasSpawn = false;  // false: a script summons it
        uint32 anchor = 0;      // an event-summoned boss: the creature (from the spawn table) whose fight brings it (EVENT_BOSSES)
        float x = 0.0f, y = 0.0f, z = 0.0f;
        std::string name;
    };

    // The mask bit of an encounter that has none (see EXTRA_BOSSES).
    uint32 const NO_ENCOUNTER_BIT = 0xFFFFFFFF;

    // Fights that open the way but have no row in `instance_encounters`: the encounter table does not list them, the dungeon's
    // doors need them dead, and the leader finds them dead by their corpse. Scarlet Halls: Commander Lindon (his exit door
    // opens when he dies), Houndmaster Braun and Armsmaster Harlan (each opens the passage behind him), in that order on the
    // way to Flameweaver Koegler, the final boss the finder credits.
    std::map<uint32, std::vector<uint32>> const& ExtraBosses()
    {
        static std::map<uint32, std::vector<uint32>> const extras =
        {
            { 1001, { 59191, 59303, 58632 } },
            // Halls of Origination: the four wardens of the Vault of Lights (Flame, Earth, Water, Air; not encounters, but their
            // deaths open the door to Anraphet).
            { 644, { 39800, 39801, 39802, 39803 } }
        };
        return extras;
    }

    // Event-summoned bosses: no spawn of their own, they come with a creature that does have one (Deadmines: Helix Gearbreaker
    // rides the Lumbering Oaf, a vehicle accessory, and the fight starts when the Oaf is pulled). The leader goes to the
    // anchor's spawn, pulls the anchor and, once it is dead, waits there (EventWaitSeconds) for the boss to show.
    struct EventBoss
    {
        uint32 mapId;
        uint32 boss;
        uint32 anchor;
    };

    std::vector<EventBoss> const& EventBosses()
    {
        static std::vector<EventBoss> const bosses = { { 36, 47296, 47297 } };
        return bosses;
    }

    // Maps where the nearest boss is not the next one (a door or a script closes the way until the earlier boss is dead):
    // the leader takes these in the order given, the final boss last. Deadmines: Glubtok opens the factory door, Helix the
    // mast room, Foe Reaper 5000 the foundry, and Admiral Ripsnarl lies beyond the Iron Clad Door. Dire Maul: the order of a
    // wing's guards, which the wing's own list filters.
    std::map<uint32, std::vector<uint32>> const& BossOrder()
    {
        static std::map<uint32, std::vector<uint32>> const orders =
        {
            { 36, { 47162, 47296, 43778, 47626 } },
            // Wailing Caverns: the north cluster first (Anacondra, Kresh, Pythas, Cobrahn), then Verdan, Skum and Serpentis last: the
            // gossip of Muyoh's event is offered only once all four fanglords are dead, and Serpentis is the step's `after`. Mutanus
            // has no spawn: the escort's script summons him.
            { 43, { 3671, 3653, 3670, 3669, 5775, 3674, 3673 } },
            { 755, { 44577, 43614, 43612 } },                       // Lost City: Husam, Lockmaw, Barim, then Siamat
            { 657, { 43878, 43873 } },                              // Vortex Pinnacle: Ertan, Altairus, then Asaad
            // Halls of Origination: Anhuur, Ptah, the Vault of Lights wardens, Anraphet, then (after the lift) the three elementals and Rajh
            { 644, { 39425, 39428, 39800, 39801, 39802, 39803, 39788, 39587, 39731, 39732 } },
            { 429, { 11489, 11488, 11487, 11496, 14326, 14322, 14321, 14323, 14325, 14324 } }  // Dire Maul: West, then North, by wing
        };
        return orders;
    }

    // Things the leader does by hand on the way that no boss fight does (data from the dungeons' scripts). A map's steps (and
    // a wing's, `dungeonId`) go strictly in the order given; a step waits for its `after` boss.
    //  - USE_OBJECT: use `count` different objects of the entry (a quick-open object, no key or skill, so the server asks for
    //    nothing); then stand `seconds` (an event that needs time). Deadmines: the Defias Cannon blows the Iron Clad Door out,
    //    after Foe Reaper 5000. Old Hillsbrad: five barrels start the first fight and open Thrall's gossip.
    //  - TALK: talk to the creature, pick the gossip option (the next pages' first option too), then stay with it until `until`
    //    (a boss) is dead or `seconds` have passed, answering the gossip of `also` and of the creature itself when it offers
    //    one on the way. Gnomeregan: Blastmaster Emi Shortfuse's event. Old Hillsbrad: Thrall's escort, Taretha on the way.
    //  - HOLD: go to the creature and stand by it, the same wait as TALK without the gossip. The Black Morass: the group
    //    defends Medivh until Aeonus is dead.
    //  - CLEAR: go to an object and clear what is around it until the script has switched it (its state leaves "ready").
    //    Dire Maul West: the five crystals whose guards keep the force field up.
    //  - QUEST_ACCEPT / QUEST_TURNIN: take or hand in `questId` at the creature, the calls the world session makes for a click
    //    on a quest giver (the source item comes with the quest, the reward is the quest's own). A bot that has done the quest
    //    before skips the step. The Culling of Stratholme: Chromie's "Dispelling Illusions" (13149), whose completion opens
    //    Arthas' first gossip.
    //  - USE_ITEM: cast the spell of an item the bot carries (`itemId`; no item, no step) on `count` different creatures of the
    //    entry nearby. The Culling of Stratholme: the Arcane Disruptor's spell on the five crate helpers.
    //  - BOARD: ride a stop-frame lift (a type-11 transport): the group gathers on its platform, the leader answers the gossip of the
    //    controller (`also`, `option`) which sets the lift moving, and when the platform stands at `targetZ` everyone steps off.
    //    Halls of Origination: the Lift of the Makers 207547 to the Vault of Lights floor (z 330), after Anraphet.
    //  - SLIPSTREAM: every member of the group answers a creature's gossip hello at once, which is all a slipstream needs (its
    //    script teleports whoever greets it). The Vortex Pinnacle; `hintX`/`hintY` pick one of several slipstreams.
    enum class StepKind
    {
        USE_OBJECT,
        TALK,
        HOLD,
        CLEAR,
        SLIPSTREAM,
        QUEST_ACCEPT,
        QUEST_TURNIN,
        USE_ITEM,
        BOARD
    };

    struct EventStep
    {
        uint32 mapId;
        uint32 dungeonId = 0;  // the wing's finder dungeon (0: every dungeon of the map)
        StepKind kind;
        uint32 entry;          // gameobject or creature entry; its spawn on the map is where the leader goes
        uint32 after = 0;      // boss entry that must be dead first (0: from the start)
        uint32 option = 0;     // TALK: the gossip option index
        uint32 until = 0;      // TALK, HOLD: boss entry whose death ends the wait
        uint32 seconds = 0;    // TALK, HOLD: the wait ends after this long at the latest; USE_OBJECT: stands this long after the last use
        uint32 count = 0;      // USE_OBJECT: how many different objects (0: one)
        uint32 also = 0;       // TALK: another creature whose gossip is answered on the way
        float hintX = 0.0f;    // which spawn when the entry has several (both 0: the first)
        float hintY = 0.0f;
        uint32 questId = 0;    // QUEST_ACCEPT, QUEST_TURNIN
        uint32 itemId = 0;     // USE_ITEM: the item the bot must carry
        uint32 spellId = 0;    // USE_ITEM: the spell cast on the target
        float fallbackX = 0.0f; // where to look when the entry has no spawn row (a summoned creature)
        float fallbackY = 0.0f;
        float fallbackZ = 0.0f;
        float targetZ = 0.0f;   // BOARD: the platform's height at its stop
        bool keyStep = false;  // only with AiPlayerbot.LfgCrawl.DireMaulKeys: the step fetches a key
        char const* name;
    };

    bool IsObjectStep(StepKind kind) { return kind == StepKind::USE_OBJECT || kind == StepKind::CLEAR; }

    std::vector<EventStep> const& EventSteps()
    {
        static std::vector<EventStep> const steps =
        {
            { .mapId = 36, .kind = StepKind::USE_OBJECT, .entry = 16398, .after = 43778, .name = "the Defias Cannon" },
            // Wailing Caverns: Muyoh (the Disciple of Naralex) offers "Let the event begin!" once Cobrahn, Pythas, Anacondra and
            // Serpentis are dead (the zone's script, gossip menu 699); his escort walks to Naralex, three waves come on the way, and
            // the script summons Mutanus the Devourer, the dungeon's last encounter, when Naralex is awake.
            { .mapId = 43, .kind = StepKind::TALK, .entry = 3678, .after = 3673, .until = 3654, .seconds = 1200, .name = "Muyoh" },
            { .mapId = 90, .kind = StepKind::TALK, .entry = 7998, .until = 7361, .seconds = 420, .name = "Blastmaster Emi Shortfuse" },
            { .mapId = 269, .kind = StepKind::HOLD, .entry = 15608, .until = 17881, .seconds = 3000, .name = "Medivh" },
            { .mapId = 560, .kind = StepKind::USE_OBJECT, .entry = 182589, .count = 5, .name = "the barrels" },
            { .mapId = 560, .kind = StepKind::TALK, .entry = 17876, .until = 18096, .seconds = 3000, .also = 18887, .name = "Thrall" },
            // Dire Maul East: Pusillin runs from stop to stop and is talked to at each (five gossip answers), then fights at the
            // last stop and drops the Crescent Key at 100 percent (his SmartAI: gossip menus 6877 to 6881).
            { .mapId = 429, .dungeonId = 34, .kind = StepKind::TALK, .entry = 14354, .seconds = 900, .keyStep = true, .name = "Pusillin" },
            { .mapId = 429, .dungeonId = 36, .kind = StepKind::CLEAR, .entry = 177259, .seconds = 240, .name = "the first crystal" },
            { .mapId = 429, .dungeonId = 36, .kind = StepKind::CLEAR, .entry = 177257, .seconds = 240, .name = "the second crystal" },
            { .mapId = 429, .dungeonId = 36, .kind = StepKind::CLEAR, .entry = 177258, .seconds = 240, .name = "the third crystal" },
            { .mapId = 429, .dungeonId = 36, .kind = StepKind::CLEAR, .entry = 179504, .after = 11487, .seconds = 240, .name = "the fourth crystal" },
            { .mapId = 429, .dungeonId = 36, .kind = StepKind::CLEAR, .entry = 179505, .after = 11487, .seconds = 240, .name = "the fifth crystal" },
            // The Culling of Stratholme: Chromie at the entrance gives the quest and the Arcane Disruptor, five crates are revealed with
            // it on their helpers (27827) along the road, Chromie (27915) is summoned in front of the city, and Arthas' first gossip
            // needs the quest done; then his escort to Mal'Ganis (gossip answered at every act, the escort ends when Mal'Ganis dies).
            { .mapId = 595, .kind = StepKind::QUEST_ACCEPT, .entry = 26527, .questId = 13149, .name = "Chromie's quest" },
            { .mapId = 595, .kind = StepKind::USE_ITEM, .entry = 27827, .count = 5, .hintX = 1550.0f, .hintY = 574.0f, .itemId = 37888,
              .spellId = 49590, .name = "the crates" },
            { .mapId = 595, .kind = StepKind::QUEST_TURNIN, .entry = 27915, .questId = 13149, .fallbackX = 1813.3f, .fallbackY = 1283.6f,
              .fallbackZ = 142.3f, .name = "Chromie" },
            { .mapId = 595, .kind = StepKind::TALK, .entry = 26499, .until = 26533, .seconds = 3000, .name = "Arthas" },
            // Halls of Origination: Brann's start gossip (menu 11339) opens the Vault of Lights event once Ptah is dead; after Anraphet
            // the Lift of the Makers (controller 207669, option 1 = second floor) takes the group up to the three elementals and Rajh.
            { .mapId = 644, .kind = StepKind::TALK, .entry = 39908, .after = 39428, .name = "Brann Bronzebeard" },
            { .mapId = 644, .kind = StepKind::BOARD, .entry = 207547, .after = 39788, .option = 1, .seconds = 60, .also = 207669,
              .targetZ = 330.0f, .name = "the Lift of the Makers" },
            { .mapId = 657, .kind = StepKind::SLIPSTREAM, .entry = 45455, .after = 43878, .hintX = -765.0f, .hintY = -41.0f, .name = "the first slipstream" },
            { .mapId = 657, .kind = StepKind::SLIPSTREAM, .entry = 45455, .after = 43873, .hintX = -1199.0f, .hintY = 107.0f, .name = "the second slipstream" }
        };
        return steps;
    }

    // Closed doors with no script that the leader opens when it stands at one: the quick-open doors, and the doors that want a
    // key, which only a member carrying it opens (OpenDoorAhead, DoorKeyItems). Dire Maul: the wing entrance doors (they close
    // again after a few seconds), the West door, the North courtyard door and inner door (the Gordok Shackle Key's) and the
    // one the Crescent Key opened. Gnomeregan: the workshop door and the final chamber's door.
    std::vector<std::pair<uint32, uint32>> const& PassDoors()
    {
        static std::vector<std::pair<uint32, uint32>> const doors =
        {
            { 90, 90858 }, { 90, 142207 },
            { 429, 177211 }, { 429, 177212 }, { 429, 177215 }, { 429, 177221 }, { 429, 177219 }, { 429, 177217 }, { 429, 179549 }
        };
        return doors;
    }

    // The keys the owner's rule demands for two Dire Maul doors (the Gordok Inner Door 177217, the door 179549 the Crescent Key
    // was for). The 5.4.8 client data (Lock.dbc rows 81 and 77) lists only Quick Open and Quick Close for them, no item, so a
    // player needs no key there today; the rule is on by AiPlayerbot.LfgCrawl.DireMaulKeys. Doors whose lock row does name an
    // item key need that item whatever the setting (DoorKeyItems).
    std::map<uint32, uint32> const& OwnerKeyDoors()
    {
        static std::map<uint32, uint32> const doors = { { 177217, 18250 }, { 179549, 18249 } };
        return doors;
    }

    std::mutex bossLock;
    std::map<std::pair<uint32, uint32>, std::vector<Boss>> bossCache;

    // The spawn table is walked once per map and difficulty and the answer kept for good.
    std::vector<Boss> const& MapBossesOf(uint32 mapId, DifficultyID difficulty)
    {
        std::lock_guard<std::mutex> guard(bossLock);
        auto const key = std::make_pair(mapId, uint32(difficulty));
        auto itr = bossCache.find(key);
        if (itr != bossCache.end())
            return itr->second;

        std::vector<Boss> bosses;
        if (DungeonEncounterList const* encounters = sObjectMgr->GetDungeonEncounterList(mapId, difficulty))
        {
            for (DungeonEncounter const* encounter : *encounters)
            {
                if (encounter->creditType != EncounterCreditType::ENCOUNTER_CREDIT_KILL_CREATURE)
                    continue;

                Boss boss;
                boss.entry = encounter->creditEntry;
                boss.encounterIndex = encounter->dbcEntry->encounterIndex;
                boss.lastDungeon = encounter->lastEncounterDungeon;
                boss.final = boss.lastDungeon != 0;
                if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(boss.entry))
                    boss.name = info->Name;
                bosses.push_back(boss);
            }
        }

        auto const extras = ExtraBosses().find(mapId);
        if (extras != ExtraBosses().end())
            for (uint32 const entry : extras->second)
            {
                bool known = false;
                for (Boss const& boss : bosses)
                    known = known || boss.entry == entry;
                if (known)
                    continue;

                Boss boss;
                boss.entry = entry;
                boss.encounterIndex = NO_ENCOUNTER_BIT;
                if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry))
                    boss.name = info->Name;
                bosses.push_back(boss);
            }

        if (!bosses.empty())
        {
            for (auto const& itr2 : sObjectMgr->GetAllCreatureData())
            {
                CreatureData const& data = itr2.second;
                if (data.mapid != mapId || !(data.spawnMask & (1u << uint32(difficulty))))
                    continue;

                for (Boss& boss : bosses)
                    if (boss.entry == data.id && !boss.hasSpawn)
                    {
                        boss.hasSpawn = true;
                        boss.x = data.posX;
                        boss.y = data.posY;
                        boss.z = data.posZ;
                    }
            }
        }

        // An event-summoned boss stands where its anchor spawns.
        if (CrawlSettings().eventSteps)
            for (EventBoss const& event : EventBosses())
                if (event.mapId == mapId)
                    for (Boss& boss : bosses)
                        if (boss.entry == event.boss && !boss.hasSpawn)
                            for (auto const& itr2 : sObjectMgr->GetAllCreatureData())
                            {
                                CreatureData const& data = itr2.second;
                                if (data.id == event.anchor && data.mapid == mapId && (data.spawnMask & (1u << uint32(difficulty))))
                                {
                                    boss.hasSpawn = true;
                                    boss.anchor = event.anchor;
                                    boss.x = data.posX;
                                    boss.y = data.posY;
                                    boss.z = data.posZ;
                                    break;
                                }
                            }

        SF_LOG_INFO("playerbots", "[CRAWL] map %u difficulty %u: %u boss encounter(s)", mapId, uint32(difficulty),
                    uint32(bosses.size()));
        return bossCache.emplace(key, std::move(bosses)).first->second;
    }

    // Maps that hold several finder dungeons (wings) of their own. A run there only knows the bosses of its wing: the
    // entries below (from the dungeons' intake notes; the wing's final boss is the encounter whose lastEncounterDungeon
    // names the wing). A wing missing here is not crawled. Dire Maul's wings need the doors and the West crystals above.
    std::set<uint32> const WING_MAPS = { 349, 429 };
    std::map<uint32, std::set<uint32>> const& Wings()
    {
        static std::map<uint32, std::set<uint32>> const wings =
        {
            { 26, { 13282, 12258 } },                      // Maraudon, Foulspore Cavern: Noxxion, Razorlash
            { 272, { 12236 } },                            // Maraudon, The Wicked Grotto: Lord Vyletongue
            { 273, { 12225, 12203, 13601, 13596, 12201 } }, // Maraudon, Earth Song Falls: Celebras, Landslide, Gizlock, Rotgrip, Theradras
            { 34, { 11490, 13280, 14327, 11492 } },         // Dire Maul East: Zevrim, Hydrospawn, Lethtendris, Alzzin
            { 36, { 11489, 11488, 11487, 11496, 11486 } },  // Dire Maul West: Tendris, Illyanna, Kalendris, Immol'thar, Tortheldrin
            { 38, { 14326, 14322, 14321, 14323, 14325, 14324, 11501 } }  // Dire Maul North: the guards, Cho'Rush, King Gordok
        };
        return wings;
    }

    std::map<std::tuple<uint32, uint32, uint32>, std::vector<Boss>> wingCache;

    // The bosses of the run's dungeon: all the encounters of a single-dungeon map, the wing's own on a wing map (none for
    // a wing that is not known, or for a run whose dungeon is not known).
    std::vector<Boss> const& BossesOf(uint32 mapId, DifficultyID difficulty, uint32 dungeonId)
    {
        std::vector<Boss> const& all = MapBossesOf(mapId, difficulty);
        if (!WING_MAPS.count(mapId))
            return all;

        std::lock_guard<std::mutex> guard(bossLock);
        auto const key = std::make_tuple(mapId, uint32(difficulty), dungeonId);
        auto itr = wingCache.find(key);
        if (itr != wingCache.end())
            return itr->second;

        std::vector<Boss> wing;
        auto const entries = Wings().find(dungeonId);
        if (entries != Wings().end())
            for (Boss boss : all)
                if (entries->second.count(boss.entry))
                {
                    boss.final = boss.lastDungeon == dungeonId;
                    wing.push_back(boss);
                }

        SF_LOG_INFO("playerbots", "[CRAWL] map %u wing (dungeon %u): %u boss(es)", mapId, dungeonId, uint32(wing.size()));
        return wingCache.emplace(key, std::move(wing)).first->second;
    }

    // Per group, while a bot-only run is inside a dungeon.
    struct RunState
    {
        std::set<uint32> done;                       // boss entries seen dead
        std::unordered_map<uint32, uint32> skipped;  // boss entry -> getMSTime() it was skipped at
        std::unordered_map<uint32, uint32> skipCount;  // boss entry -> how often it was skipped this run
        uint32 waitSince = 0;                        // 0 = not waiting for the group
        uint32 readyFreeUntil = 0;                   // after a wait ran out, no waiting until then
        uint32 progressAt = 0;
        float progressX = 0.0f, progressY = 0.0f;
        uint32 progressEntry = 0;
        uint32 leadRequestAt = 0;
        uint32 lootSince = 0;
        bool finished = false;                       // a bot-led party's final boss is dead
        bool stalled = false;                        // every boss left was skipped with nothing pulled: the run is given up
        uint32 pulledAt = 0;                         // getMSTime() the leader last pulled a pack or a boss (0: never)
        std::set<uint32> stepsDone;                  // indices into EventSteps() that are over (done, or not possible)
        int32 followStep = -1;                       // the TALK step whose creature the group is walking with
        uint32 followSince = 0;
        uint32 eventWaitEntry = 0;                   // the event-summoned boss the leader waits for
        uint32 eventWaitSince = 0;
        std::unordered_map<uint32, uint32> doorTriedAt;  // door entry -> getMSTime() the leader last used it
        std::set<uint64> usedObjects;                // gameobjects a USE_OBJECT step has used
        std::set<uint32> noKeyLogged;                // doors the group has no key for (logged once)
        std::unordered_map<uint32, uint32> stepUses; // step index -> objects used so far
        uint32 talkAt = 0;                           // the last gossip answered during a wait
        std::unordered_map<uint32, uint32> boardPhase;   // BOARD step index -> phase (0 gather, 1 riding)
        uint32 boardSince = 0;                       // getMSTime() the current BOARD phase began
        uint32 touchedAt = 0;
    };

    std::mutex runLock;
    std::unordered_map<uint64, RunState> runs;

    uint32 const FORGET_MS = 3 * 60 * 60 * IN_MILLISECONDS;
    // A bot that moved this far counts as making progress.
    float const PROGRESS_YARDS = 4.0f;
    // A group that gave its wait up walks on for this long before it may wait again.
    uint32 const READY_FREE_MS = 45 * IN_MILLISECONDS;
    // The tank asks for the lead at most this often.
    uint32 const LEAD_REQUEST_GAP_MS = 30 * IN_MILLISECONDS;
    // How far from the leader a boss corpse is looked for, and how far a living boss is looked for from its spawn.
    float const BOSS_SEARCH_YARDS = 120.0f;
    // Nearer to a boss' spawn than this and no boss found: it is not there.
    float const BOSS_ABSENT_YARDS = 25.0f;
    // A pull may need a stricter group than the walk.
    float const BOSS_EXTRA_PERCENT = 10.0f;
    // The leader loots the corpses of a fight before anything else, but never longer than this.
    uint32 const LOOT_MAX_MS = 25 * IN_MILLISECONDS;
    // Skip keys of event steps never meet a boss entry (a creature entry never has this bit).
    uint32 const STEP_KEY = 0x80000000;
    // A gameobject or a creature is used or talked to from this close.
    float const INTERACT_YARDS = 4.5f;
    // The leader walks with an escorted creature at this distance, and looks for it within the second.
    float const FOLLOW_YARDS = 8.0f;
    float const FOLLOW_SEARCH_YARDS = 150.0f;
    // A closed door this close to the leader is opened; it is not tried again for this long.
    float const DOOR_REACH_YARDS = 9.0f;
    uint32 const DOOR_RETRY_MS = 3 * IN_MILLISECONDS;
    // A gossip along the way is not answered again for this long.
    uint32 const TALK_GAP_MS = 6 * IN_MILLISECONDS;
    // The gossip pages one answer may walk through.
    uint32 const TALK_PAGES = 3;
    // The leader clears around a crystal from this close.
    float const CLEAR_YARDS = 14.0f;
    // A creature this close along an escort is answered when it offers gossip.
    float const TALK_ALONG_YARDS = 40.0f;
    // A lift's platform: members this close (on the ground) and at its height count as standing on it, and the leader waits this long for
    // the others before it calls the lift without them.
    float const PLATFORM_YARDS = 5.5f;
    float const PLATFORM_HEIGHT = 3.5f;
    uint32 const BOARD_GATHER_MS = 30 * IN_MILLISECONDS;
    // The lift is up when its height is within this of the stop's.
    float const STOP_TOLERANCE = 3.0f;
    // The Arcane Disruptor and its like are used from this close.
    float const ITEM_CAST_YARDS = 10.0f;
    // A slipstream hangs in the air: the leader is at it when it is this close on the ground.
    float const SLIPSTREAM_YARDS = 8.0f;
    // A pause between two steps this long is a fight or a death, not a stall.
    uint32 const RESUME_GAP_MS = 20 * IN_MILLISECONDS;

    bool Elapsed(uint32 since, uint32 now, uint32 ms) { return uint32(now - since) >= ms; }

    bool GroupHasPlayer(Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->GetSession() && !member->GetSession()->IsBot())
                return true;
        }
        return false;
    }

    // Which kind of bot-led run this bot is in: a finder run (an LFG group of a bot-only run, in the core's "in dungeon"
    // state) or a party the bots formed by hand (BotLfm) that has walked in. NONE: nothing here concerns it.
    enum class RunKind
    {
        NONE,
        FINDER,
        PARTY
    };

    // A map that cannot be finished without its event steps (they are off: the run is left alone, as it was before).
    bool NeedsEventSteps(uint32 mapId)
    {
        for (EventStep const& step : EventSteps())
            if (step.mapId == mapId)
                return true;
        for (EventBoss const& boss : EventBosses())
            if (boss.mapId == mapId)
                return true;
        for (auto const& door : PassDoors())
            if (door.first == mapId)
                return true;
        return false;
    }

    RunKind KindOf(Player* bot, Group*& group)
    {
        Settings const& s = CrawlSettings();
        if (!s.enable || !bot->IsInWorld() || !bot->IsAlive())
            return RunKind::NONE;

        group = bot->GetGroup();
        if (!group)
            return RunKind::NONE;

        Map* map = bot->GetMap();
        if (!map || !map->IsDungeon() || !s.maps.count(map->GetId()) || (!s.eventSteps && NeedsEventSteps(map->GetId())))
            return RunKind::NONE;

        if (!sRandomPlayerbotMgr->IsRandomBot(bot))
            return RunKind::NONE;

        if (group->isLFGGroup())
            return LfgFill::IsBotRun(bot->GetGUIDLow()) && sLFGMgr->GetState(group->GetGUID()) == lfg::LFG_STATE_DUNGEON ?
                RunKind::FINDER : RunKind::NONE;

        return BotLfm::InsideParty(bot) ? RunKind::PARTY : RunKind::NONE;
    }

    bool HasLootPending(PlayerbotAI* botAI)
    {
        return botAI->GetAiObjectContext()->GetValue<bool>("has available loot")->Get();
    }

    // Everyone alive, healthy, with mana, and near the leader. `extra` asks for a little more (a boss pull).
    bool GroupReady(Player* leader, Group* group, Settings const& s, float extra)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld())
                continue;

            if (!member->IsAlive() || member->IsInCombat())
                return false;

            if (member != leader && (member->GetMapId() != leader->GetMapId() || member->GetExactDist(leader) > s.groupRadius))
                return false;

            if (member->GetHealthPct() < std::min(100.0f, s.healthPercent + extra))
                return false;

            uint32 const maxMana = member->GetMaxPower(POWER_MANA);
            if (maxMana && member->getPowerType() == POWER_MANA &&
                float(member->GetPower(POWER_MANA)) * 100.0f / float(maxMana) < std::min(100.0f, s.manaPercent + extra))
                return false;
        }
        return true;
    }

    bool IsSkipped(RunState& state, uint32 entry, uint32 now, Settings const& s)
    {
        auto const itr = state.skipped.find(entry);
        return itr != state.skipped.end() && !Elapsed(itr->second, now, s.skipMs);
    }

    // Marks bosses done by the instance's encounter mask (the script reported the kill).
    void RefreshDone(Player* leader, std::vector<Boss> const& bosses, RunState& state)
    {
        InstanceScript* instance = leader->GetInstanceScript();
        if (!instance)
            return;

        uint32 const mask = instance->GetCompletedEncounterMask();
        for (Boss const& boss : bosses)
            if (boss.encounterIndex < 32 && (mask & (1u << boss.encounterIndex)))
                state.done.insert(boss.entry);
    }

    // The nearest boss still to do that is not skipped; the final boss only when no other is left.
    Boss const* NextBoss(Player* leader, std::vector<Boss> const& bosses, RunState& state, uint32 now, Settings const& s)
    {
        auto const order = BossOrder().find(leader->GetMapId());
        if (order != BossOrder().end())
            for (uint32 const entry : order->second)
                for (Boss const& boss : bosses)
                    if (boss.entry == entry && !boss.final && boss.hasSpawn && !state.done.count(entry) &&
                        !IsSkipped(state, entry, now, s))
                        return &boss;

        Boss const* best = nullptr;
        float bestDist = 0.0f;
        Boss const* final = nullptr;
        for (Boss const& boss : bosses)
        {
            if (state.done.count(boss.entry) || !boss.hasSpawn || IsSkipped(state, boss.entry, now, s))
                continue;

            if (boss.final)
            {
                final = &boss;
                continue;
            }

            float const dist = leader->GetExactDist(boss.x, boss.y, boss.z);
            if (!best || dist < bestDist)
            {
                best = &boss;
                bestDist = dist;
            }
        }
        return best ? best : final;
    }

    // Every boss the leader can walk to and that is not dead is on the skip list, and no pack was pulled since the first of
    // those skips (LfgStallPolicy).
    bool RunIsStalled(std::vector<Boss> const& bosses, RunState const& state, uint32 now, Settings const& s)
    {
        uint32 left = 0, skipped = 0, firstSkip = 0;
        for (Boss const& boss : bosses)
        {
            if (state.done.count(boss.entry) || !boss.hasSpawn)
                continue;

            ++left;
            auto const itr = state.skipped.find(boss.entry);
            if (itr == state.skipped.end() || Elapsed(itr->second, now, s.skipMs))
                continue;

            // A first skip can have a passing cause (no line of sight from the spot, a boss in its intro, a scripted boss
            // not at its spawn yet): only a boss skipped again after its skip ran out counts toward a stalled run.
            auto const count = state.skipCount.find(boss.entry);
            if (count == state.skipCount.end() || count->second < LfgStallPolicy::SKIPS_FOR_STALL)
                continue;

            if (!skipped++ || int32(itr->second - firstSkip) < 0)
                firstSkip = itr->second;
        }
        return LfgStallPolicy::RunStalled(left, skipped, skipped && LfgStallPolicy::PulledSince(state.pulledAt, firstSkip));
    }

    // A pack this close ahead and in sight, nearest first; a boss, a civilian and a critter are left alone.
    Unit* PullTarget(PlayerbotAI* botAI, Player* bot, Settings const& s)
    {
        GuidVector const targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get();
        Unit* best = nullptr;
        float bestDist = s.pullRange;
        for (BotGuid guid : targets)
        {
            Unit* unit = botAI->GetUnit(guid);
            Creature* creature = unit ? unit->ToCreature() : nullptr;
            if (!creature || !creature->IsAlive() || creature->IsInCombat() || creature->IsDungeonBoss() ||
                creature->isWorldBoss() || creature->IsCivilian() || creature->GetCreatureType() == CREATURE_TYPE_CRITTER)
                continue;

            float const dist = bot->GetExactDist(creature);
            if (dist < bestDist && bot->IsWithinLOSInMap(creature))
            {
                best = creature;
                bestDist = dist;
            }
        }
        return best;
    }
    // Where an entry first spawns on the map in this difficulty (a gameobject or a creature), kept for good.
    struct SpawnPoint
    {
        bool found = false;
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    std::mutex spawnLock;
    std::map<std::tuple<uint32, uint32, bool, uint32, int32, int32>, SpawnPoint> spawnCache;

    SpawnPoint const& SpawnOf(uint32 mapId, DifficultyID difficulty, bool object, uint32 entry, float hintX, float hintY)
    {
        std::lock_guard<std::mutex> guard(spawnLock);
        auto const key = std::make_tuple(mapId, uint32(difficulty), object, entry, int32(hintX), int32(hintY));
        auto itr = spawnCache.find(key);
        if (itr != spawnCache.end())
            return itr->second;

        SpawnPoint point;
        float bestDist = 0.0f;
        uint32 const mask = 1u << uint32(difficulty);
        bool const hinted = hintX != 0.0f || hintY != 0.0f;
        auto consider = [&](float x, float y, float z)
        {
            float const dist = hinted ? std::hypot(x - hintX, y - hintY) : 0.0f;
            if (!point.found || (hinted && dist < bestDist))
            {
                point = { true, x, y, z };
                bestDist = dist;
            }
        };
        if (object)
        {
            for (auto const& data : sObjectMgr->GetAllGameObjectData())
                if (data.second.id == entry && data.second.mapid == mapId && (data.second.spawnMask & mask))
                    consider(data.second.posX, data.second.posY, data.second.posZ);
        }
        else
            for (auto const& data : sObjectMgr->GetAllCreatureData())
                if (data.second.id == entry && data.second.mapid == mapId && (data.second.spawnMask & mask))
                    consider(data.second.posX, data.second.posY, data.second.posZ);

        return spawnCache.emplace(key, point).first->second;
    }

    // Waits for the group (before a pull and before the walk goes on). True: the leader stands.
    bool HoldForGroup(Player* bot, Group* group, RunState& state, Settings const& s, uint32 now, float extra)
    {
        bool const ready = GroupReady(bot, group, s, extra);
        bool const free = state.readyFreeUntil && int32(now - state.readyFreeUntil) < 0;
        if (ready || free)
        {
            state.waitSince = 0;
            return false;
        }

        if (!state.waitSince)
            state.waitSince = now;

        state.progressAt = 0;
        if (!Elapsed(state.waitSince, now, s.readyMaxWaitMs))
            return true;

        // The wait ran out (no way to drink, a member far behind): go on, and do not wait again for a while.
        SF_LOG_INFO("playerbots", "[CRAWL] %s: the group is not ready after %u s, going on", bot->GetName().c_str(),
                    s.readyMaxWaitMs / IN_MILLISECONDS);
        state.waitSince = 0;
        state.readyFreeUntil = now + READY_FREE_MS;
        return false;
    }

    // Opens the creature's gossip like a player's click and picks the option, then the first option of each page that follows
    // (up to TALK_PAGES). It makes the two calls the world session makes for a hello and for a select, C++ scripts and
    // SmartAI creatures alike (Pusillin has no C++ script): the script manager first, then the creature's AI, then the
    // database menu. False: the creature has no such option (it offers it only when the event has not run, or not yet).
    bool TalkThrough(Player* bot, Creature* creature, uint32 option)
    {
        bot->PlayerTalkClass->ClearMenus();
        if (!sScriptMgr->OnGossipHello(bot, creature) && !creature->AI()->OnGossipHello(bot))
            bot->PrepareGossipMenu(creature, creature->GetCreatureTemplate()->GossipMenuId, true);

        bool answered = false;
        uint32 lastSender = 0, lastAction = 0;
        for (uint32 page = 0; page < TALK_PAGES; ++page)
        {
            if (!bot->PlayerTalkClass->GetGossipMenu().GetItem(option))
                break;

            uint32 const menuId = bot->PlayerTalkClass->GetGossipMenu().GetMenuId();
            uint32 const sender = bot->PlayerTalkClass->GetGossipOptionSender(option);
            uint32 const action = bot->PlayerTalkClass->GetGossipOptionAction(option);
            // A script that closes its menu without clearing it leaves the same item: not answered twice.
            if (answered && sender == lastSender && action == lastAction)
                break;

            lastSender = sender;
            lastAction = action;
            creature->AI()->sGossipSelect(bot, menuId, option);
            if (!sScriptMgr->OnGossipSelect(bot, creature, sender, action))
                bot->OnGossipSelect(creature, option, menuId);

            answered = true;
        }
        return answered;
    }

    // A creature of the entry lies dead nearby and none is alive (an escort's last boss that has no encounter bit).
    bool CreatureDeadNear(Player* bot, uint32 entry)
    {
        return !bot->FindNearestCreature(entry, BOSS_SEARCH_YARDS * 2.0f, true) &&
            bot->FindNearestCreature(entry, BOSS_SEARCH_YARDS * 2.0f, false);
    }

    // The first event step of this map (and wing) that is not over; it is due when its boss (if any) is dead. A step that was
    // skipped for a while is passed over.
    int32 DueStep(uint32 mapId, uint32 dungeonId, RunState& state, uint32 now, Settings const& s)
    {
        std::vector<EventStep> const& steps = EventSteps();
        for (uint32 i = 0; i < steps.size(); ++i)
        {
            if (steps[i].mapId != mapId || (steps[i].dungeonId && steps[i].dungeonId != dungeonId) || state.stepsDone.count(i) ||
                (steps[i].keyStep && !s.keys) || IsSkipped(state, STEP_KEY | i, now, s))
                continue;

            return !steps[i].after || state.done.count(steps[i].after) ? int32(i) : -1;
        }
        return -1;
    }

    // The thing a step is about, in sight of the leader: the nearest object not used yet, the object to clear, the creature,
    // or the slipstream nearest to the step's hint.
    WorldObject* StepSubject(Player* bot, EventStep const& step, RunState const& state)
    {
        float const range = BOSS_SEARCH_YARDS;
        if (step.kind == StepKind::USE_OBJECT)
        {
            std::list<GameObject*> objects;
            bot->GetGameObjectListWithEntryInGrid(objects, step.entry, range);
            GameObject* best = nullptr;
            for (GameObject* object : objects)
                if (!state.usedObjects.count(object->GetGUID()) && (!best || bot->GetExactDist(object) < bot->GetExactDist(best)))
                    best = object;
            return best;
        }

        if (step.kind == StepKind::CLEAR)
            return bot->FindNearestGameObject(step.entry, range);

        if (step.kind == StepKind::USE_ITEM)
        {
            std::list<Creature*> creatures;
            bot->GetCreatureListWithEntryInGrid(creatures, step.entry, range);
            Creature* best = nullptr;
            for (Creature* creature : creatures)
                if (creature->IsAlive() && !state.usedObjects.count(creature->GetGUID()) &&
                    (!best || bot->GetExactDist(creature) < bot->GetExactDist(best)))
                    best = creature;
            return best;
        }

        if (step.kind == StepKind::SLIPSTREAM)
        {
            std::list<Creature*> creatures;
            bot->GetCreatureListWithEntryInGrid(creatures, step.entry, range);
            Creature* best = nullptr;
            float bestDist = 0.0f;
            for (Creature* creature : creatures)
            {
                float const dist = std::hypot(creature->GetPositionX() - step.hintX, creature->GetPositionY() - step.hintY);
                if (!best || dist < bestDist)
                {
                    best = creature;
                    bestDist = dist;
                }
            }
            return best;
        }

        return bot->FindNearestCreature(step.entry, range, true);
    }

    // The same two calls the world session makes for a gossip click on a gameobject (the controller of a lift): the menu of its
    // template, then GameObjectAI::GossipSelect and the script manager's select. False: the object has no such option.
    bool TalkThroughObject(Player* bot, GameObject* object, uint32 option)
    {
        bot->PlayerTalkClass->ClearMenus();
        if (!sScriptMgr->OnGossipHello(bot, object) && !object->AI()->GossipHello(bot))
            bot->PrepareGossipMenu(object, object->GetGOInfo()->goober.gossipID, true);

        if (!bot->PlayerTalkClass->GetGossipMenu().GetItem(option))
            return false;

        uint32 const menuId = bot->PlayerTalkClass->GetGossipMenu().GetMenuId();
        uint32 const sender = bot->PlayerTalkClass->GetGossipOptionSender(option);
        uint32 const action = bot->PlayerTalkClass->GetGossipOptionAction(option);
        object->AI()->GossipSelect(bot, menuId, option);
        if (!sScriptMgr->OnGossipSelect(bot, object, sender, action))
            bot->OnGossipSelect(object, option, menuId);
        return true;
    }

    Transport* FindTransport(Map* map, uint32 entry)
    {
        for (Transport* transport : map->GetTransports())
            if (transport->GetEntry() == entry)
                return transport;
        return nullptr;
    }

    // Seat a bot on a transport it stands on. The core's ambient transport check (PlayerbotAI) does this for a bot whose feet are on
    // an elevator deck within a second; this is the same seat done at once so the whole group is aboard before the lift moves: stop
    // first, store the offset from the transport, then add the passenger.
    void SeatOnTransport(Player* bot, Transport* transport)
    {
        if (bot->GetTransport() == transport)
            return;

        if (bot->GetMotionMaster())
            bot->GetMotionMaster()->Clear(true);
        bot->StopMoving();

        float x = bot->GetPositionX(), y = bot->GetPositionY(), z = bot->GetPositionZ(), o = bot->GetOrientation();
        transport->CalculatePassengerOffset(x, y, z, &o);
        bot->m_movementInfo.transport.pos.Relocate(x, y, z, o);
        transport->AddPassenger(bot);
    }

    // The group of a BOARD step: every living member of the map standing on the platform.
    std::vector<Player*> PlatformMembers(Group* group, Transport* lift, uint32 mapId, bool& allOn)
    {
        std::vector<Player*> riders;
        allOn = true;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive() || member->GetMapId() != mapId)
                continue;

            if (member->GetTransport() == lift || (member->GetExactDist2d(lift->GetPositionX(), lift->GetPositionY()) <= PLATFORM_YARDS &&
                std::fabs(member->GetPositionZ() - lift->GetPositionZ()) <= PLATFORM_HEIGHT))
                riders.push_back(member);
            else
                allOn = false;
        }
        return riders;
    }

    // The item keys a door asks for: those its lock row names (the core's own rule: LOCK_KEY_ITEM cases of Lock.dbc), and the
    // owner's two keys when the rule is on.
    std::vector<uint32> DoorKeyItems(GameObject const* door, Settings const& s)
    {
        std::vector<uint32> items;
        if (LockEntry const* lock = sLockStore.LookupEntry(door->GetGOInfo()->GetLockId()))
            for (uint32 i = 0; i < MAX_LOCK_CASE; ++i)
                if (lock->Type[i] == LOCK_KEY_ITEM && lock->Index[i])
                    items.push_back(lock->Index[i]);

        if (s.keys)
        {
            auto const owner = OwnerKeyDoors().find(door->GetEntry());
            if (owner != OwnerKeyDoors().end())
                items.push_back(owner->second);
        }
        return items;
    }

    // A group member that carries one of the keys (the key may have dropped for anyone).
    Player* KeyHolder(Group* group, uint32 mapId, std::vector<uint32> const& items)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive() || member->GetMapId() != mapId)
                continue;

            for (uint32 const item : items)
                if (member->HasItemCount(item, 1))
                    return member;
        }
        return nullptr;
    }

    // A closed door of PassDoors() next to the leader is opened. GameObject::Use checks no lock (the core checks locks only in
    // the open-lock spell effect), so a door that wants a key is opened only by a member who carries it; with no key the door
    // stays shut, the leader makes no progress and skips the bosses behind it, and the run ends at its time limit if need be.
    bool OpenDoorAhead(Player* bot, Group* group, RunState& state, uint32 now, Settings const& s)
    {
        for (auto const& door : PassDoors())
        {
            if (door.first != bot->GetMapId())
                continue;

            GameObject* object = bot->FindNearestGameObject(door.second, DOOR_REACH_YARDS);
            if (!object || object->GetGoState() != GOState::GO_STATE_READY)
                continue;

            uint32& triedAt = state.doorTriedAt[door.second];
            if (triedAt && !Elapsed(triedAt, now, DOOR_RETRY_MS))
                continue;

            triedAt = now;
            Player* opener = bot;
            std::vector<uint32> const keys = DoorKeyItems(object, s);
            if (!keys.empty())
            {
                opener = KeyHolder(group, bot->GetMapId(), keys);
                if (!opener)
                {
                    if (state.noKeyLogged.insert(door.second).second)
                        SF_LOG_INFO("playerbots", "[CRAWL] %s: the group has no key for the door %u, it stays shut", bot->GetName().c_str(),
                                    door.second);
                    continue;
                }
            }

            SF_LOG_INFO("playerbots", "[CRAWL] %s opens the door %u%s", opener->GetName().c_str(), door.second,
                        keys.empty() ? "" : " with its key");
            object->Use(opener);
            return true;
        }
        return false;
    }
}  // namespace

bool LfgCrawl::Wants(PlayerbotAI* /*botAI*/, Player* bot)
{
    Group* group = nullptr;
    RunKind const kind = KindOf(bot, group);
    if (kind == RunKind::NONE || GroupHasPlayer(group))
        return false;

    // A party's leader is whoever recruited it; only a finder run has the tank take over.
    if (!group->IsLeader(bot->GetGUID()))
        return kind == RunKind::FINDER && PlayerBotSpec::IsTank(bot, true);

    return !bot->IsInCombat();
}

bool LfgCrawl::PartyFinished(uint64 groupGuid)
{
    std::lock_guard<std::mutex> guard(runLock);
    auto const itr = runs.find(groupGuid);
    return itr != runs.end() && itr->second.finished;
}

bool LfgCrawlAction::Execute(Event /*event*/)
{
    Group* group = nullptr;
    RunKind const kind = KindOf(bot, group);
    if (kind == RunKind::NONE || GroupHasPlayer(group))
        return false;

    Settings const& s = CrawlSettings();
    uint32 const now = getMSTime();
    uint64 const groupGuid = group->GetGUID();

    // A tank that does not lead asks for the lead (finder runs only: a party's leader is the one BotLfm tracks); the
    // world thread changes it.
    if (!group->IsLeader(bot->GetGUID()))
    {
        if (kind != RunKind::FINDER)
            return false;

        std::lock_guard<std::mutex> guard(runLock);
        RunState& state = runs[groupGuid];
        if (state.leadRequestAt && !Elapsed(state.leadRequestAt, now, LEAD_REQUEST_GAP_MS))
            return false;

        state.leadRequestAt = now;
        PlayerbotWorldThreadProcessor::instance().QueueOperation(
            std::make_unique<GroupSetLeaderOperation>(bot->GetGUID(), bot->GetGUID()));
        SF_LOG_INFO("playerbots", "[CRAWL] %s (tank) takes the lead of its run", bot->GetName().c_str());
        return true;
    }

    // The leader answers to nobody: the master it had, the old leader, is not one any more.
    if (botAI->GetMaster())
    {
        botAI->SetMaster(nullptr);
        botAI->ResetStrategies();
    }

    Map* map = bot->GetMap();
    // The finder dungeon of the run: the group's own for a finder run, the party's for a party BotLfm formed.
    uint32 dungeonId = 0;
    if (kind == RunKind::FINDER)
        dungeonId = sLFGMgr->GetDungeon(group->GetGUID());
    else
    {
        BotLfm::PartyInfo info;
        if (BotLfm::GetParty(bot->GetGUIDLow(), info))
            dungeonId = info.dungeonId;
    }

    std::vector<Boss> const& bosses = BossesOf(map->GetId(), map->GetDifficulty(), dungeonId);
    if (bosses.empty())
        return false;

    // The run's state is worked on as a copy and stored when the step is over.
    RunState state;
    {
        std::lock_guard<std::mutex> guard(runLock);
        state = runs[groupGuid];
        if (runs.size() > 32)
            for (auto itr = runs.begin(); itr != runs.end();)
                itr = Elapsed(itr->second.touchedAt, now, FORGET_MS) ? runs.erase(itr) : std::next(itr);
    }
    // A gap since the last step (a fight, a death) is not a stall, and the fight counts as a pull for the run's stall rule.
    if (state.touchedAt && Elapsed(state.touchedAt, now, RESUME_GAP_MS))
    {
        state.progressAt = 0;
        state.pulledAt = now;
    }
    state.touchedAt = now;
    struct Store
    {
        uint64 guid;
        RunState& state;
        ~Store()
        {
            std::lock_guard<std::mutex> guard(runLock);
            runs[guid] = state;
        }
    } store{ groupGuid, state };

    RefreshDone(bot, bosses, state);

    // Dead bosses the leader can see.
    for (Boss const& known : bosses)
    {
        if (known.hasSpawn && !state.done.count(known.entry) &&
            bot->GetExactDist(known.x, known.y, known.z) < BOSS_SEARCH_YARDS &&
            !bot->FindNearestCreature(known.entry, BOSS_SEARCH_YARDS, true) &&
            bot->FindNearestCreature(known.entry, BOSS_SEARCH_YARDS, false))
            state.done.insert(known.entry);
    }

    // The corpses of the last fight first (the default roaming action outranks the loot actions, so the leader runs them
    // itself); a loot that cannot be taken is given up after a while.
    if (HasLootPending(botAI))
    {
        if (!state.lootSince)
            state.lootSince = now;

        if (!Elapsed(state.lootSince, now, LOOT_MAX_MS))
        {
            for (char const* name : { "open loot", "move to loot", "loot" })
                if (botAI->DoSpecificAction(name, Event(), true))
                    return true;

            return true;
        }
    }
    else
        state.lootSince = 0;

    // A party's run ends with its final boss: nothing marks it finished for the core (the group is no finder group), so
    // the leader notes it and the bots leave through LfgManners.
    if (kind == RunKind::PARTY && !state.finished)
        for (Boss const& known : bosses)
            if (known.final && state.done.count(known.entry))
            {
                state.finished = true;
                SF_LOG_INFO("playerbots", "[CRAWL] %s: the final boss %s is dead, the party's run is over",
                            bot->GetName().c_str(), known.name.c_str());
            }

    // One pack ahead at a time, then on toward (x, y, z); no progress for a while skips `key` (a boss entry or an event step).
    auto travel = [&](uint32 key, char const* name, float x, float y, float z) -> bool
    {
        if (Unit* target = PullTarget(botAI, bot, s))
        {
            SF_LOG_DEBUG("playerbots", "[CRAWL] %s pulls %s on the way to %s", bot->GetName().c_str(), target->GetName().c_str(),
                         name);
            if (Attack(target))
            {
                state.progressAt = 0;
                state.pulledAt = now;
                return true;
            }
        }

        if (state.progressEntry != key || !state.progressAt ||
            bot->GetExactDist2d(state.progressX, state.progressY) >= PROGRESS_YARDS)
        {
            state.progressEntry = key;
            state.progressAt = now;
            state.progressX = bot->GetPositionX();
            state.progressY = bot->GetPositionY();
        }
        else if (Elapsed(state.progressAt, now, s.stuckMs))
        {
            SF_LOG_INFO("playerbots", "[CRAWL] %s makes no progress toward %s, skipping it for a while", bot->GetName().c_str(),
                        name);
            state.skipped[key] = now;
            ++state.skipCount[key];
            state.progressAt = 0;
            return true;
        }

        return MoveTo(map->GetId(), x, y, z);
    };

    if (s.eventSteps)
    {
        std::vector<EventStep> const& steps = EventSteps();

        // A wait in progress (an escort to follow, a creature to stand by, an event to give time): it ends with its boss dead,
        // the creature gone, or the time up. The leader walks with the creature, answers the gossip it offers on the way (and
        // that of the step's second creature), and otherwise stands; the group follows the leader and fights what comes.
        if (state.followStep >= 0)
        {
            EventStep const& step = steps[state.followStep];
            bool const withCreature = step.kind == StepKind::TALK || step.kind == StepKind::HOLD;
            Creature* escorted = withCreature ? bot->FindNearestCreature(step.entry, FOLLOW_SEARCH_YARDS, true) : nullptr;
            if ((withCreature && !escorted) || (step.until && (state.done.count(step.until) || CreatureDeadNear(bot, step.until))) ||
                Elapsed(state.followSince, now, step.seconds * IN_MILLISECONDS))
            {
                SF_LOG_INFO("playerbots", "[CRAWL] %s: the wait at %s is over", bot->GetName().c_str(), step.name);
                state.followStep = -1;
            }
            else
            {
                // The scripts check no distance for a gossip hello, so the leader answers from where it walks (the second
                // creature once it is near); a hello with nothing on offer does nothing.
                if (step.kind == StepKind::TALK && Elapsed(state.talkAt, now, TALK_GAP_MS))
                {
                    state.talkAt = now;
                    for (uint32 const entry : { step.also, step.entry })
                    {
                        Creature* speaker = entry ? bot->FindNearestCreature(entry, TALK_ALONG_YARDS, true) : nullptr;
                        // A creature that turned hostile (Pusillin at his last stop) is fought, not talked to: its gossip would
                        // start the last stop again.
                        if (speaker && speaker->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP) && !speaker->IsHostileTo(bot) &&
                            !speaker->IsInCombat() && TalkThrough(bot, speaker, step.option))
                        {
                            SF_LOG_INFO("playerbots", "[CRAWL] %s answers %s on the way", bot->GetName().c_str(), speaker->GetName().c_str());
                            return true;
                        }
                    }
                }

                if (escorted && step.kind == StepKind::TALK && bot->GetExactDist(escorted) > FOLLOW_YARDS)
                    return MoveTo(map->GetId(), escorted->GetPositionX(), escorted->GetPositionY(), escorted->GetPositionZ());
                if (escorted && step.kind == StepKind::HOLD && bot->GetExactDist(escorted) > INTERACT_YARDS * 2.0f)
                    return MoveTo(map->GetId(), escorted->GetPositionX(), escorted->GetPositionY(), escorted->GetPositionZ());
                return true;
            }
        }

        if (OpenDoorAhead(bot, group, state, now, s))
            return true;

        int32 const due = DueStep(map->GetId(), dungeonId, state, now, s);
        if (due >= 0)
        {
            EventStep const& step = steps[due];
            uint32 const key = STEP_KEY | uint32(due);
            if (step.kind == StepKind::BOARD)
            {
                Transport* lift = FindTransport(map, step.entry);
                if (!lift)
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: %s is not in this map, the step is dropped", bot->GetName().c_str(), step.name);
                    state.stepsDone.insert(due);
                    return true;
                }

                // Riding: the lift was called; wait for its stop, then everybody steps off.
                if (state.boardPhase[due] == 1)
                {
                    bool const up = lift->GetPositionZ() >= step.targetZ - STOP_TOLERANCE;
                    if (up || Elapsed(state.boardSince, now, step.seconds * IN_MILLISECONDS))
                    {
                        bool allOn = true;
                        for (Player* rider : PlatformMembers(group, lift, map->GetId(), allOn))
                            if (rider->GetTransport() == lift)
                                lift->RemovePassenger(rider);

                        SF_LOG_INFO("playerbots", "[CRAWL] %s: %s %s (z %.0f), the group steps off", bot->GetName().c_str(), step.name,
                                    up ? "is up" : "did not arrive in time", lift->GetPositionZ());
                        state.stepsDone.insert(due);
                        state.progressAt = 0;
                    }
                    return true;
                }

                // Gathering: the leader walks onto the platform and waits for the group to stand on it.
                float const lx = lift->GetPositionX(), ly = lift->GetPositionY(), lz = lift->GetPositionZ();
                if (bot->GetExactDist2d(lx, ly) > PLATFORM_YARDS * 0.5f || std::fabs(bot->GetPositionZ() - lz) > PLATFORM_HEIGHT)
                    return travel(key, step.name, lx, ly, lz);

                if (!state.boardSince)
                    state.boardSince = now;

                bool allOn = true;
                std::vector<Player*> const riders = PlatformMembers(group, lift, map->GetId(), allOn);
                if (!allOn && !Elapsed(state.boardSince, now, BOARD_GATHER_MS))
                    return true;

                // The controller: the one by the platform's floor (a lift has one at every stop).
                GameObject* controller = nullptr;
                std::list<GameObject*> controllers;
                bot->GetGameObjectListWithEntryInGrid(controllers, step.also, BOSS_SEARCH_YARDS);
                for (GameObject* candidate : controllers)
                    if (std::fabs(candidate->GetPositionZ() - lz) <= PLATFORM_HEIGHT * 2.0f &&
                        (!controller || bot->GetExactDist(candidate) < bot->GetExactDist(controller)))
                        controller = candidate;

                if (!controller)
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: no controller %u by %s (z %.0f), the step is dropped", bot->GetName().c_str(),
                                step.also, step.name, lz);
                    state.stepsDone.insert(due);
                    return true;
                }

                for (Player* rider : riders)
                    SeatOnTransport(rider, lift);

                if (!TalkThroughObject(bot, controller, step.option))
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: the controller of %s offers no option %u, the step is dropped",
                                bot->GetName().c_str(), step.name, step.option);
                    state.stepsDone.insert(due);
                    return true;
                }

                SF_LOG_INFO("playerbots", "[CRAWL] %s calls %s with %u of its group aboard", bot->GetName().c_str(), step.name,
                            uint32(riders.size()));
                state.boardPhase[due] = 1;
                state.boardSince = now;
                return true;
            }

            SpawnPoint spawn = SpawnOf(map->GetId(), map->GetDifficulty(), IsObjectStep(step.kind), step.entry, step.hintX, step.hintY);
            if (!spawn.found && (step.fallbackX != 0.0f || step.fallbackY != 0.0f))
                spawn = { true, step.fallbackX, step.fallbackY, step.fallbackZ };
            if (!spawn.found)
            {
                SF_LOG_INFO("playerbots", "[CRAWL] map %u: %s has no spawn in this difficulty, the step is dropped", map->GetId(),
                            step.name);
                state.stepsDone.insert(due);
                return true;
            }

            // A step the bot has nothing to do for: a quest it has done (or cannot take), an item it does not carry.
            if (step.kind == StepKind::QUEST_ACCEPT || step.kind == StepKind::QUEST_TURNIN)
            {
                Quest const* quest = sObjectMgr->GetQuestTemplate(step.questId);
                QuestStatus const status = bot->GetQuestStatus(step.questId);
                bool const nothingToDo = !quest ||
                    (step.kind == StepKind::QUEST_ACCEPT ? status != QUEST_STATUS_NONE || !bot->CanTakeQuest(quest, false) :
                                                           status != QUEST_STATUS_COMPLETE);
                if (nothingToDo)
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: nothing to %s for %s (quest %u is %u), going on", bot->GetName().c_str(),
                                step.kind == StepKind::QUEST_ACCEPT ? "accept" : "hand in", step.name, step.questId, uint32(status));
                    state.stepsDone.insert(due);
                    return true;
                }
            }
            else if (step.kind == StepKind::USE_ITEM && !bot->HasItemCount(step.itemId, 1))
            {
                SF_LOG_INFO("playerbots", "[CRAWL] %s carries no item %u for %s, going on", bot->GetName().c_str(), step.itemId, step.name);
                state.stepsDone.insert(due);
                return true;
            }

            if (HoldForGroup(bot, group, state, s, now, 0.0f))
                return true;

            // The thing itself when it is in view (it may stand a little off its spawn point).
            WorldObject* subject = StepSubject(bot, step, state);
            float const x = subject ? subject->GetPositionX() : spawn.x;
            float const y = subject ? subject->GetPositionY() : spawn.y;
            float const z = subject ? subject->GetPositionZ() : spawn.z;

            if (!subject && bot->GetExactDist(spawn.x, spawn.y, spawn.z) <= BOSS_ABSENT_YARDS)
            {
                SF_LOG_INFO("playerbots", "[CRAWL] %s: %s is not at its spawn, dropping the step", bot->GetName().c_str(), step.name);
                state.stepsDone.insert(due);
                return true;
            }

            if (subject && step.kind == StepKind::CLEAR)
            {
                // The script switches the crystal when its guards are dead; until then the leader clears around it.
                GameObject* object = subject->ToGameObject();
                if (object->GetGoState() != GOState::GO_STATE_READY)
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: %s is cleared", bot->GetName().c_str(), step.name);
                    state.stepsDone.insert(due);
                    return true;
                }

                if (bot->GetExactDist(subject) <= CLEAR_YARDS)
                {
                    if (Unit* target = PullTarget(botAI, bot, s))
                        if (Attack(target))
                        {
                            state.progressAt = 0;
                            state.pulledAt = now;
                            return true;
                        }

                    // Nothing in view: the guards that are left stand out of sight, wait for the stuck rule to move on.
                    if (!state.progressAt || state.progressEntry != key)
                    {
                        state.progressEntry = key;
                        state.progressAt = now;
                        state.progressX = bot->GetPositionX();
                        state.progressY = bot->GetPositionY();
                    }
                    else if (Elapsed(state.progressAt, now, step.seconds * IN_MILLISECONDS))
                    {
                        SF_LOG_INFO("playerbots", "[CRAWL] %s: %s is not cleared after %u s, moving on", bot->GetName().c_str(), step.name,
                                    step.seconds);
                        state.skipped[key] = now;
                        ++state.skipCount[key];
                        state.progressAt = 0;
                    }
                    return true;
                }
                return travel(key, step.name, x, y, z);
            }

            float const reach = step.kind == StepKind::HOLD ? FOLLOW_YARDS : step.kind == StepKind::USE_ITEM ? ITEM_CAST_YARDS : INTERACT_YARDS;
            bool const there = subject && (step.kind == StepKind::SLIPSTREAM ? bot->GetExactDist2d(subject) <= SLIPSTREAM_YARDS :
                                                                               bot->GetExactDist(subject) <= reach);
            if (there)
            {
                switch (step.kind)
                {
                    case StepKind::USE_OBJECT:
                    {
                        state.usedObjects.insert(subject->GetGUID());
                        uint32 const uses = ++state.stepUses[due];
                        SF_LOG_INFO("playerbots", "[CRAWL] %s uses %s (%u of %u)", bot->GetName().c_str(), step.name, uses,
                                    std::max(1u, step.count));
                        subject->ToGameObject()->Use(bot);
                        if (uses >= std::max(1u, step.count))
                        {
                            state.stepsDone.insert(due);
                            if (step.seconds)
                            {
                                state.followStep = due;
                                state.followSince = now;
                            }
                        }
                        return true;
                    }
                    case StepKind::QUEST_ACCEPT:
                    case StepKind::QUEST_TURNIN:
                    {
                        state.stepsDone.insert(due);
                        Creature* creature = subject->ToCreature();
                        Quest const* quest = sObjectMgr->GetQuestTemplate(step.questId);
                        // The calls of the world session for a click on a quest giver (QuestHandler.cpp accept and reward).
                        if (step.kind == StepKind::QUEST_ACCEPT)
                        {
                            if (creature->hasQuest(step.questId) && bot->CanAddQuest(quest, false))
                            {
                                bot->AddQuest(quest, creature);
                                if (bot->CanCompleteQuest(step.questId))
                                    bot->CompleteQuest(step.questId);
                                creature->AI()->sQuestAccept(bot, quest);
                                SF_LOG_INFO("playerbots", "[CRAWL] %s takes the quest %u from %s", bot->GetName().c_str(), step.questId, step.name);
                            }
                        }
                        else if (creature->hasInvolvedQuest(step.questId) && bot->CanRewardQuest(quest, 0, false))
                        {
                            bot->RewardQuest(quest, 0, creature);
                            if (!sScriptMgr->OnQuestReward(bot, creature, quest, 0))
                                creature->AI()->sQuestReward(bot, quest, 0);
                            SF_LOG_INFO("playerbots", "[CRAWL] %s hands in the quest %u to %s", bot->GetName().c_str(), step.questId, step.name);
                        }
                        return true;
                    }
                    case StepKind::USE_ITEM:
                    {
                        Creature* target = subject->ToCreature();
                        state.usedObjects.insert(target->GetGUID());
                        uint32 const uses = ++state.stepUses[due];
                        SF_LOG_INFO("playerbots", "[CRAWL] %s uses the item %u on %s (%u of %u)", bot->GetName().c_str(), step.itemId, step.name,
                                    uses, std::max(1u, step.count));
                        bot->CastSpell(target, step.spellId, true);
                        if (uses >= std::max(1u, step.count))
                            state.stepsDone.insert(due);
                        return true;
                    }
                    case StepKind::SLIPSTREAM:
                    {
                        state.stepsDone.insert(due);
                        Creature* creature = subject->ToCreature();
                        SF_LOG_INFO("playerbots", "[CRAWL] %s takes the group through %s", bot->GetName().c_str(), step.name);
                        std::vector<Player*> members;
                        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                            if (Player* member = ref->GetSource())
                                if (member->IsInWorld() && member->IsAlive() && member->GetMapId() == map->GetId())
                                    members.push_back(member);
                        for (Player* member : members)
                            sScriptMgr->OnGossipHello(member, creature);
                        return true;
                    }
                    default:
                        break;
                }

                state.stepsDone.insert(due);
                Creature* creature = subject->ToCreature();
                bool const talked = step.kind == StepKind::HOLD ||
                    (bot->GetNPCIfCanInteractWith(creature->GetGUID(), UNIT_NPC_FLAG_NONE) && TalkThrough(bot, creature, step.option));
                if (talked)
                {
                    SF_LOG_INFO("playerbots", "[CRAWL] %s %s %s", bot->GetName().c_str(),
                                step.kind == StepKind::HOLD ? "stands by" : "starts the event with", step.name);
                    state.followStep = due;
                    state.followSince = now;
                }
                else
                    SF_LOG_INFO("playerbots", "[CRAWL] %s: %s offers no event now, going on", bot->GetName().c_str(), step.name);
                return true;
            }

            return travel(key, step.name, x, y, z);
        }
    }

    Boss const* boss = NextBoss(bot, bosses, state, now, s);
    if (!boss)
    {
        // Nothing left that can be reached: the final boss is dead (the core ends the run) or out of reach. When every boss
        // left is skipped and nothing was pulled since the first skip, the run is given up now instead of at its time limit.
        // The leader stands, so nothing wanders off.
        if (!state.stalled && RunIsStalled(bosses, state, now, s))
        {
            state.stalled = true;
            SF_LOG_INFO("playerbots", "[CRAWL] %s: every boss left was skipped and nothing was pulled, the run is given up",
                        bot->GetName().c_str());
            std::vector<uint32> finderBots;
            if (kind == RunKind::FINDER)
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (member && member->GetSession() && member->GetSession()->IsBot())
                        finderBots.push_back(member->GetGUIDLow());
                }
            }
            else
            {
                // The hand-formed party has no run limit of the finder; its leader calls the run over like a finished one.
                state.finished = true;
            }

            LfgFill::NoteRunFailed(dungeonId, finderBots);
        }
        return true;
    }

    // Waiting for the group before anything else: a pull and the walk both need it.
    float const bossDistance = bot->GetExactDist(boss->x, boss->y, boss->z);
    bool const bossNear = bossDistance <= s.bossPullRange;
    if (HoldForGroup(bot, group, state, s, now, bossNear ? BOSS_EXTRA_PERCENT : 0.0f))
        return true;

    // A boss within reach: pull it (an event-summoned one through its anchor, which starts the fight). Not there at all (a
    // script summons it, or it died unseen): skip it; an event-summoned boss is waited for first.
    if (bossNear)
    {
        Unit* living = boss->anchor ? bot->FindNearestCreature(boss->anchor, BOSS_SEARCH_YARDS, true) : nullptr;
        if (!living)
            living = bot->FindNearestCreature(boss->entry, BOSS_SEARCH_YARDS, true);

        if (living)
        {
            state.eventWaitSince = 0;
            SF_LOG_INFO("playerbots", "[CRAWL] %s pulls the boss %s", bot->GetName().c_str(), boss->name.c_str());
            if (Attack(living))
            {
                state.pulledAt = now;
                return true;
            }

            state.skipped[boss->entry] = now;
            ++state.skipCount[boss->entry];
            return false;
        }

        if (bossDistance <= BOSS_ABSENT_YARDS)
        {
            if (boss->anchor)
            {
                if (state.eventWaitEntry != boss->entry || !state.eventWaitSince)
                {
                    state.eventWaitEntry = boss->entry;
                    state.eventWaitSince = now;
                    SF_LOG_INFO("playerbots", "[CRAWL] %s waits for the boss %s to appear", bot->GetName().c_str(),
                                boss->name.c_str());
                }

                if (!Elapsed(state.eventWaitSince, now, s.eventWaitMs))
                    return true;

                state.eventWaitSince = 0;
            }

            SF_LOG_INFO("playerbots", "[CRAWL] %s: the boss %s is not at its spawn, skipping it", bot->GetName().c_str(),
                        boss->name.c_str());
            state.skipped[boss->entry] = now;
            ++state.skipCount[boss->entry];
            return true;
        }
    }

    return travel(boss->entry, boss->name.c_str(), boss->x, boss->y, boss->z);
}
