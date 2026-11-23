#!/usr/bin/env python3
"""Reference rules for where inside a price range a bot lists, and how high it will buy.

This is the Python model of the logic the audit proposes for a C++ policy header (pure functions, no game core);
tests/test_economy_tools.py checks its invariants. Prices are copper per unit.
"""

# Where in the (min, max) range a new listing lands, as (start, width) of the fraction of the range, by how many
# listings of the same item the auction house already shows.
POSITION_EMPTY_MARKET = (0.35, 0.45)     # nobody sells it: upper middle of the range
POSITION_THIN_MARKET = (0.15, 0.50)
POSITION_CROWDED_MARKET = (0.00, 0.25)   # sellers compete: bottom quarter
THIN_MARKET_LISTINGS, CROWDED_MARKET_LISTINGS = 1, 3
UNDERCUT_MIN, UNDERCUT_SPAN = 0.01, 0.04  # undercut the cheapest listing by 1 to 5 percent
DAILY_MOOD_SPAN = 0.10                    # the whole range shifts by up to +-10 percent of its width per day and class

# Highest fraction of the range a bot pays, by what it needs the item for.
BUY_CAP = {'gear_upgrade': 0.50, 'bag': 0.50, 'profession_material': 0.40, 'consumable': 0.35}


def listing_price(low, high, cheapest, listings, roll, undercut_roll, mood=0.0):
    """Unit price of a new listing; rolls are uniform numbers in [0, 1). Never below `low`, never above `high`."""
    start, width = POSITION_EMPTY_MARKET
    if listings >= CROWDED_MARKET_LISTINGS:
        start, width = POSITION_CROWDED_MARKET
    elif listings >= THIN_MARKET_LISTINGS:
        start, width = POSITION_THIN_MARKET
    position = min(1.0, max(0.0, start + width * roll + mood * DAILY_MOOD_SPAN))
    price = low + position * (high - low)
    if cheapest is not None and cheapest > low:
        price = min(price, cheapest * (1.0 - UNDERCUT_MIN - UNDERCUT_SPAN * undercut_roll))
    return int(min(high, max(low, price)))


def buy_cap(low, high, need):
    """Highest unit price a bot pays for an item it needs; 0 when the need is not one of the known kinds."""
    share = BUY_CAP.get(need)
    return int(low + share * (high - low)) if share is not None else 0


def will_buy(unit_price, low, high, need, money, reserve, spend_share=0.25, quantity=1):
    """A bot buys when the price is inside its cap and the purchase fits 25 percent of the gold above the reserve."""
    if unit_price <= 0 or unit_price > buy_cap(low, high, need):
        return False
    budget = spend_share * max(0, money - reserve)
    return unit_price * quantity <= budget
