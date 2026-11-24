# Wiring the auction house errand

A plan, not code. It says how the existing AH errand (`ErrandPolicy::Errand::Auction`, BT-253) will use the pure policies
and the core helpers, step by step, so the wiring is thin and every decision stays in tested headers. Design decision 0033 and
0034 and the owner's answers of 2026-11-22 are the rules; `pricing-audit.md` and `reserve-fund.md` are the numbers.

Everything here sits behind `AiPlayerbot.Auction.Enable` (default 0). Random bots only. Own faction's house only
(`AllowTwoSide.Interaction.Auction = 0` on the live server, so the auctioneer's own house is the bot's faction house).

## 1. What exists

Module (this branch and `main`):

| Piece | Where | Does |
|-------|-------|------|
| Price range, listing price, buy cap, wallet, deposit, cut, duration, `ResolveTerms` | `src/Utils/PricePolicy.h` | the money rules, checked arithmetic |
| Sell selection | `src/Utils/BotAuctionPolicy.h` | which whole stacks to list, with the vendor trip's protections |
| Reserve and the visit's money | `src/Utils/ReservePolicy.h`, `ReserveTable.h` | repair reserve plus the training, riding and mount fund; `VisitMoney` |
| Price table | `src/Manager/PriceTable.{h,cpp}` | loads `playerbots_item_price` once at start (18,892 rows) |
| Config | `src/Utils/PlayerbotAIConfig.*`, `conf/playerbots.conf.dist` | `Auction.Enable`, `LongListingMinGold`, `ReserveLookaheadLevels`, `ReserveGoalSharePercent`, `ReserveRecipeSharePercent` |
| The errand | `src/Utils/ErrandPolicy.h`, `NewRpgAction.cpp` (`NewRpgWanderNpcAction::Execute`, `FindErrandTargets`) | walks to the auctioneer, dwells 30 to 90 s, gestures; no work yet |

Core, as read in the core at `69425fca87` (the maintainer confirms the final signatures before the wiring starts):

```cpp
// AuctionHouseMgr.h
bool CanAuctionItem(Item* item, uint32 count);
AuctionListResult ListItem(Player* seller, Creature* auctioneer, uint64 itemGuid, uint32 count, uint64 bid,
                           uint64 buyout, uint32 hours, AuctionListOutcome& outcome);   // outcome: auctionId, deposit
AuctionBidResult  PlaceBid(Player* bidder, AuctionHouseObject* auctionHouse, uint32 auctionId, uint64 price,
                           AuctionBidOutcome& outcome);                                  // outcome: paid, boughtOut, auction (copy)
void AuctionHouseObject::GetListings(std::vector<AuctionListing>& out, uint32 afterId = 0, uint32 limit = 0) const;
// Player.h
MailResponseResult TakeMailMoney(uint32 mailId, uint64 money, uint32& equipError);
MailResponseResult TakeMailItem(uint32 mailId, uint32 itemGuidLow, uint32& equipError, uint32& itemCount);
bool DeleteEmptyMail(uint32 mailId);
// ScriptMgr.h: AuctionHouseScript::OnAuctionAdd / OnAuctionRemove / OnAuctionSuccessful / OnAuctionExpire
```

`ListItem` lists whole stacks only, for 12, 24 or 48 hours, takes the deposit and saves it with the auction. `PlaceBid`
bids or buys out (a price at or above the buyout is a buyout) and may free the auction. Neither sends a packet. All of them
must run on the world thread (comment in `AuctionHouseMgr.h`).

## 2. Threads: the rule that shapes the design

Bot AI runs on the map threads; the auction manager, `Player` inventory writes and the mail helpers are world thread
only. The module already has the pattern: `ArenaLadder::OnUpdate(diff)` runs from the world update script
(`mod_playerbots.cpp`), at the point of `World::Update` where the map threads have finished, so it may touch bots
directly (`Arena/ArenaLadder.h` header comment).

So:

