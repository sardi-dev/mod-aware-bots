/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// BT-154: the ladder NPC, "Pit Chronicler" (creature_template 9000100, module install data in
// data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql). It stands next to the arena battlemasters and reads the books of the bot arena ladder
// to whoever talks to it: the ranking, the page of one team and the matches in progress. Gossip only; every line is plain
// text added here, so the only database rows it needs are the template, its menu and three static texts. The module owns the
// NPC: it spawns it at run time (ArenaNpcSpawn.cpp, spots in ArenaNpcSpots.h) and no creature row exists. Nothing on the
// gossip path touches the database: it reads the RAM copy of the books (ArenaLadder.h).

#include "ArenaLadder.h"
#include "ArenaText.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"

#include <string>

namespace
{
// npc_text ids of data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql (the reserved range 9000100 to 9000199 of custom content).
constexpr uint32 TEXT_WELCOME = 9000100;  // "I keep the books of the pit. Which ladder do you want to read?"
constexpr uint32 TEXT_BOOKS = 9000101;    // "Here is what the books say."
constexpr uint32 TEXT_EMPTY = 9000102;    // "The books are empty for now. The teams are still finding their feet."

constexpr uint32 PAGE_SHIFT = 16;
constexpr uint32 ARG_MASK = 0xFFFF;
enum Page : uint32
{
    PAGE_HOME = 0,
    PAGE_LADDER = 1,
    PAGE_TEAM = 2,
    PAGE_LIVE = 3
};

constexpr uint32 LADDER_LINES = 10;   // teams shown per bracket
constexpr uint32 RESULT_LINES = 5;
constexpr uint32 SECONDS_PER_MINUTE = 60;

uint32 Action(Page page, uint32 arg)
{
    return (uint32(page) << PAGE_SHIFT) | (arg & ARG_MASK);
}

std::string ResultsLine(std::string const& results)
{
    std::string line = "Last results:";
    for (char const c : results)
        line += std::string(" ") + c;
    return results.empty() ? std::string("No matches played yet") : line;
}

class npc_bot_arena_ladder : public CreatureScript
{
public:
    npc_bot_arena_ladder() : CreatureScript("npc_bot_arena_ladder") {}

    bool OnGossipHello(Player* player, Creature* creature) OVERRIDE
    {
        ShowHome(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) OVERRIDE
    {
        uint32 const arg = action & ARG_MASK;
        switch (Page(action >> PAGE_SHIFT))
        {
            case PAGE_LADDER:
                ShowLadder(player, creature, uint8(arg));
                break;
            case PAGE_TEAM:
                ShowTeam(player, creature, arg);
                break;
            case PAGE_LIVE:
                ShowLive(player, creature);
                break;
            case PAGE_HOME:
            default:
                ShowHome(player, creature);
                break;
        }
        return true;
    }

private:
    static void ShowHome(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();

        std::vector<ArenaLadder::TeamSummary> const teams = ArenaLadder::Ladder(ArenaLadder::BRACKET_2V2);
        if (!ArenaLadder::Enabled() || teams.empty())
        {
            player->SEND_GOSSIP_MENU(TEXT_EMPTY, creature->GetGUID());
            return;
        }

        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "2v2 ladder (" + std::to_string(teams.size()) + " teams)", GOSSIP_SENDER_MAIN,
            Action(PAGE_LADDER, ArenaLadder::BRACKET_2V2));
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Teams fighting now", GOSSIP_SENDER_MAIN, Action(PAGE_LIVE, 0));
        player->SEND_GOSSIP_MENU(TEXT_WELCOME, creature->GetGUID());
    }

    static void ShowLadder(Player* player, Creature* creature, uint8 bracket)
    {
        player->PlayerTalkClass->ClearMenus();

        std::vector<ArenaLadder::TeamSummary> const teams = ArenaLadder::Ladder(bracket);
        if (teams.empty())
        {
            player->SEND_GOSSIP_MENU(TEXT_EMPTY, creature->GetGUID());
            return;
        }

        uint32 shown = 0;
        for (ArenaLadder::TeamSummary const& team : teams)
        {
            if (shown++ >= LADDER_LINES)
                break;
            std::string const line = ArenaText::Fit(ArenaText::LadderLine(team.rank, team.name, team.rating, team.wins, team.losses,
                team.draws, team.streak, team.tier));
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, line, GOSSIP_SENDER_MAIN, Action(PAGE_TEAM, team.id));
        }
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, Action(PAGE_HOME, 0));
        player->SEND_GOSSIP_MENU(TEXT_BOOKS, creature->GetGUID());
    }

    static void ShowTeam(Player* player, Creature* creature, uint32 teamId)
    {
        player->PlayerTalkClass->ClearMenus();

        ArenaLadder::TeamSummary team;
        if (!ArenaLadder::FindTeam(teamId, team))
        {
            ShowHome(player, creature);
            return;
        }

        auto line = [&](std::string const& text, uint8 icon = GOSSIP_ICON_CHAT)
        {
            player->ADD_GOSSIP_ITEM(icon, ArenaText::Fit(text), GOSSIP_SENDER_MAIN, Action(PAGE_TEAM, teamId));
        };

        std::string header = team.name + " (" + (team.faction == 0 ? "Alliance" : "Horde") + ", " + ArenaText::TierName(team.tier) + ")";
        if (team.rank)
            header += ", rank " + std::to_string(team.rank);
        line(header, GOSSIP_ICON_BATTLE);
        line("Rating " + std::to_string(team.rating) + " (best " + std::to_string(team.peakRating) + "), " +
            ArenaText::RecordText(team.wins, team.losses, team.draws));
        std::string const run = ArenaText::StreakText(team.streak);
        line((run.empty() ? std::string("No streak now") : "Streak: " + run) + ", best run " + std::to_string(team.bestStreak));
        line(ResultsLine(team.lastResults));
        if (!team.rival.empty())
            line("Rival: " + team.rival);
        for (ArenaLadder::MemberSummary const& member : team.members)
            line(ArenaText::RosterLine(member.name, member.classId, member.specId, member.healer));
        if (team.playingNow)
            line("In a match right now");

        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Back to the ladder", GOSSIP_SENDER_MAIN, Action(PAGE_LADDER, team.bracket));
        player->SEND_GOSSIP_MENU(TEXT_BOOKS, creature->GetGUID());
    }

    static void ShowLive(Player* player, Creature* creature)
    {
        player->PlayerTalkClass->ClearMenus();

        std::vector<ArenaLadder::LiveSummary> const live = ArenaLadder::LiveMatches();
        if (live.empty())
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Nobody is fighting right now", GOSSIP_SENDER_MAIN, Action(PAGE_LIVE, 0));
        for (ArenaLadder::LiveSummary const& match : live)
        {
            std::string const minutes = std::to_string(match.seconds / SECONDS_PER_MINUTE);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE,
                ArenaText::Fit(match.teamA + " against " + match.teamB + " (" + match.state + ", " + minutes + " min)"),
                GOSSIP_SENDER_MAIN, Action(PAGE_LIVE, 0));
        }
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, Action(PAGE_HOME, 0));
        player->SEND_GOSSIP_MENU(TEXT_BOOKS, creature->GetGUID());
    }
};
}  // namespace

void AddSC_npc_bot_arena_ladder()
{
    new npc_bot_arena_ladder();
}
