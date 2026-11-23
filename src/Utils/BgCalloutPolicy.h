/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGCALLOUTPOLICY_H
#define _PLAYERBOT_BGCALLOUTPOLICY_H

// BT-482: the human call-out reader (owner decisions 2026-11-18): the short lines a human writes in the battleground chat
// ("inc bs 3", "help fr", "efc mid", "3 inc st", "fc", "gm clear") are read as INFORMATION for the plans, never as commands.
// There is no command layer: a line becomes an observation (PvpObservationMath.h) that the plan weighs against what it sees,
// and a casual bot may ignore it.
//
// One parser serves the three maps with their own place words: Warsong Gulch and Twin Peaks (zones of the flag game) and
// Arathi Basin (the five nodes). Free of game types so that tests/ can run it (tests/test_bg_callout.cpp).
//
// What is read:
//  - inc, incoming       enemies come to <place>; a number may say how many ("inc bs 3", "3 inc bs")
//  - help                help is needed at <place>, or here when no place is said (the plan then uses the sender's position)
//  - efc                 the enemy carries our flag at <place> (Warsong Gulch, Twin Peaks)
//  - fc, flag            our carrier of their flag: the sender when no place is said (a bare "flag" only; with another verb
//                        "flag" is the place word for the flag room)
//  - take, cap, capture  the sender takes <node> (Arathi Basin): a claim about a friend, not an order
//  - lost                <node> is lost (Arathi Basin)
//  - clear               <place> has no enemies any more
// A line needs a place when its kind is about a place (inc, take, lost, clear). Anything else is not read: a line longer than
// a short call, a question, a negation, two different verbs, a verb of another map, or words that are no verb ("everyone go
// bs", "attack farm").