- The errand code on the map thread never calls the core helpers. On arrival it posts one `VisitRequest {botGuid,
  auctioneerGuid, seed}` to a lock-protected queue in `BotAuction` and from then on only reads the bot's visit state (an
  atomic: `None`, `Queued`, `Working`, `Done`).
- `BotAuction::OnUpdate(diff)` (new line next to `ArenaLadder::OnUpdate`) runs on the world thread: takes requests,
  and for each active visit does at most one core operation per bot per slice, with a pause of 2 to 5 s between the
  operations so the visit looks like a person at the counter. Each slice starts by checking that the bot is still online,
  in the same map, alive, and within interaction distance of the auctioneer (`Player::GetNPCIfCanInteractWith`
  semantics); otherwise the visit ends with "left".
- The visit ends at the latest when the errand's dwell ends (`DwellMs`); the map thread then drops the request.

## 3. Files and their size

| Change | Size | Notes |
|--------|------|-------|
| `src/strategy/actions/BotAuction.{h,cpp}` (new) | about 450 lines | the visit executor, the item facts, the snapshot, the mail collector; thin glue around the headers |
| `src/Utils/AuctionVisitPolicy.h` (new, pure, tested) | about 150 lines, 15 tests | visit state machine: order of phases, spacing, per-visit caps, cooldown, timeout |
| `src/Utils/AuctionMailPolicy.h` (new, pure, tested) | about 100 lines, 10 tests | parses `itemEntry:0:answer:auctionId:count`, decides what to take |
| `src/Manager/AuctionStats.{h,cpp}` (new) | about 250 lines | the weekly statistics table and the `AuctionHouseScript` |
| `BotSale.cpp` | about 40 lines moved | the per-item facts of `SelectItemsToSell` become a function both the vendor trip and the auction use (`BotSale::FactsOf`), so the protections cannot drift apart |
| `NewRpgAction.cpp` | about 30 lines | arrival hook, per-tick poll, mailbox hook |
| `mod_playerbots.cpp` | 2 lines | `BotAuction::OnUpdate(diff)` |
| `PlayerbotAIConfig.*`, `playerbots.conf.dist` | about 40 lines | the keys of section 9 |
| `tests/` | about 25 tests | for the two new headers; the executor is checked by the soak |

## 4. The visit

Preconditions, checked on arrival (map thread, cheap): `auctionEnable`; random bot; not in a group with a human leader, a
dungeon or a battleground; the auctioneer's house is the bot's own (`PricePolicy::HouseAllowed`); the bot's last visit
ended at least `VisitCooldownMinutes` ago (default 60); the price table is loaded. A bot that fails any of them just
dwells at the counter as it does today.

```
arrive (map thread) -> post request
  world thread:
  1 mail        deliverable auction mail and a mailbox within 25 yd of the bot: collect it now (section 5); else skip
  2 snapshot    listings of the house, once (section 6); money and reserve on arrival
  3 sell        selection, then one listing per slice
  4 buy         selection, then one purchase per slice
  5 end         log the summary, store the stats, release the request
```

### 4.1 Mail first

The existing ordering already does most of it: `ErrandPolicy::Choose` sends a bot with unread mail to the mailbox before
any idle errand (`town.hasMail = bot->unReadMails > 0`), so a bot with sale proceeds waiting visits the mailbox first.
What is missing is that the Mailbox errand does nothing at the box today (`CheckMailAction` is registered but in no
strategy, and it skips mail whose sender is not an online player, which auction mail never is). The wiring adds
`BotAuction::CollectMail(bot)` at the Mailbox errand's arrival and, step 1 above, at the start of an AH visit when a
mailbox is near. Money on arrival for the wallet is therefore the money after collection, and mail still in transit (the
sale mail arrives after `MailDeliveryDelay`, 1 hour by default) is never counted.

### 4.2 The wallet

