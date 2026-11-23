/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

#include "TankPackStartPolicy.h"

using namespace TankPackStartPolicy;

TEST(tank_pack_start_needs_a_pack_in_the_opening)
{
    CHECK(PackStart(PACK_ATTACKERS, 0));
    CHECK(PackStart(PACK_ATTACKERS + 2, OPENING_MS));
    CHECK(!PackStart(PACK_ATTACKERS - 1, 1000));
    CHECK(!PackStart(PACK_ATTACKERS, OPENING_MS + 1));
}
