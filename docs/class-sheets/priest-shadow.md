# Priest: Shadow (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Gap item 12 (expert
  bots refresh Shadow Word: Pain and Vampiric Touch with 3 seconds or less left) is done; the Mind Sear bystander check
  and Mind Flay clipping are left for later. The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Shadow Priest Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent names in the text are usable).
  - wowtbc.gg, "Shadow Priest PvE Guide - MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellShapeshift, SpellRadius; `data/db2/Item-sparse.db2` for glyph item
    ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core: `SpellInfo.cpp`,
    `SpellHandler.cpp`, `scripts/Spells/spell_priest.cpp`) for the spell scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, as noted in the skill), Wowhead (needs a browser).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talents (a default for every tier, a dungeon note), glyphs, stat order (with the haste soft cap),
the area thresholds (4, 6)** Method; **the 5-target area rule, the DoT refresh note, the full talent option lists (including
Phantasm, Spectral Guise, Power Infusion), the opener with a pre-pot** wowtbc. Neither guide has levelling advice, a mana plan,
a clipping rule, a threat rule or a word on what a Shadow priest does before Shadowform (24), Vampiric Touch (28) or Shadow
Word: Death (46), so those lines come from the 5.4.8 data and are marked "derived". The two guides agree on the single target
order up to small shifts; they differ on four points, and I pick as follows:

- **DoTs before or after the cooldowns.** Method puts Shadow Word: Pain and Vampiric Touch first (they are the foundation of
  Devouring Plague and Shadow Orb value); wowtbc puts them eighth but adds "refresh before the final tick". Pick Method's
  order for the core line and wowtbc's refresh note for the expert line (the bots' DoT triggers recast only when the aura is gone).
- **Opener pre-cast.** Method: Mind Blast (an orb at the pull, data: Mind Blast generates one Shadow Orb); wowtbc: Mind Spike.
  Pick Mind Blast: Mind Spike strips your own DoTs unless a Surge of Darkness charge is up (spell text and the core script
  `spell_pri_mind_spike`), so it only makes sense as the first spell, and it makes no orb.
- **Five targets.** Method gives no rule; wowtbc: DoTs plus Mind Sear as the filler. Pick wowtbc.
- **Tier 45 talent.** Method: Solace and Insanity "best in its row"; wowtbc lists all three without a pick. See Talents: the
  bots' cast check and the core script interact in a way I could not settle by reading (owner question 1).

## Talents

Tiers and columns are from Talent.dbc (the three priest specialisations share one talent list; some effects differ by
specialisation, in the spell text). "Bots today" is `AiPlayerbot.PremadeSpecLink.5.2 = 000002`
(`conf/playerbots.conf.dist:2777`; one digit per tier, column counted from 0): Void Tendrils, Body and Soul, From Darkness
Comes Light, Desperate Prayer, Twist of Fate, Halo.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000002) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Void Tendrils (Method default) | Psyfiend, Dominate Mind (Method: to control an add; no bot edge casts either) | 108920 / 108921 / 605 | Void Tendrils, same. The bots cast it on a creature that attacks them within 8 yd (`ShadowPriestStrategy.cpp:55-57`, `PriestTriggers.cpp:173-183`, 30 s cooldown, roots for 6 s) |
| 2 (30) | Body and Soul (Method default) | Angelic Feather (Method: dungeon option, needs placing); Phantasm (wowtbc lists it) | 64129 / 121536 / 108942 | Body and Soul, same. Passive: it speeds the target of a Power Word: Shield the bot casts, so it works by accident only |
| 3 (45) | **Guide: Solace and Insanity (Method). Bot-friendly: Mindbender** (derived, see below) | From Darkness Comes Light (wowtbc lists it); Mindbender (wowtbc lists it) | 139139 / 123040 / 109186 | **From Darkness Comes Light, differs and inert:** it gives Surge of Darkness (a free instant Mind Spike) and no bot line casts Mind Spike |
| 4 (60) | Desperate Prayer (Method, active); Angelic Bulwark if often below 30 percent | Spectral Guise (wowtbc only) | 19236 / 108945 / 112833 | Desperate Prayer, same; the bots cast it at critical health (`GenericPriestStrategy.cpp:54-55`) |
| 5 (75) | Twist of Fate (Method default) | Divine Insight (Method: add-heavy, movement); Power Infusion (wowtbc) | 109142 / 109175 / 10060 | Twist of Fate, same (passive). Power Infusion would also work: `PriestBoostStrategy` casts it on a hard fight (`GenericPriestStrategy.cpp:106-109`) |
| 6 (90) | Halo (Method default) | Divine Star (Method: dungeon, if it hits every target); Cascade | 120517 / 110744 / 121135 | **Halo, differs:** the only Halo line is the heal edge "medium group heal setting" (`GenericPriestStrategy.cpp:52-53`); the shadow strategy casts **Cascade** on cooldown (`ShadowPriestStrategy.cpp:19`) |

Notes on the picks (data, not guide):

