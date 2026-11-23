/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

#include "DpsMeterPolicy.h"

using namespace DpsMeterPolicy;

TEST(dps_meter_reads_the_damage_of_this_fight_only)
{
    Fight fight;
    Begin(fight, 1000, 500000, 20000);
    Summary const summary = End(fight, 21000, 620000, 38000);
    CHECK(summary.write);
    CHECK(summary.seconds == 20);
    CHECK(summary.damage == 120000);
    CHECK(summary.dps == 6000);
    CHECK(summary.petDamage == 18000);
    CHECK(summary.petDps == 900);
}

TEST(dps_meter_skips_a_short_fight_and_keeps_a_silent_long_one)
{
    Fight fight;
    Begin(fight, 0, 0, 0);
    CHECK(!End(fight, MIN_FIGHT_MS - 1, 9000, 0).write);

    // No damage in a long fight is the idle damage dealer the line is there to find.
    Begin(fight, 0, 100, 0);
    Summary const idle = End(fight, MIN_FIGHT_MS, 100, 0);
    CHECK(idle.write);
    CHECK(idle.damage == 0);
    CHECK(idle.dps == 0);
}

TEST(dps_meter_survives_a_clock_wrap_and_a_counter_reset)
{
    Fight fight;
    Begin(fight, 0xFFFFF000u, 900, 0);
    Summary const wrapped = End(fight, 0xFFFFF000u + 10000u, 1900, 0);
    CHECK(wrapped.seconds == 10);
    CHECK(wrapped.damage == 1000);

    Begin(fight, 0, 5000, 5000);
    Summary const reset = End(fight, 10000, 100, 100);
    CHECK(reset.damage == 0);
    CHECK(reset.petDamage == 0);
}

TEST(dps_meter_ends_once)
{
    Fight fight;
    CHECK(!End(fight, 20000, 1, 1).write);
    Begin(fight, 0, 0, 0);
    CHECK(End(fight, 20000, 1000, 0).write);
    CHECK(!End(fight, 40000, 2000, 0).write);
}

TEST(dps_meter_counts_cooldown_casts_per_spell)
{
    Fight fight;
    CHECK(!NoteCast(fight, 100, COOLDOWN_MIN_MS));  // no fight yet
    Begin(fight, 0, 0, 0);
    CHECK(!NoteCast(fight, 1, 1500));                // a filler is not a cooldown
    CHECK(NoteCast(fight, 100, COOLDOWN_MIN_MS));    // the first cast of a cooldown asks for the fight weight
    CHECK(!NoteCast(fight, 100, COOLDOWN_MIN_MS));
    CHECK(NoteCast(fight, 200, 180000));
    CHECK(fight.casts == 4);
    CHECK(fight.cooldownKinds == 2);
    CHECK(fight.cooldowns[0].spell == 100 && fight.cooldowns[0].casts == 2);
    CHECK(fight.cooldowns[1].spell == 200 && fight.cooldowns[1].casts == 1);
}

TEST(dps_meter_keeps_a_bounded_cooldown_list)
{
    Fight fight;
    Begin(fight, 0, 0, 0);
    for (std::uint32_t spell = 1; spell <= MAX_COOLDOWNS + 3; ++spell)
        NoteCast(fight, spell, 120000);
    CHECK(fight.cooldownKinds == MAX_COOLDOWNS);
    // A spell already listed still counts after the list is full.
    NoteCast(fight, 1, 120000);
    CHECK(fight.cooldowns[0].casts == 2);
}

TEST(dps_meter_keeps_the_heaviest_picture_of_the_fight)
{
    Fight fight;
    NoteSituation(fight, 3, 1, false);  // no fight yet: ignored
    CHECK(fight.attackers == 0);
    Begin(fight, 0, 0, 0);
    NoteSituation(fight, 3, 1, false);
    NoteSituation(fight, 2, 2, false);
    NoteSituation(fight, 300, 400, true);
    NoteSituation(fight, 1, 0, false);
    CHECK(fight.attackers == 255);
    CHECK(fight.elites == 255);
    CHECK(fight.boss);
}
