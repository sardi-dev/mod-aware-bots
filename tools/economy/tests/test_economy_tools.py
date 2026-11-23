import re
import sqlite3
import sys
import unittest
from pathlib import Path

MODULE = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import classify
import export_price_table
import export_reserve_table
import listing_policy
import price_model
import reserve_impact


def table(*rows):
    """Rows as (entry, item, chance, groupid, minref, maxcount) grouped like classify.group_rows does."""
    return classify.group_rows(rows)


class ExpandLootTest(unittest.TestCase):
    def test_plain_rows_keep_their_chance(self):
        loot = table((1, 100, 25.0, 0, 1, 1), (1, 101, 100.0, 0, 1, 1))
        self.assertEqual(classify.expand_loot(1, (loot, {})), {100: 25.0, 101: 100.0})

    def test_quest_only_rows_are_skipped(self):
        loot = table((1, 100, -50.0, 0, 1, 1))
        self.assertEqual(classify.expand_loot(1, (loot, {})), {})

    def test_zero_chance_group_rows_share_what_is_left(self):
        loot = table((1, 100, 40.0, 1, 1, 1), (1, 101, 0.0, 1, 1, 1), (1, 102, 0.0, 1, 1, 1))
        result = classify.expand_loot(1, (loot, {}))
        self.assertAlmostEqual(result[100], 40.0)
        self.assertAlmostEqual(result[101], 30.0)
        self.assertAlmostEqual(result[102], 30.0)

    def test_reference_rows_pull_in_the_reference_table(self):
        loot = table((1, 0, 50.0, 0, -9, 1))
        references = table((9, 200, 10.0, 0, 1, 1))
        self.assertAlmostEqual(classify.expand_loot(1, (loot, references))[200], 5.0)
        self.assertEqual(classify.expand_loot(1, (loot, references), 0, {}).keys(), {200})

    def test_reference_loops_end(self):
        loot = table((1, 0, 100.0, 0, -1, 1))
        classify.expand_loot(1, (loot, loot))


class CategoryTest(unittest.TestCase):
    def test_instance_and_world_classes(self):
        self.assertEqual(classify.creature_category(1, classify.MAP_RAID, True), 'raid_boss')
        self.assertEqual(classify.creature_category(1, classify.MAP_RAID, False), 'raid_trash')
        self.assertEqual(classify.creature_category(1, classify.MAP_DUNGEON, True), 'dungeon_boss')
        self.assertEqual(classify.creature_category(0, classify.MAP_DUNGEON, False), 'dungeon_trash')
        self.assertEqual(classify.creature_category(4, 0, False), 'world_rare')
        self.assertEqual(classify.creature_category(1, 0, False), 'world_elite')
        self.assertEqual(classify.creature_category(0, 0, False), 'world_normal')

    def test_primary_class_prefers_the_easier_source(self):
        self.assertEqual(classify.primary_class({'raid_boss', 'world_normal'}), 'world_normal')
        self.assertEqual(classify.primary_class({'crafted', 'vendor'}), 'vendor')
        self.assertEqual(classify.primary_class(set()), 'other')


class PriceHelpersTest(unittest.TestCase):
    def test_round_price_keeps_two_significant_digits(self):
        self.assertEqual(price_model.round_price(1234567), 1200000)
        self.assertEqual(price_model.round_price(0.2), 1)
        self.assertEqual(price_model.round_price(99.6), 100)

    def test_the_minimum_rounds_up_so_a_floor_holds(self):
        floor = 1.10 * 54321
        self.assertGreaterEqual(price_model.round_price(floor, up=True), floor)

    def test_scarcity_grows_as_supply_shrinks_and_is_clamped(self):
        factors = [price_model.scarcity(s, 1.0) for s in (100.0, 2.0, 1.0, 0.1, 0.0)]
        self.assertEqual(factors, sorted(factors))
        self.assertEqual(price_model.scarcity(1.0, 1.0), 1.0)
        self.assertEqual(factors[0], price_model.SCARCITY_MIN)
        self.assertEqual(factors[-1], price_model.SCARCITY_MAX)
        self.assertEqual(price_model.scarcity(0.5, 0.0), 1.0)

    def test_multiplier_tables_cover_every_quality_group(self):
        for table_name in ('BASE_RANGES', 'GATHERED_RANGE', 'CRAFTED_RANGE', 'RECIPE_DROP_RANGE'):
            ranges = getattr(price_model, table_name)
            self.assertEqual(set(ranges), set(price_model.GROUP_ORDER))
            for low, high in ranges.values():
                self.assertLess(low, high)


