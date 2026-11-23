#include "BotProfessionProgress.h"
#include "TestHarness.h"

using BotProfessionProgress::MayTrain;
using BotProfessionProgress::StoredPair;

namespace
{
constexpr std::uint32_t MINING = 186;
constexpr std::uint32_t HERBALISM = 182;
constexpr std::uint32_t TAILORING = 197;
constexpr std::uint32_t FISHING = 356;
}

TEST(profession_secondary_skills_are_open_to_every_bot)
{
    StoredPair const pair = {MINING, true, HERBALISM, true};
    CHECK(MayTrain(false, true, FISHING, pair, false));
}

TEST(profession_primary_only_from_the_stored_pair)
{
    StoredPair const pair = {MINING, true, HERBALISM, true};
    CHECK(MayTrain(true, false, MINING, pair, false));
    CHECK(MayTrain(true, false, HERBALISM, pair, true));
    CHECK(!MayTrain(true, false, TAILORING, pair, false));
    // A profession the bot somehow knows outside its pair gets no new ranks either.
    CHECK(!MayTrain(true, false, TAILORING, pair, true));
}

TEST(profession_without_a_pair_only_ranks_of_a_known_profession)
{
    StoredPair const none = {0, false, 0, false};
    CHECK(MayTrain(true, false, TAILORING, none, true));
    CHECK(!MayTrain(true, false, TAILORING, none, false));
}

TEST(profession_unknown_skill_is_refused)
{
    StoredPair const pair = {MINING, true, HERBALISM, true};
    CHECK(!MayTrain(false, false, 0, pair, false));
    CHECK(!MayTrain(true, false, 0, pair, false));
}

TEST(profession_free_slots_count_only_learned_primaries)
{
    CHECK(BotProfessionProgress::FreePrimarySlots(2, 0) == 2);
    CHECK(BotProfessionProgress::FreePrimarySlots(2, 1) == 1);
    CHECK(BotProfessionProgress::FreePrimarySlots(2, 2) == 0);
    CHECK(BotProfessionProgress::FreePrimarySlots(2, 3) == 0);
}