#include "AbPlanLogic.h"
#include "BgPlanCommon.h"
#include "PvpObservationMath.h"
#include "SmPlanLogic.h"
#include "TpPlanLogic.h"
#include "WsgPlanLogic.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BgCallout
{
enum class MapKind : std::uint8_t
{
    NONE,
    WSG,
    TP,
    AB,
    BFG,
    EYE,
    SM
};

// The battleground maps the reader knows. Arathi Basin's map id is the real one (the Random Battleground queue keeps its own
// type id, the map id is reliable).
constexpr std::uint32_t AB_MAP_ID = 529;
constexpr std::uint32_t WSG_MAP_ID = 489;

inline MapKind MapKindOf(std::uint32_t mapId)
{
    if (mapId == WSG_MAP_ID)
        return MapKind::WSG;
    if (mapId == TpLogic::TP_MAP_ID)
        return MapKind::TP;
    if (mapId == AB_MAP_ID)
        return MapKind::AB;
    if (mapId == BgNodeMap::GILNEAS.mapId)
        return MapKind::BFG;
    if (mapId == BgNodeMap::EYE.mapId)
        return MapKind::EYE;
    if (mapId == SmLogic::MAP_ID)
        return MapKind::SM;
    return MapKind::NONE;
}

struct Report
{
    bool valid = false;
    PvpObs::Kind kind = PvpObs::Kind::INCOMING;
    std::uint8_t place = PvpObs::PLACE_UNKNOWN;   // the map's own place (WsgLogic::Place) or node id
    std::uint8_t count = 0;                       // 0 when no number was said
};

namespace detail
{
using WsgLogic::Place;

constexpr std::uint8_t P(Place place)
{
    return static_cast<std::uint8_t>(place);
}

// The verbs: a word and the kind it says. "fc" and "flag" are the carrier words, handled apart.
inline BgPlan::WordId const* Verbs(std::size_t& count)
{
    using PvpObs::Kind;
    static BgPlan::WordId const verbs[] = {
        { "inc", static_cast<std::uint8_t>(Kind::INCOMING) },      { "incoming", static_cast<std::uint8_t>(Kind::INCOMING) },
        { "help", static_cast<std::uint8_t>(Kind::HELP) },         { "efc", static_cast<std::uint8_t>(Kind::ENEMY_CARRIER) },
        { "take", static_cast<std::uint8_t>(Kind::CLAIM) },        { "cap", static_cast<std::uint8_t>(Kind::CLAIM) },
        { "capture", static_cast<std::uint8_t>(Kind::CLAIM) },     { "lost", static_cast<std::uint8_t>(Kind::LOST) },
        { "clear", static_cast<std::uint8_t>(Kind::CLEAR) }
    };
    count = std::size(verbs);
    return verbs;
}

// Place words of the flag maps. The words that name a zone of Warsong Gulch only (tunnel, ramp, roof) are not read on Twin
// Peaks, and the lane, buff and door words of Twin Peaks (BT-474) are not read on Warsong Gulch.
inline BgPlan::WordId const* FlagPlaces(MapKind map, std::size_t& count)
{
    static BgPlan::WordId const wsg[] = {
        { "fr", P(Place::FLAG_ROOM) },     { "flag", P(Place::FLAG_ROOM) },  { "room", P(Place::FLAG_ROOM) },
        { "base", P(Place::FLAG_ROOM) },   { "home", P(Place::FLAG_ROOM) },  { "tun", P(Place::TUNNEL) },
        { "tunnel", P(Place::TUNNEL) },    { "mid", P(Place::MID) },         { "middle", P(Place::MID) },
        { "ramp", P(Place::RAMP) },        { "roof", P(Place::ROOF) },       { "gy", P(Place::GRAVEYARD) },
        { "graveyard", P(Place::GRAVEYARD) }, { "yard", P(Place::GRAVEYARD) }
    };
    static BgPlan::WordId const tp[] = {
        { "fr", P(Place::FLAG_ROOM) },     { "flag", P(Place::FLAG_ROOM) },  { "room", P(Place::FLAG_ROOM) },
        { "base", P(Place::FLAG_ROOM) },   { "home", P(Place::FLAG_ROOM) },  { "mid", P(Place::MID) },
        { "middle", P(Place::MID) },       { "gy", P(Place::GRAVEYARD) },    { "graveyard", P(Place::GRAVEYARD) },
        { "yard", P(Place::GRAVEYARD) },   { "door", P(Place::DOOR) },       { "doors", P(Place::DOOR) }
    };
    if (map == MapKind::TP)
    {
        count = std::size(tp);
        return tp;
    }
    count = std::size(wsg);
    return wsg;
}

// Twin Peaks names a lane, a berserk buff and a leaf (restoration) buff by the side it belongs to: two words ("lane a").
inline std::uint8_t TwinPeaksPair(std::string const& first, std::string const& second)
{
    bool const alliance = second == "a" || second == "ally" || second == "alliance";
    bool const horde = second == "h" || second == "horde";
    if (!alliance && !horde)
        return BgPlan::WORD_NONE;

    if (first == "lane")
        return P(alliance ? Place::LANE_A : Place::LANE_H);
    if (first == "bers" || first == "berserk" || first == "zerk")
        return P(alliance ? Place::BERS_A : Place::BERS_H);
    if (first == "leaf" || first == "regen" || first == "resto")
        return P(alliance ? Place::LEAF_A : Place::LEAF_H);
    return BgPlan::WORD_NONE;
}

// The node words of Battle for Gilneas (BT-552): lh, ww and mines with the longer names a player types. "water works" is two words
// for one place, like "gold mine".
inline BgPlan::WordId const* GilneasNodes(std::size_t& count)
{
    static BgPlan::WordId const nodes[] = {
        { "lh", 0 },    { "light", 0 },   { "lighthouse", 0 },
        { "ww", 1 },    { "water", 1 },   { "waterworks", 1 },  { "works", 1 },
        { "mines", 2 }, { "mine", 2 },    { "mn", 2 }
    };
    count = std::size(nodes);
    return nodes;
}

// The base words of Eye of the Storm (BT-585): fr, be, dr and mt with the names a player types, and "mid" for the flag in the middle
// (id 4, a place that is no base: the plan never indexes a base with it). Two-word names ("fel reaver", "blood elf", "mage tower")
// are read as one place; "tower", "ruins", "elf" and "base" stand in a line as fillers (IsEyeFiller).
inline BgPlan::WordId const* EyeNodes(std::size_t& count)
{
    static BgPlan::WordId const nodes[] = {
        { "fr", 0 },    { "fel", 0 },     { "reaver", 0 },
        { "be", 1 },    { "blood", 1 },   { "bloodelf", 1 },
        { "dr", 2 },    { "draenei", 2 },
        { "mt", 3 },    { "mage", 3 },    { "magetower", 3 },
        { "mid", 4 },   { "middle", 4 },  { "centre", 4 },    { "center", 4 }
    };
    count = std::size(nodes);
    return nodes;
}

inline bool IsEyeFiller(std::string const& word)
{
    return word == "tower" || word == "ruins" || word == "elf" || word == "base";
}

// The place words of Silvershard Mines (BT-613, sheet section 8): the three carts by the names players use (lava is the east cart, mid the
// waterfall cart, top the north cart), the Diamond depot ("long") and the two switches. "east" and "north" mean the switch of that track;
// "switch", "sw", "track" and "cart" stand in a line as fillers (IsSmFiller).
inline BgPlan::WordId const* SilvershardPlaces(std::size_t& count)
{
    static BgPlan::WordId const places[] = {
        { "lava", SmLogic::CART_EAST },          { "mid", SmLogic::CART_WATERFALL },   { "middle", SmLogic::CART_WATERFALL },
        { "waterfall", SmLogic::CART_WATERFALL }, { "wf", SmLogic::CART_WATERFALL },    { "top", SmLogic::CART_NORTH },
        { "troll", SmLogic::CART_NORTH },        { "long", SmLogic::PLACE_LONG },      { "diamond", SmLogic::PLACE_LONG },
        { "east", SmLogic::PLACE_SWITCH_EAST },  { "es", SmLogic::PLACE_SWITCH_EAST }, { "north", SmLogic::PLACE_SWITCH_NORTH },
        { "ns", SmLogic::PLACE_SWITCH_NORTH }
    };
    count = std::size(places);
    return places;
}

inline bool IsSmFiller(std::string const& word)
{
    return word == "switch" || word == "sw" || word == "track" || word == "cart";
}

inline BgPlan::WordId const* Nodes(std::size_t& count, MapKind map = MapKind::AB)
{
    using namespace AbLogic;
    if (map == MapKind::SM)
        return SilvershardPlaces(count);
    if (map == MapKind::EYE)
        return EyeNodes(count);
    if (map == MapKind::BFG)
        return GilneasNodes(count);
    static BgPlan::WordId const nodes[] = {
        { "st", NODE_STABLES },       { "stable", NODE_STABLES },     { "stables", NODE_STABLES },
        { "bs", NODE_BLACKSMITH },    { "smith", NODE_BLACKSMITH },   { "blacksmith", NODE_BLACKSMITH },
        { "farm", NODE_FARM },        { "fm", NODE_FARM },
        { "lm", NODE_LUMBER_MILL },   { "lumber", NODE_LUMBER_MILL }, { "mill", NODE_LUMBER_MILL },
        { "lumbermill", NODE_LUMBER_MILL },
        { "gm", NODE_GOLD_MINE },     { "gold", NODE_GOLD_MINE },     { "mine", NODE_GOLD_MINE },
        { "goldmine", NODE_GOLD_MINE }
    };
    count = std::size(nodes);
    return nodes;
}

inline bool IsNegation(std::string const& word)
{
    return word == "no" || word == "not" || word == "nothing" || word == "dont" || word == "don" || word == "never" ||
           word == "stop";
}

// Words that may stand in a call-out next to the verb, the place and the number.
inline bool IsFiller(std::string const& word)
{
    static char const* const fillers[] = { "at", "to", "in", "on", "the", "from", "near", "now", "here", "pls", "plz", "please",
                                           "guys", "going", "coming", "me", "us", "x" };
    for (char const* filler : fillers)
        if (word == filler)
            return true;
    return false;
}

inline bool IsNumber(std::string const& word)
{
    if (word.empty() || word.size() > 3)
        return false;
    for (char c : word)
        if (c < '0' || c > '9')
            return false;
    return true;
}

inline bool IsCount(std::string const& word, std::uint8_t& value)
{
    if (word.empty() || word.size() > 2)
        return false;
    for (char c : word)
        if (c < '0' || c > '9')
            return false;
    std::uint32_t const n = static_cast<std::uint32_t>(std::stoul(word));
    if (n < 1 || n > PvpObs::MAX_REPORTED_COUNT)
        return false;
    value = static_cast<std::uint8_t>(n);
    return true;
}

// Whether a kind is read on a map: the carriers belong to the flag maps, claims and lost nodes to the node map.
inline bool KindFitsMap(PvpObs::Kind kind, MapKind map)
{
    bool const flagMap = map == MapKind::WSG || map == MapKind::TP || map == MapKind::EYE;
    switch (kind)
    {
        case PvpObs::Kind::ENEMY_CARRIER:
        case PvpObs::Kind::OWN_CARRIER:
            return flagMap;
        case PvpObs::Kind::CLAIM:
        case PvpObs::Kind::LOST:
            return map == MapKind::AB || map == MapKind::BFG || map == MapKind::EYE || map == MapKind::SM;
        default:
            return map != MapKind::NONE;
    }
}

// A kind about a place needs one; "help", "efc" and the carrier lines may come without.
inline bool NeedsPlace(PvpObs::Kind kind)
{
    return kind == PvpObs::Kind::INCOMING || kind == PvpObs::Kind::CLAIM || kind == PvpObs::Kind::LOST ||
           kind == PvpObs::Kind::CLEAR;
}
}  // namespace detail

// Reads one line of chat. An invalid report is "not a call-out": the line is ignored.
inline Report Parse(std::string const& text, MapKind map)
{
    Report report;
    if (map == MapKind::NONE)
        return report;
    // A question is not a report ("inc bs?"), and a negation is the opposite of one.
    if (text.find('?') != std::string::npos)
        return report;

    std::vector<std::string> const words = BgPlan::CallWords(text);
    if (words.empty())
        return report;

    std::size_t verbCount = 0;
    BgPlan::WordId const* const verbs = detail::Verbs(verbCount);

    // The verb: the words of the table, all of one kind; two kinds make the line ambiguous.
    bool haveKind = false;
    PvpObs::Kind kind = PvpObs::Kind::INCOMING;
    std::size_t verbIndex = words.size();
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        if (detail::IsNegation(words[i]))
            return report;
        std::uint8_t const id = BgPlan::LookupWord(words[i], verbs, verbCount);
        if (id == BgPlan::WORD_NONE)
            continue;
        PvpObs::Kind const found = static_cast<PvpObs::Kind>(id);
        if (haveKind && found != kind)
            return report;
        if (!haveKind)
            verbIndex = i;
        haveKind = true;
        kind = found;
    }

    // "fc" and "flag" say the carrier when no other verb is there (with one, "flag" is the flag room).
    if (!haveKind)
    {
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            if (words[i] == "fc" || words[i] == "flag")
            {
                haveKind = true;
                kind = PvpObs::Kind::OWN_CARRIER;
                verbIndex = i;
                break;
            }
        }
    }
    if (!haveKind || !detail::KindFitsMap(kind, map))
        return report;

    // The place and the number. Every other word of the line must be a filler ("inc at bs now"): a line with words the reader
    // does not know ("kill efc", "escort fc please") is chat, not a call-out.
    std::uint8_t place = BgPlan::WORD_NONE;
    std::uint8_t count = 0;
    std::size_t flagCount = 0;
    BgPlan::WordId const* const flagPlaces = detail::FlagPlaces(map, flagCount);
    std::size_t nodeCount = 0;
    BgPlan::WordId const* const nodes = detail::Nodes(nodeCount, map);
    bool const nodeMap = map == MapKind::AB || map == MapKind::BFG || map == MapKind::EYE || map == MapKind::SM;
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        // The verb again ("inc inc bs") is the same verb: two kinds were turned away above.
        if (i == verbIndex || BgPlan::LookupWord(words[i], verbs, verbCount) != BgPlan::WORD_NONE)
            continue;

        std::uint8_t number = 0;
        if (!count && detail::IsCount(words[i], number))
        {
            count = number;
            continue;
        }
        if (detail::IsNumber(words[i]) && !detail::IsCount(words[i], number))
            continue;   // a number out of bounds ("inc bs 99"): the line stands, the count stays unknown

        if (place == BgPlan::WORD_NONE)
        {
            std::uint8_t found = BgPlan::WORD_NONE;
            std::size_t span = 1;
            if (nodeMap)
                found = BgPlan::LookupWord(words[i], nodes, nodeCount);
            else
            {
                if (map == MapKind::TP && i + 1 < words.size())
                {
                    found = detail::TwinPeaksPair(words[i], words[i + 1]);
                    span = 2;
                }
                if (found == BgPlan::WORD_NONE)
                {
                    found = BgPlan::LookupWord(words[i], flagPlaces, flagCount);
                    span = 1;
                }
            }
            if (found != BgPlan::WORD_NONE)
            {
                place = found;
                // "gold mine" and "lumber mill" are two words for one place.
                if (span == 1 && i + 1 < words.size() && nodeMap &&
                    BgPlan::LookupWord(words[i + 1], nodes, nodeCount) == found)
                    span = 2;
                i += span - 1;
                continue;
            }
        }

        if (!detail::IsFiller(words[i]) && !(map == MapKind::EYE && detail::IsEyeFiller(words[i])) &&
            !(map == MapKind::SM && detail::IsSmFiller(words[i])))
            return report;
    }

    if (detail::NeedsPlace(kind) && place == BgPlan::WORD_NONE)
        return report;

    report.valid = true;
    report.kind = kind;
    report.place = place == BgPlan::WORD_NONE ? PvpObs::PLACE_UNKNOWN : place;
    report.count = count;
    return report;
}