class ListingPolicyTest(unittest.TestCase):
    LOW, HIGH = 1000, 3000

    def test_a_listing_stays_inside_the_range(self):
        for listings in (0, 1, 5):
            for roll in (0.0, 0.5, 0.999):
                for mood in (-1.0, 0.0, 1.0):
                    price = listing_policy.listing_price(self.LOW, self.HIGH, 900, listings, roll, roll, mood)
                    self.assertGreaterEqual(price, self.LOW)
                    self.assertLessEqual(price, self.HIGH)

    def test_a_crowded_market_lists_lower_than_an_empty_one(self):
        empty = listing_policy.listing_price(self.LOW, self.HIGH, None, 0, 0.5, 0.0)
        crowded = listing_policy.listing_price(self.LOW, self.HIGH, None, 4, 0.5, 0.0)
        self.assertLess(crowded, empty)

    def test_undercut_goes_below_the_cheapest_listing_but_not_below_the_minimum(self):
        self.assertLess(listing_policy.listing_price(self.LOW, self.HIGH, 2000, 1, 0.99, 0.0), 2000)
        self.assertEqual(listing_policy.listing_price(self.LOW, self.HIGH, 1005, 5, 0.0, 1.0), self.LOW)

    def test_buy_cap_is_inside_the_range_and_ordered_by_need(self):
        gear = listing_policy.buy_cap(self.LOW, self.HIGH, 'gear_upgrade')
        food = listing_policy.buy_cap(self.LOW, self.HIGH, 'consumable')
        self.assertTrue(self.LOW <= food < gear <= self.HIGH)
        self.assertEqual(listing_policy.buy_cap(self.LOW, self.HIGH, 'unknown'), 0)

    def test_a_purchase_respects_the_range_and_the_gold_rule(self):
        args = dict(low=self.LOW, high=self.HIGH, need='consumable', reserve=1000)
        self.assertTrue(listing_policy.will_buy(1500, money=10000, **args))
        self.assertFalse(listing_policy.will_buy(2500, money=1000000, **args))   # above the cap
        self.assertFalse(listing_policy.will_buy(1500, money=1500, **args))      # nothing above the reserve
        self.assertFalse(listing_policy.will_buy(1500, money=5000, **args))      # 25 percent of 4000 is 1000


def camel_to_snake(name):
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()


class PriceTableExportTest(unittest.TestCase):
    def test_cap_and_bad_rows_are_left_out_and_items_are_sorted(self):
        con = sqlite3.connect(':memory:')
        con.execute('CREATE TABLE item_price (item INTEGER, price_class TEXT, lo INTEGER, hi INTEGER)')
        con.executemany('INSERT INTO item_price VALUES (?,?,?,?)', [
            (30, 'gathered', 100, 200), (10, 'vendor', 5, 5), (20, 'raid_boss', 1, export_price_table.TRADE_CAP_COPPER),
            (40, 'other', 1, export_price_table.TRADE_CAP_COPPER + 1), (50, 'vendor', 0, 9), (60, 'vendor', 9, 3)])
        rows = export_price_table.select_rows(con)
        self.assertEqual([r[0] for r in rows], [10, 20, 30])
        text = export_price_table.render(rows)
        self.assertIn('(10,1,5,5),\n(20,12,1,100000000),\n(30,2,100,200);', text)
        self.assertEqual(text, export_price_table.render(rows))

    def test_large_tables_are_split_into_several_inserts(self):
        rows = [(i, 'vendor', 1, 2) for i in range(1, export_price_table.ROWS_PER_INSERT * 2 + 2)]
        text = export_price_table.render(rows)
        self.assertEqual(text.count('INSERT INTO'), 3)

    def test_class_ids_match_the_cpp_enum(self):
        header = (MODULE / 'src' / 'Utils' / 'PricePolicy.h').read_text()
        body = re.search(r'enum class PriceClass : uint8_t\s*\{(.*?)\};', header, re.S).group(1)
        names = [camel_to_snake(n.strip()) for n in re.sub(r'=\s*\d+', '', body).split(',') if n.strip()]
        expected = {name: index for index, name in enumerate(names, start=1)}
        self.assertEqual(expected, export_price_table.PRICE_CLASS_IDS)

    def test_the_committed_sql_files_hold_valid_sorted_rows(self):
        files = sorted((MODULE / 'data' / 'sql' / 'db-world').glob('*playerbots_item_price*.sql'))
        self.assertTrue(files)
        for path in files:
            items = []
            for item, cls, low, high in re.findall(r'^\((\d+),(\d+),(\d+),(\d+)\)[,;]$', path.read_text(), re.M):
                item, cls, low, high = int(item), int(cls), int(low), int(high)
                self.assertTrue(1 <= cls <= 16 and 0 < low <= high <= export_price_table.TRADE_CAP_COPPER, (path, item))
                items.append(item)
            self.assertEqual(items, sorted(set(items)), path)
            self.assertGreater(len(items), 1000)


