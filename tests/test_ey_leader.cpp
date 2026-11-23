#include "TestHarness.h"
#include "BgCalloutPolicy.h"
#include "BgLeaderAdapters.h"
#include "AiPvpDecisionPolicy.h"
#include "BgTargetOrderPolicy.h"

// BT-585: Eye of the Storm in the call-out reader, the decision points D15 to D17 and the target order
// (docs/bg-sheets/eye-of-the-storm.md sections 8, 9 and 16).

using namespace BgCallout;
using namespace BgLeader;

namespace
{
Report Say(char const* text)
{
    return Parse(text, MapKind::EYE);
}
}  // namespace

TEST(ey_reader_knows_the_base_words)
{
    CHECK(MapKindOf(566) == MapKind::EYE && MapKindOf(761) == MapKind::BFG && MapKindOf(529) == MapKind::AB);
    Report r = Say("inc mt 4");
    CHECK(r.valid && r.kind == PvpObs::Kind::INCOMING && r.place == 3 && r.count == 4);
    CHECK(Say("help fr").place == 0 && Say("help be").place == 1 && Say("help dr").place == 2);
    CHECK(Say("lost fr").kind == PvpObs::Kind::LOST && Say("take dr").kind == PvpObs::Kind::CLAIM);
    CHECK(Say("mt clear").kind == PvpObs::Kind::CLEAR);
}

TEST(ey_reader_reads_the_long_names_as_one_place)
{
    CHECK(Say("inc fel reaver 3").valid && Say("inc fel reaver 3").place == 0);
    CHECK(Say("inc fel reaver ruins").place == 0);
    CHECK(Say("inc blood elf tower 2").place == 1);
    CHECK(Say("inc draenei ruins").place == 2);
    CHECK(Say("inc mage tower").place == 3 && Say("inc magetower 5").count == 5);
    // A tower or ruins alone is no place.
    CHECK(!Say("inc tower").valid && !Say("inc ruins").valid);
}

TEST(ey_reader_reads_the_carrier_lines_with_the_middle_as_a_place)
{
    Report r = Say("efc mid");
    CHECK(r.valid && r.kind == PvpObs::Kind::ENEMY_CARRIER && r.place == 4);
    r = Say("efc mt");
    CHECK(r.valid && r.kind == PvpObs::Kind::ENEMY_CARRIER && r.place == 3);
    CHECK(Say("fc").valid && Say("fc").kind == PvpObs::Kind::OWN_CARRIER);
    CHECK(Say("efc center").place == 4);
    CHECK(!Say("inc mid").valid || Say("inc mid").place == 4);   // a place the plan never indexes as a base
}

TEST(ey_reader_keeps_the_other_maps_apart)
{
    CHECK(!Parse("inc fr", MapKind::AB).valid);          // fr is no Arathi word
    CHECK(!Parse("inc mt", MapKind::BFG).valid);
    CHECK(Parse("inc bs 3", MapKind::AB).valid);
    CHECK(!Say("inc bs 3").valid && !Say("inc ww").valid);
    CHECK(!Say("kill efc at fr").valid);                  // words the reader does not know are chat
    CHECK(!Say("inc mt?").valid);
}

TEST(ey_decision_points_have_their_names_and_room_in_the_book)
{
    CHECK(AiPvp::IsWired(DecisionPoint::D15_EYE_FLAG) && AiPvp::IsWired(DecisionPoint::D16_EYE_CARRIER) &&
          AiPvp::IsWired(DecisionPoint::D17_EYE_EFC));
    CHECK(AiPvp::IsRepeatable(DecisionPoint::D16_EYE_CARRIER) && !AiPvp::IsRepeatable(DecisionPoint::D15_EYE_FLAG));
    CHECK(std::string(AiPvp::PointName(DecisionPoint::D15_EYE_FLAG)) == "eye_flag");
    CHECK(std::string(AiPvp::PointName(DecisionPoint::D17_EYE_EFC)) == "eye_efc");
    CHECK(static_cast<std::size_t>(DecisionPoint::D17_EYE_EFC) < AiPvp::BOOK_SIZE);
    // Every point has its own entry in the book: none collides with another (D16 and D17 would hit D1 with a book of 16).
    AiPvp::Book book;
    std::vector<DecisionPoint> points = { DecisionPoint::D1_STANCE, DecisionPoint::D9_NODE_TARGET, DecisionPoint::D13_BFG_FIGHT,
                                          DecisionPoint::D14_BFG_STAGE, DecisionPoint::D15_EYE_FLAG, DecisionPoint::D16_EYE_CARRIER,
                                          DecisionPoint::D17_EYE_EFC };
    for (std::size_t i = 0; i < points.size(); ++i)
        for (std::size_t j = i + 1; j < points.size(); ++j)
            CHECK(&book.At(points[i]) != &book.At(points[j]));
    CHECK(std::string(AiPvp::PointQuestion(DecisionPoint::D16_EYE_CARRIER)).find("flag carrier") != std::string::npos);
}

