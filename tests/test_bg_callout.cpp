#include "TestHarness.h"
#include "BgCalloutPolicy.h"

// BT-482: the human call-out reader: one parser for the three maps, read as information and never as a command.

using namespace BgCallout;
using PvpObs::Kind;
using WsgLogic::Place;

namespace
{
std::uint8_t Pl(Place place)
{
    return static_cast<std::uint8_t>(place);
}
}  // namespace

TEST(callout_map_kinds_follow_the_map_ids)
{
    CHECK(MapKindOf(489) == MapKind::WSG);
    CHECK(MapKindOf(726) == MapKind::TP);
    CHECK(MapKindOf(529) == MapKind::AB);
    CHECK(MapKindOf(30) == MapKind::NONE);
    CHECK(!Parse("inc fr", MapKind::NONE).valid);
}

TEST(callout_case_and_punctuation_do_not_matter)
{
    Report r = Parse("INC, Bs!!", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == AbLogic::NODE_BLACKSMITH);
    r = Parse("  Inc...fr  ", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == Pl(Place::FLAG_ROOM));
    r = Parse("efc-mid", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::ENEMY_CARRIER && r.place == Pl(Place::MID));
}

TEST(callout_place_aliases_of_the_flag_maps)
{
    for (char const* word : { "fr", "room", "base", "home" })
        CHECK(Parse(std::string("inc ") + word, MapKind::WSG).place == Pl(Place::FLAG_ROOM));
    CHECK(Parse("inc tunnel", MapKind::WSG).place == Pl(Place::TUNNEL));
    CHECK(Parse("inc middle", MapKind::WSG).place == Pl(Place::MID));
    CHECK(Parse("inc graveyard", MapKind::WSG).place == Pl(Place::GRAVEYARD));
    CHECK(Parse("inc yard", MapKind::WSG).place == Pl(Place::GRAVEYARD));
    // "flag" is the flag room when another verb is there.
    Report const r = Parse("inc flag", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == Pl(Place::FLAG_ROOM));
}

TEST(callout_twin_peaks_words_are_read_only_there)
{
    Report r = Parse("inc lane a", MapKind::TP);
    CHECK(r.valid && r.place == Pl(Place::LANE_A));
    r = Parse("efc lane h", MapKind::TP);
    CHECK(r.valid && r.kind == Kind::ENEMY_CARRIER && r.place == Pl(Place::LANE_H));
    r = Parse("inc bers a 2", MapKind::TP);
    CHECK(r.valid && r.place == Pl(Place::BERS_A) && r.count == 2);
    r = Parse("help leaf h", MapKind::TP);
    CHECK(r.valid && r.kind == Kind::HELP && r.place == Pl(Place::LEAF_H));
    r = Parse("inc door", MapKind::TP);
    CHECK(r.valid && r.place == Pl(Place::DOOR));
    r = Parse("inc gy", MapKind::TP);
    CHECK(r.valid && r.place == Pl(Place::GRAVEYARD));

    // Warsong Gulch has no lanes or doors, Twin Peaks has no tunnel, ramp or roof.
    CHECK(!Parse("inc lane a", MapKind::WSG).valid);
    CHECK(!Parse("inc door", MapKind::WSG).valid);
    CHECK(!Parse("inc tunnel", MapKind::TP).valid);
    CHECK(!Parse("inc ramp", MapKind::TP).valid);
    // Lane words need their side.
    CHECK(!Parse("inc lane", MapKind::TP).valid);
}

TEST(callout_node_aliases_of_arathi_basin)
{
    CHECK(Parse("inc st", MapKind::AB).place == AbLogic::NODE_STABLES);
    CHECK(Parse("inc smith", MapKind::AB).place == AbLogic::NODE_BLACKSMITH);
    CHECK(Parse("inc fm", MapKind::AB).place == AbLogic::NODE_FARM);
    CHECK(Parse("inc lumber mill", MapKind::AB).place == AbLogic::NODE_LUMBER_MILL);
    CHECK(Parse("inc lumbermill", MapKind::AB).place == AbLogic::NODE_LUMBER_MILL);
    CHECK(Parse("inc goldmine", MapKind::AB).place == AbLogic::NODE_GOLD_MINE);
    // A flag-map place is not a node.
    CHECK(!Parse("inc fr", MapKind::AB).valid);
}