- Void Tendrils (108920, a talent spell whose data row says level 12): 4.0 percent of base mana, 30 s cooldown, roots enemies
  near the priest (8 yd in the bots' code, from the tooltip) for 6 s; killing a tendril frees the target (spell text).
- **From Darkness, Comes Light (109186)** for Shadow means Surge of Darkness (87160): Vampiric Touch ticks have a 20 percent
  chance to make the next Mind Spike free, instant, 50 percent stronger and not strip the DoTs (limit 2 charges). Mind Spike
  (73510) is learned at 44, so the talent at 45 fits, but it needs an edge for the proc (like `SurgeOfLightProcTrigger`,
  `PriestTriggers.cpp:157`, for Discipline). Without that edge it does nothing.
- **Solace and Insanity (139139):** for Shadow, Mind Flay cast on a target that has your Devouring Plague does 33 percent more
  damage per orb consumed (spell text of 2944). In the core this is a scripted swap: `spell_pri_mind_flay_selector`
  (`spell_priest.cpp`, in the core) refuses the Mind Flay cast in its check (`SPELL_FAILED_DONT_REPORT`) and schedules a cast of
  Mind Flay (Insanity, 129197) one millisecond later. The bots run the cast check (`AI/PlayerbotAI.cpp:3871-3895`) before every
  Mind Flay, and the check itself schedules an Insanity cast; the result "refused" is read as a failed cast, so the bot may try
  the alternative node (`smite`, swapped to Mind Flay) and schedule a second Insanity, and may do so from triggers that only
  test the spell. I could not decide by reading whether this runs clean. **To check live before it is picked.**
- **Mindbender (123040)** replaces Shadowfiend (Talent.dbc override column = 34433; 60 s cooldown against 180 s; 15 s against
  12 s; the Discipline sheet reads the same). The real client path swaps the spell in `SpellHandler.cpp:1570-1612` (talent
  override); the bots' `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3730-3780`) has the specialisation swap and the aura swaps but
  **no talent override**, and `SpellInfo::OverrideSpellList` is commented out in this core (`SpellInfo.cpp:718-742`). So a
  Mindbender bot keeps casting Shadowfiend (34433, still in its book from 42): nothing breaks, the talent just does not act
  until the override is added. With the override, the mana edge at medium mana comes back every minute instead of every three.
- Twist of Fate (109142): after damaging or healing a target below 35 percent health, +15 percent damage for 10 s (effect
  data). Passive, so a dungeon bot gets it on the tail of every pack.
- Desperate Prayer (19236): instant heal of 30 percent of maximum health, 120 s cooldown (the bots gate it by the
  critical health trigger).

Proposed link (guide aligned and safe for the code that exists): **`001000`** = Void Tendrils, Body and Soul, **Mindbender**,
Desperate Prayer, Twist of Fate, **Cascade**. The tier 45 change is the safe pick for the bots (see above; Method's pick is
`002000` once the live check passes); the tier 90 change is the only one where the bots have a damage line today (Cascade
runs on cooldown from the default actions; Halo only on a group heal need; Halo is Method's raid default because it
is cast on cooldown, which the bots do not do). Owner questions 1 and 2.

## Glyphs

Types from GlyphProperties.dbc (the apply spell has effect 74; 0 major, 1 minor), item ids from `Item-sparse.db2`, effect text
from Spell.dbc. Slots unlock in pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`). The config order
is major, minor, major, minor, minor, major; a glyph goes to the first free slot of its type. "Bots today" is
`AiPlayerbot.PremadeSpecGlyph.5.2 = 42400,77101,42398,87392,43342,79515` (`conf.dist:2778`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Inner Sanctum | 42400 | both guides; 6 percent less spell damage taken while Inner Fire is up (effect data), and the bots always keep Inner Fire up | yes, slot 1 (level 25) |
| Major | Fade | 42398 | both guides; Fade also cuts all damage taken by 10 percent (effect data); the bots cast Fade (24) | yes, slot 3 (level 50) |
| Major | Vampiric Embrace | 79515 | both guides; converts 50 percent more damage to healing but the duration is 5 s shorter (effect data). Vampiric Embrace is learned at 78, so a slot at 75 does nothing for three levels and little for a bot | yes, slot 6 (level 75) |
| Major, alternative | Dispersion | 45753 | both guides (personal cooldown); 15 s less cooldown (120 to 105 s, effect data); learned at 60, and the bots cast Dispersion at low mana and at critical health | no |
| Major, alternative | Inner Fire | 42402 | wowtbc (physical damage); more armor from Inner Fire | no |
| Major | Mind Flay | 79513 | not named by either guide; Mind Flay no longer slows the victim and instead gives movement speed on damage. Of use only when the bot moves between casts (it does not) | no |
| Major | Shadow Word: Death | 79514 (85708 has the same effect) | not named by either guide; casts at any health for 25 percent damage but the priest takes the same damage (spell text). **Never for a bot** | no |
| Major | Mind Blast | 42414 | not named; a critical Mind Blast roots the target 4 s | no |
| Minor | Shadow | 77101 | cosmetic (less transparent Shadowform) | yes, slot 2 (level 25) |
| Minor | Shadowy Friends | 87392 | cosmetic (Shadowform extends to non-combat pets) | yes, slot 4 (level 50) |
| Minor | Shadow Ravens | 43342 | cosmetic (Shadow Orbs appear as ravens) | yes, slot 5 (level 75) |
| Minor | Dark Archangel | 43374 | cosmetic (a Dark Archangel form after Devouring Plague) | no |

Disagreement: Method lists Vampiric Embrace first, then Fade, Dispersion, Inner Sanctum; wowtbc lists Inner Sanctum, Fade,
Vampiric Embrace as standard, Dispersion as personal cooldown, Inner Fire for physical damage. Both say no glyph raises damage
and Method says the minors are free. The premade follows wowtbc's standard list with three cosmetic minors. Proposed
`PremadeSpecGlyph.5.2` (derived: a glyph in a slot must do something at the level the slot unlocks): **`42400,77101,42398,87392,43342,45753`**
(Dispersion in the last major slot instead of Vampiric Embrace; the minors stay, they are free). Owner question 3.

## Stat order

Method: enough Spirit and Hit (Expertise is also named) to reach 15 percent spell hit chance, then Haste (soft cap 18,215
rating at level 90), then Critical Strike (slightly above Mastery), then Mastery. wowtbc has no stat order. The caps are level
90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; Spell Power is not an item stat in 5.x, inference: the data here does not list item stats).
- **Spirit counts as Hit for a Shadow priest**: Spiritual Precision (47573, learned at 10) grants hit equal to any Spirit gained
  from items. So until the hit cap Spirit is worth as much as Hit; after it, nothing (Shadow has no Meditation, which is
  Discipline and Holy only, so Spirit gives no mana regeneration in combat).
- Then Haste (Mind Flay and both DoTs tick faster, Mind Blast comes back sooner; Shadowform's raid haste aura stacks with
  nothing), then Critical Strike (Shadowy Apparitions at 42 come from critical Shadow Word: Pain ticks).
- **Mastery last below 80**: Mastery: Shadowy Recall is learned at 80 (SpecializationSpells), so before it a Mastery rating does
  nothing.
- Expertise is on Method's line but a caster's spells ignore it (inference); skip it.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:537-547`, shared with the Balance druid): Intellect 0.3, **Spirit 0.6**,
  Spell Power 1.0, Hit 1.1, Haste 1.0, Critical 0.8, **Mastery 0.87 at every level**. Against the guide: Spirit outranks
  Intellect two to one (a cloth piece with Spirit and no Intellect would beat one with Intellect), Mastery counts below 80
  where it is dead, and the hit cap check (`:920-940`) reads the hit rating only, not Spirit.

