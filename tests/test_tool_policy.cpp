#include "BotToolPolicy.h"
#include "TestHarness.h"

#include <set>

using namespace BotToolPolicy;

namespace
{
std::vector<std::uint32_t> Want(std::set<std::uint32_t> skills, std::set<std::uint32_t> items)
{
    return ToolsToBuy([&skills](std::uint32_t s) { return skills.count(s) > 0; },
                      [&items](std::uint32_t i) { return items.count(i) > 0; });
}
}

TEST(tools_miner_without_a_pick_buys_one)
{
    auto const want = Want({SKILL_MINING_ID}, {});
    CHECK(want.size() == 1 && want[0] == MINING_PICK);
}

TEST(tools_any_accepted_tool_is_enough)
{
    CHECK(Want({SKILL_MINING_ID}, {MINING_PICK}).empty());
    CHECK(Want({SKILL_MINING_ID, SKILL_SKINNING_ID}, {GNOMISH_ARMY_KNIFE}).empty());
}

TEST(tools_engineer_and_smith_buy_the_hammer_once)
{
    auto const want = Want({SKILL_BLACKSMITHING_ID, SKILL_ENGINEERING_ID}, {});
    CHECK(want.size() == 2);
    CHECK(want[0] == BLACKSMITH_HAMMER && want[1] == ARCLIGHT_SPANNER);
}

TEST(tools_no_profession_no_tools)
{
    CHECK(Want({}, {}).empty());
}
