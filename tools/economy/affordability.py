#!/usr/bin/env python3
"""Affordability check: can bots (and low level players) pay the proposed price ranges?

usage: affordability.py DB_FILE BOT_MONEY_TSV

BOT_MONEY_TSV has one line per character: level, money in copper, 1 for a random bot and 0 otherwise (aggregates are
printed only; no names). Budget rule from the owner: a bot spends at most 25 percent of its gold per visit above a
reserve, so budget = 0.25 * max(0, money - reserve) with the reserve of the module's BotSalePolicy::PurchaseReserve (5 copper per level squared, never above one gold per
10 levels).
"""
import sqlite3
import sys
from collections import defaultdict

RESERVE_COPPER_PER_LEVEL_SQUARED = 5     # mirrors BotSalePolicy::PurchaseReserve in src/Utils/BotSalePolicy.h
COPPER_PER_GOLD = 10000
LEVELS_PER_RESERVE_GOLD = 10
SPEND_SHARE = 0.25
PERCENTILES = (0.25, 0.50, 0.75, 0.90)
BANDS = list(range(0, 100, 10))
SETS = {
    'gear green/blue (class 2, 4)': "i.class IN (2, 4) AND i.quality IN (2, 3)",
    'consumables (class 0)': "i.class = 0",
    'materials (class 7)': "i.class = 7",
}


def percentile(values, share):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(share * len(ordered)))] if ordered else 0


def budget(money, level):
    reserve = min(RESERVE_COPPER_PER_LEVEL_SQUARED * level * level, level // LEVELS_PER_RESERVE_GOLD * COPPER_PER_GOLD)
    return SPEND_SHARE * max(0, money - reserve)


def main():
    con = sqlite3.connect(sys.argv[1])
    money = defaultdict(list)
    for line in open(sys.argv[2]):
        level, copper, is_bot = (int(x) for x in line.split())
        if is_bot:
            money[min(level // 10 * 10, 90)].append((level, copper))
    print('| level band | bots | gold p25 / p50 / p75 / p90 | budget p50 (copper) | ' +
          ' | '.join(SETS) + ' (share of level-appropriate listings under the p50 / p75 budget) |')
    rows_median = []
    for band in BANDS:
        if band not in money:
            continue
        rows = money[band]
        gold = [m for _, m in rows]
        budgets = {s: percentile([budget(m, l) for l, m in rows], s) for s in PERCENTILES}
        cells, medians = [], []
        for name, where in SETS.items():
            low, high = max(band, 1), band + 9
            query = ('SELECT p.lo, p.hi FROM item_price p JOIN item_template i ON i.entry = p.item WHERE {} AND '
                     'CASE WHEN i.class = 7 THEN i.ilvl ELSE MAX(i.reqlevel, 0) END BETWEEN ? AND ?').format(where)
            if name.startswith('consum'):
                query = query.replace('MAX(i.reqlevel, 0)', 'CASE WHEN i.reqlevel > 0 THEN i.reqlevel ELSE i.ilvl END')
            found = list(con.execute(query, (low, high)))
            lows = [r[0] for r in found]
            medians.append('{:.2f} / {:.2f}'.format(percentile(lows, 0.5) / 10000.0, percentile([r[1] for r in found], 0.5) / 10000.0))
            if not lows:
                cells.append('n/a')
                continue
            share50 = sum(1 for v in lows if v <= budgets[0.5]) / len(lows)
            share75 = sum(1 for v in lows if v <= budgets[0.75]) / len(lows)
            cells.append('{:.0f}% / {:.0f}% (n={})'.format(100 * share50, 100 * share75, len(lows)))
        rows_median.append('| {} | {} |'.format(band, ' | '.join(medians)))
        print('| {} | {} | {:.1f} / {:.1f} / {:.1f} / {:.1f} | {} | {} |'.format(
            band, len(rows), *[percentile(gold, s) / 10000.0 for s in PERCENTILES], int(budgets[0.5]), ' | '.join(cells)))
    print()
    print('| level band | ' + ' | '.join(SETS) + ' (median min / median max, gold) |')
    print('\n'.join(rows_median))


if __name__ == '__main__':
    main()

