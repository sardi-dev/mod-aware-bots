# Role sheet: damage dealer (all damage specialisations)

- **Status:** draft of 2026-11-12, first version, written from reading the code at `main` (commit b518acb, with BT-155
  `77fb012` in it). Nothing here was run and no log of a damage dealer fight was available beyond the Deadmines diagnosis.
  A line is **read** when a call site was read, **inference** when it follows from the code but no log shows it, **to check
  live** when only a test can tell. Levels, ids and costs come from the class sheets (`docs/class-sheets/`, 5.4.8 data); this
  sheet does not repeat the rotations.
- **Specialisations covered:** melee: arms and fury warrior, frost and unholy death knight, assassination, combat and
  subtlety rogue, retribution paladin, enhancement shaman, feral druid (cat), windwalker monk. Ranged physical: beast
  mastery, marksmanship and survival hunter. Casters: arcane, fire and frost mage, affliction, demonology and destruction
  warlock, shadow priest, elemental shaman, balance druid. Class sheets: `warrior-arms.md` through `monk-windwalker.md`.
  This sheet owns what they share in a group: hitting the right target at the right time without pulling threat or dying,
  and the group jobs (interrupts, crowd control, dispels, off-heals).
- **Owner direction (2026-11-12):** smarter roles that squeeze more value in dungeons and raids; bots that defend the group
  and follow the leader's marks; bots are led like players (design decision 0016). Tickets behind this sheet: the Deadmines death of the
  leader (the defend diagnosis notes section 4: both damage dealers killed the 300 hp marked parrot while four
  unmarked mobs killed the leader; BT-155 fixed the hold, not the order) and ticket 126 (an affliction warlock low on
  health against three mobs only channelled Drain Life; owner rule: keep DoTs on every attacker, Fear one, use the
  Voidwalker or a defensive, Drain Life only below about 35 percent; the warlock edge itself is a separate fix in progress).
- **Skill tiers (owner 2026-11-10, `AI/BotSkill.h`: 10 percent expert, 60 good, 30 casual, casual reacts 400 to 900 ms
  late):** every rule is marked **core** (all tiers), **good** (good and expert) or **expert**. PvE stays deterministic
  (design decision 0014): none of these rules rolls dice or asks a model. The shared target and threat code has no tier test today;
  only some class files use `BotSkill::Uses`.
- **Scope:** five-player dungeons first (a human leader who marks, three to four bots); raids in section 11.
- **Size key:** S under about 50 lines plus a test, M up to about 300 lines or a new value, L a new system or navigation work.

## State on 2026-11-17

The gap list of section 10 was written on 2026-11-12. What the code does now (read at `main` `bb57269` plus this run's
branch), with the ledger row:

| # | Gap | State |
|---|-----|-------|
| 1, 2 | threat gate spares utility, bot-led groups keep threat rules | done, BT-161 (`8cf2851`) |
| 3 | target order: member attacked, skull, cross, tank target, 3 s stickiness | done, BT-161 (`e8b33d7`); a new attacker of the healer waits up to 3 s behind the sticky target (design) |
| 4 | self-defence ladder | open as a shared system; the warlock part is BT-159, the walk to the tank for an unseen attacker BT-190 |
| 5 | interrupts: cast check, one kicker per cast, pack casters | done, BT-164 |
| 6, 7 | area spells next to crowd control, one controller per mark | part 1 done, part 2 per BT-165 |
| 8 | off-heals when the healer is dead or empty | done, BT-161 |
| 9 | cooldown timing by fight weight | done for 3 minute cooldowns and Bloodlust (BT-161); resurrections and Innervate exempted on 2026-11-17; the per class rules of the sheets sit in their strategies (BT-262, BT-264, BT-341, BT-342) |
| 10 | a damage dealer in combat opens on an unpulled mark when no member is attacked | open, rare (`DefendPolicy::HoldMark` keeps the `!botInCombat` clause) |
| 11 | pets follow the bot's target | open, mitigated by the target order of gap 3 |
| 12 | positioning (spread, walk to the tank, cone avoidance) | partly: avoid aoe for group damage dealers (BT-161, BT-294); no spreading |
| 13 | dispels by damage dealers | not read |

Measured since 2026-11-17: every fight of a damage dealer in a group writes a `[DPSFIGHT]` line (damage per second, casts,
cooldowns by fight weight, skill tier); `tools/dps_by_spec.py` reads them. Per specialisation status:
`docs/class-sheets/damage-status.md`.

## 1. Evidence used