// ---------------------------------------------------------------------------------------------------------------------
// Who heeds a report. A casual bot may ignore some (owner 2026-11-18): the roll is fixed per bot and per line (the time it
// was received), so a replay gives the same answer.
// ---------------------------------------------------------------------------------------------------------------------
constexpr std::uint32_t CASUAL_HEED_PERCENT = 65;
constexpr std::uint32_t HEED_ROLL_MIX = 2246822519u;

inline bool Heeds(BotSkillMath::Tier tier, std::uint32_t guidLow, std::uint32_t receivedMs)
{
    if (tier != BotSkillMath::Tier::CASUAL)
        return true;
    return BotSkillMath::Mix(guidLow ^ (receivedMs * HEED_ROLL_MIX)) % 100 < CASUAL_HEED_PERCENT;
}

// ---------------------------------------------------------------------------------------------------------------------
// The effect on the assault group of Arathi Basin: a claim ("take lm") or a lost node ("st lost") is one more reason to go
// there, worth CLAIM_BONUS_POINTS at full confidence, added to the swing score of that node among the targets of the good
// bots. A clearly better target still wins. Experts keep their own numbers, casual bots go to the nearest node.
// ---------------------------------------------------------------------------------------------------------------------
constexpr int CLAIM_BONUS_POINTS = 60;

inline void ApplyClaimBonus(std::vector<AbLogic::TargetScore>& targets, std::uint8_t node, std::uint32_t confidencePercent)
{
    int const bonus = static_cast<int>(CLAIM_BONUS_POINTS * confidencePercent / 100);
    for (AbLogic::TargetScore& t : targets)
        if (t.node == node)
            t.score += bonus;

    std::stable_sort(targets.begin(), targets.end(), [](AbLogic::TargetScore const& a, AbLogic::TargetScore const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return a.node < b.node;
    });
}
}  // namespace BgCallout

#endif