class ReserveTableExportTest(unittest.TestCase):
    NAMES = {1: 'Apprentice Riding', 2: 'Journeyman Riding', 3: 'Expert Riding', 4: "Flight Master's License",
             5: 'Apprentice Alchemist', 6: 'Journeyman Alchemist', 7: 'Apprentice Blacksmith', 8: 'Apprentice Cook',
             9: 'Frostbolt', 10: 'Master Riding', 11: 'Artisan Riding', 12: 'Journeyman Herbalist'}

    def test_a_class_uses_the_template_most_of_its_trainers_reference(self):
        refs = [(8, 200008, 82), (8, 200200, 7), (8, 200202, 13), (2, 200100, 6), (2, 200002, 42), (6, 200006, 8)]
        self.assertEqual(export_reserve_table.class_templates(refs), {8: 200008, 2: 200002, 6: 200006})

    def test_a_tie_takes_the_lower_template(self):
        self.assertEqual(export_reserve_table.class_templates([(1, 200300, 5), (1, 200001, 5)]), {1: 200001})

    def test_class_costs_are_cumulative_by_level(self):
        rows = [(200008, 9, 100, 0, 0, 5), (200008, 10, 250, 0, 0, 5), (200008, 11, 1000, 0, 0, 12),
                (200008, 12, 40, 0, 0, 0), (200001, 9, 7, 0, 0, 3), (200200, 9, 999, 0, 0, 3)]
        table = export_reserve_table.class_cost_table({8: 200008, 1: 200001}, rows)
        self.assertEqual(len(table[8]), export_reserve_table.MAX_LEVEL + 1)
        self.assertEqual((table[8][0], table[8][1], table[8][4], table[8][5], table[8][11], table[8][12], table[8][90]),
                         (0, 40, 40, 390, 390, 1390, 1390))   # the spell with level 0 counts from level 1
        self.assertEqual(table[1][2], 0)
        self.assertEqual(table[1][3], 7)
        self.assertEqual(table[8], sorted(table[8]))

    def test_levels_above_the_cap_count_at_the_cap(self):
        table = export_reserve_table.class_cost_table({8: 200008}, [(200008, 9, 500, 0, 0, 200)])
        self.assertEqual(table[8][export_reserve_table.MAX_LEVEL], 500)
        self.assertEqual(table[8][export_reserve_table.MAX_LEVEL - 1], 0)

    def test_profession_ranks_keep_the_crafting_ones_and_the_latest_level(self):
        rows = [(200400, 5, 10, 0, 0, 5), (201010, 5, 10, 171, 0, 5), (200400, 6, 500, 171, 50, 10),
                (200401, 7, 10, 0, 0, 5), (200305, 8, 100, 0, 0, 0), (200404, 12, 500, 182, 50, 0), (200400, 9, 5, 0, 0, 1)]
        ranks = export_reserve_table.profession_ranks(rows, self.NAMES)
        self.assertEqual(ranks, [(5, 10, 'Apprentice'), (10, 500, 'Journeyman')])   # cooking and herbalism are not crafting

    def test_riding_steps_are_in_buying_order_with_the_cheapest_price(self):
        rows = [(200301, 3, 2500000, 762, 150, 60), (200300, 1, 40000, 0, 0, 20), (200301, 1, 50000, 0, 0, 20),
                (200301, 2, 500000, 762, 75, 40), (200301, 4, 2500000, 762, 225, 60), (200301, 10, 50000000, 762, 300, 80),
                (200301, 11, 50000000, 762, 225, 70), (200302, 9, 100, 0, 0, 0), (200302, 1, 0, 0, 0, 20)]
        steps = export_reserve_table.riding_steps(rows, self.NAMES)
        self.assertEqual([(s[0], s[1], s[2], s[3]) for s in steps],
                         [(75, 40000, 20, 'Apprentice'), (150, 500000, 40, 'Journeyman'), (225, 2500000, 60, 'Expert'),
                          (0, 2500000, 60, "Flight Master's License"), (300, 50000000, 70, 'Artisan'),
                          (375, 50000000, 80, 'Master')])

    def test_the_header_is_deterministic_and_balanced(self):
        table = export_reserve_table.class_cost_table({8: 200008}, [(200008, 9, 500, 0, 0, 7)])
        text = export_reserve_table.render(table, [(5, 10, 'Apprentice')], [(75, 40000, 20, 'Apprentice')], (10000, 20))
        self.assertEqual(text, export_reserve_table.render(table, [(5, 10, 'Apprentice')], [(75, 40000, 20, 'Apprentice')], (10000, 20)))
        self.assertEqual(text.count('{'), text.count('}'))
        self.assertIn('{ 75, 40000, 20 },  // Apprentice', text)
        self.assertIn('constexpr uint32_t MOUNT_PRICE = 10000;', text)
        self.assertEqual(text.count('{  // class '), export_reserve_table.CLASS_SLOTS)

    def test_the_committed_header_matches_the_riding_ladder_and_the_cpp_constants(self):
        text = (MODULE / 'src' / 'Utils' / 'ReserveTable.h').read_text()
        steps = re.findall(r'^    \{ (\d+), (\d+), (\d+) \},  // (.+)$', text, re.M)
        self.assertEqual([s[3] for s in steps], list(export_reserve_table.RIDING_ORDER))
        for skill, _, _, name in steps:
            self.assertEqual(int(skill), export_reserve_table.RIDING_SKILL.get(name, 0))
        self.assertIn('MAX_LEVEL = {};'.format(export_reserve_table.MAX_LEVEL), text)
        self.assertIn('CLASS_SLOTS = {};'.format(export_reserve_table.CLASS_SLOTS), text)
        classes = re.findall(r'\{  // class (\d+)\n(.*?)\n    \},', text, re.S)
        self.assertEqual(len(classes), export_reserve_table.CLASS_SLOTS)
        for _, body in classes:
            values = [int(v) for v in re.findall(r'\d+', body)]
            self.assertEqual(len(values), export_reserve_table.MAX_LEVEL + 1)
            self.assertEqual(values, sorted(values))


