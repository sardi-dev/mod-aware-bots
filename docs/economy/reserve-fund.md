# The reserve fund of the bots' auction house

Design decision 0033 point 6 and the owner's answer of 2026-11-22: before the auction house may spend, a bot keeps gold for its
repairs, its next trainer spells, its next riding skill and its mount. This document is the model behind
`src/Utils/ReservePolicy.h`; the numbers come from the 5.4.8 data, the code is pure (no game core) and tested.

## 1. The rule

```
reserve = repair reserve                       BotSalePolicy::PurchaseReserve(level), unchanged
        + min( goals , goal share x gold above the repair reserve )
        + recipe share x gold above the repair reserve        only a bot with a primary crafting profession, at most 700 gold

goals   = class trainer spells in the next few levels      only while the bot pays for them (section 3)
        + profession ranks in the next few levels          per profession the bot practises
        + the next riding step                             one step, when its level is near
        + a mount                                          the cheapest vendor mount, when it has none
```

- "The next few levels" is `AiPlayerbot.Auction.ReserveLookaheadLevels`, default 5 (range 0 to 20). A step or spell
  counts from `level + 5` on; a riding step already due (its level is reached) counts at once.
- The goal share is `AiPlayerbot.Auction.ReserveGoalSharePercent`, default 75 (owner answer 2026-11-22; the first
  recommendation was 50). It is the one thing in this model that is not from the data; section 5 shows why it exists and
  what 100 does. The impact table in section 5 was computed with the cap of 50 percent; at 75 the median bot keeps more
  back and its purchase budget is about half of the 50 percent column (rerun `reserve_impact.py` with `SHARE_PERCENT = 75`
  for exact figures).
- The recipe share is `AiPlayerbot.Auction.ReserveRecipeSharePercent`, default 10 (owner answer 2026-11-22: "a share for
  crafters"). A bot with at least one primary crafting profession (alchemy, blacksmithing, enchanting, engineering,
  inscription, jewelcrafting, leatherworking, tailoring; `ReservePolicy::IsCraftingSkill`, not herbalism, mining or
  skinning) keeps this share of the gold above the repair reserve for recipes. It sits beside the goal share, never
  adds to more than 100 percent with it (it is cut to what the goal share leaves, so the literal goal share of 100
  leaves none) and never holds more than 700 gold, a little above what all green recipes of one profession cost at skill
  425 (section 3). Ten percent is about the cost of the green recipes the median bot can afford: 4 to 8 gold at levels 40
  and 60. The reserved amount (`Reserve::heldForRecipes`, `RecipeBudget`) is exposed for a recipe purchase: the auction
  wallet leaves it alone, a recipe purchase may spend it but never gold below the repair reserve. No recipe-buying
  consumer exists yet (`BotTraining::LearnAffordable` spends "free money for spells", not this reserve); the vendor and
  later the auction house will use `RecipeBudget`.
- Purchases (bids and buyouts) keep the whole reserve and may spend a quarter of the gold above it per visit
  (`PricePolicy::MakeWallet`, unchanged). A deposit is held back by the repair reserve only: a bot saving for a
  5,000 gold riding skill must still be able to list the goods that pay for it. Deposits and purchases draw on the same
  quarter of the visit's gold (`ReservePolicy::VisitMoney`).

## 2. Where each number comes from

All queries are read-only SELECTs through a read-only database wrapper script on 2026-11-22; spell names come from
`data/dbc/Spell.dbc`. `tools/economy/export_reserve_table.py` repeats every query and writes `src/Utils/ReserveTable.h`
(about 10 KB; rerun it and review the diff after any change to the trainer data).