| Source | What it shows |
|--------|---------------|
| the defend diagnosis notes section 4 | `DpsTargetValue::Calculate` returned the skull unless `IsTanksPull` held; the two damage dealers killed a bystander (300 hp, marked by the harness) for about 8 s while the leader fell from 81 to 45 percent, then the casters; the two melee mobs stayed at 100 percent until the leader died. |
| `77fb012` (BT-155, on `main`) | An unpulled mark is now held while any member is attacked (`DefendPolicy::HoldMark`), also once the damage dealer is in combat. Which attacker is picked instead is still the old lifetime rule (section 2). |
| Findings BT-112, BT-113, BT-141, BT-118 | Zero threat division fixed; the marked target stays the tank's until the group is ready; hidden attackers; the 45 s hold fallback. No row exists for BT-155 on `main`. |
| Class sheets, the gap lists | Interrupts reach "the current target only" for most classes; Fear, Polymorph, Banish, Freezing Trap and Cyclone fire only on a marked target; cooldowns fire "at the first tick of any fight" for several classes. |
| Owner ticket 126 | Drain Life as the only action with three mobs on a warlock (edge at `GenericWarlockStrategy.cpp:104-108`, priority `ACTION_INTERRUPT` at "low health" 45 percent, above every DoT at 17.5 to 19.5). |

## 2. What a good damage dealer does

### Before the pull
- Waits where the group is, drinks below 50 percent mana (core, `AiPlayerbot.DungeonDrinkMana`, `GenericTriggers.cpp:53`), eats
  and bandages for health. The tank pulls a mark only when every member is alive, out of combat, within 40 yd, at 60 percent
  health or more and 50 percent mana or more (`MarkTriggers.cpp` `MemberReady`), so a damage dealer that does not refill
  holds the whole group.
- Pre-pull jobs (good): a hunter Misdirection on the tank, a rogue Tricks of the Trade on the tank (class sheets; both have an
  edge today), a warlock Soulstone on the healer, a mage or warlock food and healthstone.
- Never opens on an unpulled mark while a bot tank is in the group (core): `AttackersValue::HoldMarkForTank` keeps the skull
  out of the attackers for up to 45 s, `DpsTargetValue.cpp` `IsTanksPull` keeps it out of the target pick.

### At the pull and the first seconds (waiting for the tank's first hits)
- The damage dealer opens when the mob has the tank on its threat list (the mob enters the attackers of the group) and then
  limits itself by threat (core): single target actions stop at 80 percent of the highest tank threat on the current target,
  area actions at 50 percent of the highest over all attackers (`ThreatStrategy.cpp` `ThreatMultiplier`, `ThreatValues.cpp`).
  A tank with no threat on a mob that is already fighting reads as 100 percent, so nobody starts on a mob the tank has not
  touched. This is the waiting rule. It exists only when a human is the master (gap 2) and it also stops things it should not
  (gap 1).
- Expert: burst cooldowns are held until the tank has threat and then used at once on a boss or elite pack (section 5).
- Ranged damage dealers stand at spell range (28.5 yd, `AiPlayerbot.SpellDistance`) from their target or closer to the
  group; melee stand behind the mob (`behind` strategy, `AiFactory.cpp:574`).

### Target priority in a fight (what the bot should do, in this order)
1. **A mob that is attacking a non-tank member and is not held** (core, defend). Healer first, then a caster, then the rest,
   then the leader. Today only the hold of an unpulled mark knows this (BT-155); the pick does not (gap 3).
2. **Skull** if it is pulled (core, exists: `RtiTargetValue`, `IsHighPriority` in the three strategies).
3. **Cross** (good, missing): the second kill mark. `RtiValue` is "skull" and nothing reads index 6.
4. **An interruptible caster in reach that nobody else is interrupting** (good, section 4).
5. **The tank's current target** (core, missing): a damage dealer that has no mark and no member to defend should help on
   what the tank holds. Today the pick is the lifetime band rule alone (`CasterFindTargetSmartStrategy::IsBetter`: mobs that
   die in 5 to 30 s first, then those under 5 s, then the rest; non-casters: nearest in range, then lowest health).
6. **Adds that arrive** with a mob on a member first (item 1), the tank's second target next.
7. Never a mob carrying a breakable crowd control of the group, and never the CC mark (moon) as a kill target (core, exists
   for index 4 only).
8. **Stickiness** (good, missing for melee): stay on the current target while it is among the top choices for 3 s
   (named constant); the caster strategy has a partial rule at low health, the melee and combo strategies none.

### Threat limits
- Single target 80 percent, area 50 percent (core, exists, values above). Needs three exemptions (gap 1): an interrupt, a
  defensive or a crowd control never waits for threat; a mob that already attacks the bot (victim is the bot) gets the
  defensive ladder (section 8), not a pause; a bot with no living tank in range is not limited (exists: the value is 0 with
  no living tank).
- Threat tools by class are in section 7; their use is "when two or more mobs attack me" (`medium threat`, `MyAttackerCountTrigger`
  with amount 2): warlock Soulshatter, rogue Gouge and Vanish, priest Fade, hunter Feign Death. Other classes have none.