TEST(ey_candidate_names_are_the_words_of_the_sheet)
{
    CHECK(std::string(CandidateName(DecisionPoint::D15_EYE_FLAG, 0)) == "take_now");
    CHECK(std::string(CandidateName(DecisionPoint::D15_EYE_FLAG, 2)) == "deny_only");
    CHECK(std::string(CandidateName(DecisionPoint::D16_EYE_CARRIER, 3)) == "mage_tower");
    CHECK(std::string(CandidateName(DecisionPoint::D16_EYE_CARRIER, EyFlag::HOLD_BACK)) == "hold_back");
    CHECK(std::string(CandidateName(DecisionPoint::D17_EYE_EFC, 1)) == "hunt_two_cut");
    CHECK(std::string(CandidateName(DecisionPoint::D17_EYE_EFC, 2)) == "neutralize_target_base");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, EYE_NODE_ID_BASE + 1)) == "blood_elf");
    CHECK(std::string(CandidateName(DecisionPoint::D9_NODE_TARGET, BFG_NODE_ID_BASE + 1)) == "waterworks");
}

TEST(ey_offers_put_the_rule_first)
{
    EyFlag::ActionInput a;
    a.ourBases = 2;
    Offer const flag = EyeFlagOffer(a);
    CHECK(flag.point == DecisionPoint::D15_EYE_FLAG && flag.Fallback() == static_cast<std::uint8_t>(EyFlag::Action::TAKE_NOW));

    EyFlag::BaseFact facts[EyFlag::BASE_COUNT] = {};
    facts[2].owned = true;
    facts[2].guards = 1;
    facts[2].distance = 201.0f;
    facts[3].owned = true;
    facts[3].guards = 1;
    facts[3].distance = 196.0f;
    facts[3].nearOwnStart = true;
    Offer const carrier = EyeCarrierOffer(EyFlag::Destinations(facts, false, 0));
    CHECK(carrier.point == DecisionPoint::D16_EYE_CARRIER && carrier.options.size() == 2 && carrier.Fallback() == 3);

    EyFlag::EfcInput e;
    e.theirBases = 4;
    e.distanceToEfc = 300.0f;
    Offer const efc = EyeEfcOffer(e);
    CHECK(efc.point == DecisionPoint::D17_EYE_EFC && efc.Fallback() == static_cast<std::uint8_t>(EyFlag::EfcAction::HUNT_ALL));
}

TEST(ey_target_order_hits_the_enemy_carrier_first_for_good_and_expert_bots)
{
    using namespace BgTargetOrder;
    std::vector<Foe> foes(3);
    foes[0].guid = 1;
    foes[0].healer = true;
    foes[0].distance = 10.0f;
    foes[1].guid = 2;
    foes[1].carrier = true;
    foes[1].distance = 20.0f;
    foes[1].healthPercent = 100.0f;
    foes[2].guid = 3;
    foes[2].squishy = true;
    foes[2].distance = 5.0f;
    CHECK(Pick(Tier::EXPERT, true, foes) == 2);
    CHECK(Pick(Tier::EXPERT, false, foes) == 2);
    CHECK(Pick(Tier::GOOD, false, foes) == 2);
    CHECK(Pick(Tier::CASUAL, false, foes) != 2);      // a casual bot keeps its own target
    // A carrier out of reach is not picked.
    foes[1].distance = 45.0f;
    CHECK(Pick(Tier::GOOD, false, foes) == 1);
    // Without a carrier the order is unchanged.
    foes[1].carrier = false;
    CHECK(Pick(Tier::GOOD, false, foes) == 1);
}
