#!/usr/bin/env python3
"""Print the worked examples of the audit as a Markdown table.

usage: examples.py DB_FILE [ITEM_ID ...]      (defaults to the audit's list)
"""
import sqlite3
import sys

DEFAULT_IDS = (
    2770, 2447, 2589, 14047, 13468, 72092, 72238, 14344,
    2840, 3860, 118, 9206, 13510, 76084, 37663, 76141, 12419, 41344, 44931, 98604,
    159, 1710, 22829, 4777, 6274,
    9771, 6591, 24723, 13095, 4090, 31318, 87695,
    31164, 30733,
    35640, 16712, 16828, 45468,
    15731, 24205, 86380, 41468, 78330,
)
QUALITY_NAMES = {0: 'grey', 1: 'white', 2: 'green', 3: 'blue', 4: 'epic'}
COPPER_PER_SILVER, COPPER_PER_GOLD = 100, 10000


def money(copper):
    gold, rest = divmod(int(copper), COPPER_PER_GOLD)
    silver, bronze = divmod(rest, COPPER_PER_SILVER)
    parts = []
    if gold:
        parts.append('{:,}g'.format(gold))
    if silver:
        parts.append('{}s'.format(silver))
    if bronze or not parts:
        parts.append('{}c'.format(bronze))
    return ' '.join(parts)


def main():
    con = sqlite3.connect(sys.argv[1])
    ids = [int(x) for x in sys.argv[2:]] or DEFAULT_IDS
    print('| id | item | source class | quality | req lvl | SellPrice | BuyPrice (per unit) | proposed range |')
    print('|---|---|---|---|---|---|---|---|')
    for item_id in ids:
        row = con.execute('SELECT i.entry, i.name, p.price_class, i.quality, i.reqlevel, i.ilvl, i.sellprice, i.buyprice, '
                          'i.buycount, p.lo, p.hi FROM item_price p JOIN item_template i ON i.entry = p.item '
                          'WHERE i.entry = ?', (item_id,)).fetchone()
        if not row:
            print('| {} | not priced | | | | | | |'.format(item_id))
            continue
        entry, name, kind, quality, level, ilvl, sell, buy, count, lo, hi = row
        print('| {} | {} | {} | {} | {} | {} | {} | {} to {} |'.format(
            entry, name.replace('|', '/'), kind, QUALITY_NAMES.get(quality, quality), level or 'ilvl {}'.format(ilvl),
            money(sell), money(buy / max(1, count)), money(lo), money(hi)))


if __name__ == '__main__':
    main()