```cpp
ReservePolicy::Facts  f = ...;   // level, class, SKILL_RIDING pure value, knows 90267, KnowsMount, profession count,
                                 // classSpellsPaid = !AutoLearnTrainerSpells, pendingClassTrainCopper = "train cost" value
ReservePolicy::Reserve  r = ReservePolicy::ComputeReserve(f, params, bot->GetMoney());
ReservePolicy::VisitMoney vm = ReservePolicy::MakeVisitMoney(bot->GetMoney(), r);
```

Logged once per visit: `[AUCTION] <bot> L<level> visit: money <g> reserve <g> (repair <g> fund <g> of goals <g>) budget <g>`.

### 4.3 Selling

1. Facts: for every stack in the bags `BotSale::FactsOf(item)` fills `BotSalePolicy::ItemFacts`; the auction adds
   `crafted` (the bot made it), `tradable` (`AuctionHouseMgr::CanAuctionItem(item, count)`), `usageKeeps` (item usage
   value wants it), `vendorSellPrice`, and `range` from `PriceTable::Find` (`hasRange` false means "never listed").
2. `BotAuctionPolicy::SellPlan`: `rates` from the house entry (`AuctionHouseEntry::depositPercent`, `cutPercent`),
   `longListingAboveCopper = LongListingMinGold x 10000`, `maxListings = MaxListingsPerVisit` (default 4),
   `activeListings` = the bot's own listings in the snapshot, `maxActiveListings = MaxActiveListings` (default 20),
   `depositBudget = ReservePolicy::DepositLeft(vm, money)`.
3. `SelectListings(stacks, plan)` returns whole stacks, best margin first, each with its planned duration and deposit.
4. One listing per slice, re-validated on the spot: the item is looked up again by guid (it may have been used, traded
   or sold to a vendor meanwhile), `CanAuctionItem` again, then
   - the market of the item from the snapshot: `Market{ cheapest unit price among other sellers, listings }`;
   - `unit = PricePolicy::ListingPrice(range, market, roll, undercutRoll, DailyMood(day, priceClass))` with `day =
     GameTime / 86400`, `roll` and `undercutRoll` from the visit's seed;
   - `total = unit x count` (checked); `ResolveTerms(total, count, vendorSellPrice, rates, longAbove,
     DepositLeft(vm, money), terms)` settles duration and deposit and falls back from 24 to 12 hours; false skips the stack
     (logged at debug: `skip entry <e>: no terms`);
   - buyout = `total`, start bid = 95 percent of it rounded up, at least 1 copper (an assumption to confirm in the soak:
     it lets a human bid, and a bot only ever pays the buyout);
   - `SpendDeposit(vm, money, terms.deposit)` as the check, then
     `sAuctionMgr->ListItem(bot, auctioneer, item->GetGUID(), count, bid, buyout, terms.durationSeconds / 3600, outcome)`;
     afterwards the real deposit (`outcome.deposit`, which also carries `Rate.Auction.Deposit`) replaces the planned one in
     the wallet (`wallet.committed += max(0, outcome.deposit - terms.deposit)`). A startup warning is logged when a
     `Rate.Auction.*` is not 1, because the policy's deposit and cut assume the default rates.
5. Result handling: see section 7.

Expired listings come back by mail as the item; the mail collector puts it in the bags and a later visit lists it again with
a new roll. A bot never cancels a listing and never lists the same stack twice.

### 4.4 Buying