| Number | Source |
|--------|--------|
| Class spell cost by level | `world.npc_trainer`: a class trainer row has `spell < 0`, a reference to a trainer template (entry >= 200000). Each class uses one template, the one most of its trainers reference: `-200001` .. `-200011` for classes 1 to 11 (82 mage trainers use 200008, 7 to 13 use the portal templates 200200 to 200202, which are left out). Cost is `spellcost`, the level is `reqlevel`. Per class the header holds the cumulative cost by level (91 numbers). Totals over levels 3 to 85: 97 gold (paladin) to 277 gold (death knight, level 55 and up), most classes 112 to 161 gold. |
| Riding steps | `world.npc_trainer` templates 200300 and 200301 (the 22 riding trainer NPCs, `creature_template.trainer_type = 1`, use them): Apprentice Riding 33389 `spellcost` 40,000 (4 gold) at level 20; Journeyman 33392 500,000 (50 gold) at 40; Expert 34092 2,500,000 (250 gold) at 60; Flight Master's License 90269 2,500,000 (250 gold) at 60; Artisan 34093 50,000,000 (5,000 gold) at 70; Master 90266 50,000,000 (5,000 gold) at 80. Cold Weather Flying 54198 (5,000,000, 500 gold, level 68) is not reserved: it only opens Northrend flight. The skill each step gives (75, 150, 225, 300, 375) is what `BotFactory::InitMountSpells` and `BotMountPolicy` already use. The learned spells are 33388, 33391, 34090, 34091, 90265 and 90267 (`SpellEffect.dbc`: the trainer spell has effects 44 and 36, the learned one 118 and 3). |
| Mount | `world.item_template` class 15 subclass 5 joined to `world.npc_vendor` with `ExtendedCost = 0` and `RequiredLevel <= 20`: the lowest `BuyPrice` is 10,000 copper (1 gold) at level 20. 49 items share it (the racial mounts, the dragon turtles). Level 40 mounts cost 100,000 (10 gold), but one mount serves every ground tier on 5.4.8 (`BotMountPolicy`), so only the first is reserved. |
| Profession ranks | `npc_trainer` templates 200400 to 200410 and 201010; rank spells found by name in `Spell.dbc` (Apprentice, Journeyman, Expert, Artisan, Master, Grand Master, Illustrious Grand Master) for the eight crafting professions, which share costs and levels: 10 copper at level 5, 500 at 10, 5,000 at 20, 50,000 at 35, 100,000 at 50, 350,000 at 65, 500,000 at 75. Gathering professions have the same costs at levels 10, 25, 40 and 55 (earlier), cooking, first aid, fishing and archaeology the same costs with no level; those are not modelled (section 6). |

## 3. What random bots really pay for (and why the class part is off)

- **Class spells are free for random bots today.** `AiPlayerbot.AutoLearnTrainerSpells` (default 1, design decision 0016) makes
  `BotFactory::LearnTrainerSpells` learn every green class trainer spell at each level-up
  (`AutoMaintenanceOnLevelupAction.cpp:124`); only trade skill and mount trainers are skipped for random bots
  (`BotFactory.cpp` near line 1746). So the class part of the fund is zero unless that switch is turned off:
  `Facts::classSpellsPaid` is `!AutoLearnTrainerSpells`. When it is on, the part is the table window plus the existing
  "train cost" value (`TrainCostValue`: class spells learnable now).
- **Riding and professions are paid** (`BotTraining`, BT-197 and BT-218): the bot walks to a trainer and pays the
  `spellcost`, after the reputation discount, from "free money for spells" (money minus guild, repair and ammo needs).
  That check does not see the auction house, which is why the fund is needed: today nothing stops a bot from spending
  its Journeyman Riding money on a bag.
- **Recipes are not in the fund; a crafter keeps a separate share for them** (section 1). A profession trainer also teaches recipes, and `BotTraining::LearnAffordable` buys
  every green one it can pay. The cost grows with the skill: for one crafting profession the green recipes cost 0.1 to
  3 gold up to skill 125, 3 to 8 gold at 200, 9 to 46 gold at 275, 67 to 163 gold at 350, 325 to 654 gold at 425.
  Reserving them would add hundreds of gold at levels 70 and up, for items the bot chooses to buy; they stay
  discretionary, so the rule is a share of the spare gold with a ceiling, not the recipe list.

