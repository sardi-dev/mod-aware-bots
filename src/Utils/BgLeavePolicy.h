/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGLEAVEPOLICY_H
#define _PLAYERBOT_BGLEAVEPOLICY_H

// The pure rules of the `botscenario <bot|all> bgleave` test verb: which bots it takes out of a battleground or arena and where
// they land. After a bots-only match the seeded bots stayed flagged as inside a battleground (live, build 28: `bg` refused with
// "already in a battleground" after `bgend`, a teleport out and `leavegroup`), so the next match could not be seeded. The verb
// removes the bot from its match, clears its queue slots and battleground ids, and sends it to the point it entered from, or to
// its homebind when that point is missing or is itself a battleground map.
// Pure logic, free of game types (tests/test_bg_leave_policy.cpp). The glue is Scenario/BotScenarioQueue.cpp.

#include <cctype>
#include <string>

namespace BgLeavePolicy
{
// The name the verb reads as "every seeded test bot that is stuck in a match".
inline bool IsAllName(std::string const& name)
{
    if (name.size() != 3)
        return false;
    for (std::size_t i = 0; i < 3; ++i)
        if (std::tolower(static_cast<unsigned char>(name[i])) != "all"[i])
            return false;
    return true;
}

// `botscenario - bgleave all` is how the other name-less verbs are written (`botscenario - bgtest on`), so the verb also reads the
// target from its next word when the name is a dash.
inline std::string TargetName(std::string const& name, std::string const& nextWord)
{
    return name == "-" && !nextWord.empty() ? nextWord : name;
}

struct Facts
{
    bool isBot = false;
    bool inMatch = false;       // the character stands in a live battleground or arena object
    bool matchHasHuman = false; // a player that is no bot takes part in that match
    bool staleBgId = false;     // flagged as inside a battleground, but no match object exists any more
};

// `all` never touches a match a human plays in (the fill bots of a real match stay), nor anyone who is no bot; it takes the bots
// of a bots-only match and the bots left flagged by a match that is gone. A bot named on its own is always taken (the tester asked).
inline bool IncludedInAll(Facts const& f)
{
    if (!f.isBot)
        return false;
    return (f.inMatch && !f.matchHasHuman) || f.staleBgId;
}

enum class Landing
{
    STAY,         // not on a battleground or arena map: the bot is where it should be
    ENTRY_POINT,  // the point the bot entered the match from
    HOMEBIND      // the entry point is missing or is a battleground map itself (a bot put there by a test teleport)
};

inline Landing LandingFor(bool onMatchMap, bool entryValid, bool entryOnMatchMap)
{
    if (!onMatchMap)
        return Landing::STAY;
    return entryValid && !entryOnMatchMap ? Landing::ENTRY_POINT : Landing::HOMEBIND;
}

inline char const* LandingName(Landing landing)
{
    switch (landing)
    {
        case Landing::ENTRY_POINT: return "entry";
        case Landing::HOMEBIND: return "homebind";
        case Landing::STAY: break;
    }
    return "stay";
}
}  // namespace BgLeavePolicy

#endif