## What the specialisation knows, by level (5.4.8 data)

A Shadow priest knows the Priest class line (SkillLineAbility 804, classMask priest) plus the Shadow rows of
SpecializationSpells (spec 258). Ids are the spells the player casts; the data holds many hidden rows of the same names.
Costs are percent of base mana; the global cooldown is 1.5 s.

| Level | Spell (id) |
|------:|------------|
| 1 | Smite (585, 2.7 percent, 1.5 s) |
| 3 | Shadow Word: Pain (589, 4.4 percent, instant, 18 s, ticks every 3 s) |
| 5 | Power Word: Shield (17, 6.1 percent, instant, 6 s cooldown, 15 s absorb, Weakened Soul 15 s) |
| 7 | Flash Heal (2061, 5.9 percent, 1.5 s) |
| 9 | Inner Fire (588, no cost, no duration: armor and 10 percent spell power) |
| 10 | **Mind Flay (15407, 1.0 percent, 3 s channel with 3 ticks, slows 50 percent; replaces Smite)**; Spiritual Precision (47573) |
| 12 | Psychic Scream (8122, 3.0 percent, 8 yd, 8 s fear) |
| 15 | tier 1 talent |
| 18 | Resurrection (2006, out of combat) |
| 21 | **Mind Blast (8092, 3.0 percent, 1.5 s, 8 s cooldown, +1 Shadow Orb); Devouring Plague (2944, spends all orbs, 6 s); Shadow Orbs (95740)** |
| 22 | Power Word: Fortitude (21562, 1.0 percent, the whole group, 60 min) |
| 24 | **Shadowform (15473, 2.6 percent; +25 percent Shadow damage, raid spell haste 5 percent)**; Fade (586, 3.5 percent, 30 s cooldown, 10 s) |
| 26 | Renew (139, 2.6 percent); Dispel Magic (528, hostile target only) |
| 28 | **Vampiric Touch (34914, 3.0 percent, 1.5 s, 15 s; 2 percent of maximum mana per tick, 5 ticks)** |
| 30 | tier 2 talent |
| 34 | Levitate (1706) |
| 42 | Shadowfiend (34433, 3 min cooldown, 12 s, returns mana); Shadowy Apparitions (78203, passive); Mind Vision |
| 44 | Mind Spike (73510, 1.0 percent, 1.5 s; strips your DoTs from the target unless Surge of Darkness is up) |
| 45 | tier 3 talent |
| 46 | **Shadow Word: Death (32379, 2.6 percent, instant, 8 s cooldown, only under 20 percent health; +1 orb; if the target lives the cooldown resets once per 9 s)** |
| 48 | Binding Heal (32546; needs the form off) |
| 52 | **Silence (15487, 0.9 percent, 45 s cooldown, 5 s; a non-player victim's cast is interrupted)** |
| 54 | Fear Ward (6346, 0.9 percent, 3 min) |
| 60 | **Dispersion (47585, 120 s cooldown, 6 s: 90 percent less damage taken, 6 percent mana per second, cannot cast)**; tier 4 talent |
| 66 | Hymn of Hope (64901, 6 min cooldown, 8 s channel) |
| 68 | Prayer of Mending (33076; allowed in Shadowform) |
| 72 | Mass Dispel (32375) |
| 74 | Psychic Horror (64044, 45 s cooldown, spends all orbs for a longer horror) |
| 75 | tier 5 talent |
| 76 | **Mind Sear (48045, 3.0 percent, 5 s channel, 1 s ticks, 10 yd around the target)** |
| 78 | Vampiric Embrace (15286, 3 min cooldown, 15 s: heals the group for 50 percent of your single target Shadow damage, split) |
| 80 | Mastery: Shadowy Recall (77486); Inner Will |
| 84 | Leap of Faith (73325) |
| 87 | Void Shift (142723, 10 min cooldown) |
| 90 | tier 6 talent |

A Shadow priest does not know Heal, Greater Heal, Prayer of Healing, Penance, Inner Focus, Spirit Shell, Holy Nova, Purify,
Circle of Healing or the Chakras (Discipline and Holy rows in SpecializationSpells; Penance and Inner Focus are
Discipline only), so every edge that names one of them is a request for an unknown spell (see "Spell names" below).

### Specialisation swaps and the form rules (the question of the day)

- **Swaps for Shadow (SpecializationSpells, spec 258):** exactly one, **Smite becomes Mind Flay** (level 10). The module applies
  it for bot casts in `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3738-3753`), so the nodes `smite` (pull, the end of the
  Mind Flay chain) cast Mind Flay from 10 on. The pull strategy (`PriestAiObjectContext.cpp:44`, "smite") therefore pulls with a
  3 s channel, which is fine. There is no swap for Mind Blast: it is a Shadow spell outright (the Discipline and Holy specs do
  not have it).
- **Not swapped by the bots: the talent override** (Mindbender over Shadowfiend), see Talents.
- **Aura swaps work** (`:3765-3780`): Shadowform carries three actionbar overrides, Halo to the Shadow Halo (120644), Cascade to
  the Shadow Cascade (127632), Divine Star to 122121. Name lookup resolves `halo` to the talent id 120517 (lowest known id) and
  the swap turns it into 120644 while Shadowform is up. So the names the strategies ask for are the right ones.
- **What Shadowform allows** (read from the spell data: Spell.dbc field 20 gives the SpellShapeshift row; `Stances` is the mask
  of forms a spell may be cast in, `StancesNot` the forms it cannot; Shadow is form 28, bit 27; the core rule is
  `SpellInfo::CheckShapeshift`, `SpellInfo.cpp:1195-1230`):
  - **Blocked in Shadowform** (`StancesNot` holds Shadow): Flash Heal, Binding Heal, Resurrection, Holy Nova, Purify, Hymn of
    Hope, Greater Heal, Heal, Prayer of Healing, Spirit Shell, the Holy Halo (but not the Shadow Halo).
  - **Allowed in Shadowform**: Power Word: Shield, Renew, Inner Fire, Fade, Dispel Magic, Levitate, Prayer of Mending, Mass
    Dispel, Desperate Prayer, Psychic Scream, Shadowfiend, Silence, Void Tendrils, Psyfiend, Dominate Mind, Shackle Undead, and all
    Shadow spells; Power Word: Fortitude, Fear Ward, Leap of Faith, Power Infusion, Vampiric Embrace, Vampiric Touch, Dispersion,
    Psychic Horror, Void Shift have no rule at all.
  - The Shadowform spell text lists only Renew, Prayer of Mending and Leap of Faith as allowed Holy spells; the data allows
    more, and the data is what the cast check reads.
