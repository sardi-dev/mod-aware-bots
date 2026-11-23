# PvP class packages (rules pass, lane 2): BT-650 and up

The shared PvP layers (awareness, interrupt gate, defensives and medallion, fake casts, the burst window, the team play with the
healer lock and the control chain, the 1v1 engine) decide what every class shares. A class package adds only what one
specialisation does inside them, as candidate options with a deterministic score and the skill tiers of design decision 0015: an expert does
all of it, a good bot the simple half, a casual bot keeps the older rotation. Pure decisions live in `src/Utils/Pvp<Spec>Policy.h`
with tests; the game side reads what a player sees and sits in `src/PvP/Pvp<Spec>.cpp` and the class triggers. Every package
answers only for an enemy player target: PvE is untouched. Order: ladder popularity (Frost Mage, Mistweaver, Affliction,
Restoration Druid, Arms).

Every rule here is a class rotation change: each waits for the owner's in-game test.

## Frost Mage (BT-650 to BT-656)

Policy `src/Utils/PvpFrostMagePolicy.h`, tests `tests/test_pvp_frost_mage.cpp`, glue `src/PvP/PvpFrostMage.cpp`, triggers in
`MageTriggers.cpp`, edges in `FrostMageStrategy.cpp`.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-650 | Ring of Frost is an armed link of the healer control chain: cast under the enemy healer while a stun already holds it for the 2 s the ring needs to arm (and at most 1.2 s longer), never as an opener. It follows the incapacitate diminishing returns (it shares them with Polymorph). `armMs` on the control candidate, a ground row in the shared kit | expert | 1, 8 |
| BT-651 | Alter Time (level 87) is the go of the burst: inside the burst window, with Icy Veins and a Fingers of Frost charge or a Brain Freeze proc on the bar (a good bot: either), never while Hypothermia is on the mage or the rewind runs | expert, good | 2, 3 |
| BT-652 | The Water Elemental casts Freeze under the kill target before Deep Freeze (Deep Freeze needs a frozen target; a Freeze root is one) and banks a Fingers of Frost charge when the bar has room; not on a held or sheeped target. A Freeze root on the target now fires the Deep Freeze edge | expert, good | 6 |
| BT-653 | Frozen Orb against a player is a window cooldown, not a filler on cooldown, and is never cast into a target held by a sheep (the orb breaks it). PvE keeps the old rule; a casual bot keeps the filler | expert, good | 2 |
| BT-654 | In a battleground or arena with an enemy healer near the target, Deep Freeze is kept for the healer (the control chain casts it) until the healer is held 2.5 s or more; a Fingers of Frost proc no longer spends it on a damage dealer | expert, good | 1 |
| BT-655 | Presence of Mind goes to an instant control against a player: the Polymorph the team plan names for the healer, else Frostjaw on the kill target in the window (the route of the Fire package) | expert, good | 1 |
| BT-656 | Incanter's Ward (the ladder's tier 90 pick, cast by nothing before) goes up when the burst window opens, and when a player hits the mage below 90 percent health; a casual bot shields when attacked | all | 2 (burst step 1) |

What stays as it was: Frost Nova, Blink, Cone of Cold and the kite (shared mage edges and the 1v1 engine), the healer lock links
Polymorph, Deep Freeze and Frostjaw (BT-629), Counterspell priority (BT-521), Ice Block (hp rule).

Not built, with the reason: armor by enemy comp (Molten against melee, Mage Armor against casters; a swap needs the armor glyph
and a comp read, sheet BT-027), pet placement away from the mage against melee cleave (needs a pet move action), Living Bomb
timing around a sheep (the damage hold of BT-630 already blocks the cast; the explosion cannot be cancelled).