TEST(callout_a_number_may_come_before_or_after_and_stays_in_bounds)
{
    CHECK(Parse("inc bs 3", MapKind::AB).count == 3);
    CHECK(Parse("3 inc bs", MapKind::AB).count == 3);
    CHECK(Parse("inc 5 bs", MapKind::AB).count == 5);
    CHECK(Parse("inc bs 15", MapKind::AB).count == 15);
    // Out of bounds: the report stands, the count is unknown.
    Report r = Parse("inc bs 99", MapKind::AB);
    CHECK(r.valid && r.count == 0);
    r = Parse("inc bs 16", MapKind::AB);
    CHECK(r.valid && r.count == 0);
    r = Parse("inc bs 0", MapKind::AB);
    CHECK(r.valid && r.count == 0);
    // A second number is not read: the line has a word the reader does not know.
    CHECK(!Parse("inc bs 4 6", MapKind::AB).valid);
}

TEST(callout_a_missing_place_is_allowed_only_where_the_kind_is_not_about_a_place)
{
    Report r = Parse("help", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::HELP && r.place == PvpObs::PLACE_UNKNOWN);
    r = Parse("help 3", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::HELP && r.place == PvpObs::PLACE_UNKNOWN && r.count == 3);
    r = Parse("efc", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::ENEMY_CARRIER && r.place == PvpObs::PLACE_UNKNOWN);

    CHECK(!Parse("inc", MapKind::WSG).valid);
    CHECK(!Parse("inc 3", MapKind::AB).valid);
    CHECK(!Parse("take", MapKind::AB).valid);
    CHECK(!Parse("lost", MapKind::AB).valid);
    CHECK(!Parse("clear", MapKind::WSG).valid);
}

TEST(callout_the_carrier_words_say_the_carrier)
{
    Report r = Parse("fc", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::OWN_CARRIER && r.place == PvpObs::PLACE_UNKNOWN);
    r = Parse("FLAG!", MapKind::TP);
    CHECK(r.valid && r.kind == Kind::OWN_CARRIER);
    r = Parse("fc mid", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::OWN_CARRIER && r.place == Pl(Place::MID));
    // Not on the node map.
    CHECK(!Parse("fc", MapKind::AB).valid);
    CHECK(!Parse("flag", MapKind::AB).valid);
}

TEST(callout_claims_and_lost_nodes_belong_to_the_node_map_and_are_not_orders)
{
    Report r = Parse("take lm", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::CLAIM && r.place == AbLogic::NODE_LUMBER_MILL);
    r = Parse("cap farm", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::CLAIM);
    r = Parse("st lost", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::LOST);
    r = Parse("gm clear", MapKind::AB);
    CHECK(r.valid && r.kind == Kind::CLEAR);
    r = Parse("fr clear", MapKind::WSG);
    CHECK(r.valid && r.kind == Kind::CLEAR);

    CHECK(!Parse("take fr", MapKind::WSG).valid);
    CHECK(!Parse("st lost", MapKind::WSG).valid);
}

TEST(callout_command_like_lines_are_not_read)
{
    for (char const* line : { "everyone go bs", "attack farm", "all defend st", "send 3 to gm", "back to base", "def fr",
                              "go go go", "stay at bs", "need healers" })
        CHECK(!Parse(line, MapKind::AB).valid);
    for (char const* line : { "everyone go fr", "def fr", "back", "kill efc", "escort fc please" })
        CHECK(!Parse(line, MapKind::WSG).valid);
}

TEST(callout_questions_negations_and_mixed_verbs_are_not_read)
{
    CHECK(!Parse("inc bs?", MapKind::AB).valid);
    CHECK(!Parse("where is the efc?", MapKind::WSG).valid);
    CHECK(!Parse("no inc bs", MapKind::AB).valid);
    CHECK(!Parse("dont help fr", MapKind::WSG).valid);
    CHECK(!Parse("nothing incoming st", MapKind::AB).valid);
    // Two different verbs: ambiguous.
    CHECK(!Parse("inc bs take st", MapKind::AB).valid);
    CHECK(!Parse("inc help fr", MapKind::WSG).valid);
    // The same verb twice is one verb.
    CHECK(Parse("inc inc bs", MapKind::AB).valid);
}