### Area against single target
- Area actions start from 3 targets within 8 yd of the current target (`MediumAoeTrigger`; light is 2, high is 4,
  `GenericTriggers.h:296-310`). Class rules on top (read): warlock Seed of Corruption from 4 (affliction, level 60), Rain of
  Fire from 3, balance Hurricane from 5 (`DruidTriggers.h:158`), shaman Fire Nova and Chain Lightning from 3, frost death
  knight Howling Blast and windwalker Spinning Crane Kick from 3 (class sheets), arms Bladestorm from 3 (class sheet).
- The count includes mobs under a breakable crowd control and the area spells hit them (gap 5). Damage over time on another
  attacker already skips them (`AttackerWithoutAuraTargetValue.cpp` `HasBreakableByDamageCrowdControlAura`).
- Proposed shared rule (good): never cast an area spell while a mob with a breakable CC aura stands within the spell radius of
  the target; in a raid map count 4 and 6 instead of 3 and 4 (more adds, longer fights, more threat).

### When the healer is low or dead (what to do instead)
- Healer below 30 percent mana or dead (core for the first two lines):
  1. Stop pulling more: the readiness test already refuses a dead member and a member under 50 percent mana (`MemberReady`).
  2. Use own defensives and consumables earlier (healthstone, health potion, class cooldowns), and keep damage on the mob that
     hurts most (section 2, target priority item 1).
  3. Hybrid damage dealers heal themselves and the tank (good): retribution paladin Word of Glory or Flash of Light, feral
     druid Rejuvenation and Healing Touch (Predatory Swiftness), enhancement and elemental shaman Healing Surge,
     windwalker monk nothing worth casting. Today these live in `offheal` strategies that are opt-in by chat
     (`co +offheal`, `PaladinAiObjectContext.cpp:99`, `DruidAiObjectContext.cpp:62`): nothing turns them on (gap 8).
  4. Combat resurrection of the healer (good): druid Rebirth (56), death knight Raise Ally (72), a warlock Soulstone cast on
     the healer before the pull (`soulstone master` exists as an opt-in toggle, `GenericWarlockNonCombatStrategy.cpp:129-139`).
  5. Ask in chat once (existing `DungeonChat` pattern) so the owner sees why the group stops.

### After the fight
- Loot as the group rules say, refill, drink, wait for the tank's next mark (existing). A damage dealer with a pet sends
  it back to defend.

## 3. Priority order inside the pick (policy to implement)

Pure function, no game types (testable like `DefendPolicy`, `ThreatMath`): input a list of candidates (attacker kind: victim
role, is marked kill, is held mark, is crowd controlled, is interruptible caster, remaining life, distance, current target),
output the best index. Order: held mark and crowd controlled out; victim healer or damage dealer; pulled skull; pulled cross;
interruptible caster in reach; tank's target; lifetime band; distance. Every class strategy keeps its own refinement (combo
finishing, DoT spreading) on top.

## 4. Interrupt and crowd control duty in a group

### Interrupts (today)
- Most classes interrupt only the **current target** (`InterruptSpellTrigger`, target "current target",
  `GenericTriggers.cpp:469`), and so do an enemy healer (`enemy healer target`). All damage dealers share the skull, so
  all of them aim at one mob and the others cast on members unchecked.
- The "enemy caster in range" edges (value `enemy caster target`: the current target first, then every attacker within 30 yd
  that casts an interruptible spell, `PvpRotationValues.cpp`) cover PvE only where a class wrote a pack variant: arms and
  protection warrior `pummel on pack caster`, frost and unholy death knight `mind freeze on pack caster`, warlock
  `mortal coil on enemy caster` and `pet interrupt` (not map gated). Rogue `kick on enemy caster` is gated to
  battlegrounds and arenas (`RogueActions.cpp:75-79`); hunter Counter Shot, mage Counterspell, shaman Wind Shear, paladin
  Rebuke, cat Skull Bash and monk Spear Hand Strike carry the same PvP gate in their action headers (read from comments for
  the last five, inference).
- `PlayerbotAI::IsInterruptableSpellCasting` (`PlayerbotAI.cpp:3667`) tests that the target is casting and that the bot's own
  spell is an interrupt; it does not test that the target's cast can be interrupted, how much is left, or how dangerous it
  is. The PvP value has the right test (`EnemyCasterTargetValue::IsCastingInterruptible`).
- `MopDungeon::InterruptGate` (`dungeons/MopDungeonShared.h:223`) already solves the duty split for the MoP dungeons: the first
  ready interrupter in group order fires at once, the next after 500 ms if the cast is still running, `InterrupterRank`
  counts bots only. It is used by MoP dungeon triggers only.

