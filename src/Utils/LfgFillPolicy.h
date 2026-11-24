#ifndef AWARE_BOTS_LFG_FILL_POLICY_H
#define AWARE_BOTS_LFG_FILL_POLICY_H

#include <cstdint>

// What the finder fill (Manager/LfgFill.cpp, ReleaseStaleBots) does with a bot it queued, by the bot's finder state.
// BT-205: the fill forgot a bot as soon as its state left "queued", so also while a proposal was open. When the proposal
// failed (a member declined, or did not answer in time), the core put every member that had accepted back in the queue
// (LFGMgr::RemoveProposal, "Readding to queue") and nothing released those bots any more: they stayed queued after the
// player was gone, out of reach of every later fill (FreeForFinder takes only bots with no finder state) until they
// logged out. A bot in a role check or a proposal is now kept and judged again once it is back in the queue.
namespace LfgFillPolicy
{
// lfg::LfgState of the core (DungeonFinding/LFG.h), by value; LfgFill.cpp checks that the numbers still match.
enum class QueueState : uint8_t
{
    None = 0,
    RoleCheck = 1,
    Queued = 2,
    Proposal = 3,
    Boot = 4,
    Dungeon = 5,
    FinishedDungeon = 6,
    RaidBrowser = 7
};

enum class Track : uint8_t
{
    Forget,  // out of the queue: no finder state, or in a group (the dungeon, a kick vote)
    Wait,    // the core is asking (role check, proposal); the answer decides, the fill does not touch it
    Judge    // queued: release it when nobody waits for its dungeons any more or it waited too long
};

constexpr Track TrackQueuedBot(QueueState state)
{
    switch (state)
    {
        case QueueState::Queued:
            return Track::Judge;
        case QueueState::RoleCheck:
        case QueueState::Proposal:
            return Track::Wait;
        default:
            return Track::Forget;
    }
}

// A bot judged again after it waited on a proposal came back from a failed one; the fill says so once.
constexpr bool BackFromFailedProposal(bool waitedOnProposal, Track now)
{
    return waitedOnProposal && now == Track::Judge;
}

// BT-207: "no free bot fits. Missing: 1 tank" said what was missing, never where the bots of that level were. When the fill
// has nothing to give it now counts the finder bots of the player's side near the player's level by the first reason
// FreeForFinder (LfgFill.cpp) finds, and the free ones by role.
enum class Hold : uint8_t
{
    Free,
    Grouped,       // in a group or invited (a bot party, an old finder group)
    Busy,          // dead, fighting, flying or teleporting
    Battleground,  // in a battleground or arena, or queued for one
    Instance,      // inside an instance
    Blocked,       // deserter or dungeon cooldown
    Queued,        // has a finder state already (queued, proposal, dungeon)
    Refused,       // the finder refused it a moment ago
    Shopping,      // a caster on its way to buy drink
    Count
};

constexpr int ROLE_COUNT = 3;  // tank, healer, damage, as LfgFill.cpp's RoleIndex

struct Census
{
    uint32_t held[uint32_t(Hold::Count)] = {};
    uint32_t freeByRole[ROLE_COUNT] = {};

    // role: 0 tank, 1 healer, 2 damage, anything else unknown (counted as free, not by role).
    void Add(Hold hold, int role)
    {
        ++held[uint32_t(hold)];
        if (hold == Hold::Free && role >= 0 && role < ROLE_COUNT)
            ++freeByRole[role];
    }

    uint32_t Total() const
    {
        uint32_t total = 0;
        for (uint32_t count : held)
            total += count;
        return total;
    }
};

// A party bot is considered only for a missing tank or healer with no free bot of that role in the band.
constexpr bool RoleShortage(int role, uint32_t missing, uint32_t freeInBand)
{
    return (role == 0 || role == 1) && missing > 0 && freeInBand == 0;
}

constexpr bool PartyEligible(bool grouped, bool allMembersBots, bool finderGroup, bool otherwiseFree)
{
    return grouped && allMembersBots && !finderGroup && otherwiseFree;
}

// Counts are the free bots remaining in the same band after the selected party is removed.
constexpr bool KeepsFreeGroup(uint32_t tanks, uint32_t healers, uint32_t damage)
{
    return tanks >= 1 && healers >= 1 && damage >= 3;
}

// A bot the census looks at: of the finder's levels, within the fill's level window of the waiting player.
constexpr bool InReach(uint32_t botLevel, uint32_t playerLevel, uint32_t window, uint32_t minLevel)
{
    return botLevel >= minLevel && (botLevel > playerLevel ? botLevel - playerLevel : playerLevel - botLevel) <= window;
}

// Stockade proof run 2026-11-23: the finder filled a level 21 tank for a level 24 player, the pack mobs were level 23 to 24,
// and one pull chained 14 mobs and wiped. A tank takes the hits of mobs that level up to the player's, so a filled tank must
// not trail the player by more than a few levels; healers and damage dealers keep the plain level window. The floor is the
// player's level minus `below`, and never under the dungeon's own minimum level.
constexpr uint32_t TankLevelFloor(uint32_t playerLevel, uint32_t below, uint32_t dungeonMinLevel)
{
    uint32_t const wanted = playerLevel > below ? playerLevel - below : 0;
    return wanted > dungeonMinLevel ? wanted : dungeonMinLevel;
}

constexpr bool TankLevelOk(uint32_t botLevel, uint32_t playerLevel, uint32_t below, uint32_t dungeonMinLevel)
{
    return botLevel >= TankLevelFloor(playerLevel, below, dungeonMinLevel);
}

// Night proof runs 2026-11-16 (Shadowfang Keep): two of four filled bots were in a fight a few seconds after they queued
// and turned the proposal down, which sends everyone back to the queue and asks the player to accept again (the run found
// no group in 2 min). A player in a fight accepts too and is ported when the fight is over: the core only refuses the
// teleport itself (LFGMgr::TeleportPlayer), and the teleport retry asks again every 15 s. Only a dead bot declines, it
// cannot be ported at all until it is back on its feet.
constexpr bool AcceptsProposal(bool dead)
{
    return !dead;
}
}  // namespace LfgFillPolicy

#endif
