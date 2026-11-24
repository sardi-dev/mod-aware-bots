#include "TestHarness.h"
#include "WsgPlanLogic.h"

using namespace WsgLogic;

TEST(wsg_combat_escort_respects_role_objective_and_control)
{
    for (Role role : {Role::TAKER, Role::BACKUP, Role::ESCORT})
    {
        CHECK(EscortClosesInCombat(role, Kind::ESCORT_CARRIER, false, true, true, false, 12.1f, 12.0f));
        CHECK(!EscortClosesInCombat(role, Kind::HUNT_EFC, false, true, true, false, 50.0f, 12.0f));
    }
    for (Role role : {Role::HOME, Role::MID, Role::NONE})
        CHECK(!EscortClosesInCombat(role, Kind::ESCORT_CARRIER, false, true, true, false, 50.0f, 12.0f));
    CHECK(!EscortClosesInCombat(Role::ESCORT, Kind::ESCORT_CARRIER, false, false, true, false, 50.0f, 12.0f));
    CHECK(!EscortClosesInCombat(Role::ESCORT, Kind::ESCORT_CARRIER, false, true, false, false, 50.0f, 12.0f));
    CHECK(!EscortClosesInCombat(Role::ESCORT, Kind::ESCORT_CARRIER, false, true, true, true, 50.0f, 12.0f));
    CHECK(!EscortClosesInCombat(Role::ESCORT, Kind::ESCORT_CARRIER, false, true, true, false, 12.0f, 12.0f));
}

TEST(wsg_selected_healers_close_and_can_stop_to_heal)
{
    CHECK(EscortClosesInCombat(Role::HOME, Kind::HEAL_CARRIER, true, true, true, false, 25.0f, HEALER_LEASH_YARDS));
    CHECK(!EscortClosesInCombat(Role::HOME, Kind::HEAL_CARRIER, true, true, true, true, 50.0f, HEALER_LEASH_YARDS));
    CHECK(HealerStopsToHeal(25.0f, 50.0f));
    CHECK(!HealerStopsToHeal(29.0f, 50.0f));
}

TEST(wsg_covering_mid_peel_is_limited_to_eligible_fighters)
{
    CHECK(MayPeelCarrier(Role::MID, Kind::COVER_CARRIER_REAR, false, Tier::GOOD, 35.0f, 35.0f));
    CHECK(MayPeelCarrier(Role::MID, Kind::COVER_CARRIER_REAR, false, Tier::EXPERT, 10.0f, 35.0f));
    CHECK(!MayPeelCarrier(Role::MID, Kind::COVER_CARRIER_REAR, false, Tier::GOOD, 35.1f, 35.0f));
    CHECK(!MayPeelCarrier(Role::MID, Kind::HOLD_MID, false, Tier::GOOD, 10.0f, 35.0f));
    CHECK(!MayPeelCarrier(Role::MID, Kind::COVER_CARRIER_REAR, true, Tier::GOOD, 10.0f, 35.0f));
    CHECK(!MayPeelCarrier(Role::MID, Kind::COVER_CARRIER_REAR, false, Tier::CASUAL, 10.0f, 35.0f));
    CHECK(!MayPeelCarrier(Role::HOME, Kind::COVER_CARRIER_REAR, false, Tier::GOOD, 10.0f, 35.0f));
    CHECK(MayPeelCarrier(Role::ESCORT, Kind::HUNT_EFC, false, Tier::GOOD, 50.0f, 35.0f));
}