TEST(callout_only_a_short_line_is_read)
{
    CHECK(Parse("inc bs 3", MapKind::AB).valid);
    CHECK(!Parse("inc bs because i saw three of them coming down the road", MapKind::AB).valid);
    CHECK(Parse("inc at bs now pls", MapKind::AB).valid);
    CHECK(!Parse("inc bs lol", MapKind::AB).valid);                                  // an unknown word: chat
    CHECK(!Parse("", MapKind::AB).valid);
    CHECK(!Parse("   ", MapKind::AB).valid);
    CHECK(!Parse("!!!", MapKind::WSG).valid);
}

TEST(callout_every_line_a_bot_says_is_a_line_a_human_could_say)
{
    // The words the plans put in their own lines ("efc %place", "%n inc fr", "inc %node %num") read back to the same thing.
    for (Place place : { Place::FLAG_ROOM, Place::TUNNEL, Place::MID, Place::RAMP, Place::ROOF, Place::GRAVEYARD })
    {
        Report const r = Parse(std::string("efc ") + WsgLogic::PlaceWord(place), MapKind::WSG);
        CHECK(r.valid && r.place == Pl(place));
    }
    for (Place place : { Place::LANE_A, Place::LANE_H, Place::BERS_A, Place::BERS_H, Place::LEAF_A, Place::LEAF_H, Place::DOOR,
                         Place::FLAG_ROOM, Place::MID, Place::GRAVEYARD })
    {
        Report const r = Parse(std::string("efc ") + WsgLogic::PlaceWord(place), MapKind::TP);
        CHECK(r.valid && r.place == Pl(place));
    }
    Report const r = Parse("3 inc fr", MapKind::WSG);
    CHECK(r.valid && r.count == 3);
}

TEST(callout_a_casual_bot_may_ignore_a_line_the_others_always_heed)
{
    using BotSkillMath::Tier;
    std::uint32_t heeded = 0;
    std::uint32_t const total = 4000;
    for (std::uint32_t guid = 1; guid <= total; ++guid)
    {
        CHECK(Heeds(Tier::EXPERT, guid, 123456));
        CHECK(Heeds(Tier::GOOD, guid, 123456));
        bool const casual = Heeds(Tier::CASUAL, guid, 123456);
        CHECK(casual == Heeds(Tier::CASUAL, guid, 123456));   // fixed per bot and line
        if (casual)
            ++heeded;
    }
    CHECK(heeded > total * 60 / 100);
    CHECK(heeded < total * 70 / 100);

    // Another line, another roll.
    std::uint32_t differs = 0;
    for (std::uint32_t guid = 1; guid <= 200; ++guid)
        if (Heeds(Tier::CASUAL, guid, 1000) != Heeds(Tier::CASUAL, guid, 5000))
            ++differs;
    CHECK(differs > 0);
}

TEST(callout_a_claim_weighs_on_the_targets_and_a_better_target_still_wins)
{
    using AbLogic::TargetScore;
    std::vector<TargetScore> targets = { { AbLogic::NODE_FARM, 240 }, { AbLogic::NODE_GOLD_MINE, 200 },
                                         { AbLogic::NODE_LUMBER_MILL, 100 } };
    // 40 points behind plus a claim at 70 percent (42 points): ahead.
    ApplyClaimBonus(targets, AbLogic::NODE_GOLD_MINE, 70);
    CHECK(targets.front().node == AbLogic::NODE_GOLD_MINE);
    CHECK(targets.front().score == 200 + CLAIM_BONUS_POINTS * 70 / 100);

    // A node far behind is not carried to the top by a claim.
    targets = { { AbLogic::NODE_FARM, 240 }, { AbLogic::NODE_GOLD_MINE, 200 }, { AbLogic::NODE_LUMBER_MILL, 100 } };
    ApplyClaimBonus(targets, AbLogic::NODE_LUMBER_MILL, 70);
    CHECK(targets.front().node == AbLogic::NODE_FARM);
    CHECK(targets.back().node == AbLogic::NODE_LUMBER_MILL);

    // A node that is no target gains nothing, and an old claim (confidence 0) weighs nothing.
    targets = { { AbLogic::NODE_FARM, 240 } };
    ApplyClaimBonus(targets, AbLogic::NODE_STABLES, 70);
    CHECK(targets.size() == 1 && targets[0].score == 240);
    ApplyClaimBonus(targets, AbLogic::NODE_FARM, 0);
    CHECK(targets[0].score == 240);
}