1. Needs, from existing systems, never new ones: `GearUpgrade` from the item usage value (equip upgrade for the bot's
   spec), `Bag` when it holds an empty-slot bag smaller than the offer, `Consumable` when food, drink or potions are
   low (`BotSalePolicy::SupplyLow` and the bot's own item usage), `ProfessionMaterial` for a crafting profession of
   its own (up to 60 held) or a secondary skill (up to 20), never for a profession it lacks or a material it only
   gathers (`MaterialWanted`, the levels of section 4.5).
2. For each snapshot listing that is not the bot's own and has a buyout: range from the table, `need`, then
   `PricePolicy::WillBuy(buyout, itemCount, range, need, vm.wallet, money, vm.reserve)`. The same rule for a bot's and a
   human's listing: the owner is read only to skip the bot's own. Candidates sorted by how far under their cap they are;
   at most `MaxPurchasesPerVisit` (default 3).
3. One purchase per slice: `CanBuy(vm, money, buyout)` again, the listing re-fetched by id from the house
   (`GetAuctionsMap(faction)->GetAuction(id)`; never a stored pointer), then
   `sAuctionMgr->PlaceBid(bot, house, id, buyout, outcome)`. On success `SpendPurchase` records `outcome.paid`.
4. Phase 1 buys out only. Bidding (the first bid at the minimum, outbid mail, bids that lock gold) is phase 2 after the
   soak, as it adds an outbid-refund path to test.

What the bot buys arrives by mail (`SendAuctionWonMail`), so the item is in the bags after the next mailbox visit.

## 5. Collecting auction mail

`BotAuction::CollectMail(bot)`, world thread, at most 10 mails per call, for mails with `messageType == MAIL_AUCTION` that
`IsDelivered()`:

| Subject answer (`itemEntry:0:answer:auctionId:count`) | Attachment | Action |
|---|---|---|
| 2 successful | money (bid + deposit - cut) | `TakeMailMoney(id, m->money, err)`; counted as a sale (`gold in`) |
| 3 expired | the item | `TakeMailItem`; a stack that no longer fits waits |
| 1 won | the item | `TakeMailItem` |
| 0 outbid | the bid back | `TakeMailMoney` (phase 2) |
| 4, 5 cancelled | money or item | as above |

Rules: a failed take (`MAIL_ERR_EQUIP_ERROR` with bags full, `EQUIP_ERR_TOO_MUCH_GOLD`) leaves the mail untouched and sets
a "bags full" flag that the vendor trip already reacts to (`BotSalePolicy::BagsAlmostFull`); a mail is deleted only with
`Player::DeleteEmptyMail`, which refuses a mail that still holds money, items or COD, so nothing is lost by a bug. Mail
from players is left to the existing mail code; auction mail is never routed through `CheckMailAction`, which would mail
the item back to a sender that does not exist.

## 6. The snapshot and the re-fetch

- One snapshot per visit, on the world thread, step 2: `house->GetListings(listings)` with `afterId = 0, limit = 0`. It
  copies scalars (id, entry, count, bids, buyout, owner, bidder, expiry), so nothing points into the manager. A house with
  a few thousand listings costs microseconds; the visit cooldown and the world-thread slice (one operation per bot per
  tick) keep the total small. A hard cap of 5,000 listings per snapshot logs `[AUCTION] snapshot truncated` rather than
  stalling a tick.
- From it: the bot's own active listings, the market per item being listed (`cheapestUnit` of others' listings divided by
  count, rounded up, and the number of listings), and the buy candidates.
- Every purchase re-fetches by auction id right before `PlaceBid`, and every result is read as truth: the snapshot is only
  a plan. `outcome.auction` is a copy, because a buyout frees the entry.

## 7. Failure handling

| Event | Action |
|-------|--------|
| `ListItem`: `ItemNotFound`, `InvalidCount`, `ItemNotAllowed`, `InvalidPrice` | skip this stack, debug line; the stack is not tried again this visit |
| `ListItem`: `NotEnoughMoney` | stop selling, warn (the policy should have prevented it: a bug or a rate change) |
| `ListItem`: `InvalidDuration`, `InvalidAuctioneer` | end the visit, error line once per bot per hour |
| `PlaceBid`: `NotFound` | someone else bought it or it expired: next candidate |
| `PlaceBid`: `InvalidPrice`, `BidTooLow` | the listing changed: re-read it once and retry if it is still under the cap, else next |
| `PlaceBid`: `OwnAuction` | bug: error line, end buying |
| `PlaceBid`: `NotEnoughMoney` | end buying, warn |
| bot left the map, died, logged out, moved away | end the visit, keep nothing half-done (each core call is one transaction) |
| world tick over budget | the executor takes at most `AuctionSlicesPerTick` operations per world tick (default 3 bots) and skips the tick if the last update exceeded the activity-scale threshold |
| table missing or empty | the loader already logs; the visit does nothing |
| bot deleted with active listings | the auction stays until it expires (the core does not mail a missing character); the bot erase tool should list this, no helper deletes auctions |