## 4. The table of goals by level

A bot that bought every riding step below its level, with two professions and no mount missing:

| Level | Goals (gold) | What they are |
|------:|-------------:|---------------|
| 15 to 19 | 6 | Apprentice Riding (4), the mount (1), the Expert rank of two professions (0.5 each) |
| 20 | 5 | Apprentice Riding and the mount, not yet bought |
| 21 to 29 | 0 | the next step (Journeyman, level 40) is more than 5 levels away |
| 30 to 34 | 10 | Artisan rank of two professions (5 each) |
| 35 to 40 | 50 | Journeyman Riding |
| 41 to 44, 50 to 54 | 0 | nothing within 5 levels |
| 45 to 49 | 20 | Master rank of two professions (10 each) |
| 55 to 59 | 250 | Expert Riding |
| 60 | 320 | Expert Riding (250) and the Grand Master rank of two professions (70) |
| 61 to 64 | 70 | the Grand Master ranks (the licence is bought at 61 in this scenario) |
| 65 to 69 | 5,000 | Artisan Riding |
| 70 | 5,100 | Artisan Riding and the Illustrious rank of two professions |
| 71 to 74 | 100 | the Illustrious ranks |
| 75 to 80 | 5,000 | Master Riding |
| 81 and up | 0 | everything bought |
| 85 to 90, stuck at Expert and the licence | 5,000 | Artisan Riding, never bought |

A bot that has bought nothing keeps only the next step: Apprentice Riding and the mount, 5 gold, from level 15 on.

## 5. Effect on the bots' gold