class ReserveImpactTest(unittest.TestCase):
    """The Python reference must give the numbers tests/test_reserve_policy.cpp checks on the C++ side."""

    def setUp(self):
        self.table = reserve_impact.load_table()

    def test_the_table_is_read_from_the_committed_header(self):
        self.assertEqual(len(self.table['riding']), 6)
        self.assertEqual(self.table['ranks'][3], (35, 50000))
        self.assertEqual(self.table['mount'], (10000, 20))

    def test_goals_match_the_cpp_tests(self):
        self.assertEqual(reserve_impact.goals(self.table, 14, 0, False, False, 0), 0)
        self.assertEqual(reserve_impact.goals(self.table, 15, 0, False, False, 0), 50000)
        self.assertEqual(reserve_impact.goals(self.table, 25, 75, False, False, 0), 10000)
        self.assertEqual(reserve_impact.goals(self.table, 35, 75, False, True, 0), 500000)
        self.assertEqual(reserve_impact.goals(self.table, 60, 225, False, True, 0), 2500000)
        self.assertEqual(reserve_impact.goals(self.table, 65, 225, True, True, 0), 50000000)
        self.assertEqual(reserve_impact.goals(self.table, 90, 375, True, True, 0), 0)
        self.assertEqual(reserve_impact.goals(self.table, 31, 75, False, True, 2), 100000)

    def test_the_share_cap_matches_the_cpp_tests(self):
        self.assertEqual(reserve_impact.reserve(100000, 20, 50000, 50), (2000, 49000))
        self.assertEqual(reserve_impact.reserve(100000, 20, 50000, 100), (2000, 50000))
        self.assertEqual(reserve_impact.reserve(30000, 20, 50000, 100), (2000, 28000))
        self.assertEqual(reserve_impact.reserve(1500, 20, 50000, 50), (2000, 0))

    def test_the_scenarios_follow_the_ladder(self):
        self.assertEqual(reserve_impact.on_schedule(self.table, 20), (0, False, False))
        self.assertEqual(reserve_impact.on_schedule(self.table, 21), (75, False, True))
        self.assertEqual(reserve_impact.on_schedule(self.table, 61), (225, True, True))

    def test_impact_of_a_bot_that_holds_less_than_its_goal(self):
        result = reserve_impact.impact(self.table, [(20, 100000)], 20, 0, reserve_impact.on_schedule)
        self.assertEqual((result['goal'], result['kept50'], result['kept100']), (50000, 49000, 50000))
        self.assertGreater(result['old'], result['new50'])
        self.assertGreater(result['new50'], result['new100'])
        self.assertEqual(result['new100'], (100000 - 52000) * 0.25)


if __name__ == '__main__':
    unittest.main()
