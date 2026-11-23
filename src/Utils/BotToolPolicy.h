#ifndef BOT_TOOL_POLICY_H
#define BOT_TOOL_POLICY_H

#include <algorithm>
#include <cstdint>
#include <vector>

// BT-219: random bots buy the tools their professions need at a vendor, as players do, instead of getting them from the
// factory. Pure rules; the item lists match what the loot pipeline accepts (LootObjectStack.cpp IsLootPossible).
namespace BotToolPolicy
{
struct ToolRule
{
    std::uint32_t skill;
    std::uint32_t buyItem;                  // the plain vendor tool
    std::vector<std::uint32_t> accepted;    // any of these already does the job
};

constexpr std::uint32_t SKILL_BLACKSMITHING_ID = 164;
constexpr std::uint32_t SKILL_MINING_ID = 186;
constexpr std::uint32_t SKILL_ENGINEERING_ID = 202;
constexpr std::uint32_t SKILL_SKINNING_ID = 393;

constexpr std::uint32_t MINING_PICK = 2901;
constexpr std::uint32_t SKINNING_KNIFE = 7005;
constexpr std::uint32_t BLACKSMITH_HAMMER = 5956;
constexpr std::uint32_t ARCLIGHT_SPANNER = 6219;
constexpr std::uint32_t GNOMISH_ARMY_KNIFE = 40772;

inline std::vector<ToolRule> const& Rules()
{
    static std::vector<ToolRule> const rules = {
        {SKILL_MINING_ID, MINING_PICK, {756, 778, 1819, 1893, 1959, MINING_PICK, 9465, 20723, GNOMISH_ARMY_KNIFE, 40892, 40893}},
        {SKILL_SKINNING_ID, SKINNING_KNIFE, {SKINNING_KNIFE, GNOMISH_ARMY_KNIFE, 40893, 12709, 19901}},
        {SKILL_BLACKSMITHING_ID, BLACKSMITH_HAMMER, {BLACKSMITH_HAMMER, GNOMISH_ARMY_KNIFE, 40893}},
        {SKILL_ENGINEERING_ID, BLACKSMITH_HAMMER, {BLACKSMITH_HAMMER, GNOMISH_ARMY_KNIFE, 40893}},
        {SKILL_ENGINEERING_ID, ARCLIGHT_SPANNER, {ARCLIGHT_SPANNER, GNOMISH_ARMY_KNIFE}},
    };
    return rules;
}

// The tools to buy: one per rule whose skill the bot has and none of whose accepted items it holds, without repeats.
template <typename HasSkill, typename HasItem>
std::vector<std::uint32_t> ToolsToBuy(HasSkill hasSkill, HasItem hasItem)
{
    std::vector<std::uint32_t> wanted;
    for (ToolRule const& rule : Rules())
    {
        if (!hasSkill(rule.skill))
            continue;

        if (std::any_of(rule.accepted.begin(), rule.accepted.end(), hasItem))
            continue;

        if (std::find(wanted.begin(), wanted.end(), rule.buyItem) == wanted.end())
            wanted.push_back(rule.buyItem);
    }
    return wanted;
}
}

#endif