### Proposed duty (design)
- Core: the first ready interrupter on the current target kicks every cast of 1.5 s or more that can be interrupted
  (exists, minus the cast check).
- Good: any caster in reach is interrupted, once per cast, by the first ready interrupter in group order, with the gate
  above promoted to a shared header; melee-range interrupts are taken by melee, ranged by whoever is in range.
- Expert: dangerous casts first (a heal, a fear or other crowd control, a big nuke; a data list per spell family or the
  spell's `SchoolMask` and mana cost), the longest cooldown saved for them, a second control (stun, Gouge, Paralysis,
  Hammer of Justice) when no interrupt is ready (the Scarlet Monastery branch `fix/scarlet-monastery-adds-interrupts`
  has this for one dungeon only).

### Crowd control (today)
- The `cc` strategy is attached to mage, warlock, hunter, balance and cat druid, and windwalker monk (expert tier only,
  `AiFactory.cpp:543`). Not attached: rogue (`RogueCcStrategy` exists), shadow priest, shaman, retribution paladin, death
  knight, warrior (read).
- In a dungeon map a crowd control spell fires only on the mob carrying the group's **moon** icon (`HasCcTargetTrigger`,
  `rti cc` = "moon"); in any other map (raids included, `Map::IsDungeon()` is false there) any extra attacker is a target.
  Kill-pick excludes only icon 4 (moon).
- The check "is this mob already controlled" is per spell (`FindCurrentCcTargetStrategy`: aura of that spell), so a mage, a
  warlock and a hunter in one group each cast their own crowd control on the same moon mob (inference from the code;
  to check live).

### Proposed duty (design)
- One mark, one controller (good): the first able class in a fixed order per mark takes it, others skip. Mapping to confirm
  with the owner (question 1): moon polymorph or hex or the same-role controls; square freezing trap (hunter); triangle sap
  or shackle (rogue, priest); diamond and circle unused; skull and cross are kill marks.
- Controller order by role, fitted to the spell list in section 7: mage Polymorph (humanoid, beast, critter), druid Hibernate
  or Cyclone, hunter Freezing Trap (any, ground), warlock Fear or Banish (demon, elemental), rogue Sap (before the fight),
  priest Shackle Undead, monk Paralysis, shaman Hex (75). Fear is an emergency self-defence and never a mark duty (section 8).
- A broken control is renewed only if the mob is not yet the tank's target and the tank has the threat of the rest.

## 5. Cooldown timing by fight

- Fight weight (good, missing as a shared value): **trash** (fewer than 3 attackers, none elite), **pack** (3 or more attackers),
  **elite pack** (an elite with 2 more attackers), **boss** (`IsDungeonBoss`, `isWorldBoss`). Today the same test is rewritten
  in each class: mage `MageImportantPullTrigger` (`MageTriggers.cpp:222`), warlock (`WarlockTriggers.cpp:24-52`), death knight
  (`DKTriggers.cpp:257`), paladin `RetBossDevotionTrigger` (`PaladinTriggers.cpp:77`), druid (`DruidTriggers.cpp:135-162`,
  `:552`). The generic `BoostTrigger` uses the `balance` value (group level sum against the attackers) at 50
  (`GenericTriggers.cpp:416`).
- Rule (core): long cooldowns (3 minutes and up: Bloodlust or Heroism, Power Infusion, Summon Doomguard, Army of the Dead) only
  on an elite pack or a boss; 1 to 2 minute cooldowns on a pack of 3 or more; short ones on cooldown.
- Rule (good): not before the tank has threat (the threat gate), not on a mob under 20 percent health, not while a member is
  crowd controlled.
- Rule (expert): Bloodlust or Heroism when the healer is not dead and "Sated" is gone; one caster per cooldown (do not stack
  two Power Infusions).
- Today (read): `ShamanBoostStrategy` casts Heroism and Bloodlust at the first tick when `balance` is under 50 (a level based
  guess, not a fight weight); death knight Summon Gargoyle and Unholy Frenzy fire at the first tick of any fight; rogue Killing
  Spree has no condition (class sheets).

## 6. Positioning

- **Melee** (core): behind the mob, within melee reach, off the tank's side when two mobs are held (`behind` strategy,
  `not behind target` trigger, `AiFactory.cpp:574`); step out of a frontal cone of a boss (dungeon scripts only).
- **Ranged** (good): at spell range from the target but near the group, not on top of the healer or the tank, 6 yd apart from other
  ranged when a damage over time or area effect is known on the encounter. Today: `reach spell` at 28.5 yd; the formation
  keeper `combat formation` (`CombatStrategy.cpp:49`) is not in any default list; `avoid aoe` is added only with a human
  master (`AiFactory.cpp:321-326`).
