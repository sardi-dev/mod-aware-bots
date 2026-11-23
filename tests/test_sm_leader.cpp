#include "TestHarness.h"
#include "BgCalloutPolicy.h"
#include "SmPlanLogic.h"

// BT-613: Silvershard Mines in the call-out reader: the words of the carts, the depot and the switches, read as information.

using namespace BgCallout;
using PvpObs::Kind;

TEST(sm_callout_the_map_id_is_727)
{
    CHECK(MapKindOf(727) == MapKind::SM);
    CHECK(MapKindOf(726) == MapKind::TP);
    CHECK(MapKindOf(SmLogic::MAP_ID) == MapKind::SM);
}

TEST(sm_callout_the_cart_names_of_the_players_are_read)
{
    Report r = Parse("inc lava 3", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == SmLogic::CART_EAST && r.count == 3);
    r = Parse("3 inc mid", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == SmLogic::CART_WATERFALL && r.count == 3);
    r = Parse("help top", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::HELP && r.place == SmLogic::CART_NORTH);
    for (char const* word : { "mid", "middle", "waterfall", "wf" })
        CHECK(Parse(std::string("inc ") + word, MapKind::SM).place == SmLogic::CART_WATERFALL);
    for (char const* word : { "top", "troll" })
        CHECK(Parse(std::string("inc ") + word, MapKind::SM).place == SmLogic::CART_NORTH);
    CHECK(Parse("inc long", MapKind::SM).place == SmLogic::PLACE_LONG);
    CHECK(Parse("inc diamond", MapKind::SM).place == SmLogic::PLACE_LONG);
}

TEST(sm_callout_claims_lost_carts_and_clear_lines)
{
    Report r = Parse("take lava", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::CLAIM && r.place == SmLogic::CART_EAST);
    r = Parse("lost mid", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::LOST && r.place == SmLogic::CART_WATERFALL);
    r = Parse("top lost", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::LOST && r.place == SmLogic::CART_NORTH);
    r = Parse("clear lava", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::CLEAR && r.place == SmLogic::CART_EAST);
}

TEST(sm_callout_the_switches_are_places_and_the_filler_words_stand_in_a_line)
{
    Report r = Parse("inc east switch 2", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::INCOMING && r.place == SmLogic::PLACE_SWITCH_EAST && r.count == 2);
    r = Parse("help north sw", MapKind::SM);
    CHECK(r.valid && r.kind == Kind::HELP && r.place == SmLogic::PLACE_SWITCH_NORTH);
    CHECK(Parse("inc es", MapKind::SM).place == SmLogic::PLACE_SWITCH_EAST);
    CHECK(Parse("inc ns", MapKind::SM).place == SmLogic::PLACE_SWITCH_NORTH);
    r = Parse("inc the lava cart now", MapKind::SM);
    CHECK(r.valid && r.place == SmLogic::CART_EAST);
}

TEST(sm_callout_other_maps_words_and_carrier_lines_are_not_read_here)
{
    CHECK(!Parse("efc lava", MapKind::SM).valid);          // no flag on this map
    CHECK(!Parse("fc", MapKind::SM).valid);
    CHECK(!Parse("inc bs", MapKind::SM).valid);            // an Arathi Basin word
    CHECK(!Parse("inc lava", MapKind::AB).valid);          // and the cart words are read only on map 727
    CHECK(!Parse("inc lava", MapKind::EYE).valid);
}

TEST(sm_callout_chat_that_is_not_a_call_is_not_read)
{
    CHECK(!Parse("everyone go lava", MapKind::SM).valid);
    CHECK(!Parse("inc lava?", MapKind::SM).valid);
    CHECK(!Parse("no inc lava", MapKind::SM).valid);
    CHECK(!Parse("take lava and inc top", MapKind::SM).valid);   // two verbs of different kinds
    CHECK(!Parse("inc", MapKind::SM).valid);                      // a place is needed
    CHECK(!Parse("attack the cart", MapKind::SM).valid);
    // A number out of bounds leaves the line standing without a count.
    Report const r = Parse("inc lava 99", MapKind::SM);
    CHECK(r.valid && r.count == 0);
}

TEST(sm_callout_every_place_id_is_an_observation_place_below_the_unknown_mark)
{
    for (char const* word : { "lava", "mid", "top", "long", "es", "ns" })
    {
        Report const r = Parse(std::string("inc ") + word, MapKind::SM);
        CHECK(r.valid && r.place < PvpObs::PLACE_UNKNOWN);
    }
}
