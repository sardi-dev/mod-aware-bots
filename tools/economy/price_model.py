#!/usr/bin/env python3
"""Reference implementation of the proposed price model: a (min, max) price range in copper for every listable item.

usage: price_model.py DB_FILE        (DB_FILE after classify.py)

Writes the table `item_price` (item, price_class, quality_group, anchor, sell, lo, hi, supply, scarcity, reagent_cost).
The constants below are the proposal; the audit document explains each one. Every number is derived from the item row
(BuyPrice, SellPrice, Quality), its source class and drop supply (classify.py), and for crafted goods the recipe
reagents (export_recipes.py), so the same function can be ported to C++ over a generated table.
"""
import math
import sqlite3
import sys
from collections import defaultdict

VENDOR_MARKUP = 1.10                 # owner rule: floor = vendor price + 10 percent
VENDOR_SPREAD = 1.50                 # a vendor item may sell up to 50 percent over its floor
VENDOR_LIMITED_SPREAD = 3.00         # vendor stock that sells out (maxcount > 0): up to three times its floor
SELL_FLOOR = 1.10                    # nobody lists below what the vendor pays for it, plus margin for the cut
NOTIONAL_BUY_PER_SELL = 4.0          # BuyPrice is 4 to 5 times SellPrice for gear; used when BuyPrice is missing
MIN_BUY_PER_SELL = 2.0               # a BuyPrice under this multiple of SellPrice is not a notional value (MoP materials)
FALLBACK_LEVEL_BAND = 5              # items with no price at all take the median anchor of peers within this item level
FALLBACK_CLASSES = (0, 2, 3, 4, 7, 9, 16)   # keys, gifts and miscellaneous junk with no price stay unpriced
NO_FALLBACK_PRIMARY = ('other', 'quest_reward')   # and so does anything with no real source
FALLBACK_SKILL_BAND = 50             # recipes with no price: peers within this required skill rank
SCARCITY_KINDS = ('world_normal', 'world_elite', 'world_rare', 'world_chest', 'dungeon_trash', 'dungeon_boss',
                  'raid_trash', 'raid_boss', 'gathered', 'recipe')
EXCLUDED_NAME_MARKERS = ('zz', 'deprecated', 'test ', ' test', '[ph]', 'unused', ' old', 'old ', '(old')
CRAFT_MARGIN_LO, CRAFT_MARGIN_HI = 1.25, 2.00   # over the reagent cost
DEMAND_REAGENT_RECIPES = 5           # a material used by this many recipes or more gets a demand bump
DEMAND_BUMP = 1.2

QUALITY_GROUPS = {0: 'grey', 1: 'white', 2: 'green', 3: 'blue', 4: 'epic', 5: 'epic', 6: 'epic', 7: 'epic'}
GROUP_ORDER = ('grey', 'white', 'green', 'blue', 'epic')

# (lo, hi) multipliers on the item's SellPrice (what a vendor pays for it), per quality group. Markets pay a
# multiple of the vendor value that grows with quality (disenchant and gear value), so gear is anchored on SellPrice;
# BuyPrice is only the floor of vendor-sold goods and the anchor of recipes.
BASE_RANGES = {
    'grey':  (1.1, 1.6),
    'white': (1.5, 3.0),
    'green': (2.0, 5.0),
    'blue':  (6.0, 15.0),
    'epic':  (20.0, 60.0),
}
GATHERED_RANGE = {'grey': (3.0, 8.0), 'white': (5.0, 12.0), 'green': (5.0, 12.0), 'blue': (6.0, 15.0), 'epic': (10.0, 30.0)}
# Crafted goods: the reagent cost sets the price; this range on SellPrice is only the floor for cheap recipes.
CRAFTED_RANGE = {'grey': (2.0, 4.0), 'white': (3.0, 8.0), 'green': (4.0, 10.0), 'blue': (10.0, 25.0), 'epic': (30.0, 90.0)}
# Recipes are anchored on BuyPrice (or the peer median when it is 0): multiples of the notional vendor price.
RECIPE_DROP_RANGE = {'grey': (2.0, 4.0), 'white': (2.0, 4.0), 'green': (3.0, 6.0), 'blue': (5.0, 10.0), 'epic': (8.0, 20.0)}
RECIPE_VENDOR_SPREAD = 2.5
# Multiplier on the base range for where the item drops (applied to both ends).
SOURCE_FACTOR = {
    'world_normal': 1.0, 'world_elite': 1.25, 'world_rare': 1.5, 'world_chest': 1.5,
    'dungeon_trash': 1.5, 'dungeon_boss': 2.0, 'raid_trash': 2.5, 'raid_boss': 3.5,
    'container': 1.0, 'quest_reward': 1.0, 'other': 1.0, 'vendor_token': 1.5,
}
# Scarcity: items rarer than the typical item of their class and quality cost more, common ones a little less.
# supply = expected copies of the item per kill of every creature that drops it (sum of drop chances);
# factor = (median supply of the class and quality / item supply) ^ SCARCITY_EXPONENT, clamped.
SCARCITY_EXPONENT = 0.25
SCARCITY_MIN, SCARCITY_MAX = 0.85, 1.80
MIN_SUPPLY = 1e-4
CLASS_TRADE_GOODS, CLASS_RECIPE, SUBCLASS_BOOK = 7, 9, 0   # class 9 subclass 0: class spell books, not profession recipes
DROP_KINDS = ('world_normal', 'world_elite', 'world_rare', 'world_chest', 'dungeon_trash', 'dungeon_boss',
              'raid_trash', 'raid_boss', 'dungeon_chest', 'raid_chest', 'container', 'pickpocket')