- **Caster movement** (good): finish a cast before moving unless the fight asks for it; instants while moving (class sheets
  carry the lists).
- **Behind the tank, not ahead** (core): never past the tank toward the next pack. The follow code keeps the bot near the leader.
- **Under attack** (core): walk to the tank when a mob is on the bot and the tank is within about 25 yd, rather than away from
  the mob (`FleeAction` backs off 5 yd from the current target only, `MovementActions.cpp:1720`). Warlock affliction and
  destruction carry `warlock flee close` outside PvP instances (`AffliWarlockStrategy.cpp:88`), which walks a caster away from
  a melee mob in a group (inference: may add to ticket 126).

## 7. Spells that fill each job (level, from the class sheets)

"Edge" means the bots have a trigger for it today. Defensives per class: see each sheet, section "Cooldowns and defensives".

| Class and specialisation | Interrupt | Crowd control and control | Dispel and steal | Threat tool or escape | Edge today |
|--------------------------|-----------|---------------------------|------------------|-----------------------|-----------|
| Warrior arms, fury | Pummel (24); Disrupting Shout (talent 45) | Intimidating Shout (52); Hamstring, Piercing Howl | none | none | Pummel on the target and a pack caster (arms), enemy healer; shouts partly |
| Death knight frost, unholy | Mind Freeze (57); Strangulate | Chains of Ice (snare), Death Grip | none | none | Mind Freeze on the target, enemy healer, pack caster |
| Rogue (all) | Kick (18) | Sap (12, out of combat), Gouge (22), Blind (38), Kidney Shot (40) | none | Vanish (34), Feint (44), Tricks of the Trade (78) | Kick on the target and enemy healer; Gouge and Vanish at "medium threat"; Sap strategy not attached |
| Hunter BM, MM, survival | Counter Shot (22); Silencing Shot (30, MM) | Freezing Trap (28), Scare Beast (36), Distracting Shot (52) | Tranquilizing Shot (35) | Feign Death (32), Misdirection (76), Disengage | Counter Shot on the target; traps and Scare Beast on the moon mark only |
| Mage arcane, fire, frost | Counterspell (8) | Polymorph (14), Frost Nova, Dragon's Breath (fire, 62) | Remove Curse (29), Spellsteal | Invisibility, Ice Block | Counterspell on the target and enemy healer; Polymorph on the moon mark |
| Warlock affliction, demonology, destruction | Felhunter Spell Lock (pet: Felhunter 29, Spell Lock 50), Mortal Coil (talent 30) | Fear (14), Banish (32), pet Seduction | Felhunter Devour Magic (38) | Soulshatter (66) | Pet interrupt and Mortal Coil on any caster in range; Fear and Banish on the moon mark |
| Priest shadow | Silence (52) | Shackle Undead, Void Tendrils (talent), Psychic Scream | Dispel Magic (26, hostile only) | Fade | Silence on the target and enemy healer; Fade at "medium threat"; `cc` not attached |
| Shaman elemental, enhancement | Wind Shear (16) | Hex (75), Earthbind, Thunderstorm | Purge (12), Cleanse Spirit (18, curses) | none | Wind Shear on the target and enemy healer; Purge and Cleanse Spirit; no Hex edge |
| Druid balance | Solar Beam (28) | Entangling Roots (10), Hibernate (66), Cyclone (78), Typhoon | Remove Corruption (22) | none | Solar Beam on the target; roots, Hibernate and Cyclone on the moon mark |
| Druid feral (cat) | Skull Bash (64) | same control spells | Remove Corruption | none | Skull Bash on the target; pack caster variant PvP only |
| Monk windwalker | Spear Hand Strike (32) | Paralysis (44) | Detox (20, poison and disease) | none | Three Spear Hand Strike edges; Paralysis only in the `cc` strategy of expert bots |
| Paladin retribution | Rebuke (36) | Hammer of Justice (7), Repentance (talent) | Cleanse (20, poison and disease) | none | Rebuke on the target and enemy caster; Hammer of Justice as second interrupt; no `cc` attached |

Off-heals and combat resurrection of damage dealers: retribution paladin Word of Glory and Flash of Light (`offheal`,
opt-in), feral druid Rejuvenation, Healing Touch and Rebirth (56), elemental and enhancement shaman Healing Surge (solo
heal edge on enhancement), death knight Raise Ally (72), warlock Soulstone and Healthstone (self and master). Group buffs of
damage dealers (Horn of Winter, Battle Shout, Trueshot Aura equivalents, Mark of the Wild) are class sheet matter.

## 8. The self-defence ladder (ticket 126, generalised)

Trigger: two or more mobs attack the bot (`medium threat` amount 2 already exists) or its health is under 65 percent with a
mob on it. The owner's rule for the affliction warlock, as a ladder every damage dealer follows with its own spells:

