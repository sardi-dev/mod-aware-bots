# Pricing audit and proposed price model for the bots' auction house

- **Date:** 2026-11-22
- **Status:** Approved by the owner on 2026-11-22 (design decision 0034); one retune allowed after the maintainer's Wowhead checks
  (section 13). The auction house code is written behind `AiPlayerbot.Auction.Enable`, off by default.
- **Author:** economy design. Every number below comes from the 5.4.8 world database or the client data
  (`data/dbc`, `data/db2`, not part of this repository); the tools that produced them are in `tools/economy/` and rerun in about two minutes.

## 1. Verdict in one page

1. The item table has 72,262 rows but only **19,711 can be put on an auction house** (27 percent). The rest is bind on
   pickup (45,198), quest bound (6,260), quest class (550), deprecated (467) or conjured (76). The model prices
   18,925 of the listable items; the other 786 stay unpriced on purpose (test and deprecated names such as `zz` and
   `[PH]`, keys, gifts and rows with no price and no real source).
2. Prices are clean enough to anchor on. 57,492 items (80 percent) have both a BuyPrice and a SellPrice; BuyPrice is
   exactly 5 times SellPrice for 39,872 items and 4 times for 13,926. The exception is 1,872 items (mostly Pandaria
   materials and consumables) whose BuyPrice is only about 1.15 times SellPrice and says nothing about value. A
   peer-median fallback prices 140 items that have neither price (recipes, materials, consumables, gems).
3. The owner's floor is exact in the model: **all 1,600 gold-vendor items start at ceil(1.10 x BuyPrice / BuyCount)**,
   checked in the data. Everything else starts at least 10 percent above what a vendor pays the seller.
4. The expensive classes the owner asked for are expensive by explicit multipliers (section 5): profession goods are
   priced from their reagents with a 25 to 100 percent margin, dungeon and raid gear is 1.5 to 3.5 times a world drop
   of the same quality, rare world gear 1.5 times plus a scarcity premium up to 1.8.
5. A caveat that shapes the whole design: **modern gear is bind on pickup.** Only 347 priced items come from dungeon
   or raid sources (24 dungeon boss, 187 dungeon trash, 136 raid trash and boss) and just 19 of the 7,286 items in the
   Dungeon Journal are tradable. The "dungeon and raid items are expensive" rule therefore covers a small, mostly
   Classic to Wrath BoE set. The big supply is world greens and blues (5,393), crafted goods (4,223) and materials.
6. Affordability: bots hold a median of 1 gold or less below level 30, 11 gold at 30 to 39, 63 at 40 to 49, 20 at
   50 to 59 and 136 at 60 to 69. At minimum price the median bot can afford 85 to 99 percent of level-appropriate
   consumables and 74 to 98 percent of materials at every level below 90, and 78 to 98 percent of green and blue gear
   at levels 1 to 9, 30 to 49 and 60 to 69. The poorer half of bots at 10 to 29 and 50 to 59 cannot buy gear, and
   nearly everyone at 90 cannot (level 90 gear starts at about 120 gold against a median of 113). That is the right
   outcome for an earned economy, not a reason to lower prices.
7. The model is one pure function over one precomputed table: `GetPriceRange(item)` returning (min, max) copper per
   unit plus a source class. The table is 18,892 rows (the 18,925 priced items minus 33 above the trading cap), about 295 KiB in memory and 429 KB as SQL.

## 2. Method and data