Never a retry loop inside a tick, never a blanket mail or auction delete. After three failed operations in a visit the visit
ends.

## 8. Logging

All lines start with `[AUCTION]` and one bot name, grep-friendly. INFO is one summary line per visit plus errors; per
operation lines are DEBUG, and INFO for the bots in `AiPlayerbot.DebugBotNames`.

```
[AUCTION] <bot> L<n> visit: money <g> reserve <g> (repair <g> fund <g> of goals <g>) budget <g>
[AUCTION] <bot> list entry <e> x<count> unit <c> total <c> min <c> max <c> house <id> hours <h> deposit <c> (listing <id>)
[AUCTION] <bot> buy  entry <e> x<count> paid <c> cap <c> need <gear|bag|material|consumable> from <bot|human> (listing <id>)
[AUCTION] <bot> skip entry <e>: <reason>
[AUCTION] <bot> mail <answer> entry <e> x<count> money <c> item <ok|full|error>
[AUCTION] <bot> visit done: listed <n> bought <n> deposits <c> spent <c> mail <c> in <ms> ms
[AUCTION] sale auction <id> owner <bot|human> bidder <bot|human> bid <c> deposit <c> cut <c>      (AuctionHouseScript)
[AUCTION] expire auction <id> owner <bot|human> entry <e> x<count> deposit lost <c>
```

Names, never guids alone, and no player-supplied text. The `[AUCTION] config` line already exists (`PlayerbotAIConfig.cpp`).

## 9. Config keys (all `AiPlayerbot.Auction.*`)

Exist: `Enable`, `LongListingMinGold` (5), `ReserveLookaheadLevels` (5), `ReserveGoalSharePercent` (75), `ReserveRecipeSharePercent` (10).
To add with the wiring: `MaxListingsPerVisit` (4), `MaxActiveListings` (20), `MaxPurchasesPerVisit` (3),
`VisitCooldownMinutes` (60), `SlicesPerTick` (3), `OnlyBots` (comma list of names; empty means every random bot; the soak
uses it), `Stats.Enable` (1). The 25 percent per visit stays a constant (`PricePolicy::VISIT_SPEND_PERCENT`), as the owner
set it; it is not a key.

## 10. Metrics for the training loop

Two sources, both exactly-once because both run on the world thread:

1. The executor itself: listings (count, value at buyout, deposits) and purchases (count, paid, seller type) per bot.
2. `AuctionHouseScript::OnAuctionSuccessful(ah, entry)` and `OnAuctionExpire`: sales and expirations with gross, cut and
   deposit, who owned and who won (bot or human), regardless of which code path made the auction. A human buying a bot's
   listing is exactly "owner is a random bot and bidder is a human".

Stored in the playerbots database in `playerbots_auction_stats`, created at start like the liking tables
(`LikingStore::EnsureTablesLocked` is the precedent), one row per (bot guid, week start):

```
listings, listed_copper, deposits_paid, sales, sales_gross, cuts_paid, expired, deposits_lost,
bought, bought_paid, bought_from_human, human_buys, human_buys_copper, mail_gold_in
```

The sale mail pays `bid + deposit - cut`, so the deposit paid at listing comes back with it. Per bot: gold out =
`deposits_paid + bought_paid`; gold in = the money of the collected sale mail = `sales_gross + deposits_of_sold - cuts_paid`;
net gain of a sold listing = `sales_gross - cuts_paid`; an expired listing loses its deposit (`deposits_lost`). The soak's
conservation test compares these sums with the money the bots really took from their mail (`mail_gold_in`) and with the
characters table, so no gold appears or vanishes unexplained.