- **What the bots do about it:** the action nodes put a "remove shadowform" prerequisite in front of Inner Fire, Power Word:
  Fortitude (and on party), Holy Nova, Renew (and on party), Flash Heal, Greater Heal and Heal
  (`GenericPriestStrategyActionNodeFactory.h:37-136`, `PriestNonCombatStrategyActionNodeFactory.h:34-116`). Only Flash Heal, Greater
  Heal, Heal, Holy Nova and Resurrection need it (data above); for Inner Fire, Fortitude and Renew it throws the form away for
  no reason (gap 3). The cast check passes `SPELL_FAILED_NOT_SHAPESHIFT` through (`AI/PlayerbotAI.cpp:3885`), which is why the
  prerequisite exists.

## How the bots run this specialisation (read)

- A Shadow priest gets the `shadow` combat strategy (`dps` in the priest factory is `ShadowPriestStrategy`,
  `PriestAiObjectContext.cpp:56-57,64`) plus `shadow debuff`, `shadow aoe`, `dps assist` (`Factory/AiFactory.cpp:331-335, 350`)
  and the shared `boost`, `racials`, `potions`, `chat` (`:316`). `cure` is added, but its priest edges return early for Shadow
  (`GenericPriestStrategy.cpp:92-93`), so no dispel or disease cure at all. `cc` (Shackle Undead) is not added. Out of
  combat: `nc` (`PriestNonCombatStrategy`), `buff` (Fortitude on the party) and the shared food and follow strategies.
- A priest with no specialisation (levels 1 to 9) runs the holy heal set with `healer dps` (BT-038), so Smite is the attack
  until 10; the Shadow set starts when the specialisation does.