| Data | Where it comes from |
|------|---------------------|
| Items | `world.item_template` (72,262 rows). 67,027 rows have exactly the BuyPrice and SellPrice of `Item-sparse.db2`; the other 5,160 differ (the database is the server's truth and is used) |
| Vendors | `world.npc_vendor` (53,797 item rows, 12,645 items): 4,021 items for gold (382 of them with limited stock at every vendor), 8,624 only for tokens or currency (`ExtendedCost`) |
| Loot | `creature_loot_template` (615,935 rows), `reference_loot_template`, `gameobject_loot_template`, `skinning`, `pickpocketing`, `fishing`, `prospecting`, `milling`, `disenchant`, `item_loot`, `spell_loot`, `mail_loot` |
| Creatures and places | `creature_template` (rank, level, `ScriptName`, difficulty entries), `creature` (map per entry), `Map.dbc` (instance type: 1 dungeon, 2 raid), `gameobject_template` and `gameobject` |
| Recipes | `SkillLineAbility.dbc` (field 1 skill, 2 spell, 5 minimum rank, 8 and 9 trivial ranks), `SpellEffect.dbc` (effects 24, 59 and 157 create an item; field 11 is the item, field 6 the count), `Spell.dbc` (names, field 19 = record id in `SpellReagents.db2`), `SkillLine.dbc` (names). 4,904 recipe rows for 4,839 distinct items |
| Reagents | `data/db2/SpellReagents.db2` (WDB2; the `.dbc` is an empty file). 13,999 reagent rows. A reader for both formats is `tools/economy/export_recipes.py`; a separate journal audit tool reads only WDBC |
| Boss loot | `JournalEncounterItem.dbc`, `JournalEncounter.dbc`, `JournalInstance.dbc` (7,286 distinct items; first-level reference for dungeon and raid loot) |
| Auction rules | core `AuctionHouseMgr.cpp` and `AuctionHouse.dbc`: faction houses take a 5 percent cut and a 5 percent deposit rate, the neutral house 15 and 25; deposit = 0.15 x SellPrice x count x (duration in 12 h units) |
| Bots | read-only aggregates from `characters.characters` joined to `auth.account` (`RNDBOT%` usernames: 330 characters), `character_inventory`, `item_instance`, `character_skills` |

Rules used to say an item **can be listed** (`classify.py`): not bind on pickup, not quest bound (bonding 4), not
conjured (`Flags` 0x2, the core refuses it), not bind to account (0x08000000), not deprecated (0x10), not a quest class
item. Price is not part of the test (rows with no price are handled in 3.4). The core's own test is `Item::CanBeTraded` plus the conjured
flag (`AuctionHouseHandler.cpp`). Items bots currently wear are never listed (`BotSalePolicy` protected sets).

## 3. Item population

### 3.1 By class

| Class | Name | Rows | Listable | BoP | BoE | No binding | With both prices | With no price |
|------:|------|-----:|---------:|----:|----:|-----------:|-----------------:|--------------:|
| 4 | Armor | 42,865 | 9,718 | 33,059 | 8,243 | 1,542 | 39,490 | 3,114 |
| 2 | Weapon | 8,122 | 2,405 | 5,676 | 1,762 | 667 | 7,925 | 150 |
| 12 | Quest | 7,292 | 0 | 923 | 1 | 543 | 526 | 6,744 |
| 15 | Miscellaneous | 4,309 | 1,836 | 2,378 | 3 | 1,843 | 2,569 | 1,274 |
| 0 | Consumable | 3,216 | 1,870 | 978 | 4 | 1,957 | 2,155 | 951 |
| 9 | Recipe | 3,210 | 1,178 | 1,744 | 0 | 1,466 | 1,934 | 1,231 |
| 3 | Gem | 1,316 | 1,085 | 228 | 0 | 1,088 | 1,239 | 70 |
| 7 | Trade goods | 1,129 | 1,022 | 78 | 1 | 1,043 | 991 | 76 |
| 16 | Glyph | 472 | 462 | 0 | 0 | 472 | 470 | 2 |
| 1 | Container | 211 | 119 | 91 | 66 | 54 | 188 | 9 |
| 13, 10, 6, 8, 17 | Key, money, projectile, generic, battle pet | 120 | 16 | 43 | 0 | 20 | 5 | 110 |

(BoP and BoE do not add up to the row count: bonding 3 "bind on use" and 4 "quest" are the rest.)

### 3.2 By quality (listable items)

| Quality | Rows | Listable | Of those BoE |
|---------|-----:|---------:|-------------:|
| 0 grey | 3,563 | 2,632 | 254 |
| 1 white | 15,075 | 4,796 | 20 |
| 2 green | 15,794 | 7,224 | 6,180 |
| 3 blue | 12,688 | 3,540 | 2,571 |
| 4 epic | 24,902 | 1,507 | 1,000 |
| 5 to 7 | 240 | 12 | 1 |

24,902 epics sounds large, but 23,345 of them are BoP: the table holds every difficulty and every upgrade tier of
Pandaria raid and PvP gear.

### 3.3 By item level (all rows and listable)

| Item level | 0-10 | 11-30 | 31-50 | 51-70 | 71-100 | 101-300 | 301-450 | 451+ |
|------------|-----:|------:|------:|------:|-------:|--------:|--------:|-----:|
| Rows | 13,720 | 4,835 | 5,127 | 6,633 | 7,021 | 14,979 | 8,903 | 11,044 |
| Listable | 3,749 | 2,565 | 2,486 | 3,122 | 2,758 | 2,825 | 1,609 | 597 |

The market is thick up to item level 300 and thin for Pandaria gear (597 items above 450).

### 3.4 How BuyPrice and SellPrice behave

- 57,492 rows have both prices, 505 only BuyPrice, 534 only SellPrice, 13,731 neither. Of the 13,731, 6,744 are quest
  items and most of the rest are BoP or test rows; 651 are tradable by their flags (recipes 347, Pandaria reagents such as Spirit
  Dust 74249, keys, gifts). 140 of them (recipes, materials, consumables, gems, weapons and armor with a real source)
  get the median anchor of their peers (same class, quality and item level band, recipes by required skill rank); the
  rest, which have no price and no real source, stay unpriced.
- Ratio BuyPrice per unit to SellPrice, items with both: exactly 5.0 for 39,872, exactly 4.0 for 13,926, between 4.1
  and 4.9 for 248, above 5.1 for 1,504, **below 1.5 for 1,872** (mostly class 0, 7 and 15 from Pandaria: Ghost Iron
  Ore 72092 has SellPrice 1,500 and BuyPrice 1,725). For those, BuyPrice is not a value; the model uses SellPrice.
- `BuyCount` is 1 for 71,446 items; 654 have 5, 48 have 200 (ammunition), a few 25, 20, 10, 6. A vendor charges
  BuyPrice for `BuyCount` units, so the unit price is BuyPrice / BuyCount and all auction prices are per unit.
- BuyPrice reaches 1,200,000,000 copper (120,000 gold) and SellPrice 300,000,000. 33 priced items end up above 10,000
  gold at their maximum; the module should never trade those (section 8).

## 4. Where each listable item comes from

`classify.py` expands every loot table (loot groups share the chance the explicit rows leave, reference rows pull in
their table and roll `maxcount` times, negative chances are quest-only and skipped), maps each loot id to the
creatures and game objects that use it and each creature to its map, then gives every item the tags of all its sources
and one **primary class**: the easiest source wins, in this order.

`vendor` (gold) > `crafted` > `gathered` > `world_normal` > `world_elite` > `world_rare` > `world_chest` >
`dungeon_trash` > `dungeon_boss` > `raid_trash` > `raid_boss` > `container` > `quest_reward` > `other`

The easiest source sets the price because that is what a buyer can fall back on. Creature class: on a raid map a boss
or trash mob; on a dungeon map a boss or trash mob; in the open world rank 4 and rank 2 are rare, rank 3 is a world
boss counted as rare, rank 1 elite, rank 0 normal. A boss is rank 3 or a `ScriptName` starting with `boss_` (dungeon
bosses are mostly rank 1 in this database: only 11 creatures on dungeon maps have rank 3), and difficulty variants
inherit the map and boss status of their base entry. Creatures with no spawn row are tagged `summoned` and do not make
an item world or instance loot (813,022 loot rows). Nodes and chests (game object type 3): on a dungeon or raid map
they are instance chests, elsewhere trade goods are `gather_node` and the rest `world_chest`.

### 4.1 Result (priced listable items, by class and quality)

| Source class | grey | white | green | blue | epic | Total | What it is |
|--------------|-----:|------:|------:|-----:|-----:|------:|-----------|
| vendor (gold) | 104 | 1,372 | 112 | 10 | 2 | 1,600 | sold for gold by an NPC (125 of the 4,021 gold items have no spawned vendor in this database and are counted anyway) |
| crafted | 2 | 1,264 | 963 | 1,376 | 618 | 4,223 | created by a profession spell (4,839 items; by recipe rows: Jewelcrafting 866, Leatherworking 847, Blacksmithing 805, Tailoring 605, Inscription 595, Engineering 357, Alchemy 316, Cooking 235, Archaeology 143, First Aid 22) |
| gathered | 106 | 275 | 84 | 33 | 5 | 503 | skinning, mining and herb nodes, fishing, prospecting, milling, disenchanting |
| recipe | 0 | 46 | 376 | 136 | 203 | 761 | class 9 patterns, plans, designs, formulae (not spell books) not sold for gold |
| world_normal | 1,386 | 400 | 4,762 | 631 | 118 | 7,297 | any ordinary creature drops it |
| world_elite | 10 | 1 | 0 | 8 | 5 | 24 | only elites |
| world_rare | 2 | 22 | 58 | 113 | 31 | 226 | rares, rare elites, world bosses |
| world_chest | 30 | 18 | 5 | 9 | 12 | 74 | treasure chests and similar |
| dungeon_trash | 31 | 13 | 5 | 134 | 4 | 187 | non-boss dungeon creatures |
| dungeon_boss | 8 | 5 | 0 | 11 | 0 | 24 | dungeon bosses and chests, plus the Journal |
| raid_trash | 6 | 2 | 0 | 10 | 28 | 46 | |
| raid_boss | 22 | 4 | 1 | 2 | 61 | 90 | |
| container | 87 | 117 | 52 | 19 | 20 | 295 | lockboxes, bags, spell and mail loot, pickpocket |
| quest_reward | 5 | 42 | 9 | 10 | 25 | 91 | only a quest hands it out |
| vendor_token | 2 | 8 | 0 | 110 | 72 | 192 | sold only for honor, valor and other currencies |
| other | 775 | 844 | 695 | 715 | 263 | 3,292 | no source found in this database (cut content, unimplemented loot) |

Examples with ids: vendor Refreshing Spring Water 159, Greater Healing Potion 1710; crafted Mithril Bar 3860, Flask
of Spring Blossoms 76084, Helm of Command 41344, Windripper Leggings 44931; gathered Linen Cloth 2589, Runecloth 14047,
Golden Lotus 72238, Large Brilliant Shard 14344; world_normal Greenweave Gloves 9771, Assault Band 13095, Singing
Crystal Axe 31318; world_elite Battlechaser's Greaves 12555; world_rare Nethersteel Gauntlets 31164, Hope Ender
30733; world_chest Cast Iron Shackles 37891; dungeon_trash Darkweb Bindings 35640; dungeon_boss Shadowcraft Gloves
16712, Lightforge Gauntlets 16724; raid_trash Cenarion Belt 16828; raid_boss Leggings of Lost Love 45468;
recipes Pattern: Runic Leather Gauntlets 15731, Design: Rigid Star of Elune 24205; container Perfect Jagged Dark Jade
41468; quest_reward Darkmoon Card: Earthquake 62048; vendor_token Replica Helm of Heroism 78330; other Latent
Kor'kron Boots 97827.

Reading the table: **3,292 listable items (17 percent) have no source here.** They are priced like world drops without a
scarcity premium. They are unreachable for bots unless a human puts them on the house, which is exactly why they need
a price. Bots carry (not wear) 1,696 item types today; 1,121 of those are listable, 716 of them world drops.

### 4.2 Supply

The model also needs how scarce an item is. `supply` = the sum of the drop chances of all creatures (or nodes) that
drop it, in copies per kill of each. Examples: a world green has a median supply of 0.07, a blue 0.09, an epic 0.03,
while a white cooking material has 0.4. `classify.py` stores the best single chance and the number of distinct sources
in `item_class`.

## 5. Proposed price model

All prices are copper per single item. Definitions: `S` = SellPrice, `U` = BuyPrice / BuyCount. Gear, materials and
consumables are anchored on `S`; vendor-sold items on `U` (the floor); recipes on the notional buy price `N` = U when
U is at least 2 x S, else 4 x S (Pandaria items). An item with neither price takes the median `N` of its peers (same
class, quality and item level band; recipes by required skill rank) and `S` = N / 4. A quality group is grey (0),
white (1), green (2), blue (3), epic (4 and above).

### 5.1 The rule for each class

| Source class | Minimum | Maximum | Why |
|--------------|---------|---------|-----|
| vendor (gold) | **1.10 x U**, exactly, rounded up to the copper | 1.5 x min; 3 x min when every NPC that sells it has a capped stock (`npc_vendor.maxcount` > 0); recipes 2.5 x min | owner rule: floor is vendor price plus 10 percent. Nobody pays much above a price anyone can get at an NPC; a sold-out vendor item can go higher |
| gathered | S x (3 grey, 5 white, 5 green, 6 blue, 10 epic) x f | S x (8, 12, 12, 15, 30) x f | materials are worth a multiple of the vendor value that grows with tier. f is the scarcity factor |
| crafted | max(S x (2, 3, 4, 10, 30), **1.25 x reagent cost**) | max(S x (4, 8, 10, 25, 90), **2.0 x reagent cost**) | owner rule: profession goods are expensive. Reagent cost = what the crafter pays for the cheapest recipe (each reagent at its own minimum price, divided by the number made). The S term is the floor for cheap recipes |
| recipe (not for gold) | N x (2, 2, 3, 5, 8) x f | N x (4, 4, 6, 10, 20) x f | recipes keep their value: patterns of expensive goods cost real money. Recipes are anchored on the notional buy price because their SellPrice is a poor guide (a Pandaria recipe can have none) |
| world_normal | S x (1.1, 1.5, 2, 6, 20) x f | S x (1.6, 3, 5, 15, 60) x f | the base table: a drop anyone can farm is worth a few times what a vendor pays |
| world_elite | base x 1.25 | base x 1.25 | harder to farm |
| world_rare, world_chest | base x 1.5 x f | base x 1.5 x f | owner rule: very rare open-world gear is expensive |
| dungeon_trash | base x 1.5 | base x 1.5 | owner rule: dungeon items are expensive |
| dungeon_boss | base x 2.0 | base x 2.0 | |
| raid_trash | base x 2.5 | base x 2.5 | owner rule: raid items are expensive |
| raid_boss | base x 3.5 | base x 3.5 | |
| container, quest_reward, other | base x 1.0 | base x 1.0 | no premium: no evidence of rarity |
| vendor_token | base x 1.5 | base x 1.5 | token gear (replicas, PvP BoE) that a player earned |

`base` is the world_normal row of the same quality group; the class factor multiplies both ends. The scarcity factor f
(5.2) applies to gathered, recipe and every world, dungeon and raid class, and not to vendor, crafted, container,
quest_reward, vendor_token or other items. **Grey items take no
class or scarcity factor** (junk is worth what a vendor pays, wherever it dropped).

Every minimum is raised to at least `1.1 x S` (a bot never lists below what a vendor would pay it, plus 10 percent for
the 5 percent cut and the deposit lost on an expired listing), and every maximum is at least 1.2 x the minimum.
Minimums round up and maximums round to two significant digits so listed prices look like players' prices.

### 5.2 Scarcity factor f

`f = clamp((typical supply / item supply) ^ 0.25, 0.85, 1.80)`, where `typical supply` is the median supply of items
with the same source class and quality group. An item as common as its peers has f = 1; one with a tenth of the
supply has f = 1.78; one ten times as common has 0.85. It applies to the world, dungeon, raid, gathered and recipe
classes. Materials used by five or more recipes get an extra 1.2 (demand).

### 5.3 Choosing a price inside the range

Reference code and tests: `tools/economy/listing_policy.py`.

1. **Position.** The range runs from 0 (minimum) to 1 (maximum). A new listing lands at:
   - empty market for the item on this house: 0.35 + 0.45 x r (upper middle);
   - one or two listings: 0.15 + 0.50 x r;
   - three or more listings: 0.00 + 0.25 x r (sellers compete, bottom quarter);
   with r a uniform random number. A daily "mood" per source class and day shifts the position by up to 10 percent of
   the range so the market moves.
2. **Undercut.** If the cheapest existing listing is above the minimum, the new price is the lower of its position
   price and that listing minus 1 to 5 percent, never below the minimum.
3. **Stacks.** The unit price times the stack size; stacks of the same item at the same price are normal.
4. **Do not list** when the minimum is below what the vendor pays after cut and deposit (never true under 5.1), or
   when the item is a protected item (`BotSalePolicy`), or the maximum is above the 10,000 gold trading cap.
5. **Bid and buyout.** Buyout at the chosen price, starting bid 90 percent of it (rounded up). Humans' listings keep
   their own prices; the model only judges them when a bot considers buying.

### 5.4 How bots decide to buy

A bot buys a listing when the unit price is at or below the **cap for its need** and the purchase fits the owner's
budget rule (at most 25 percent of the gold above the reserve, the reserve being the existing
`BotSalePolicy::PurchaseReserve`: 5 copper x level squared, at most 1 gold per 10 levels):

| Need | Cap (fraction of the range above the minimum) |
|------|------------------------------------------------|
| gear upgrade for the bot's spec | 0.50 |
| bag | 0.50 |
| materials for its own profession | 0.40 |
| consumables it uses | 0.35 |

A listing above the maximum is never bought (a human who overprices simply does not sell to bots). A human listing
below the minimum is a bargain: the cap rule still applies, so bots do not hoard.

## 6. Worked examples

Copper per unit. Source class is what the model decided. "Req lvl" shows the item level (ilvl) where the item has no
required level. All 43 rows are produced by `tools/economy/examples.py`.

| id | item | source class | quality | req lvl | SellPrice | BuyPrice (per unit) | proposed range |
|---|---|---|---|---|---|---|---|
| 2770 | Copper Ore | vendor | white | ilvl 10 | 5c | 20c | 22c to 66c |
| 2447 | Peacebloom | vendor | white | ilvl 5 | 10c | 40c | 44c to 1s 30c |
| 2589 | Linen Cloth | gathered | white | ilvl 5 | 13c | 55c | 74c to 1s 80c |
| 14047 | Runecloth | gathered | white | ilvl 50 | 4s | 16s | 31s to 72s |
| 13468 | Black Lotus | gathered | green | ilvl 60 | 10s | 40s | 54s to 1g 30s |
| 72092 | Ghost Iron Ore | gathered | white | ilvl 86 | 15s | 86c | 64s to 1g 50s |
| 72238 | Golden Lotus | gathered | green | ilvl 90 | 1g | 38s 33c | 11g to 26g |
| 14344 | Large Brilliant Shard | gathered | blue | ilvl 55 | 0c | 3g 60s | 6g 20s to 15g |
| 2840 | Copper Bar | crafted | white | ilvl 10 | 10c | 40c | 36c to 96c |
| 3860 | Mithril Bar | crafted | white | ilvl 40 | 4s | 16s | 15s to 38s |
| 118 | Minor Healing Potion | crafted | white | 1 | 5c | 20c | 1s 40c to 2s 20c |
| 9206 | Elixir of Giants | crafted | white | 38 | 7s | 28s | 23s to 56s |
| 13510 | Flask of the Titans | crafted | white | 50 | 12s 50c | 50s | 1g 10s to 1g 70s |
| 76084 | Flask of Spring Blossoms | crafted | white | 85 | 25s | 1g | 28g to 43g |
| 37663 | Titansteel Bar | crafted | green | ilvl 80 | 1g 25s | 5g | 20g to 31g |
| 76141 | Imperial Amethyst | crafted | blue | ilvl 90 | 3g 50s | 14g | 35g to 88g |
| 12419 | Radiant Boots | crafted | green | 53 | 1g 64s 21c | 8g 21s 7c | 6g 60s to 16g |
| 41344 | Helm of Command | crafted | blue | 78 | 6g 99s 25c | 34g 96s 27c | 70g to 170g |
| 44931 | Windripper Leggings | crafted | epic | 80 | 5g 6s 83c | 25g 34s 18c | 230g to 460g |
| 98604 | Snow Lily Britches | crafted | epic | 90 | 36g 50s 86c | 182g 54s 32c | 3,500g to 5,500g |
| 159 | Refreshing Spring Water | vendor | white | 1 | 1c | 5c | 6c to 8c |
| 1710 | Greater Healing Potion | vendor | white | 21 | 1s 25c | 5s | 5s 50c to 16s |
| 22829 | Super Healing Potion | vendor | white | 55 | 50s | 2g | 2g 20s to 6g 60s |
| 4777 | Ironwood Maul | vendor | green | 13 | 14s 8c | 70s 40c | 77s 44c to 2g 30s |
| 6274 | Pattern: Blue Overalls | vendor | white | ilvl 20 | 1s | 4s | 4s 40c to 11s |
| 9771 | Greenweave Gloves | world_normal | green | 20 | 5s 32c | 26s 62c | 9s 30c to 23s |
| 6591 | Battleforge Wristguards | world_normal | green | 22 | 10s 7c | 50s 39c | 20s to 50s |
| 24723 | Dreghood Pauldrons | world_normal | green | 60 | 2g 20s 15c | 11g 76c | 4g 10s to 10g |
| 13095 | Assault Band | world_normal | blue | 39 | 66s 46c | 2g 65s 84c | 3g 60s to 8g 90s |
| 4090 | Mug O' Hurt | world_normal | blue | 41 | 2g 7s 36c | 10g 36s 84c | 12g to 29g |
| 31318 | Singing Crystal Axe | world_normal | epic | 70 | 12g 33s 28c | 61g 66s 41c | 280g to 820g |
| 87695 | Treads of Edward the Odd | world_normal | epic | 90 | 24g 1s 38c | 120g 6s 93c | 480g to 1,400g |
| 31164 | Nethersteel Gauntlets | world_rare | blue | 66 | 2g 44s 73c | 12g 23s 65c | 23g to 57g |
| 30733 | Hope Ender | world_rare | epic | 70 | 11g 63s 63c | 58g 18s 17c | 350g to 1,000g |
| 35640 | Darkweb Bindings | dungeon_trash | blue | 74 | 3g 94s 57c | 19g 72s 87c | 31g to 75g |
| 16712 | Shadowcraft Gloves | dungeon_boss | blue | 54 | 1g 64s 93c | 8g 24s 67c | 20g to 49g |
| 16828 | Cenarion Belt | raid_trash | epic | 60 | 1g 79s 72c | 8g 98s 64c | 94g to 280g |
| 45468 | Leggings of Lost Love | raid_boss | epic | 80 | 15g 7s 70c | 75g 38s 50c | 900g to 2,700g |
| 15731 | Pattern: Runic Leather Gauntlets | recipe | green | ilvl 54 | 35s | 1g 40s | 4g to 7g 80s |
| 24205 | Design: Rigid Star of Elune | recipe | blue | ilvl 70 | 3g | 12g | 62g to 120g |
| 86380 | Pattern: Imperial Silk Gloves | recipe | epic | 1 | 7g 50s | 30g | 300g to 730g |
| 41468 | Perfect Jagged Dark Jade | container | green | ilvl 70 | 1g | 4g | 2g to 5g |
| 78330 | Replica Helm of Heroism | vendor_token | epic | 60 | 3g 47s 88c | 17g 39s 40c | 110g to 310g |

What the examples show:

- **Vendor items sit just above the NPC price** (Spring Water 6c to 8c; Copper Ore 22c, exactly 1.1 x 20c). The wide
  upper end (3 times the floor) appears only where every NPC sells a capped stock (Peacebloom, Super Healing Potion).
- **Profession goods follow their reagents.** Flask of Spring Blossoms (about 22 gold of reagents) lists at 28 to 43
  gold, while the same SellPrice as a non-profession item would give 0.4 to 1 gold. A level 90 crafted epic (Snow Lily
  Britches: 28 Hardened Magnificent Hide at roughly 100 gold each) is in the thousands.
- **Level 70 epics, by origin:** Singing Crystal Axe (ordinary world drop) 280 to 820 gold, Hope Ender (rare) 350 to
  1,000. **Level 80 epics:** crafted Windripper Leggings 230 to 460, raid-boss Leggings of Lost Love 900 to 2,700.
- **Blue gear ladder:** Assault Band L39 world drop 3.6 to 8.9 gold; Shadowcraft Gloves L54 dungeon boss 20 to 49;
  Nethersteel Gauntlets L66 rare 23 to 57; Darkweb Bindings L74 dungeon trash 31 to 75.
- **Prices sit well under the NPC BuyPrice for common gear** (Dreghood Pauldrons: BuyPrice 11 gold 76 silver, range
  4 to 10 gold), as in the real game, where players do not pay NPC prices for random greens.

These are model outputs, not market facts. Section 10 lists the items the maintainer should check on Wowhead MoP Classic
before the numbers are final; I did not compare any price to live Wowhead data.

### 6.1 Median ranges by class and quality (gold, min to max, number of items)

| Class | grey | white | green | blue | epic |
|-------|------|------|------|------|------|
| vendor | 0.07-0.12 (104) | 0.24-0.48 (1,372) | 1.10-3.30 (112) | 1,650-2,500 (10) | n=2 |
| gathered | 0.02-0.05 (106) | 0.07-0.16 (275) | 1.20-2.80 (84) | 31-76 (33) | 36-110 (5) |
| crafted | n=2 | 0.68-1.10 (1,264) | 3.60-8.50 (963) | 80-150 (1,376) | 320-810 (618) |
| recipe | - | 4.40-8.60 (46) | 2.60-5.20 (376) | 51-100 (136) | 150-360 (203) |
| world_normal | 0.14-0.19 (1,386) | 0.01-0.03 (400) | 4.10-10 (4,762) | 21-51 (631) | 130-380 (118) |
| world_elite | 0.28-0.40 (10) | n=1 | - | 29-71 (8) | 99-300 (5) |
| world_rare | n=2 | 0.01-0.02 (22) | 0.20-0.48 (58) | 29-71 (113) | 310-910 (31) |
| world_chest | 0.01-0.01 (30) | 0.17-0.34 (18) | 1.50-3.80 (5) | 9.10-23 (9) | 200-600 (12) |
| dungeon_trash | 0.15-0.21 (31) | 0.08-0.15 (13) | 0.03-0.07 (5) | 12-29 (134) | 610-1,800 (4) |
| dungeon_boss | 0.98-1.40 (8) | 0.11-0.22 (5) | - | 24-59 (11) | - |
| raid_trash | 0.34-0.48 (6) | n=2 | - | 43-110 (10) | 140-400 (28) |
| raid_boss | 12-16 (22) | 0.07-0.13 (4) | n=1 | n=2 | 550-1,600 (61) |
| container | 0.11-0.16 (87) | 0.15-0.30 (117) | 2.00-5.00 (52) | 24-59 (19) | 100-300 (20) |
| quest_reward | 0.00-0.00 (5) | 0.02-0.04 (42) | 0.15-0.36 (9) | 0.87-2.20 (10) | 220-640 (25) |
| vendor_token | n=2 | 0.02-0.04 (8) | - | 23-57 (110) | 150-440 (72) |
| other | 0.00-0.00 (775) | 0.09-0.17 (844) | 0.03-0.06 (695) | 24-60 (715) | 230-680 (263) |

Medians mix all levels, so a class with many low-level items (world_rare green, quest_reward) looks cheap. Use the
by-level view in section 7.

## 7. Affordability

Bots (330 random-bot characters, levels 1 to 90, copper from `characters.characters.money`):

| Level band | Bots | Gold p25 / p50 / p75 / p90 |
|-----------:|-----:|----------------------------|
| 1-9 | 51 | 0.8 / 1.0 / 1.2 / 27.3 |
| 10-19 | 46 | 0.2 / 0.6 / 24.5 / 42.8 |
| 20-29 | 75 | 0.4 / 0.9 / 19.2 / 35.5 |
| 30-39 | 65 | 1.6 / 10.8 / 36.5 / 51.8 |
| 40-49 | 32 | 44.2 / 63.4 / 86.9 / 129.8 |
| 50-59 | 14 | 1.0 / 19.7 / 90.2 / 137.4 |
| 60-69 | 33 | 55.5 / 136.3 / 944.2 / 1,509.7 |
| 90 | 14 | 22.9 / 113.2 / 134.7 / 976.6 |

(No bots between 70 and 89. Gold is bimodal: many are poor, a few have thousands.) The 46 other characters (alt bots
and the owner's) are not included in the bot figures.

Share of level-appropriate listings whose **minimum** price fits a bot's budget at the median / 75th percentile gold
(budget = 25 percent of gold above the reserve):

| Level band | Gear, green and blue | Consumables | Materials |
|-----------:|---------------------|-------------|-----------|
| 1-9 | 98% / 99% | 86% / 88% | 79% / 80% |
| 10-19 | 37% / 100% | 96% / 100% | 81% / 93% |
| 20-29 | 4% / 98% | 85% / 100% | 74% / 100% |
| 30-39 | 78% / 96% | 99% / 100% | 94% / 99% |
| 40-49 | 93% / 95% | 96% / 99% | 98% / 98% |
| 50-59 | 36% / 82% | 89% / 97% | 87% / 100% |
| 60-69 | 89% / 100% | 97% / 100% | 97% / 97% |
| 90 | 1% / 3% | 91% / 91% | 56% / 62% |

Median minimum / maximum price of those listings in gold: gear 0.05 / 0.12 (1-9), 0.16 / 0.38 (10-19), 0.44 / 1.10
(20-29), 0.98 / 2.40 (30-39), 2.2 / 5.3 (40-49), 6.6 / 16 (50-59), 12 / 28 (60-69), 120 / 290 (90);
consumables 0.02 to 0.57 gold; materials 0.01 to 25 gold (level 90 materials: Golden Lotus class).

**Verdict.**

- Consumables and materials are affordable for the median bot at almost every level (74 to 99 percent of listings
  below level 90). Gear is affordable for the median bot at levels 1-9, 30-49 and 60-69 (78 to 98 percent), and for
  the 75th percentile almost everywhere except level 90.
- The poor half of levels 10-29 and 50-59 cannot buy gear: they hold 0.2 to 1 gold against gear at 0.2 to 7 gold.
  Their budget covers consumables and cheap materials. That is the intended outcome (they earn first), and the
  25 percent rule keeps them from spending their repair money.
- **Level 90 gear is out of reach for almost all bots** (minimum about 120 gold against 113 gold median, 25 percent
  budget about 27). At 90 the realistic bot purchases are consumables, bags and materials. Do not lower the prices to
  fix this: Pandaria gear prices come from the client's own SellPrice.
- Level 40 characters (the owner's included): gear of level 40 to 49 lists at a median of 2.2 to 5.3 gold, so the
  63 gold the bots' median holds at that level fits it easily; raid and crafted epics (130 to 810 gold at the median)
  are out of reach until much later. The humans can buy any bot listing, since bots price against the same table.
- Market size today: the 1,696 item types bots carry (not wear) are worth about 4,180 gold at the minimum prices and
  870 gold at vendor prices, spread over 330 bots, so even listing everything would not flood the house with gold;
  the auction house pays about 4.8 times the vendor value on what bots hold, a real incentive to list.

## 8. Where the model plugs in

One interface, no game core inside (so it unit-tests like `BotSalePolicy`):

```cpp
// src/Utils/PricePolicy.h  (proposal)
enum class PriceClass : uint8_t { Vendor = 1, Gathered, Crafted, Recipe, WorldNormal, WorldElite, WorldRare,
                                  WorldChest, DungeonTrash, DungeonBoss, RaidTrash, RaidBoss, Container,
                                  QuestReward, VendorToken, Other };
struct PriceRange { uint32_t minCopper; uint32_t maxCopper; PriceClass source; };

// Per unit. Not found means "do not trade" (BoP, quest, unpriced, test item, or above the trading cap).
bool GetPriceRange(uint32_t itemId, PriceRange& out);
uint32_t ListingPrice(PriceRange const&, uint32_t cheapestUnit, uint32_t listings, float roll, float undercutRoll, float mood);
uint32_t BuyCap(PriceRange const&, BuyNeed);                       // 0.50 gear and bag, 0.40 materials, 0.35 consumables
bool     WillBuy(uint32_t unitPrice, PriceRange const&, BuyNeed, uint64_t money, uint64_t reserve, uint32_t quantity);
```

- **Implemented (2026-11-22):** `src/Utils/PricePolicy.h` (the policy), `src/Utils/BotAuctionPolicy.h` (what to list),
  `src/Manager/PriceTable.{h,cpp}` (the loader), `data/sql/db-world/2026_11_22_00_playerbots_item_price.sql` (the table;
  it lives in the **world** database, the maintainer's default, not in the playerbots database named below) and the key
  `AiPlayerbot.Auction.Enable` (default 0). The names in the sketch above changed: `WillBuy` takes the total price
  and a per-visit `Wallet`, `ListingPrice` takes a `Market`, and `BuyNeed` gained a `None` that is never bought.
- `GetPriceRange` is a lookup in a table loaded once at startup. A sorted array of 18,892 records of 16 bytes
  (item, min, max, class) is about **295 KiB**.
- Source of the data: a module table `playerbots_item_price (item, price_class, min_price, max_price)` in the
  playerbots database, created and filled through the module's SQL updater (design decision 0028), generated by
  `tools/economy/export_price_table.py` (429 KB of SQL, 97 KB gzipped). Prices are unsigned 32-bit; the generator leaves out the 33 items whose maximum is
  above the 10,000 gold trading cap. The generator is deterministic and rerun after any change to the constants.
  The module reads the table with the core's existing playerbots database connection; a missing row means "no trade".
- `ListingPrice` and `BuyCap` are the Python of `tools/economy/listing_policy.py` ported; `WillBuy` reuses
  `BotSalePolicy::PurchaseReserve` and the 25 percent share.
- Inputs from the live game that the table cannot hold and the auction code must supply: the listings of the same item
  on this house (cheapest unit price and count), the day's mood, and the bot's need.
- Wiring: the AH errand in `ErrandPolicy.h` already sends bots to the auctioneer; a new `AuctionPolicy.h` decides what to
  list (items in the bag outside `BotSalePolicy`'s protected sets and over the 60-per-item reserve, replaced by the
  recipe-aware commitment list) and what to buy. Deposits and cuts are the core's (5 percent cut, 5 percent deposit
  rate at faction houses, 15 and 25 at the neutral one); the model needs no change for them.

Exclusions, all applied by the tools: items named `zz`, `deprecated`, `[PH]`, `test`, `unused`; bind to account; and
(in `export_price_table.py`) items above the trading cap of 10,000 gold at the maximum: 33 items (the lowest has a maximum of
11,000 gold). Portable Hole 51809 (3,300 to 5,000 gold, a bag that 636 bots carry)
is below the cap and stays in the table (corrected in section 13).

## 9. Known gaps and risks (ordered)

1. **Nothing is calibrated against the live game.** The multipliers are reasoned from how the real game's markets
   behave (disenchant value for greens, materials at several times vendor value) and checked only for internal
   consistency. The Wowhead list in section 10 is the calibration set; expect the constants in `price_model.py` to move
   by tens of percent, not the structure.
2. **3,292 items have no source in the database** (cut content or loot not yet imported). They are priced generically
   and no bot can loot them.
3. **Crafted reagent chains use the minimum price of each reagent**, including reagents that are bind on pickup or
   crafted from other crafts; the cost of Pandaria epics (Snow Lily Britches) is therefore a model number. Chains are
   walked to any depth with a cycle guard.
4. **Enchanting, Archaeology and Skinning/Herbalism entries are not "crafted goods".** Enchanting formulae create no
   item; Archaeology items (143) have no reagents and no price class of their own.
5. **Supply ignores spawn counts and kill rates.** A creature that is killed constantly counts like a rare spawn. The
   scarcity factor is capped at 1.8 so the error is bounded.
6. **Stacks and charges** (BuyCount 5 to 200) are handled as unit prices; item stack limits are the core's.
7. **Upgrade and reforge systems of 5.4** (`ItemUpgrade`, `ScalingStatDistribution`) are ignored: one price per item entry.
8. **A bot cannot judge usability** from this table: whether an item suits its class, spec and armor type is the job of
   the existing item-usage values, not the price model.
9. **Currency-priced goods** (8,624 token-only items) are not priced by their token cost. Only the 192 tradable ones get
   the `vendor_token` class.
10. **Duplicate names** (for example two Windwool Bandages, 72985 and 82829) are separate entries and priced separately.

## 10. Facts for the lead to confirm on Wowhead MoP Classic (design decision 0010)

I had no browser. Confirm these ids, in this order; a wrong anchor here moves many prices:

1. **72092 Ghost Iron Ore, 72094 Black Trillium Ore, 72238 Golden Lotus, 72988 Windwool Cloth:** SellPrice and typical
   auction value in 5.4. The database has BuyPrice only 1.15 times SellPrice for them; check that SellPrice is right
   (Ghost Iron Ore 15 silver, Black Trillium Ore 3 silver 50, Golden Lotus 1 gold) and how rare each node is.
2. **74249 Spirit Dust and 74248 Sha Crystal:** the database has no price on Spirit Dust and 12 gold on Sha Crystal
   (Enchanting and Jewelcrafting materials). Confirm their sell prices.
3. **76084 Flask of Spring Blossoms, 76090 Potion of the Mountains:** vendor and typical value of Pandaria
   consumables (BuyPrice 1 gold, SellPrice 25 silver).
4. **98604 Snow Lily Britches, 86380 Pattern: Imperial Silk Gloves, 82447 Imperial Silk:** whether each is bind on equip
   (the database says BoE, BoE, BoP) and their source (recipe from vendor, drop or crafted).
5. **87695 Treads of Edward the Odd, 31318 Singing Crystal Axe, 4090 Mug O' Hurt:** whether they are BoE open-world
   drops and which creatures drop them (the model treats them as common world drops with a low rate).
6. **22829 Super Healing Potion, 33447 Runic Healing Potion, 1710 Greater Healing Potion:** sold by vendors in MoP
   Classic, and at which price (they set the floor for all potions). Also **2447 Peacebloom, 2770 Copper Ore, 4306 Silk
   Cloth:** are they sold by an NPC at all (the database lists vendors, Peacebloom with a stock of 1).
7. **51809 Portable Hole:** vendor price (database: 3,000 gold) and why 636 are in bots' bags.
8. **24205 Design: Rigid Star of Elune, 15731 Pattern: Runic Leather Gauntlets:** drop or vendor sources and rarity
   (the model uses drop with scarcity f).
9. **12555 Battlechaser's Greaves, 37891 Cast Iron Shackles, 16712 Shadowcraft Gloves:** confirm the BoE blues of
   dungeons, elites and chests exist as listed.
10. **Auction house cut and deposit** in 5.4 (5 and 15 percent) match the core's `AuctionHouse.dbc`.

## 11. Owner questions (recommended option first)

1. **Anchor for non-vendor goods.** (a) Price from SellPrice multiples, as proposed (recommended); (b) price from
   BuyPrice multiples, which makes common gear about 4.5 times dearer. With (a) a level 60 green is 4 to 10 gold;
   with (b) 18 to 45.
2. **Vendor ceiling.** (a) Vendor items may list up to 1.5 times the floor, 3 times when the NPC stock is limited
   (recommended); (b) cap at 1.25 times, which keeps bots from ever making real profit on NPC goods.
3. **Pandaria gear and crafted epics at thousands of gold.** (a) Keep the data-driven prices; bots simply do not buy
   them and humans can (recommended); (b) cut epics by half so a rich bot can sometimes buy.
4. **Dungeon and raid items.** Only about 330 are tradable. (a) Accept a small market (recommended); (b) later make
   Timewalking or heroic BoE drops tradable so the rule has more to price.
5. **Reserve.** (a) Reuse `BotSalePolicy::PurchaseReserve` (5 copper x level squared, at most 1 gold per 10 levels;
   recommended); (b) add a mount fund (for example 20 gold from level 20) so the auction house never delays the first
   mount.
6. **Daily market mood** of up to 10 percent of each range. (a) On (recommended); (b) off, fully static ranges.
7. **Trading cap.** Bots ignore items above 10,000 gold at the maximum (a, recommended) or trade everything.
8. **Where the table lives.** (a) A module SQL table through the updater (recommended); (b) a generated header, which
   needs a rebuild per price change.
9. **Calibration.** (a) After the maintainer's Wowhead checks, let me retune the constants once within plus or minus 40
   percent without asking again (recommended); (b) every change goes back to the owner.
10. **Neutral auction houses** (15 percent cut) are used by bots only on their own faction's houses (a, recommended) or
    all three.

## 12. Reproduce

```sh
tools/economy/README.md   # commands, data checks
python3 -I -m unittest discover -s tools/economy/tests -v   # 16 tests
```

## 13. Retune 2026-11-22

design decision 0034 point 9 lets the economy design move the multipliers once, within plus or minus 40 percent, after the
lead's Wowhead MoP Classic checks. **Nothing is changed.** `price_model.py` and the generated table are exactly as
audited; rerunning `price_model.py` and `export_price_table.py` on the same world data gives a byte-identical
`price.sql` (18,892 rows), the three data checks in `tools/economy/README.md` return 0, and the 16 tool tests pass.

What the Wowhead results say, item by item (ours = `world.item_template` and the model's range, in the table the
module loads):

| Item | Wowhead MoP Classic | Ours | Verdict |
|------|---------------------|------|---------|
| 2770 Copper Ore | sells 5c | SellPrice 5c | anchor confirmed |
| 1710 Greater Healing Potion | sells 1s 25c | SellPrice 1s 25c; vendor floor 5s 50c | anchor confirmed |
| 2589 Linen Cloth | sells 13c | SellPrice 13c; range 74c to 1s 80c | anchor confirmed |
| 76084 Flask of Spring Blossoms | sells 25s | SellPrice 25s; range 28g to 43g from the reagents | anchor confirmed |
| 51809 Portable Hole | sells 25g, a vendor sells it | SellPrice 25g, BuyPrice 3,000g | real data; range 3,300g to 5,000g, see the correction below |
| 72092 Ghost Iron Ore, 72988 Windwool Cloth, 72094 Black Trillium Ore | 15s, 17s 50c, 3s 50c | the same SellPrice | anchors confirmed; Pandaria materials are sold for tokens, so their BuyPrice says nothing and SellPrice is the right anchor |
| 72238 Golden Lotus | 1 copper | SellPrice 1g | Wowhead is the outlier (MoP Classic rescale); keep ours |
| 31318, 98604, 4090 (gear) | 15g 41s 60c, 45g 63s 57c, 2g 59s 21c | 12g 33s 28c, 36g 50s 86c, 2g 7s 36c | exactly 1.25 times ours on all three; a constant rescale by MoP Classic, not a data error; keep the 5.4.8 data (design decision 0034) |
| 74249 Spirit Dust, 74248 Sha Crystal | no sell price | no SellPrice; peer anchor (Spirit Dust 71c to 1g 10s, Sha Crystal 190g to 320g) | nothing to compare; both stay on the model's craft and peer rules |
| 82447 Imperial Silk | bind on pickup | bind on pickup | matches; correctly not in the table |
| 16712 Shadowcraft Gloves | dropped by Shadow Hunter Vosh'gajin at about 15 percent | `dungeon_boss`, 21 percent in `creature_loot_template`; range 20g to 49g | class confirmed; the rate gap is a loot table question (content-db), see below |

Why no multiplier moves:

1. Every comparable SellPrice matches, so the anchor (SellPrice, not BuyPrice) is right for the 1,872 Pandaria items
   whose BuyPrice is only 1.15 times SellPrice.
2. The only differences (the 1.25 factor on gear and the Golden Lotus copper) are the ones design decision 0034 names as "not a
   reason to change our sell prices". Wowhead showed no auction value, only vendor values, so it cannot say the market
   multipliers (2 to 5 times SellPrice for green, 6 to 15 for blue, 4 to 12 for gathered goods) are too high or too
   low.
3. The Shadowcraft Gloves drop rate (21 against about 15 percent) would move that one price by (21/15)^0.25, about
   +7 percent through the scarcity factor, inside the model's own noise and far below the 40 percent allowance. The
   rate is a loot-table difference to check with the content team, not a constant to tune. If the table is corrected,
   the generator picks the new rate up on its next run.
4. A retune against a source that shows no market prices would be a guess. The right calibration is the live market:
   the first soak with the switch on gives real listing and sale counts, and a later change goes through a new SQL file
   (applied files are immutable, design decision 0028) with a new section here.

One correction to section 8: the generator leaves out items whose **maximum** is above the cap, and the Portable
Hole's maximum is 5,000 gold, so it stays in the table (range 3,300g to 5,000g); section 8 wrongly listed it among the
exclusions. Bots carry 636 of them as bags. They would list one only when the range minimum beats the vendor value
after deposit and cut, and almost never buy one (the buy cap is 0.50 of the range above its minimum at best, and a
bot's budget is a quarter of its gold above the reserve).
