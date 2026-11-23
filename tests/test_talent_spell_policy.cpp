/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

#include "TalentSpellPolicy.h"

#include <set>
#include <string>

using namespace TalentSpellPolicy;

TEST(talent_spell_table_is_in_enum_order_with_unique_names)
{
    std::set<std::string> names;
    for (std::size_t i = 0; i < TABLE.size(); ++i)
    {
        CHECK(static_cast<std::size_t>(TABLE[i].talent) == i);
        CHECK(names.insert(TABLE[i].name).second);
        CHECK(TABLE[i].spellId != 0);
        CHECK(std::string(TABLE[i].name).rfind("talent ", 0) == 0);
    }
    CHECK(TABLE.size() == static_cast<std::size_t>(Talent::COUNT));
}

TEST(talent_spell_psyfiend_never_fears_in_a_pve_group)
{
    Facts f;
    f.grouped = true;
    f.attackersOnBot = 4;
    f.healthPct = 20.0f;
    CHECK(!WantsPsyfiend(f));
    f.grouped = false;
    CHECK(WantsPsyfiend(f));
    f.healthPct = PEEL_HEALTH_PCT;
    CHECK(!WantsPsyfiend(f));
    Facts pvp;
    pvp.grouped = true;
    pvp.enemyPlayerClose = true;
    CHECK(WantsPsyfiend(pvp));
}

TEST(talent_spell_escape_is_for_a_hit_non_tank_low_on_health)
{
    Facts f;
    f.attackersOnBot = 1;
    f.healthPct = ESCAPE_HEALTH_PCT - 1.0f;
    CHECK(WantsEscape(f));
    f.tank = true;
    CHECK(!WantsEscape(f));
    f.tank = false;
    f.attackersOnBot = 0;
    CHECK(!WantsEscape(f));
    f.healthPct = ESCAPE_HEALTH_PCT;
    f.attackersOnBot = 1;
    CHECK(!WantsEscape(f));
}

TEST(talent_spell_speed_kites_players_and_chases_as_melee)
{
    Facts caster;
    CHECK(!WantsKiteSpeed(caster));
    caster.enemyPlayerClose = true;
    CHECK(WantsKiteSpeed(caster));
    CHECK(WantsSpeedOfLight(caster));

    Facts melee;
    melee.melee = true;
    melee.enemyPlayerClose = true;
    CHECK(!WantsKiteSpeed(melee));
    melee.targetIsPlayer = true;
    melee.targetDistance = 25.0f;
    CHECK(WantsSpeedOfLight(melee));
    melee.targetDistance = CHASE_MIN_YD;
    CHECK(!WantsSpeedOfLight(melee));
    melee.targetDistance = 25.0f;
    melee.targetIsPlayer = false;
    CHECK(!WantsSpeedOfLight(melee));
    melee.targetIsPlayer = true;
    melee.rooted = true;
    CHECK(!WantsSpeedOfLight(melee));
}

TEST(talent_spell_freedom_needs_a_slow_that_matters)
{
    Facts f;
    CHECK(!WantsFreedom(f));
    f.impaired = true;
    CHECK(!WantsFreedom(f));
    f.rooted = true;
    CHECK(WantsFreedom(f));
    f.rooted = false;
    f.pvp = true;
    CHECK(WantsFreedom(f));
    f.pvp = false;
    f.melee = true;
    f.targetDistance = CHASE_MIN_YD + 1.0f;
    CHECK(WantsFreedom(f));
    f.targetDistance = 3.0f;
    CHECK(!WantsFreedom(f));
}

TEST(talent_spell_leg_sweep_interrupts_and_stuns_packs)
{
    Facts f;
    f.tank = true;
    CHECK(!WantsLegSweep(f));
    f.casterInRadius = true;
    CHECK(WantsLegSweep(f));
    f.casterInRadius = false;
    f.enemiesInRadius = AREA_STUN_PACK - 1;
    CHECK(!WantsLegSweep(f));
    f.enemiesInRadius = AREA_STUN_PACK;
    CHECK(WantsLegSweep(f));

    Facts dps;
    dps.attackersOnBot = PEEL_ATTACKERS;
    dps.enemiesInRadius = PEEL_ATTACKERS;
    CHECK(WantsLegSweep(dps));
    dps.enemiesInRadius = 0;  // the attackers are out of the 5 yd sweep
    CHECK(!WantsLegSweep(dps));
}

TEST(talent_spell_self_peels_wait_for_danger)
{
    Facts f;
    f.attackersOnBot = PEEL_ATTACKERS;
    f.healthPct = 90.0f;
    CHECK(!WantsSelfPeel(f));
    CHECK(WantsRingOfFrost(f));  // a ring is no heal: two mobs on the mage are enough
    f.healthPct = PEEL_HEALTH_PCT - 1.0f;
    CHECK(WantsSelfPeel(f));
    f.tank = true;
    CHECK(!WantsSelfPeel(f));
    CHECK(!WantsRingOfFrost(f));
}

TEST(talent_spell_displacer_beast_is_a_cat_gap_closer)
{
    Facts f;
    f.catForm = true;
    f.facingTarget = true;
    f.targetDistance = 15.0f;
    CHECK(WantsLeap(f));
    f.targetDistance = LEAP_MAX_YD + 1.0f;
    CHECK(!WantsLeap(f));
    f.targetDistance = 15.0f;
    f.catForm = false;  // a bear tank or a caster would be pulled into cat form
    CHECK(!WantsLeap(f));
    f.catForm = true;
    f.facingTarget = false;
    CHECK(!WantsLeap(f));
}

TEST(talent_spell_divine_star_and_holy_prism)
{
    Facts shadow;
    shadow.facingTarget = true;
    shadow.targetDistance = 20.0f;
    CHECK(WantsDivineStar(shadow));
    shadow.targetDistance = STAR_RANGE_YD + 1.0f;
    CHECK(!WantsDivineStar(shadow));

    Facts healer;
    healer.healer = true;
    healer.facingTarget = true;
    healer.targetDistance = 10.0f;
    CHECK(!WantsDivineStar(healer));
    healer.healTargetPct = 70.0f;
    CHECK(WantsDivineStar(healer));
    CHECK(WantsPrismHeal(healer));
    CHECK(!WantsPrismDamage(healer));

    Facts ret;
    ret.targetDistance = 5.0f;
    CHECK(WantsPrismDamage(ret));
    CHECK(!WantsPrismHeal(ret));
}

TEST(talent_spell_shout_lichborne_ground_ward)
{
    Facts w;
    w.targetIsPlayer = true;
    w.targetSnared = true;
    w.targetDistance = 8.0f;
    CHECK(WantsStaggeringShout(w));
    w.targetDistance = SHOUT_RADIUS_YD + 1.0f;
    CHECK(!WantsStaggeringShout(w));

    Facts dk;
    dk.fearCasterNear = true;
    CHECK(!WantsLichborne(dk));
    dk.pvp = true;
    CHECK(WantsLichborne(dk));
    dk.enemyPlayerClose = true;
    dk.healthPct = PEEL_HEALTH_PCT - 1.0f;
    CHECK(WantsDesecratedGround(dk));

    Facts resto;
    resto.healer = true;
    CHECK(!WantsCenarionWard(resto));
    resto.mainTankInCombat = true;
    CHECK(WantsCenarionWard(resto));
    resto.hasAura = true;
    CHECK(!WantsCenarionWard(resto));
    Facts feral;
    feral.attackersOnBot = 1;
    feral.healthPct = SELF_WARD_PCT - 1.0f;
    CHECK(WantsCenarionWard(feral));
}