`tools/economy/reserve_impact.py` (reference of the C++ policy, tested against it) takes the gold of the 330 random bots on
2026-11-22 (`characters.characters` joined to `auth.account`, the audit's method) and asks: if a bot holding what the
bots near this level hold stood at exactly this level with this fund, how much of its gold would be reserved, and what is
left to buy with per visit? Medians; "cap 50%" is the default share, "no cap" the literal rule (`share = 100`).

| Level | Scenario | Bots near | Median gold | Repair + goals (gold) | Share of the median bot's gold reserved, cap 50% / no cap | Purchase budget per visit now / cap 50% / no cap (gold) |
|------:|----------|----------:|------------:|----------------------:|-----------------------------------------------------------|---------------------------------------------------------|
| 20 | on schedule | 68 | 0.52 | 0.20 + 5 | 70% / 100% | 0.08 / 0.04 / 0.00 |
| 40 | on schedule | 51 | 44.16 | 0.80 + 50 | 51% / 100% | 10.84 / 5.42 / 0.00 |
| 60 | on schedule | 40 | 78.15 | 1.80 + 320 | 51% / 100% | 19.09 / 9.54 / 0.00 |
| 85 | bought all | 14 (levels 90) | 113.17 | 3.61 + 0 | 4% / 4% | 27.39 / 27.39 / 27.39 |
| 85 | stuck at Expert and the licence | 14 (levels 90) | 113.17 | 3.61 + 5,000 | 52% / 100% | 27.39 / 13.69 / 0.00 |
| 90 | bought all | 14 | 113.17 | 4.05 + 0 | 4% / 4% | 27.28 / 27.28 / 27.28 |
| 90 | stuck at Expert and the licence | 14 | 113.17 | 4.05 + 5,000 | 52% / 100% | 27.28 / 13.64 / 0.00 |

("Bots near" are the random bots within 5 levels; there are none between 70 and 89, so level 85 uses the level 90 bots'
gold. "Now" is today's rule without a fund.) The same table at every fifth level is in `reserve_impact.py` output.

What it says:

- Taken literally (no cap), the fund leaves the median bot **no purchase budget at all** at levels 15 to 20, 30 to 40,
  55 to 70 and, if it never bought Artisan Riding, at 85 and 90. The goals are larger than the gold the median bot holds
  (0.5 gold at 20 against 5; 78 gold at 60 against 320; 113 gold at 90 against 5,000). A bot below its fund buys nothing
  until it has earned past it, which for the 5,000 gold steps is never at today's incomes.
- The 50 percent cap keeps a bot's purchasing power at about half of today's: the median bot keeps half of its gold above
  the repair reserve for the goal and may spend a quarter of the rest per visit. A bot above its fund (levels 40 to 50
  with 60 to 100 gold, the 925 gold level 65 bots) is unaffected by the cap and keeps the whole fund.
- Selling is never blocked: deposits are held back by the repair reserve only. A level 20 bot with 0.52 gold (reserve
  0.20) can still pay deposits up to a quarter of the 0.32 above the repair reserve, about 8 silver, enough for a few
  cheap stacks.

## 6. Gaps, in order, with sizes

1. **Wire the fund into the visit** (small, about half a day): `BotAuction` builds `ReservePolicy::Facts` from the bot
   and calls `ComputeReserve`; described in `auction-wiring.md`. Needs: `SKILL_RIDING` pure value, a known mount
   (`KnowsMount` in `BotShopping.cpp`), spell 90267, the practised profession count (the stored pair plus the secondary
   skills the bot trained), the config keys.
2. **Make the other existing purchases honour the fund** (small): `BotShopping` (mount purchase), `BotSale` (supplies)
   and `BotTraining` currently use `PurchaseReserve` or "free money for spells". The fund is meant for the auction house
   only, as asked; the vendor trips keep their rules, so a bot may still buy food with gold the auction house may not
   touch. Recommended: leave it, since food is needed more than a bag.
3. **Secondary skills and gathering professions** (small): cooking, first aid, fishing, archaeology ranks have no level
   gate and are not in the fund; gathering ranks come 10 levels earlier than the crafting ones. The error is under 20
   gold for any level.
4. **Class spells when `AutoLearnTrainerSpells` is off** (none today): the table is there; the "train cost" value would
   need to count only the bot's own faction's trainers (it counts every trainer, so a mage's Alliance and Horde portal
   spells add up).
5. **Recipes** (medium, only if the owner wants it): section 3. The cheapest honest rule is a fixed share, not a
   reservation of the whole recipe list.
6. **A cold weather flying and Pandaria flight licence** (small): 500 gold at level 68 and the level 90 licence are not
   reserved; both need a decision on whether bots should fly in those zones.

## 7. Facts for the lead to confirm on Wowhead MoP Classic

Prices and levels as our 5.4.8 database holds them (Wowhead may show the 1.25 times rescale seen for gear, design decision 0034):

- Apprentice Riding 4 gold, level 20; Journeyman 50 gold, level 40; Expert 250 gold, level 60; Flight Master's License
  250 gold, level 60; Artisan 5,000 gold, level 70; Master 5,000 gold, level 80; Cold Weather Flying 500 gold, level 68.
- The cheapest vendor mount at level 20 costs 1 gold (Brown Horse Bridle 5656, Gray Ram 5864, Red Mechanostrider 8563).
- Profession ranks: Journeyman 5 silver (level 10), Expert 50 silver (20), Artisan 5 gold (35), Master 10 gold (50),
  Grand Master 35 gold (65), Illustrious Grand Master 50 gold (75).
- Class trainer total for one class (mage, 117 gold from level 3 to 85) as a plausibility check only.

## 8. Tests

- `tests/test_reserve_policy.cpp`: 23 tests (table shape and the riding prices, every goal, the share cap, the visit
  money including a poor bot that still lists, deposits against purchases, overflow-safe levels).
- `tools/economy/tests/test_economy_tools.py`: the generator functions on synthetic rows, the committed header, and the
  Python reference against the same numbers the C++ tests check.
