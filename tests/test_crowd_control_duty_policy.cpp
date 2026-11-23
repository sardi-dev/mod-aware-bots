#include "TestHarness.h"
#include "CrowdControlDutyPolicy.h"

TEST(cc_duty_mark_spell_mapping)
{
    using namespace CrowdControlDutyPolicy;
    CHECK(MatchesMark(MoonIcon, Spell::Polymorph));
    CHECK(MatchesMark(MoonIcon, Spell::Hex));
    CHECK(MatchesMark(MoonIcon, Spell::Repentance));
    CHECK(MatchesMark(MoonIcon, Spell::Hibernate));
    CHECK(MatchesMark(MoonIcon, Spell::Cyclone));
    CHECK(MatchesMark(SquareIcon, Spell::FreezingTrap));
    CHECK(MatchesMark(TriangleIcon, Spell::Sap));
    CHECK(MatchesMark(TriangleIcon, Spell::ShackleUndead));
    CHECK(!MatchesMark(MoonIcon, Spell::FreezingTrap));
    CHECK(!MatchesMark(TriangleIcon, Spell::Polymorph));
    CHECK(!MatchesMark(0, Spell::Polymorph));
    CHECK(!MatchesMark(1, Spell::Polymorph));
    CHECK(!MatchesMark(2, Spell::Polymorph));
    CHECK(!MatchesMark(MoonIcon, Spell::Other));
}

TEST(cc_duty_stable_selection)
{
    using namespace CrowdControlDutyPolicy;
    Candidate const candidates[] = {{1, true, true, false}, {2, false, false, false},
                                    {3, true, false, true}, {9, true, false, false},
                                    {7, true, false, false}};
    Selection forward, reverse;
    for (auto const& candidate : candidates)
        forward.Add(candidate);
    for (int i = 4; i >= 0; --i)
        reverse.Add(candidates[i]);
    CHECK(forward.Guid() == 7);
    CHECK(reverse.Guid() == 7);
    Selection healer;
    healer.Add(candidates[0]);
    healer.Add(candidates[1]);
    CHECK(healer.Guid() == 0);
    healer.Add(candidates[2]);
    CHECK(healer.Guid() == 3);
}

TEST(cc_duty_target_safety)
{
    using namespace CrowdControlDutyPolicy;
    CHECK(SafeTarget(true, false, false));
    CHECK(!SafeTarget(false, false, false));
    CHECK(!SafeTarget(true, true, false));
    CHECK(!SafeTarget(true, false, true));
}

TEST(cc_duty_reapply_delay_and_target_change)
{
    using namespace CrowdControlDutyPolicy;
    using BotSkillMath::Tier;
    ReapplyGate gate;
    CHECK(!gate.Ready(0, false, 0, Tier::CASUAL));
    CHECK(gate.Ready(10, false, 100, Tier::CASUAL));
    CHECK(!gate.Ready(10, true, 200, Tier::CASUAL));
    CHECK(!gate.Ready(10, false, 300, Tier::CASUAL));
    CHECK(!gate.Ready(10, false, 3299, Tier::CASUAL));
    CHECK(gate.Ready(10, false, 3300, Tier::CASUAL));
    CHECK(!gate.Ready(10, true, 3400, Tier::EXPERT));
    CHECK(gate.Ready(10, false, 3500, Tier::EXPERT));
    CHECK(!gate.Ready(10, true, 3600, Tier::GOOD));
    CHECK(!gate.Ready(10, false, 3700, Tier::GOOD));
    CHECK(gate.Ready(10, false, 4700, Tier::GOOD));
    CHECK(!gate.Ready(10, true, 4800, Tier::CASUAL));
    CHECK(!gate.Ready(10, false, 4900, Tier::CASUAL));
    CHECK(gate.Ready(11, false, 4901, Tier::CASUAL));
}