the maintainer's metrics collector (not published) gets one more query set (read-only, as the rest of it) that prints, for the
week: listings, sales, expiry share, median and p90 gold in and out per bot, human purchases (count, copper), the share of
listings priced in the bottom, middle and top third of their range, listings outside their range (must be 0), the active
listings of the house by price class, and the number of bots that visited. the metrics notes get one row per pass.

## 11. The soak plan

Gate: the owner's rule is that the switch stays 0 on the live server until the unit tests and an overnight soak pass.

1. **Unit tests** (done for the policies; the two new headers add about 25): the module suite (1,590 tests before this work) and the economy tool tests.
2. **Build and a dry day, daytime, no human**: `Auction.Enable = 1`, `Auction.OnlyBots` = 10 test bots of levels 20 to 60 on
   one faction, standing in a capital with a mailbox near its AH. Check the `[AUCTION]` summary lines, no error lines, the
   price table loaded, listings on the house with prices inside their ranges, deposits equal to the policy's.
3. **A human probe**: one test client character of the same faction lists three stacks at prices inside and outside the
   ranges and buys one bot listing; the bots must buy the listing that is inside the range and under the cap, ignore the
   others, and the human must receive the proceeds by mail.
4. **Overnight soak (8 hours, nobody online, heavy work allowed)**: the same 10 bots plus 40 more across two cities, every
   30 minutes the metrics collector's series mode. Pass when: no crash or `[AUCTION]` error line above the expected skips; world
   update average within 5 ms of the baseline and memory flat; zero listings outside their range; the house population
   stays between 20 and 1,000 listings and not one item type is more than 30 percent of it; sales happen (at least one per
   10 bots per night, from the bots' own purchases and the human probe); the conservation check holds (total gold in
   minus gold out of the bots equals sales minus cuts minus deposits lost, and no gold appears that no mail or sale
   explains); every bot ends above its repair reserve; no bot is stuck at the auctioneer past its dwell; no mail is left
   undeleted with money in it for more than 2 hours.
5. **Second night**: all random bots of one faction, `OnlyBots` empty. Same checks, plus the median gold of bots per level
   band against `reserve-fund.md` section 5 (no band loses more than a quarter of its median gold to deposits and cuts).
6. **Then** the owner's decision on the live switch. Rollback is the switch: bots stop at once; existing auctions run out
   and return by mail, humans' purchases of bot listings still work.

## 12. Order of work and sizes

1. `AuctionMailPolicy.h` and `AuctionVisitPolicy.h` with tests (pure, 1 day).
2. `BotSale::FactsOf` extraction with the existing sale tests unchanged (half a day).
3. `BotAuction` executor, mail collector and the errand hooks (2 days), syncheck on every changed file.
4. `AuctionStats` and the metrics queries (1 day).
5. Dry day, probe, overnight soak (a day of attended time and a night).
6. Phase 2: bids and the outbid refund path, only after the soak.

Not in this plan, by decision: any gold or items created for bots; alt bots; the neutral houses; two-sided auctions; a
price baseline other than the table; blanket auction or mail cleanup; a second reserve mechanism beside
`PurchaseReserve` and `ReservePolicy`.

## 13. As built (phase 1, branch fix/economy-auction-errand)

What was wired, and where it differs from the plan above. Everything stays behind `AiPlayerbot.Auction.Enable` (default 0).