1. **Keep the damage engine running** on every attacker (DoTs on each mob; a melee rotation on the one in reach). Core.
2. **Move to the tank** when the tank is within about 25 yd and no mob is rooted on the bot. Core. Today: no such move.
3. **One control** on one mob: Fear (warlock), Frost Nova and Polymorph (mage), Hex (shaman), Paralysis (monk), Gouge
   (rogue), Hammer of Justice (paladin), roots (druid), Intimidating Shout (warrior). Good. Never Fear within about 30 yd of
   an unpulled pack (it runs into it): add that guard to the Fear edge the separate fix adds.
4. **A pet or a defensive cooldown**: Voidwalker Suffering and Sacrificial Pact (warlock), Barkskin, Evasion, Icebound
   Fortitude, Die by the Sword, Deterrence, Divine Protection, Astral Shift, Dispersion, Ice Block. Core, with the health
   lines of the sheets (45 percent first defensives, 25 percent last ones).
5. **A threat dump**: Soulshatter, Fade, Feign Death, Vanish, Invisibility, Misdirection. Good.
6. **A self-heal channel last**: Drain Life under about 35 percent (owner), Healthstone and potion at 25 percent, Death
   Strike, Word of Glory. Core.

What blocks the ladder today (all read): the threat multiplier (gap 1) zeroes steps 1 to 6 for a bot whose threat is above
80 percent of the tank's, which is exactly the bot a mob is hitting (`CastBuffSpellAction` and `CastSpellAction` are `Single`
type, crowd control, snare, protect and Shoot casts are exempt, heals are `Aoe` type and stop at 50 percent, `GenericSpellActions.h:28,141,239,324-346`); Drain Life sits above
every DoT at `ACTION_INTERRUPT` for "low health" 45 percent (`GenericWarlockStrategy.cpp:104`); `warlock flee close` moves a
warlock away from a melee mob (inference).

## 9. Thresholds and where they live

| Rule | Value | Where |
|------|-------|-------|
| Single target threat limit | 80 percent of the highest tank threat on the current target | `ThreatStrategy.cpp` `ThreatMultiplier` |
| Area threat limit | 50 percent of the highest tank threat over all attackers | same |
| Tank with no threat on a fighting mob | reads 100 (damage dealers wait) | `ThreatValues.cpp` `ThreatValue::Calculate` |
| Area actions | light 2, medium 3, high 4, within 8 yd of the current target | `GenericTriggers.h:296-310` |
| Aoe count and position radius | `AiPlayerbot.AoeRadius` = 10 | `AoeValues.cpp`, config |
| Held mark fallback | 45 s per bot | `AttackersValue.cpp:23` |
| Group ready for a pull | members alive, out of combat, 40 yd, health 60, mana 50 | `MarkTriggers.cpp` `MemberReady` |
| Drink in a dungeon | below 50 percent mana | `AiPlayerbot.DungeonDrinkMana` |
| Health lines | critical 25, low 45, medium 65, almost full 85 | `AiPlayerbot.*Health` |
| Spell range, melee range | 28.5 yd, 0.75 yd | `AiPlayerbot.SpellDistance`, `MeleeDistance` |
| Lifetime bands for the pick | 5 to 30 s first, under 5 s second, over 30 s last; plus 10 when in range (range + 5 yd) | `DpsTargetValue.cpp:101-130` |
| Enemy caster range | 30 yd | `PvpRotationValues.cpp:26` |
| Interrupt stagger | 500 ms per rank ahead | `MopDungeonShared.h:223` |
| Proposed: Drain Life | below 35 percent (owner) | the separate warlock fix |
| Proposed: kill target stickiness | 3 s | new constant in a policy header |
| Proposed: ladder trigger | 2 or more attackers on the bot, or health under 65 with a mob on it | new trigger on `medium threat` |

## 10. What the bots do today and the gap list

State at `main` b518acb, read. Sizes by the key above. "Tier" is the lowest tier that uses the rule.