- Rank of an action (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90; the
  engine takes the highest action that is useful and possible; an action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65; mana: low 15, medium 40, high 65. The Shadow priest's actions in rank order:
  Silence 41 and 40 (interrupt), Fade 55, Desperate Prayer and Dispersion 25 (critical health and low mana), Vampiric Touch 23,
  Void Tendrils 23, Devouring Plague 22 (3 orbs), Shadowfiend 22 (medium mana), Mind Sear 24 (3 attackers), Power Word: Shield 21
  (being attacked) and 20 (60 percent health, low health), Shadow Word: Pain 21, Shadowform 20, Vampiric Embrace 16 and 20, the DoTs on
  other attackers 15 and 14, then the default list: Cascade 5.4, Mind Blast 5.3, Mind Flay 5.2, Shadow Word: Death 5.1, Shoot 5.
- **The bot cannot cancel its own channel except Mind Sear** (`CanCastSpell` returns "channeling" for any new cast,
  `AI/PlayerbotAI.cpp:3840`; `cancel channel` is wired only to the Mind Sear check). So a Mind Flay always runs its three
  ticks, and a DoT that ends inside the channel is recast up to 3 s late.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/priest/` at commit `784434b`. `SP` is `ShadowPriestStrategy.cpp`, `GP` is `GenericPriestStrategy.cpp`, `NC` is
`PriestNonCombatStrategy.cpp`.

### Levels 15 to 29

Known: Mind Flay (10), Shadow Word: Pain, Power Word: Shield, Flash Heal, Inner Fire, Psychic Scream, Resurrection (18), Mind
Blast, Devouring Plague and Shadow Orbs (21), Fortitude (22), Shadowform and Fade (24), Renew (26), the tier 1 talent. Vampiric
Touch arrives at 28. A pull fight is short at these levels; the cheap strong button is Mind Flay at 1.0 percent, and the
shield at 6.1 percent is the one expensive defensive. From 10 to 20 the whole kit is Mind Flay, Shadow Word: Pain, Shield and
Flash Heal.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, Inner Fire missing | Inner Fire (588, 9) | yes: `NC:28-29` (10); **its node removes Shadowform first** (`GenericPriestStrategyActionNodeFactory.h:37-43`), which the data does not require (gap 3) |
| 2 | core | out of combat, a member lacks it, from 22 | Power Word: Fortitude (21562, 22) | yes: `NC:83-85` (11, the `buff` strategy); same needless "remove shadowform" in its nodes (`:51-64`) |
| 3 | core | out of combat, from 24 | **Shadowform up before the pull** (derived: otherwise the first cast of every fight is a 2.6 percent global cooldown spent in combat; both guides list it as upkeep) | **missing out of combat:** the only edge is the combat trigger (`SP:32-33`, 20; `PriestTriggers.cpp:125`, no known-spell test); and any heal, Renew or Fortitude in between removes it again |
| 4 | core | in combat, no Shadow Word: Pain of mine on the target | Shadow Word: Pain (589, 3) | yes: 21 (`SP:99-100`); on other attackers 15 (`:73-75`); recast only when the aura is gone |
| 5 | core | from 28, no Vampiric Touch of mine | Vampiric Touch (34914, 28): the mana engine (10 percent of maximum mana per cast) | yes: 23 (`SP:91-92`); on other attackers 14 (`:76-78`) |
| 6 | core | 3 orbs, from 21 | Devouring Plague (2944, 21) | yes: 22 (`SP:97-98`, trigger `PriestTriggers.cpp:225`); it waits while the bot's own plague is on the target (debuff action rule). From 21 to 45 orbs come only from Mind Blast (8 s cooldown), so 3 orbs take about 24 s: it fires about once per long fight (derived) |
| 7 | core | cooldown ready, from 21 | Mind Blast (8092, 21) | yes: default 5.3 (`SP:20`); nothing outranks it but the triggers above |
| 8 | core | filler | Mind Flay (15407, 10) | yes: default 5.2 (`SP:21`); the chain `mind blast` > `mind flay` > `smite` > `shoot` (`ShadowPriestStrategyActionNodeFactory.h:26-48`) covers the levels before 21 |
| 9 | good | target under 20 percent, from 46 | Shadow Word: Death (32379, 46) | see line 20 (arrives later in the band) |
| 10 | good | being attacked | Power Word: Shield on self (17, 5), skipped while Weakened Soul is on | yes: 21 (`GP:38-39`), 20 at 60 percent health (`SP:59-60`) and at low health (`GP:62-63`); the server refuses it on a Weakened Soul carrier and the next line runs the same tick (`Engine.cpp:241-246`, per the Discipline sheet) |
| 11 | good | two or more attackers on the bot, from 24 | Fade (586, 24), group only, never against players | partly: "medium threat" = 2 attackers on the bot (`GP:33`, 55; `PriestActions.cpp:11-23`). Derived: threat above 70 percent of the tank's is the better key, because the threat brake stops every damage action at 80 (`ThreatStrategy.cpp:20-30`) (gap 8) |
| 12 | good | health under 30 percent, from 26 | Renew on self (139, 26) | yes: 20 (`SP:63-64`) |
| 13 | good | solo, health under 25 percent, below 24 | Flash Heal on self (2061, 7) (derived: no Shadowform before 24, no Renew before 26, and the shield is on a 15 s Weakened Soul lock) | **missing:** no self heal edge in combat; the out-of-combat "party member" edges heal the bot itself only between fights (`NC:48-68`) |
| 14 | expert | Mind Flay running, Mind Blast ready | stop the channel after the second tick and cast Mind Blast (derived: the first two ticks are two thirds of the damage, and Mind Blast is an orb) | missing: no channel cancel except Mind Sear |
| 15 | expert | a DoT has 3 s or less left | refresh it (wowtbc: before the final tick; Method: before it ends) | missing: the DoT triggers fire when the aura is gone (`PriestTriggers.h:23,25`; `DebuffTrigger` `beforeDuration` 0, `GenericTriggers.h:398-405`) |

Before the spells are learned (derived): at 10 to 20 the order is Shadow Word: Pain, Mind Flay (the bot has no Mind Blast, no
Devouring Plague, no Shadowform); at 21 Mind Blast joins; at 24 Shadowform. The chain falls through in that order already
(`smite` is the Mind Flay swap, `mind blast` falls to `mind flay`).

### Levels 30 to 44

As above, plus the tier 2 talent (30), Levitate (34), Shadowfiend (42), Mind Spike (44).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 16 | core | mana under 40 percent, a fight with a boss, an elite or a big pack, from 42 | Shadowfiend (34433, 42) (derived: both guides say on cooldown; for a dungeon bot the fights that matter are boss and elite fights, and trash packs of 20 to 40 s do not repay a 3 min cooldown) | partly: "medium mana" (40) at 22 (`GP:65-69`), target "current target" (`PriestActions.h:255-260`); it fires at the first tick below 40 percent, on any pack. The "boost" edge in `PriestBoostStrategy` (`GP:110`) names a trigger that exists in no trigger factory (`PriestTriggers.h:252-258`), so it never fires |
| 17 | good | from 28, many mobs | the DoTs on every attacker before any area spell (both guides: DoTs on everything, then the area button) | yes: 15 and 14 (`SP:73-78`); but Mind Sear at 24 outranks them from 3 attackers (line 25) |
| 18 | good | 15 to 45 mana | Vampiric Touch keeps the mana: 5 ticks of 2 percent | yes (line 5); no other mana tool exists until Shadowfiend (42) and Dispersion (60), so drink at 50 percent (`DungeonDrinkMana`, `conf.dist:2272`) |
| 19 | core | no good use | **do not cast Mind Spike** (73510, 44; derived: it removes the bot's own Shadow Word: Pain and Vampiric Touch from the target unless a Surge of Darkness charge is up, and there is no orb in it) | none cast it (correct) |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Shadow Word: Death (46), Binding Heal (48), Silence (52), Fear Ward (54).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | target under 20 percent health | Shadow Word: Death (32379, 46): above Mind Flay and below Mind Blast (both guides; it is the second orb source) | **wrong order:** default 5.1 (`SP:22`) sits **below** Mind Flay (5.2), which can always be cast, so Shadow Word: Death runs only when the bot moves (the code comment says "cast during movement"). The priority list wants it above the filler |
| 21 | good | a casting enemy | Silence (15487, 52); 45 s cooldown, 5 s, interrupts a non-player's cast | yes: 41 (`SP:42-43`, `INTERRUPT_TRIGGER` `PriestTriggers.h:140`), enemy healer 40 (`:44-46`); the same line is the first interrupt in PvE; the pvp "any casting enemy" edge needs a battleground (`PriestActions.cpp:155-158`) |
| 22 | good | a fear caster ahead, on the tank | Fear Ward on the main tank (6346, 54) (derived: a feared tank breaks the pull; cost 0.9 percent, one cast per 3 min) | yes: 23 (`GP:35-36`, known-spell and cooldown test `PriestTriggers.cpp:47-52`) |
| 23 | good | health under 65 percent in a group | nothing special: the healer covers; Power Word: Shield and Renew on self only | yes (lines 10 and 12) |
| 24 | good | 3 or more attackers, from 46 | Shadow Word: Death on the one under 20 percent, then DoTs on all | see line 20 |
| 25 | good | 3 or 4 attackers within 8 yd | DoTs on all, then the single target list (Method: more than 4 targets for Mind Sear; wowtbc: 5 with Mind Sear as filler, 6 or more spam) | **wrong threshold and order from 76:** "medium aoe" is 3 attackers within 8 yd and fires Mind Sear at 24 (`SP:83-84`, `GenericTriggers.h:300-304`), above the DoT edges and the orb spenders, so a pack of three is Sear after Sear and the DoTs and Devouring Plague wait. Mind Sear is level 76, so at 45 to 59 the name is unknown |

### Levels 60 to 74

As above, plus Dispersion (60), the tier 4 talent, Hymn of Hope (66), Prayer of Mending (68), Mass Dispel (72), Psychic Horror (74).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | core | mana under 15 percent, or health under 25 percent | Dispersion (47585, 60): 36 percent mana in 6 s and 90 percent less damage taken | yes: both edges at 25 (`SP:34-37`), alternative the mana potion (`ShadowPriestStrategyActionNodeFactory.h:50-56`). Casting stops for 6 s; the Glyph of Dispersion shortens the cooldown to 105 s |
| 27 | good | health under 25 percent | Desperate Prayer (19236, talent 60), instant 30 percent heal | yes: 25 (`GP:54-55`) |
| 28 | good | low mana, the group also low | Hymn of Hope (64901, 66) | partly: 20 (`GP:71-72`), after Dispersion; a channel the fight waits out (to check) |
| 29 | expert | spare orbs | Psychic Horror (64044, 74), a longer horror per orb | not used (it competes with Devouring Plague for the orbs); correct for PvE |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Twist of Fate (passive) | works |
| 76 | Mind Sear (48045): more than 4 targets after the DoTs (Method), 6 or more spam | edge exists at 3 attackers, see line 25; the channel is cancelled when fewer than 2 attackers remain (`PriestTriggers.cpp:205-222`); in a group the action is an area action, braked while any attacker's threat for the bot is 50 percent of the tank's or more (`PriestActions.h:284-289`, `ThreatStrategy.cpp:20-24`), and every damage spell is braked at 80 (`:27-29`). It hits everything within 10 yd of the target (`SpellRadius` 13), so a bot can pull the next pack or break a crowd control: the trigger counts attackers, not bystanders (to check live) |
| 78 | Vampiric Embrace on a hard fight (wowtbc defensive 1) | yes: 16 at every pull (`SP:40-41`), and 20 at 40 percent health (`:61-62`); a fresh pull with a full group wastes the 3 min cooldown (derived: wait until the group is hurt) |
| 80 | Mastery: Shadowy Recall; stat order of Method | passive; stat weights (see Stat order) |
| 84 | Leap of Faith | no edge (fine) |
| 87 | Void Shift: swap health with a party member (Method: no use given) | no edge |
| 90 | Method single target: Inner Fire, Shadowform, Shadow Word: Pain and Vampiric Touch, Shadowfiend, Devouring Plague at 3 orbs, Mind Blast, Shadow Word: Death, Mind Flay in the plague, Halo, Mind Flay | all there except the order of Shadow Word: Death, Shadowfiend on cooldown and Halo; Cascade on cooldown instead of Halo |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Shadowfiend / Mindbender | 42 / talent 45 | on cooldown | a boss, an elite or a hard pack, or mana under 40 percent | at medium mana only (line 16); Mindbender never (no talent override) |
| Power Infusion | talent 75 | no use given | a hard fight | `boost`, the "balance" test (`PriestTriggers.h:137`; `GenericTriggers.cpp:416-424`); only if the talent is picked |
| Cascade / Halo / Divine Star | talent 90 | Halo or Divine Star on cooldown | on cooldown for a boss fight | Cascade 5.4 in the default list; Halo only as a heal |
| Dispersion | 60 | glyph, defensive | low mana or health under 25 percent | yes |
| Desperate Prayer | talent 60 | Method defensive | health under 25 percent | yes |
| Vampiric Embrace | 78 | wowtbc defensive | the group is hurt | every pull (above) |
| Fade | 24 | glyph (Method, wowtbc) | threat above 70 percent of the tank's | at 2 attackers on the bot |
| Power Word: Shield | 5 | not in a guide | when attacked, when under 60 percent | yes |
| Void Tendrils | talent 15 | Method default | a melee mob on the bot | yes (`SP:53-57`) |
| Psychic Scream | 12 | not named | **never in a dungeon** (derived: it scatters the pack and breaks crowd control around a tank); pvp only | pvp only (`PriestActions.cpp:124-141`; `GP:49-50`), correct |
| Silence | 52 | not named | every interruptible cast of the pull target | yes |

## Healer triage and mana

Triage: not applicable (Shadow does not heal in combat; in Shadowform it cannot cast the direct heals). Healing that exists:
out of combat the non-combat engine heals hurt group members, or the bot itself when solo (the "party member to heal" value
returns the bot with no group, `PartyMemberToHealValue.cpp:35-38`): Flash Heal under 25 percent, Renew at low and medium
health and as a top-up under 85 percent, with Penance and Greater Heal in the list as names that never resolve (`NC:48-68`).
Derived: Renew is the right cheap spell (2.6 percent, allowed in Shadowform); Flash Heal costs 5.9 percent and the form.

Mana: Vampiric Touch returns 10 percent of the maximum mana per cast; Dispersion 36 percent; Shadowfiend about every 3 min and
Mindbender every minute; Hymn of Hope from 66. Shadow has no Meditation, so Spirit adds nothing in combat. The bots: Dispersion at
15 percent (25), Shadowfiend at 40 percent (22), Hymn of Hope at 15 percent (20), the mana potion as the Dispersion alternative,
and the generic drink rule (`AiPlayerbot.DungeonDrinkMana = 50`, `conf.dist:2272`). Mana costs of the rotation: Mind Flay 1.0,
Mind Blast 3.0, Vampiric Touch 3.0, Shadow Word: Pain 4.4, Shadow Word: Death 2.6, Mind Sear 3.0 percent; Devouring Plague
costs only orbs.

## Spell names the Shadow strategies ask for that a Shadow priest does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp:121-149`) and caches the answer 20 s per name and bot (`:24`). From the code (inference, not
checked against the log), the names a level 25 Shadow priest asks for and does not know, by source; the "learned at" column is
for Shadow:

