#!/usr/bin/env python3
"""How much of the bots' gold the reserve fund keeps back, by level (the Python reference of src/Utils/ReservePolicy.h).

usage: reserve_impact.py BOT_MONEY_TSV [--levels 20 40 60 85 90] [--spread 5]

BOT_MONEY_TSV is the file affordability.py reads: level, copper, 1 for a random bot (aggregates only are printed). For
each level it takes the random bots within --spread levels, computes the fund for each at its own level and prints the
median gold, the fund and the share kept back with the share cap (50 percent of the gold above the repair reserve) and
without it (the owner's literal rule), and the median purchase budget per visit before and after. Two scenarios per level
for the riding steps: "on schedule" (the bot bought every step whose level is below its own) and, from level 65, "stuck"
(it bought only up to Expert Riding and the licence, because the later steps cost 5,000 gold each).
"""
import argparse
import re
import sys
from pathlib import Path

MODULE = Path(__file__).resolve().parents[2]
COPPER_PER_GOLD = 10000
RESERVE_COPPER_PER_LEVEL_SQUARED = 5   # BotSalePolicy::PurchaseReserve
LEVELS_PER_RESERVE_GOLD = 10
LOOKAHEAD = 5
SHARE_PERCENT = 50
VISIT_SHARE = 0.25
MAX_LEVEL = 90


def load_table(path=MODULE / 'src' / 'Utils' / 'ReserveTable.h'):
    text = Path(path).read_text()
    riding = [tuple(int(v) for v in m) for m in re.findall(r'^    \{ (\d+), (\d+), (\d+) \},  // (?:Apprentice|Journeyman|Expert|Flight|Artisan|Master)', text, re.M)]
    ranks = [tuple(int(v) for v in m) for m in re.findall(r'^    \{ (\d+), (\d+) \},  // (?:Apprentice|Journeyman|Expert|Artisan|Master|Grand|Illustrious)', text, re.M)]
    mount = int(re.search(r'MOUNT_PRICE = (\d+);', text).group(1)), int(re.search(r'MOUNT_LEVEL = (\d+);', text).group(1))
    return {'riding': riding, 'ranks': ranks, 'mount': mount}


def repair_reserve(level):
    return min(level * level * RESERVE_COPPER_PER_LEVEL_SQUARED, level // LEVELS_PER_RESERVE_GOLD * COPPER_PER_GOLD)


def next_riding(table, riding_skill, licence):
    for skill, cost, level in table['riding']:
        if not (riding_skill >= skill if skill else licence):
            return cost, level
    return None


def goals(table, level, riding_skill, licence, knows_mount, professions):
    total = 0
    step = next_riding(table, riding_skill, licence)
    if step and step[1] <= level + LOOKAHEAD:
        total += step[0]
    if not knows_mount and table['mount'][1] <= level + LOOKAHEAD:
        total += table['mount'][0]
    total += sum(cost for lvl, cost in table['ranks'] if level < lvl <= level + LOOKAHEAD) * min(professions, 4)
    return total


def on_schedule(table, level):
    """Riding skill, licence and mount of a bot that bought every step whose level is below its own."""
    skill, licence = 0, False
    for given, _, needs in table['riding']:
        if needs < level:
            if given:
                skill = given
            else:
                licence = True
    return skill, licence, level > table['mount'][1]


def stuck_at_expert(table, level):
    """A bot that stopped after Expert Riding and the licence (it cannot pay 5,000 gold)."""
    skill, licence, mount = on_schedule(table, min(level, 61))
    return skill, licence, True


def reserve(money, level, goal_total, share_percent):
    repair = repair_reserve(level)
    spare = max(0, money - repair)
    held = min(goal_total, spare * share_percent // 100)
    return repair, held


def budget(money, total_reserve):
    return max(0, money - total_reserve) * VISIT_SHARE


def median(values):
    ordered = sorted(values)
    return ordered[len(ordered) // 2] if ordered else 0


def impact(table, bots, level, spread, scenario, professions=2):
    """The gold of the random bots within `spread` levels, against the fund of a bot at exactly `level`."""
    golds = [money for lvl, money in bots if abs(lvl - level) <= spread]
    if not golds:
        return None
    state = scenario(table, level)
    total = goals(table, level, state[0], state[1], state[2], professions)
    repair = repair_reserve(level)
    kept50, kept100, old_budget, new50, new100, share50, share100 = [], [], [], [], [], [], []
    for money in golds:
        held50 = reserve(money, level, total, SHARE_PERCENT)[1]
        held100 = reserve(money, level, total, 100)[1]
        kept50.append(held50)
        kept100.append(held100)
        old_budget.append(budget(money, repair))
        new50.append(budget(money, repair + held50))
        new100.append(budget(money, repair + held100))
        if money:
            share50.append(min(1.0, (repair + held50) / money))
            share100.append(min(1.0, (repair + held100) / money))
    return {'n': len(golds), 'gold': median(golds), 'goal': total, 'repair': repair, 'kept50': median(kept50),
            'kept100': median(kept100), 'old': median(old_budget), 'new50': median(new50), 'new100': median(new100),
            'share50': median(share50), 'share100': median(share100)}


def read_bots(path):
    bots = []
    for line in open(path):
        level, copper, is_bot = (int(x) for x in line.split())
        if is_bot:
            bots.append((level, copper))
    return bots


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('bot_money')
    parser.add_argument('--levels', type=int, nargs='*', default=[20, 40, 60, 85, 90])
    parser.add_argument('--spread', type=int, default=5)
    args = parser.parse_args()
    table, bots = load_table(), read_bots(args.bot_money)
    print('| level | scenario | bots | median gold | repair + fund (gold) | share of the median bot\'s gold reserved, cap 50% / no cap '
          '| purchase budget per visit now / cap 50% / no cap (gold) |')
    print('|---:|---|---:|---:|---:|---|---|')
    for level in args.levels:
        scenarios = [('on schedule', on_schedule)]
        if level >= 65:
            scenarios.append(('stuck at Expert + licence', stuck_at_expert))
        for name, scenario in scenarios:
            r = impact(table, bots, level, args.spread, scenario)
            if r is None:
                print('| {} | {} | 0 | no bots near | {:.2f} + {:.1f} | | |'.format(
                    level, name, repair_reserve(level) / COPPER_PER_GOLD, goals(table, level, *scenario(table, level), 2) / COPPER_PER_GOLD))
                continue
            print('| {} | {} | {} | {:.2f} | {:.2f} + {:.1f} | {:.0f}% / {:.0f}% | {:.2f} / {:.2f} / {:.2f} |'.format(
                level, name, r['n'], r['gold'] / COPPER_PER_GOLD, r['repair'] / COPPER_PER_GOLD, r['goal'] / COPPER_PER_GOLD,
                100 * r['share50'], 100 * r['share100'], r['old'] / COPPER_PER_GOLD, r['new50'] / COPPER_PER_GOLD,
                r['new100'] / COPPER_PER_GOLD))


if __name__ == '__main__':
    sys.exit(main())