| # | Gap | Size | Tier | State |
|---|-----|------|------|-------|
| 1 | **The threat gate stops the wrong actions.** `ThreatMultiplier` zeroes every `Single` action at 80 percent of the tank's threat, including interrupts, self buffs and defensives, and has no exemption for a mob that already attacks the bot (`neglect threat` is read and never written). With the tank at zero threat the value reads 100, so damage dealers also stand still on a mob the tank has not touched, e.g. an unmarked pack hitting the leader while the tank is idle. Fix: exempt interrupts, defensives, heals, crowd control; do not apply the limit when the current target's victim is the bot; keep the "tank has not touched it" wait to a few seconds and not when a member is below 60 percent. Pure policy header plus tests. (`ThreatStrategy.cpp:7-28`, `ThreatValues.cpp:32-80`, `GenericSpellActions.h:19-30`.) | S | core | read; the effect on the Deadmines run is inference (see gap 2) |
| 2 | **Threat discipline, mark rti and avoid aoe exist only with a human master.** `AiFactory.cpp:559-568` adds `threat` inside `if (facade->HasGameClientMaster())`; `avoid aoe` the same (`:321-326`). In a bot-led or finder group damage dealers have no threat limit and no wait for the tank. Fix after gap 1: add `threat` to every non-tank, non-healer bot in a group; keep `avoid aoe` for human masters or extend it. The Deadmines damage dealers fought mobs the tank had not touched, which fits a group without the strategy (the leader there was a harness bot; inference). To check live: the `[STRAT]` combat list of a damage dealer in a bot-led finder group, and after the owner joins a bot-led group (does the engine rebuild). | S | core | read |
| 3 | **The target pick ignores who is being hit and the tank's target.** Only skull is a kill mark; cross and the other icons are not read; BT-155 holds an unpulled mark but the replacement is the old lifetime or distance rule (`DpsTargetValue.cpp:101-130,194-229,271-310`), with three near-identical strategy classes. Add the order of section 2 as a pure policy (victim role, kill marks, caster in reach, tank target, stickiness), tests, and feed the three strategies. Also fixes the defend case where two damage dealers pick different mobs by distance. | M | core (member first, tank target), good (cross, caster), good (stickiness) | read |
| 4 | **No self-defence ladder for damage dealers.** Owner ticket 126 for the warlock; the same shape for every class: a shared trigger "attacked by two or more or hurt with a mob on me" and per class edges in the order of section 8. Parts exist (`medium threat` edges for four classes, class defensives at health lines, Soulshatter, Fade, Feign Death, Vanish) but none walks to the tank and none orders the steps. The Drain Life edge itself is the separate fix; do not duplicate it. | M (shared trigger and walk) + per class edges | core | read |
| 5 | **Interrupt duty: current target only, PvE pack variants for three classes, no cast check, no split.** See section 4. Promote `InterruptGate` to a shared header; add the cast test (interruptible, at least 1.5 s of cast left); add PvE pack variants (rogue, hunter, mage, shaman, paladin, cat, monk) behind the gate at the good tier; dangerous casts first at the expert tier. | M | core (cast test), good (pack variants), expert (danger order) | read; class edges beyond the three listed are inference from comments |
| 6 | **Area spells and the area count ignore crowd controlled mobs.** `AoeTrigger::IsActive` counts every attacker (`GenericTriggers.cpp:282-303`); the area casts have no check; a Blizzard, Rain of Fire, Flamestrike, Hurricane or Whirlwind next to a Polymorph or Fear target wakes it. Reuse `HasBreakableByDamageCrowdControlAura`. | S to M | good | read |
| 7 | **Crowd control is one mark and everyone casts on it.** See section 4. Mapping of marks to classes, one controller per mark, Sap and Shackle and Hex and Repentance strategies attached (rogue, priest, shaman, paladin) behind the good tier, and the raid-map auto mode (`HasCcTargetTrigger` outside dungeons) switched off. Needs owner question 1. | M | good | read |
| 8 | **Nothing reacts when the healer is dead or empty.** Off-heal strategies are opt-in by chat; no combat resurrection of the healer outside the druid and death knight edges (class sheets); no soulstone on the healer by default. Add a trigger "no living healer, or healer under 15 percent mana" that switches the off-heal lines on for retribution, feral, enhancement and elemental, limited to members under 35 percent. | M | good | read |
| 9 | **Cooldowns are timed per class and often not at all.** Shared fight weight value (section 5), class triggers read it (mage, warlock, death knight, paladin, druid already have a local copy), Heroism and Bloodlust on boss or elite pack only, death knight Gargoyle and Unholy Frenzy, rogue Killing Spree gated. | M | good | read |
| 10 | **A damage dealer already in combat opens on an unpulled mark when no member is attacked.** `DefendPolicy::HoldMark` has `!botInCombat` in the tank-pull clause: a leftover DoT, a pet or a trailing mob lets the bot open on the next skull. Fix: hold while any group member is out of combat and the tank has the mark pending, not only when the bot is. One boolean, test in `test_defend.cpp`. | S | core | read; rare |
| 11 | **Pets attack the bot's target, which can be an untanked add.** `pet attack` (`CombatStrategy.cpp:18`) follows the current target; a hunter or warlock pet pulls what the bot hits. Make the pet follow the tank's target for the first seconds. | S | good | inference |
| 12 | **Positioning: no spreading, no walk to the tank, no cone avoidance.** `combat formation` is not in a default list; `avoid aoe` needs a human master; melee only set behind; ranged stand at spell range. Owner pain: stacking on the leader. Needs a navigation query and a stack test. | L | good, expert | read |
| 13 | **Dispels are class edges without a duty split.** `cure` strategies exist (mage, shaman, balance, retribution, windwalker, hunter Tranquilizing Shot, shadow Dispel Magic, Felhunter Devour Magic); the healer dispels first and damage dealers should only when no healer lives or the debuff is lethal (fear, polymorph, root on the tank). Not read in this run beyond the class list. | S to M | good | not read |
| 14 | **Hybrid damage dealers' own group buffs and blessings overlap** (e.g. Kings, Might, Mark of the Wild, Horn of Winter): class sheet matter (`bkings` and `bmight2` for paladins). | S | core | out of scope here |