| Name | Learned at (Shadow) | Asked from |
|------|---------------------|------------|
| `vampiric touch` | 28 | `SP:92` (debuff, every tick while the aura is missing; the trigger has no known-spell test); `vampiric touch on attacker` `SP:76-78` |
| `renew` | 26 | `SP:63-64` at 30 percent health, `NC:57,63,68` (`renew on party`) |
| `shadow word: death` | 46 | default action every tick, `SP:22` |
| `cascade` | talent 90, and only the Cascade pick | default action every tick, `SP:19`; so also at 90 with Halo or Divine Star |
| `shadowfiend` | 42 | "medium mana", `GP:68` |
| `inner focus` | never (Discipline only) | "medium mana", `GP:69` |
| `vampiric embrace` | 78 | `SP:41` (trigger active every tick of every fight without the aura) and `SP:62` |
| `dispersion` | 60 | `SP:34-37` (low mana, critical health) |
| `silence`, `silence on enemy healer` | 52 | `SP:43,46`, only when an enemy casts |
| `hymn of hope` | 66 | `GP:72` (low mana) |
| `mind sear` | 76 | `SP:84` ("medium aoe" from three attackers) |
| `desperate prayer` | talent 60 | `GP:54` (critical health) |
| `halo` | talent 90, and only the Halo pick | `GP:53` ("medium group heal setting") |
| `power infusion` | talent 75, and only the Power Infusion pick | `GP:109` (the boost trigger) |
| `shadowform` | 24 | `SP:33` (trigger active every tick without the aura, no known-spell test) |
| `mind blast` | 21 | default action, below 21 only |
| `chakra: sanctuary` | never (Holy only) | `NC:33-34` (trigger calls the spell id every evaluation) |
| `penance on party`, `greater heal on party`, `circle of healing on party` | never (Discipline or Holy) | `NC:52-53,58-59,64,70-71` |
| `fade` | 24 | `GP:33` (below 24 only) |
| `fear ward` | 54 | gated by a spell id test, no spam (`PriestTriggers.cpp:47-52`) |

