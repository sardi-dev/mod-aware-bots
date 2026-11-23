#include "TestHarness.h"
#include "RaidGroupPolicy.h"

using namespace RaidGroupPolicy;

namespace
{
Candidate Bot(std::uint32_t guid, std::uint8_t classId, std::uint8_t level = 90)
{
    Candidate c;
    c.guid = guid;
    c.classId = classId;
    c.level = level;
    c.sameFaction = true;
    c.isBot = true;
    return c;
}
}  // namespace

TEST(raid_group_only_free_bots_of_the_faction_in_the_level_window_are_eligible)
{
    CHECK(Eligible(Bot(1, 1), 90, 90));
    Candidate c = Bot(1, 1);
    c.sameFaction = false;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.isBot = false;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.grouped = true;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.inBattleground = true;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.inInstance = true;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.alive = false;
    CHECK(!Eligible(c, 90, 90));
    c = Bot(1, 1);
    c.excluded = true;
    CHECK(!Eligible(c, 90, 90));
    CHECK(!Eligible(Bot(1, 1, 89), 90, 90));
    CHECK(Eligible(Bot(1, 1, 89), 85, 90));
}

TEST(raid_group_room_is_the_party_or_the_raid_size)
{
    CHECK((Room(1, false)) == (4u));
    CHECK((Room(5, false)) == (0u));
    CHECK((Room(5, true)) == (35u));
    CHECK((Room(40, true)) == (0u));
}

TEST(raid_group_converts_to_a_raid_past_five_members)
{
    CHECK(!NeedsRaid(5));
    CHECK(NeedsRaid(6));
}

TEST(raid_group_pick_takes_the_classes_in_turn_and_the_lower_guid_first)
{
    std::vector<Candidate> all = { Bot(30, 5), Bot(10, 1), Bot(20, 1), Bot(40, 8), Bot(50, 5) };
    std::vector<std::uint32_t> const picked = Pick(all, 4, 90, 90);
    CHECK((picked.size()) == (4u));
    CHECK((picked[0]) == (10u));   // class 1
    CHECK((picked[1]) == (30u));   // class 5
    CHECK((picked[2]) == (40u));   // class 8
    CHECK((picked[3]) == (20u));   // second round, class 1 again
}

TEST(raid_group_pick_is_the_same_for_the_same_state_and_stops_when_the_pool_is_short)
{
    std::vector<Candidate> all = { Bot(3, 1), Bot(1, 1), Bot(2, 1) };
    CHECK((Pick(all, 10, 90, 90)) == (Pick(all, 10, 90, 90)));
    CHECK((Pick(all, 10, 90, 90).size()) == (3u));
    CHECK((Pick(all, 0, 90, 90).size()) == (0u));
    CHECK((Pick({}, 5, 90, 90).size()) == (0u));
}