| Plan | As built |
|------|----------|
| `AuctionVisitPolicy.h`, `AuctionMailPolicy.h` (pure) | Done, with `tests/test_auction_visit_policy.cpp` (14 tests) and `tests/test_auction_mail_policy.cpp` (8 tests). The visit policy also holds the `OnlyBots` name list, the gate that decides whether a bot may start a visit, the totals and their text. |
| Buy selection | `BotAuctionPolicy::SelectPurchases`, `MarketOf`, `QuoteListing`, `StartBidFor`, `MaterialWanted`, `ConsumableWanted`, `NeedOf` (`tests/test_bot_auction_wiring.cpp`, 19 tests). |
| `BotSale::FactsOf` extraction | Done: `FactsOf`, `UsageKeeps`, `MaterialUseOf`, `IsUsableSupply`, `SupplyHeld` and `ProfessionsOf` are public, and `SelectItemsToSell` uses them, so the vendor trip and the auction cannot drift apart. `BotShopping::KnowsMount` is public for the reserve. |
| `BotAuction.{h,cpp}` | Done in `src/strategy/actions/`: request vector under a mutex, one record per bot, `OnUpdate` on the world thread (next to the arena ladder), at most `SlicesPerTick` operations per tick over all bots, 2 to 5 seconds between the operations of one bot, a 75 second ceiling per visit. The errand (`NewRpgWanderNpcAction`) posts the request on arrival at an auctioneer or a mailbox and keeps the bot at the counter while the visit works (never longer than the dwell plus the ceiling). |
| Mailbox errand collects mail | Done: arrival at a mailbox posts a mailbox-only visit; an auction visit collects first when a mailbox is within 25 yards. Handled auction mail is marked read so the errand stops sending the bot back for it. |
| Buying | Buyout only. Needs come from the item usage value (gear and bag upgrades), the bot's own profession materials up to the keep cap of their level (60 crafting, 20 secondary skill; section 4.5), and food or drink while the bot holds fewer than the low water. `AiPlayerbot.Auction.Buy.Enable = 0` turns buying off and keeps selling and mail. |
| `AuctionStats` table and the metrics collector | Not built. The counters are in the log: one `[AUCTION] ... done` line per visit and an `[AUCTION] summary` line every `SummaryMinutes` (and at shutdown). An `AuctionHouseScript` logs every sale and expiry of a random bot's listing (`[AUCTION] sale`, `[AUCTION] expire`), which is also how a human buying a bot listing shows up. |
| Config keys | `OnlyBots`, `MaxListingsPerVisit`, `MaxActiveListings`, `MaxPurchasesPerVisit`, `VisitCooldownMinutes`, `SlicesPerTick`, `Buy.Enable`, `SummaryMinutes` (and the five that existed). |

Known limits of phase 1:

- Every action line is INFO, not DEBUG as section 8 planned, so the soak can be read without a debug build; with the whole random population this is a few lines per visit and per bot an hour. Raise the cooldown or name the bots to quiet it.
- Cooldown is per bot in memory (60 minutes after a visit); it restarts with the server. A mailbox-only visit has no cooldown.
- The reserve counts class trainer spells only when `AutoLearnTrainerSpells` is 0, from the table window; the "train cost" value of the spells learnable now is not added.
- A consumable need compares the food or drink the bot holds (of that kind, all items) with the low water of 5, and the keep cap of 40 per item.
- An expired listing comes back by mail and is listed again by a later visit with a fresh roll; the deposit of an expired listing is lost. The soak measures how often that happens (`expired` against `sales` in the summary).
- At most 40 listings per visit are read for a purchase need (the usage value is not free), in the house's id order, and only those whose price is under the highest cap of any need; the rest are skipped.

## 14. Soak runbook