PvP set: no change. Every pick the rules use is in the set already (Presence of Mind, Ring of Frost, Cold Snap, Living Bomb,
Incanter's Ward, Glyph of Deep Freeze); Frost Water Elemental glyph has 1 of 11 on the ladder (9 percent) and stays out.

What the duel bench should show for a Frost bot: Pet Freeze then Deep Freeze early in the fight (the Freeze cast on the debug
log as `[PVPFROST] kind=PET_FREEZE`), Frozen Orb and Alter Time only after the window opened (`[PVPFROST] kind=ALTER_TIME` once
per burst, never with Hypothermia up), the ward before Icy Veins.

## Mistweaver Monk (BT-657 to BT-662)

Policy `src/Utils/PvpMistweaverPolicy.h`, tests `tests/test_pvp_mistweaver.cpp`, glue `src/PvP/PvpMistweaver.cpp`, one trigger and one
action class (`PvpMistweaverTrigger`, `PvpMistweaverAction`) in `MonkActions.h`, edges in `MistweaverMonkStrategy::InitTriggers`. Every
edge answers only in a fight against players (a battleground, a duel, or an ally with a player on him); the dungeon chains of BT-135
are untouched and casual bots keep them.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-657 | Healing Sphere (level 64) is cast under the hurt ally as a spot heal: from 80 percent health down to the critical line (30 percent, where the direct heals take over); an expert keeps one of the three sphere slots free; the most hurt ally, and the one a player is hitting, scores higher | expert (line 80), good (line 70) | 3 |
| BT-658 | Life Cocoon as a tempo tool on the ally the enemy team is working on: at 55 percent when an enemy burst aura is up on a player hitting him (the burst list of the team state), at 45 percent without one; a good bot at 40 percent; never on an ally who has it, never with nobody hitting. The BT-135 emergency edge (30 percent, tank 10) stays | expert, good | 5 |
| BT-659 | Thunder Focus Tea is pressed when the weakest ally is at or under 60 percent (50 for a good bot), a player is hitting him and, for an expert, an enemy kick is not seen spent; the tea then gives an instant Surging Mist on the weakest ally below 75 percent (any tier) | expert, good | 4 |
| BT-660 | Mana Tea (two stacks) in a lockout (silenced, mana at 90 percent or less, even with allies hurt) and in a quiet moment (nobody under 70 percent, no player on the monk, mana at or under 80 percent for an expert, 65 for a good bot) | expert, good | 1 |
| BT-661 | Tiger's Lust (the ladder's tier 15 pick) frees the monk or a rooted or slowed ally in a PvP fight | expert, good | none (idle pick) |
| BT-662 | Nimble Brew gets a row in the shared catalog of control breaks (every monk): the shared break layer casts it before the medallion against a stun, fear, root or horror | all | 7 |

Not built, with the reason: Ring of Peace or Leg Sweep by comp (the ladder is 91 percent Leg Sweep and the set holds it; the talent
spell edge exists), Paralysis target choice (needs the awareness cooldown read of an enemy), Fortifying Brew when stunned (the
shared defensive layer reads the catalog tempo row), Revival safeguards against Affliction (needs a DoT count), statue and
Transcendence placement (a placement search), Renewing Mist upkeep on the kill target (BT-157 keeps it on the heal target value).

PvP set: no change. Tiger's Lust (tier 15, 100 percent), Chi Wave, Chi Brew, Leg Sweep, Healing Elixirs (91 percent), Invoke Xuen
(100 percent) and the glyphs of Life Cocoon and Enduring Healing Sphere (both 100 percent) all have rules now; the Ring of Peace
the guide prefers with stuns on the team is the ladder's 9 percent pick, so Leg Sweep stays.

What the duel bench should show: a Mistweaver bot is rarely in a duel (a healer); in a skirmish or an arena pair look for
`[PVPMONK] kind=SPHERE` under hurt allies, `kind=COCOON` once per enemy burst, `kind=THUNDER_FOCUS_TEA` followed by
`kind=SURGING_MIST`, and `kind=MANA_TEA` right after a silence.

## Affliction Warlock (BT-663 to BT-667)