Not on the list: `shoot` is a wand spell (Wands skill line 228, priest included): it resolves, and the attack action refuses
without a wand in the ranged slot; whether the bots ever carry a wand is to check. Fix direction: give the buff and debuff
triggers a known-spell test (the pattern of `FearWardOnMainTankTrigger`), gate the default actions by level, delete the Holy
and Discipline names from the Shadow paths (Chakra, Penance, Greater Heal, Circle of Healing, Inner Focus), and the log
count should fall to near zero.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Mind Blast and Devouring Plague arrive at 21, Shadowform at 24, Vampiric Touch at 28, Shadowfiend
  at 42, Mind Spike at 44, Shadow Word: Death at 46, Silence at 52, Dispersion at 60, Psychic Horror at 74, Mind Sear at 76,
  Vampiric Embrace at 78, Void Shift at 87; below those levels the lists above start without them.
- Mind Flay is the Shadow replacement of Smite from level 10: a Shadow priest has no real Smite, and a Smite request is a
  Mind Flay (spec swap).
- Shadow Word: Death is castable only under 20 percent health (spell text); the cooldown reset when the target lives is limited
  to once per 9 s. Method's "Shadow Word: Death on cooldown" means "when the target is in the execute range".
- wowtbc's Mind Spike pre-cast works but strips the DoTs once any are on the target; use Mind Blast as the first cast.
- Method's Halo as an opener pre-cast needs the Halo talent at 90 only.
- Mind Sear is 5 s, 1 s ticks, 10 yd; the bots' trigger radius is 8 yd around the current target.
- Both guides assume Inner Fire up in Shadowform; the data allows it, but the bots' node removes the form (gap 3).
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.5.2` (`conf/playerbots.conf.dist:2777`) from `000002` to **`001000`** (Mindbender
   at 45, Cascade at 90). Reason: From Darkness Comes Light is inert without a Mind Spike edge and Halo is cast only as a group
   heal; Cascade has a default-action line. Method's own pick for 45 (Solace and Insanity, `002000`) waits for the live check
   in Talents. The comment at `:2774-2775` needs the same change.
2. Glyph order `PremadeSpecGlyph.5.2` (`:2778`) to `42400,77101,42398,87392,43342,45753` (Dispersion instead of Vampiric
   Embrace in the 75 slot; Vampiric Embrace is learned at 78 and the bots cast it at every pull).
3. Stat weights for Shadow (`Mgr/Item/StatsWeightCalculator.cpp:537-547`, shared with the Balance druid): Intellect 1.5 as the
   mage and warlock branches have it, Spirit 1.1 (equal to Hit until the hit cap, because Spiritual Precision turns Spirit into
   hit), Mastery 0 below level 80 (as the warlock branch, `:526-535`); keep Hit 1.1, Haste 1.0, Critical 0.8. The hit cap check
   (`:920-940`) should add Spirit.

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Unknown-name spam** (the level 25 log, list above): known-spell tests on `ShadowformTrigger` (`PriestTriggers.cpp:125`),
   `VampiricEmbraceTrigger`/`vampiric touch`/`shadow word: pain` debuff triggers (`PriestTriggers.h:23-27`), level gates on the
   default list (`SP:19-23`: `cascade`, `shadow word: death`), the medium-mana edge (`GP:65-69`: delete `inner focus` for Shadow),
   Chakra, Penance, Greater Heal and Circle of Healing in the non-combat set (`NC:33-34, 48-71`) for any non-Holy,
   non-Discipline priest. Verify with the debug log: the `resolved=0` lines per bot fall to near zero.
5. **Shadowform upkeep**: cast it out of combat as well (a non-combat edge on the same trigger, from 24, `NC`), and take the
   needless `remove shadowform` prerequisite off Inner Fire, Power Word: Fortitude (and on party), Renew (and on party)
   (`GenericPriestStrategyActionNodeFactory.h:37-94`, `PriestNonCombatStrategyActionNodeFactory.h:55-67`); keep it on Flash Heal, Greater Heal, Heal, Holy Nova, Resurrection (data above).
   Then check live that Inner Fire and Renew really cast in the form.
6. **Shadow Word: Death order** (`SP:19-23`): between Mind Blast and Mind Flay (default 5.25), with a cast-when-under-20 rule
   it already has in the spell. Also it is the second orb source from 46.
7. **Mind Sear**: raise the threshold to 5 attackers within 10 yd of the target (Method: more than 4; wowtbc: 5 filler,
   6 spam), put it below the DoT edges so each target gets its two DoTs first, and add a "no bystander inside 10 yd" test
   (`SP:83-84`, `GenericTriggers.h:300-304`). Level 76 only: low weight for the band, high for the 61 to 90 notes.
8. **Fade by threat** (`GP:33`): add a trigger on the threat value of the current target at 70 (the brake stops damage at 80,
   `ThreatStrategy.cpp:20-30`); keep the 2-attackers edge. Fade is 24.
9. **Shadowfiend** on a boss, an elite or a hard pack at the pull, and at medium mana as today (`GP:65-69`); needs
   the Mindbender talent override if the premade changes (gap 10).
10. **Talent override in `BotOverrideSpell`** (`AI/PlayerbotAI.cpp:3730`): add the third branch of `SpellHandler.cpp:1570-1612`
    (Talent.OverrideSpellID) so Mindbender replaces Shadowfiend. Affects Discipline and Holy as well.
11. **Vampiric Embrace** only when the group is hurt (two members under 80 percent) or the bot is under 40 percent; not at the
    pull (`SP:40-41`). Level 78: low weight.
12. **DoT refresh window** (expert): pass a refresh time of one channel (about 3 s) to the Shadow Word: Pain and Vampiric Touch
    triggers (`PriestTriggers.h:23,25`, `DebuffTrigger` argument `beforeDuration`); and cancel Mind Flay after tick 2 when
    Mind Blast is ready (the cancel action exists for Mind Sear).
13. **Solo leveling self heal** (derived): Flash Heal on self under 25 percent below level 24, Renew from 26 (exists at 30 percent,
    `SP:63-64`; consider 45 percent), and the out-of-combat top-up list limited to Renew (`NC:48-68`).
14. **Surge of Darkness line** only if From Darkness Comes Light stays the pick: a trigger on aura 87160 and a Mind Spike
    action (like `SurgeOfLightProcTrigger`, `PriestTriggers.cpp:157`).
15. Skill tiers: the Shadow strategies have no tier gating (`BotSkill` is used in one priest trigger only,
    `PriestTriggers.cpp:90`). Lines marked good and expert above (Fade, Silence, Fear Ward, Renew on self, the DoT refresh
    window, the channel cancel, Shadow Word: Death order beyond the default) are the ones a casual bot should drop.
16. To check live: that Inner Fire, Renew, Fortitude and Power Word: Shield cast in Shadowform without removing it; Shadowform
    cast out of combat; the Insanity interplay (Solace and Insanity); the Smite request really casts Mind Flay at 10 to 20 (pull
    and the end of the chain); `shoot` with no wand; Mind Sear against bystanders; Hymn of Hope at 15 percent; the Cascade
    swap in Shadowform (`halo`/`cascade` resolve to the Shadow ids).

Owner questions:

1. Tier 45: Mindbender (safe, needs gap 10 to act; premade `001000`), or Solace and Insanity (Method's pick, `002000`; a
   scripted swap that the bots' cast check may fight, to check live first), or From Darkness Comes Light (current; needs
   gap 14).
2. Tier 90: Cascade (a damage line exists, 10 percent mana, the bot casts it on cooldown) or Halo (Method's raid default, cast
   by the bot only on a group heal need).
3. Glyphs: Dispersion in the 75 slot (usable) or Vampiric Embrace (both guides, usable from 78 only, and the bot opens fights
   with it).
4. Shadowfiend on boss, elite and hard-pack fights only, or on every pack at medium mana as today.
5. Should a Shadow priest heal members out of combat at all (Renew top-ups burn the mana it needs for the next pull), or only
   itself and only below 45 percent.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 4 players of this specialisation (rating 2463 to 2609). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 4 players in all (noted, not changed)
- **BT-324 result:** open (4 players)
- **Set now:** `PremadeSpecLinkPvp.5.2 = 110121`, `PremadeSpecGlyphPvp.5.2 = 42398,77101,42402,87392,43342,42404`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Psyfiend 100% / - / 4 of 4 | Psyfiend | Psyfiend | matches |
| 2 (30) | Angelic Feather 100% / - / 4 of 4 | Angelic Feather | Angelic Feather | matches |
| 3 (45) | From Darkness, Comes Light 100% / - / 4 of 4 | From Darkness, Comes Light | From Darkness, Comes Light | matches |
| 4 (60) | Spectral Guise 100% / - / 4 of 4 | Spectral Guise | Spectral Guise | matches |
| 5 (75) | Divine Insight 100% / - / 4 of 4 | Divine Insight | Divine Insight | matches |
| 6 (90) | Divine Star 75% / - / 3 of 4, Cascade 25% / - / 1 of 4 | Divine Star | Divine Star | in the ladder, split |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Inner Sanctum (42400) | 75% | - | 3 of 4 | no | no |
| major | Inner Fire (42402) | 75% | - | 3 of 4 | yes | yes |
| major | Vampiric Embrace (79515) | 50% | - | 2 of 4 | no | no |
| major | Fade (42398) | 25% | - | 1 of 4 | yes | yes |
| major | Mass Dispel (42404) | 25% | - | 1 of 4 | yes | yes |
| major | Prayer of Mending (42417) | 25% | - | 1 of 4 | no | no |
| major | Dispersion (45753) | 25% | - | 1 of 4 | no | no |
| minor | Shadow (77101) | 100% | - | 4 of 4 | yes | yes |
| minor | Dark Archangel (43374) | 75% | - | 3 of 4 | no | no |
| minor | Shadow Ravens (43342) | 25% | - | 1 of 4 | yes | yes |
| minor | Shackle Undead (43373) | 25% | - | 1 of 4 | no | no |
| minor | the Sha (104120) | 25% | - | 1 of 4 | no | no |
| minor | Shadowy Friends (87392) | 0% | - | 0 of 4 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** four players in 3v3, none in 2v2: all six tiers match the set. Glyphs: Inner Fire 75 and Shadow 100 are held; Inner Sanctum 75 and Dark Archangel 75 are not; Fade, Shadow Ravens and Mass Dispel (25 each) are.

## PvP class package: bots today (BT-688 to BT-690, 2026-11-19)

Status of the digest's section 12 rules after the Shadow package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 Mind Blast, Shadow Word: Death | as before |
| 2 Devouring Plague at 3 orbs | as before (the orb gate); the control pairing is not built |
| 3 DoTs on two targets, procs | as before (no proc handling) |
| 4 Dispersion at 40 percent | the shared defensive layer (catalog tempo row) |
| 5 Psychic Horror then Scream, Silence in a window | Psychic Horror and Silence are rows of the control kit (BT-629); Scream is not |
| 6 Psyfiend by the healer | not built (the talent policy places it under the attackers) |
| 7 Mass Dispel | done for a magic immunity, with the glyph (BT-690); not for a crowd controlled healer |
| 8 Leap of Faith | done (BT-689) |
| 9 Prayer of Mending and shield on self | the generic `being attacked` shield; Prayer of Mending as before |
| 10 Vampiric Embrace with the burst | done (BT-688) |
| 11 Shadowfiend early | as before |
| 12 kite around a pillar | as before (arena cover) |
| 13 juke a kick | not built |

Set: unchanged (4 ladder players, BT-451). The Glyph of Mass Dispel now has a rule.