Config (the maintainer sets these in the server's `playerbots.conf` only for the soak):

```
AiPlayerbot.Auction.Enable = 1
AiPlayerbot.Auction.OnlyBots = "name1,name2,..."   # random bots only; a bot that is not a random bot never visits
```

The named bots must be random bots that are logged in, of one faction, standing in a town with an auctioneer of their own
faction (the errand scans 200 yards), with gold above their repair reserve and sellable items in their bags. Everything
else keeps its default; a cooldown of 60 minutes per bot gives several visits overnight.

Read in the morning, in the worldserver log (the `playerbots` logger is at info):

| grep | Means |
|------|-------|
| `\[AUCTION\] config` | the keys the server started with, and `onlyBots=` the number of names |
| `\[AUCTION\] ready` | the price table size and the core's `Rate.Auction.*` |
| `\[AUCTION\] .* visit:` | the money, reserve and budgets of each visit |
| `\[AUCTION\] .* list .* ok` | a listing (unit price, buyout, start bid, range, hours, deposit) |
| `\[AUCTION\] .* buy .* paid` | a purchase (price, cap, need, seller) |
| `\[AUCTION\] .* mail ` | auction mail taken (sold, expired, won) |
| `\[AUCTION\] sale` and `\[AUCTION\] expire` | what happened to the listings |
| `\[AUCTION\] .* done` | one line per visit with the totals and why it ended |
| `\[AUCTION\] summary` | run counters and interval measurements |
| `refused`, `errors [1-9]`, `BID ONLY`, `snapshot truncated`, `Rate.Auction` | what must be zero or explained |

The interval field `mailBacklogMax` is the largest count seen in one visited mailbox before collection, including all
currently delivered auction mails with money or items, even when already read. It includes won items and refunds as well as
sale proceeds and returns, excludes mail in transit, and is not a total across bots. A zero can also mean no mailbox was visited.
No database query is made for this measurement.

Pass when: no `refused: code` line except for stacks that changed; no `BID ONLY`; no `errors` above 0; every listing's unit
price inside the range the line prints; deposits equal to the policy's (5 percent rate, 12 or 24 hours); every visiting
bot ends above its repair reserve; the `mail gold` of the visits equals the sum of the `sale` lines' `bid + deposit - cut`
once the 1 hour mail delay has passed; and the bots that listed also sold or expired something (sales need buyers: the
human probe of section 11).

### 4.5 Profession materials: keep levels (owner decision 2026-11-23)

The first soak listed nothing because every material fell under the vendor trip's flat keep of 60. The auction keeps a
profession material in three levels by what the bot does with it (`BotAuctionPolicy::MaterialUse`, `MaterialKeepCap`);
the vendor trip keeps its own rule (60 of anything a profession of the bot touches).

| Level | Keep per item | When |
|-------|---------------|------|
| Crafting | 60 | a recipe of one of the bot's crafting professions (Alchemy, Blacksmithing, Enchanting, Engineering, Inscription, Jewelcrafting, Leatherworking, Tailoring) takes the item and the bot can make it |
| Secondary, Gathered | 20 | only a secondary skill (Cooking, First Aid, Fishing, Archaeology) takes it, or the bot gathers that kind of material (Mining for ore and stone, Herbalism for herbs, Skinning for leather) or smelts it with Mining, and no crafting profession of its own uses it |
| None | 0 | no profession of the bot uses it: every stack is listed (a trade good of any kind, also elemental, parts and sacs the vendor trip never sells) |

"Used by" is read from data, not from the item class (the old `ProfessionUses` guessed ore for Cooking bots and cloth for
none): the skill line abilities of the client data (`SkillLineAbility.dbc`) give the recipes of each skill, their spells
give the reagents (`SpellReagents.db2`), so an item maps to the recipes that take it. A recipe counts for a bot that knows
the spell, or whose skill value has reached the lowest `reqskillvalue` any trainer teaches it at (`npc_trainer`, read from
`ObjectMgr`; a recipe a profession gives on learning counts from its minimum rank). Ore smelted into a bar that a
crafting profession works counts for that profession when the bot also has the smelting (Mining), and Prospecting and
Milling (no reagents; the core checks the item's `RequiredSkillRank` against Jewelcrafting and Inscription) are added from
the item flags. The table is built once on first use (`BotSale.cpp`, `BuildRecipeUses`, a few thousand rows).

Known limits: Disenchanting (gear for an Enchanting bot) is not a material use; a recipe above the bot's skill value that
it has not learned does not keep anything back; a profession item a bot crafted is listed like other loot it has no use for.