Policy `src/Utils/PvpAfflictionPolicy.h`, tests `tests/test_pvp_affliction.cpp`, glue `src/PvP/PvpAffliction.cpp`, one trigger class
(`PvpAfflictionTrigger`) and one action (`CastHowlOfTerrorPvpAction`) in the warlock files, edges in `AffliWarlockStrategy::InitTriggers`,
a kit row in `PvpTeam.cpp`. Every edge answers only in a fight against players; PvE and casual bots keep the older rotation.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-663 | Shadowfury (the ladder's tier 30 pick, an area stun on the ground under the healer) is a row of the shared control kit: it opens the healer lock and Fear (the existing kit row) follows as the stun runs out. It is not cast when an enemy within 8 yd of the healer is held in a breakable control (the hit would free it). An instant control now counts as on its way for 0.6 s instead of 2.5 s, so a 3 s stun does not hold the next link back for most of its length (applies to every instant link of the chain) | expert (chain), good (opener) | 3 |
| BT-664 | Howl of Terror as a peel: a melee player within 8 yd that is attacking the warlock; an expert at any health, a good bot at 85 percent or less, a casual bot at 55 percent or less; never into an enemy a teammate holds in a breakable control (not read by a casual bot) | all | 6 |
| BT-665 | Fel Flame while the shadow school is locked out (Corruption, which has no cooldown of its own, shows a cooldown after an interrupt): the DoTs are shut, the instant fire spell is not | expert, good | 12 |
| BT-666 | Haunt against a player waits for the burst window, a target at 30 percent health or less, or (expert) a stunned or rooted one; never on a feared or sheeped target (its damage frees it). PvE and casual bots keep Haunt as maintenance | expert, good | 11 |
| BT-667 | The Healthstone is used beside a running Dark Regeneration at 50 percent health or less (the regeneration heals over 6 s, the stone at once); before, the stone waited for the critical line | expert, good | 9 |

Not built, with the reason: Soulburn Soul Swap spreading (the core scripts the inhale and the exhale as separate spells and an
override aura; the bot cast path resolves a spell by name and needs a live read of what the button becomes before it can be
encoded), snapshot refresh on a damage proc (needs the DoT refresh table for snapshots, `WarlockDotRefresh.h`), Fear on a damage
dealer when the healer's dispel is spent (needs the awareness book to read dispels, which it does not record), Demonic Circle
placement (no circle spells in the module), Soulburn pet summon (the pet is summoned out of combat only), DoTs on enemy pets.

PvP set: no change. Dark Regeneration (tier 15, 100 percent), Shadowfury (tier 30, 100 percent), Unbound Will (80 percent, in the
catalog of cheap breaks), Grimoire of Supremacy (90 percent) and Archimonde's Darkness (100 percent) are all picks the rules or the
existing edges use; the glyphs are passive (Siphon Life, Unstable Affliction, Eternal Resolve). Eternal Resolve removes the
Unending Resolve button the generic low health edge presses; the edge fails and falls through to Dark Regeneration, so the ladder's
100 percent glyph stays.

What the duel bench should show: an Affliction bot casts no Haunt in the first seconds of a duel against a free target (it waits
for the duel window, which is open for the whole duel, so Haunt is cast at once in a duel: the gate bites only in team fights); in
a skirmish `[PVPTEAM] kind=CONTROL spell=30283` on the enemy healer, then a Fear link, and `[PVPLOCK] kind=HOWL` against a melee
opponent.

## Restoration Druid (BT-668 to BT-672)

Policy `src/Utils/PvpRestoDruidPolicy.h`, tests `tests/test_pvp_resto_druid.cpp`, glue `src/PvP/PvpRestoDruid.cpp`, the ally reading shared with
the Mistweaver package in `src/PvP/PvpAllyFacts.cpp`, one trigger class and one action class (`PvpRestoDruidTrigger`, `PvpRestoDruidAction`)
in the druid files, edges in `HealDruidStrategy::InitTriggers`. Every edge answers only in a fight against players (a battleground, a duel, or an ally
with a player on him); the main tank rules of BT-123 and BT-293 stay, and casual bots keep them.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-668 | Cenarion Ward on the ally a player is hitting (95 percent health or less for an expert, 80 for a good bot), and on the weakest ally while the druid is silenced (it works while silenced); the most hurt and most attacked ally first; never on an ally who has it | expert, good | 3, 6 |
| BT-669 | Ironbark early on the ally a player is hitting: at 80 percent or less when an enemy burst aura is up on one of the attackers (expert), at 55 percent without a burst (expert) or 45 percent (good bot) | expert, good | 3 |
| BT-670 | Nature's Swiftness then an instant Cyclone on an enemy damage dealer that shows a burst aura and is hitting an ally (the healer is the control chain's business); without Nature's Swiftness a hard cast only when no enemy kick is left unspent; not on a held target or one at the Cyclone diminishing return of immune | expert | 5 |
| BT-671 | Displacer Beast (the ladder's tier 15 pick, 100 percent; it had a cat form rule only) as the escape from a melee player within 10 yd that is hitting the druid: at 60 percent health or less for an expert, 45 for a good bot, 30 for a casual bot; the druid turns its back to the melee first | all | 7 |
| BT-672 | Typhoon (tier 45, 100 percent; no Restoration rule before) as the expert's peel against that melee player at 75 percent or less when Displacer Beast is not there; not when an enemy near the druid is held in a breakable control | expert | none (idle pick) |

Not built, with the reason: the first Cyclone on an off damage dealer with a swap onto the healer (needs the team's kill target, which the
arena plan owns), Faerie Fire silence in bear form (the Fae Silence glyph is 88 percent of the ladder; it stays and is the next rule to
build), pre-swap Bear Form and pre-HoTs (a heal of the druid leaves the form again, and the swap needs a read of the enemy's intent),
Disorienting Roar before the Cyclone on the healer (the talent edge peels; a chain link needs the ring-style arming, which an expert's chain
could use later), Wild Mushroom and Bloom placement, Heart of the Wild follow-up, safe Tranquility, the double cast with a caster partner.

PvP set: no change. Displacer Beast, Cenarion Ward and Typhoon (all 100 percent of the ladder) now have rules; Heart of the Wild
(talent policy) and Disorienting Roar (self peel) already did; Soul of the Forest is a passive of the Swiftmend edges; the Cyclone glyph is used
by BT-670. The Fae Silence glyph (88 percent) has no rule yet and stays.

What the duel bench should show for a Restoration Druid bot: duels are rare for a healer; in a skirmish or an arena pair look for
`[PVPDRUID] kind=CENARION_WARD` on the focused ally, `kind=IRONBARK` right after an enemy cooldown, `kind=NATURES_SWIFTNESS` followed by
`kind=CYCLONE` on a bursting enemy, and `kind=DISPLACER_BEAST` against a melee opponent at about 60 percent health.

## Arms Warrior (BT-673 to BT-676)

Policy `src/Utils/PvpArmsPolicy.h`, tests `tests/test_pvp_arms.cpp`, glue `src/PvP/PvpArms.cpp`, two triggers, one action and one multiplier in the warrior
files, edges and the multiplier in `ArmsWarriorStrategy`. Every edge answers only against an enemy player target; PvE and casual bots keep the older rotation.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-673 | The stun chain: Storm Bolt (the ladder's tier 90 pick, 75 percent; no Arms rule cast it before) or Shockwave (tier 60, 75 percent) right after Charge or Heroic Leap landed, or inside the burst window, on a target that is not already held. An expert needs a full stun (its book reads the stun diminishing returns at the first step); Shockwave is the pick when its cone catches a second player or Storm Bolt is spent | expert, good | 4 |
| BT-674 | Disarm (the course: after a stun, so it cannot be dodged) on a melee player at 5 yd that is stunned, and for an expert also one that shows a burst aura; a good bot inside the window | expert, good | 9 |
| BT-675 | Recklessness, Skull Banner and Avatar are one go against a player: held until the burst window is open, and for an expert until 80 rage is pooled (given up after 5 s of an open window, because the heroic strike dump keeps rage low); a good bot goes with the window. A multiplier on the Arms strategy blocks the three casts when the go is not there | expert, good | 2 |
| BT-676 | Touch of Karma (122470) is an untouchable target in the shared immunity read of the burst window, the team go and the arena plan: the window closes on a monk under Karma and the focus looks elsewhere (every class) | all | 13 |

Not built, with the reason: Charge held until the enemy used its own gap closer (the awareness book does not record mobility), Heroic Leap to
rejoin, interrupt targets by school (the shared interrupt gate of BT-521 orders them), Thunder Clap to spread Deep Wounds (two or three targets in
a team fight), Defensive Stance and an early Demoralizing Banner against stealth (needs the enemy comp read), Staggering Shout opener, Spell
Reflection as a swap trigger (only Karma is read), Shattering Throw only on an immunity (already so: the trigger reads Divine Shield, Ice Block and the protection
hands).

PvP set: no change. Storm Bolt and Shockwave (75 percent each) now have an Arms rule; Staggering Shout and Safeguard have talent-policy edges;
Double Time is a passive of Charge and the glyphs (Bull Rush 100 percent, Death From Above 75, Mortal Strike 62) are passive. Second Wind and
Enraged Regeneration split the ladder 50/50, so the set keeps Second Wind and the Enraged Regeneration edge stays idle for it.

What the duel bench should show for an Arms bot (rogue, mage and paladin opponents): `[PVPARMS] kind=STUN` right after the Charge, Disarm on a
rogue that Storm Bolt held, and `[PVPBURST]` open with Recklessness cast only afterwards (not in the first tick of the duel, where rage is zero).

## Elemental Shaman (BT-677 to BT-681)

Policy `src/Utils/PvpElementalPolicy.h`, tests `tests/test_pvp_elemental.cpp`, glue `src/PvP/PvpElemental.cpp`, one trigger class and one action class
(`PvpElementalTrigger`, `PvpElementalAction`) in the shaman files, edges and one multiplier in the caster (Elemental) strategy. Every edge answers only against enemy
players; PvE and casual bots keep the older rotation.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-677 | Ancestral Swiftness (the ladder's tier 60 pick of 100 percent) is pressed before the Hex on the enemy healer when the shared control chain names this shaman's Hex and an enemy kick is not seen spent (the instant Hex cannot be kicked); in a team fight with Hex known and ready, a multiplier keeps the generic edge from casting Swiftness on cooldown. The Hex itself is the existing row of the control kit | expert (press), expert and good (hold) | 2 |
| BT-678 | Grounding Totem on a hostile player's control cast (polymorph, fear, hex, sleep and the like) on its way to the shaman, or for an expert to an ally within 30 yd, when enough of the cast is left (350 ms expert, 550 ms good bot); the cast is tracked from the first look | expert, good | 4 |
| BT-679 | Call of the Elements (the ladder's tier 45 pick, 100 percent) when the Grounding Totem is spent and another control cast is on its way with enough left for both totems; it had no Elemental rule | expert | none (idle pick) |
| BT-680 | Ascendance, the Fire Elemental Totem and Elemental Mastery are held against a player until the burst window is open (they were cast on cooldown by the "known" edges); PvE and casual bots unchanged | expert, good | 3 |
| BT-681 | Spiritwalker's Grace (level 87) while the shaman is moving with a target to cast at and an enemy player within 30 yd (expert) or 15 yd (good bot) | expert, good | 9 |

Not built, with the reason: Flame Shock on every enemy (the multi-target edge needs the attacker value), Shamanistic Rage on a stun (the Elemental set
has no Shamanistic Rage glyph, 29 percent of the ladder, and the courses disagree whether it works while silenced), school lock fallbacks (Fire lock has no
spell without a cooldown of its own to read it by; the nature lock already falls through to Lava Burst), Healing Tide and Healing Surge for allies (the
off-heal policy of the module), Purge by priority (a shared dispel rule), Thunderstorm on a ledge map.

PvP set: no change. Ancestral Swiftness (tier 60) and Call of the Elements (tier 45) now have rules; Windwalk Totem has the talent policy edge, Ancestral Guidance the
low health edge, Nature's Guardian and Unleashed Fury are passive. The glyph slots split on the ladder (Healing Stream Totem 57 percent against the 60 percent line),
so the set keeps its majors; Lightning Shield and Totemic Encirclement (100 and 71 percent) are held.

What the duel bench should show: shamans are rarely cast against in a bench duel (the Hex row needs an enemy healer), so look in a skirmish for
`[PVPSHAMAN] kind=GROUNDING` after an enemy `Polymorph` or `Fear` cast starts, `kind=ANCESTRAL_SWIFTNESS` right before `[PVPTEAM] kind=CONTROL spell=51514`, and
Ascendance only after `[PVPBURST] open=1`.

## Holy Paladin (BT-683 to BT-687)

Policy `src/Utils/PvpHolyPaladinPolicy.h`, tests `tests/test_pvp_holy_paladin.cpp`, glue `src/PvP/PvpHolyPaladin.cpp`, the ally reading and the incoming control reader shared with the
Mistweaver, Restoration Druid and Elemental packages in `src/PvP/PvpAllyFacts.cpp`, one trigger class and one action class (`PvpHolyPaladinTrigger`, `PvpHolyPaladinAction`) in the paladin
files, edges in `HealPaladinStrategy::InitTriggers`. Every edge answers only in a fight against players; the main tank edges of BT-293 stay and casual bots keep them.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-683 | Denounce in the downtime on an enemy player in 30 yd: the paladin is standing, nobody is under 80 percent (90 for a good bot), the target is not held by a sheep or fear, and (expert) no enemy kick is left unspent. With this rule the Glyph of Denounce (43867, 70 percent of the 3v3 ladder) goes into the PvP set in place of Blessed Life (40 percent): this closes BT-452 | expert, good | 12 |
| BT-684 | Hand of Sacrifice on the ally a control cast is on its way to (an expert, with at least 300 ms of the cast left), and on an ally at 40 percent or less (expert; 35 for a good bot) that a player is hitting, while the paladin itself is above 60 percent; never on the paladin, never on an ally who has it | expert, good | 6 |
| BT-685 | Hand of Protection on a stunned ally at 70 percent or less under melee (expert), on an ally at 60 percent or less when a melee attacker shows a burst aura (expert), and at 35 percent or less (good bot); not on a melee damage dealer who is not stunned (the bubble stops his attacks), never when no melee player is hitting him | expert, good | 5 |
| BT-686 | Devotion Aura (silence and interrupt immunity, 180 s) when the paladin is silenced and an ally is at 75 percent or less (expert; 50 for a good bot) | expert, good | 8 |
| BT-687 | Beacon of Light on the ally the most players are hitting: placed when none is up, and (expert) moved off an ally nobody is hitting; the main tank rule of the Holy strategy stays for a casual bot | expert, good | 1 |

Not built, with the reason: Divine Protection when stunned and the Divine Shield against medallion rules (the shared defensive layer reads the catalog tempo and immunity rows, BT-522), Fist of Justice
and Blinding Light chains (Hammer of Justice and Repentance are rows of the control kit; the Fist of Justice talent is passive on Hammer of Justice and Blinding Light has no kit row), Word of Glory and
Eternal Flame spenders by Holy Power, Divine Favor and Avenging Wrath timing (the group heal edges keep them), pillar play and instants-only after a swap.

PvP set: one change, with the rule that uses it (owner decision 2026-11-19): `PremadeSpecGlyphPvp.2.0` takes the Denounce glyph (43867) instead of Blessed Life (41110). The Denounce glyph is 70 percent of the
3v3 ladder (8 of 11 pooled with 2v2) against Blessed Life 40 percent, so the change follows the 60 percent rule of BT-450 and goes with BT-683. Every other pick stays: all six tiers match the ladder
(Speed of Light 90, Fist of Justice 90, Eternal Flame 100, Clemency 80, Divine Purpose 70, Holy Prism 90), and Clemency (two charges of the Hands) is what BT-684 and BT-685 spend.

What the duel bench should show: Holy paladins are healers, so a bench duel is not their case; in a skirmish or an arena pair look for `[PVPPALADIN] kind=HAND_OF_SACRIFICE` right after an
enemy control cast begins on a teammate, `kind=HAND_OF_PROTECTION` on a stunned healer or caster, `kind=BEACON_OF_LIGHT` following the focus, and `kind=DENOUNCE` only between heals.

## Shadow Priest (BT-688 to BT-690)

Policy `src/Utils/PvpShadowPriestPolicy.h`, tests `tests/test_pvp_shadow_priest.cpp`, glue `src/PvP/PvpShadowPriest.cpp`, one trigger class, one action class and one multiplier in the priest
files, edges and the multiplier in the Shadow strategy. Every edge answers only in a fight against players; PvE and casual bots keep the older rotation. This package is short because the
ladder has four Shadow players (rank 10 of 17 specs) and the guide's other rules are covered by the shared layers or need pieces that do not exist yet.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-688 | Vampiric Embrace goes with the burst or a hurt team: two allies under 80 percent, or the window open with one hurt, or the priest at 50 percent or less; a multiplier holds the generic edge (it cast the 3 minute cooldown on every fight) otherwise. PvE and casual bots unchanged | expert, good | 10 |
| BT-689 | Leap of Faith on an ally who is stunned or rooted, at least 8 yd away, with a player hitting him: at 65 percent health or less (expert), 40 (good bot); the most hurt and most attacked first | expert, good | 8 |
| BT-690 | Mass Dispel (with the Glyph of Mass Dispel, which the set holds and nothing used) on the target when it holds Ice Block, Divine Shield or Hand of Protection, if the priest is not silenced and is within 30 yd | expert, good | 7 |

Not built, with the reason: Psychic Scream after Psychic Horror and Psyfiend by the healer (Scream strikes around the priest, not at a target, and Psyfiend is a ground summon whose placement needs its own
rule; Psychic Horror and Silence are rows of the control kit), Dispersion at 40 percent (the shared defensive layer reads the catalog row), Mind Control off a ledge, the kick juke (the shared fake cast rule is for
casts the bot starts), the self shield (the generic `being attacked` edge exists), orb and proc timing of the rotation.

PvP set: no change (the ladder has 4 Shadow players, too few to compare, BT-451). The picks have rules: Psyfiend, Angelic Feather, Spectral Guise and Divine Star have talent policy edges, From Darkness Comes Light
and Divine Insight are passives, and the Glyph of Mass Dispel now has BT-690.

What the duel bench should show: a Shadow bot in a skirmish with a mage or paladin that bubbles: `[PVPPRIEST] kind=MASS_DISPEL` after Ice Block or Divine Shield goes up (only if the glyph aura shows on the
priest: check the glyph spell id live, 55691 or 57190), and Vampiric Embrace only after allies drop under 80 percent.

## Subtlety Rogue (BT-691 to BT-693)

Policy `src/Utils/PvpSubtletyPolicy.h`, tests `tests/test_pvp_subtlety.cpp`, glue `src/PvP/PvpSubtlety.cpp`, one trigger class, one action class and one multiplier in the
rogue files, edges and the multiplier in `SubtletyRogueStrategy`. Every edge answers only against enemy players; PvE and casual bots keep the older rotation. Like the Shadow
package this one is short: the ladder has no Subtlety player in the pooled top 100 (the guide is the only source), and the guide's chain rules (Blind on the healer, Kidney, Gouge) are rows of the
shared control kit already.

| Id | Rule | Tier | Source line (digest section 12) |
|----|------|------|--------------------------------|
| BT-691 | Sap on a second enemy from stealth, before the rogue opens: an unaware enemy player within 10 yd that is not the kill target and not held, the healer first (80), a caster next (60); a good bot saps any second enemy (40); an expert reads the incapacitate diminishing returns of its book; the Glyph of Blind and the kit's Blind stay for the healer chain | expert, good | 4 |
| BT-692 | Shadow Dance, Shadow Blades and Marked for Death are the go: held against a player until the burst window is open (the Assassination and Subtlety edges cast them on cooldown); PvE and casual bots unchanged | expert, good | 3 |
| BT-693 | Tricks of the Trade on the best damage partner (not a healer, not a tank, within 60 yd and in sight) when the team go opens on the kill target; the partner showing a burst aura first; the older edge on a bot main tank stays | expert | 10 |

Not built, with the reason: re-stealth behind a pillar and the opener order against another stealth class (a placement and awareness read), the Hemorrhage and Backstab choice by position (the builder
edges exist), Kidney Shot on the kill target in a setup (the kit casts it on the healer; a kill-target stun is the Arms chain's pattern and needs the team's kill target), Evasion facing and Vanish by comp (the
shared defensive layer), Cheat Death or Elusiveness by comp (the set holds Elusiveness).

PvP set: no change. Subterfuge, Nerve Strike, Elusiveness and Prey on the Weak are passives, Burst of Speed has the talent policy edge (freedom), and Marked for Death has its edge (now held for the window by
BT-692); the glyphs are passive or cosmetic (Blind, Distract, Hemorrhaging Veins, Blurred Speed, Safe Fall, Cheap Shot).

What the duel bench should show for a Subtlety bot against a mage or a priest: no Shadow Dance in the first seconds unless the duel window rule opens it (a duel is open for the whole fight, so the
hold bites only in team fights); in a skirmish `[PVPROGUE] kind=SAP` on a healer before the first hit, and `kind=TRICKS_OF_THE_TRADE` right after `[PVPTEAM] kind=GO`.