## 11. Raids (brief, raids wait: owner 2026-11-08)

- Marks: more kill marks (skull, cross, then the rest in a fixed order), the held-mark rule is off in raids
  (`AttackersValue::HoldMarkForTank` checks `isRaidGroup`) while `IsTanksPull` and BT-155 still apply; the cross and tank target
  rules above apply unchanged.
- Threat: `hasTank` is any living tank; with several tanks the limit reads the highest. Needs the main tank flag
  (`PlayerBotSpec::IsMainTank`) and a taunt swap rule from a tank rule.
- Area: raise the area thresholds (4 and 6), no area spell near a crowd controlled add, no area spell on a target with a
  break-on-damage mechanic (dungeon and raid scripts set `prioritized targets`, which the pick already honours).
- Interrupts: an interrupt rotation by name (rank in the group list is the existing gate; add a per boss list), not per class.
- Cooldowns: Heroism and Bloodlust on a boss with a plan (pull, phase), one per boss; the raid timeline is a raid sheet job.
- Dispels and off-heals: assignments per raid group; none exist.
- Positioning: spread, stack and cleave rules per encounter; `RaidInterrupts` only drops casts for three Hyjal abilities.
- The 25 member loops (`IsTanksPull`, `ThreatValue`, the pick) are per bot and per tick and untested at that size.

## 12. Tests

Pure headers with tests in `tests/` (no game types), in the pattern of `DefendPolicy.h`, `ThreatMath.h`:
`DpsPickPolicy.h` (section 3 order, stickiness, held and crowd controlled out), `ThreatGatePolicy.h` (the exemptions and the
tank-untouched wait), `InterruptDuty.h` (rank and stagger, moved from `MopDungeonShared.h`), `FightWeight.h` (trash, pack,
elite pack, boss), `LadderPolicy.h` (the order of section 8 as a function of health, attackers, ready tools). Existing
`tests/test_threat.cpp` and `tests/test_defend.cpp` stay; `bash tests/run.sh` must still pass. Game-side code is
syntax-checked only until the maintainer builds.

## 13. Live test plan (Deadmines or Shadowfang Keep, owner as leader, three bots)

1. Pull two marked packs: damage dealers wait for the tank (first 3 s no damage on a mob the tank has not touched), then open.
2. Walk into an unmarked pack as the leader: the damage dealers hit the mobs on the leader or healer, not the marked bystander.
3. A mob on a damage dealer: it walks to the tank, uses a control or a defensive, the warlock keeps DoTs up, Drain Life
   only under 35 percent (separate fix).
4. A pack with a caster: the first ready interrupter kicks, the others hold; a second caster is taken by the next interrupter.
5. A polymorph on moon with a Blizzard mage: the area spell waits.
6. Kill the healer: the tank stops pulling, the hybrid damage dealers heal members under 35 percent, one chat line.
7. Log lines to add for the run: `[DPSPICK] bot target reason`, `[LADDER] bot step`, `[INTERRUPT] bot spell caster rank`.

## 14. Owner questions

1. **Marks for crowd control and kill order.** Recommended: skull and cross are kill marks in that order; moon is the
   sheep-type control (mage, druid, shaman, warlock banish); square is the hunter's trap; triangle is sap or shackle; diamond,
   circle and star are not read (they stay free for you). Alternative: only skull and moon as today.
2. **Fear as self-defence.** Recommended: the warlock Fear is a self-defence step (one mob, not near an unpulled pack), not a
   crowd control duty. Alternative: Fear only on the moon mark as today.
3. **Off-heal switch.** Recommended: hybrid damage dealers switch on their off-heal automatically when no healer lives or the
   healer is under 15 percent mana, for members under 35 percent. Alternative: keep it opt-in by chat.
4. **Threat gate for bot-led groups.** Recommended: the same limit as in your groups (80 and 50 percent) once the
   exemptions are in. Alternative: leave bot-led groups without it.
5. **Cooldown rule.** Recommended: 3 minute cooldowns on elite packs and bosses only, including Bloodlust and Heroism.
   Alternative: keep today's "outmatched" test (a level based guess).