def round_price(value, up=False):
    """Round to two significant digits so listings look like players' prices (never below 1 copper).

    The minimum of a range rounds up, so it never drops under a floor it was derived from.
    """
    rounding = math.ceil if up else round
    if value < 100:
        return max(1, int(rounding(value)))
    digits = int(math.floor(math.log10(value))) - 1
    step = 10 ** digits
    return int(rounding(value / step)) * step


def scarcity(supply, typical):
    """Price factor of an item with this drop supply, against the median supply of its class and quality."""
    if typical <= 0:
        return 1.0
    factor = (typical / max(supply, MIN_SUPPLY)) ** SCARCITY_EXPONENT
    return min(SCARCITY_MAX, max(SCARCITY_MIN, factor))


def main():
    con = sqlite3.connect(sys.argv[1])
    items = {}
    for row in con.execute('SELECT i.entry, i.name, i.class, i.subclass, i.quality, i.buyprice, i.buycount, i.sellprice, '
                           'i.reqlevel, i.ilvl, i.reqskillrank, c.listable, c.primary_class, c.vendor_gold, c.vendor_token '
                           'FROM item_template i JOIN item_class c ON c.item = i.entry'):
        keys = ('entry name cls sub quality buy count sell reqlevel ilvl reqskillrank listable primary vendor token').split()
        items[row[0]] = dict(zip(keys, row))
    # Limited stock: every gold vendor of the item sells a capped number (maxcount > 0), so it can sell out everywhere.
    limited = {r[0] for r in con.execute(
        'SELECT item FROM npc_vendor WHERE type = 1 AND extcost = 0 GROUP BY item HAVING MIN(maxcount) > 0')}
    supply = defaultdict(float)
    for item, kind, chance in con.execute('SELECT item, kind, chance FROM item_source'):
        supply[(item, kind)] += chance / 100.0
    pool_kinds = {
        'gathered': ('skinning', 'gather_node', 'fishing', 'prospecting', 'milling', 'disenchant'),
        'dungeon_boss': ('dungeon_boss', 'dungeon_chest', 'journal_dungeon'),
        'raid_boss': ('raid_boss', 'raid_chest', 'journal_raid'),
        'container': ('container', 'spell_loot', 'mail', 'pickpocket'),
        'world_chest': ('world_chest',),
    }
    recipe_of = {}
    for item, spell, count in con.execute('SELECT item, spell, MAX(count) FROM recipes GROUP BY item, spell'):
        recipe_of.setdefault(item, []).append((spell, count))
    reagents = defaultdict(list)
    for spell, item, count in con.execute('SELECT spell, item, count FROM reagents'):
        reagents[spell].append((item, count))
    uses = defaultdict(int)
    for spell, item, count in con.execute('SELECT DISTINCT spell, item, count FROM reagents'):
        uses[item] += 1

    def raw_anchor(item):
        unit = item['buy'] / max(1, item['count'])
        if item['sell'] > 0:
            return unit if unit >= MIN_BUY_PER_SELL * item['sell'] else NOTIONAL_BUY_PER_SELL * item['sell']
        return unit

    peers = defaultdict(list)
    for item in items.values():
        value = raw_anchor(item)
        if value > 0 and item['listable']:
            band = item['reqskillrank'] // FALLBACK_SKILL_BAND if item['cls'] == CLASS_RECIPE else item['ilvl'] // FALLBACK_LEVEL_BAND
            peers[(item['cls'], item['quality'], band)].append(value)

    def anchor(item):
        value = raw_anchor(item)
        if value > 0:
            return value
        if item['cls'] not in FALLBACK_CLASSES or item['primary'] in NO_FALLBACK_PRIMARY:
            return 0.0
        band = item['reqskillrank'] // FALLBACK_SKILL_BAND if item['cls'] == CLASS_RECIPE else item['ilvl'] // FALLBACK_LEVEL_BAND
        pool = peers.get((item['cls'], item['quality'], band)) or peers.get((item['cls'], item['quality'], band - 1)) \
            or peers.get((item['cls'], item['quality'], band + 1)) or []
        return sorted(pool)[len(pool) // 2] if pool else 0.0

    def sell_anchor(item):
        if item['sell'] > 0:
            return float(item['sell'])
        return anchor(item) / NOTIONAL_BUY_PER_SELL

    memo, in_progress = {}, set()

    def reagent_price(entry):
        """Minimum of the model range for any item (listable or not): what a crafter pays for a reagent."""
        if entry in memo:
            return memo[entry]
        item = items.get(entry)
        if item is None:
            return 0.0
        if entry in in_progress:
            return sell_anchor(item)
        in_progress.add(entry)
        lo, hi, *_ = price_range(item)
        in_progress.discard(entry)
        memo[entry] = lo
        return memo[entry]

    def reagent_cost(entry):
        best = None
        for spell, count in recipe_of.get(entry, []):
            parts = reagents.get(spell)
            if not parts:
                continue
            cost = sum(reagent_price(r) * n for r, n in parts) / max(1, count)
            best = cost if best is None else min(best, cost)
        return best

    def source_profile(item):
        """(price class, lo multiplier, hi multiplier, drop supply) of an item no gold vendor sells."""
        group = QUALITY_GROUPS.get(item['quality'], 'epic')
        primary = item['primary']
        entry = item['entry']
        if item['cls'] == CLASS_RECIPE and item['sub'] != SUBCLASS_BOOK:
            kind, (lo_m, hi_m), kinds = 'recipe', RECIPE_DROP_RANGE[group], DROP_KINDS
        elif primary == 'crafted':
            kind, (lo_m, hi_m), kinds = 'crafted', CRAFTED_RANGE[group], ('craft',)
        elif primary == 'gathered':
            kind, (lo_m, hi_m), kinds = 'gathered', GATHERED_RANGE[group], pool_kinds['gathered']
        else:
            kind = primary if primary in SOURCE_FACTOR else 'other'
            if item['token'] and primary == 'other':
                kind = 'vendor_token'
            lo_m, hi_m = BASE_RANGES[group]
            kinds = pool_kinds.get(kind, (kind,))
        return kind, lo_m, hi_m, sum(supply.get((entry, k), 0.0) for k in kinds)

    supplies = defaultdict(list)
    for item in items.values():
        if item['listable'] and not item['vendor']:
            kind, _, _, sup = source_profile(item)
            if sup > 0:
                supplies[(kind, QUALITY_GROUPS.get(item['quality'], 'epic'))].append(sup)
    typical_supply = {key: sorted(v)[len(v) // 2] for key, v in supplies.items()}

    def price_range(item):
        entry = item['entry']
        group = QUALITY_GROUPS.get(item['quality'], 'epic')
        base = anchor(item) if item['cls'] == CLASS_RECIPE and item['sub'] != SUBCLASS_BOOK else sell_anchor(item)
        floor_sell = SELL_FLOOR * item['sell']
        cost = None
        if item['vendor']:
            unit = item['buy'] / max(1, item['count'])
            lo = VENDOR_MARKUP * unit
            spread = VENDOR_LIMITED_SPREAD if entry in limited else VENDOR_SPREAD
            if item['cls'] == CLASS_RECIPE and item['sub'] != SUBCLASS_BOOK:
                spread = RECIPE_VENDOR_SPREAD
            hi = lo * spread
            kind = 'vendor'
            factor = 1.0
        else:
            kind, lo_m, hi_m, sup = source_profile(item)
            # Junk is worth what a vendor pays for it, wherever it dropped: no source or scarcity premium on grey.
            factor = 1.0 if group == 'grey' else SOURCE_FACTOR.get(kind, 1.0) * (
                scarcity(sup, typical_supply.get((kind, group), 0.0)) if kind in SCARCITY_KINDS else 1.0)
            if item['cls'] == CLASS_TRADE_GOODS and uses.get(entry, 0) >= DEMAND_REAGENT_RECIPES:
                factor *= DEMAND_BUMP
            lo, hi = base * lo_m * factor, base * hi_m * factor
            if kind == 'crafted':
                cost = reagent_cost(entry)
                if cost:
                    lo, hi = max(lo, CRAFT_MARGIN_LO * cost), max(hi, CRAFT_MARGIN_HI * cost)
        lo = max(lo, floor_sell)
        hi = max(hi, lo * 1.2)
        return lo, hi, kind, group, base, cost, factor

    con.execute('DROP TABLE IF EXISTS item_price')
    con.execute('CREATE TABLE item_price (item INTEGER PRIMARY KEY, price_class TEXT, quality_group TEXT, anchor REAL, '
                'sell INTEGER, lo INTEGER, hi INTEGER, factor REAL, reagent_cost REAL)')
    rows = []
    for entry, item in items.items():
        name = ' ' + item['name'].lower()
        if not item['listable'] or any(marker in name for marker in EXCLUDED_NAME_MARKERS) or anchor(item) <= 0:
            continue
        lo, hi, kind, group, base, cost, factor = price_range(item)
        low_price = math.ceil(lo - 1e-9) if kind == 'vendor' else round_price(lo, up=True)   # vendor floor stays exact
        rows.append((entry, kind, group, base, item['sell'], low_price, max(round_price(hi), low_price), factor, cost))
    con.executemany('INSERT INTO item_price VALUES (?,?,?,?,?,?,?,?,?)', rows)
    con.commit()
    print('priced', len(rows), 'items')


if __name__ == '__main__':
    main()
