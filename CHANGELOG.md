# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- **Temple of Kotmogu leader stances and call-outs** (2026-11-24)
  - Summary: Every 20 s the side's bot leader projects both scores from the public score and picks balanced, hold,
    push or all-in with hysteresis; the side's bots shift their support and hunter split to match, and the leader
    announces a change in battleground chat. Bots call an enemy carrier they see, by orb colour, at a rate that rises
    with their tier; calls expire, and a human's colour report is read too.
  - Why: Roadmap 0.2: the approved playbook gives the leader the score race and the team shared, aging information.
- **Temple of Kotmogu plays its approved playbook, first part** (2026-11-24)
  - Summary: Bots count enemy orb carriers only when a living teammate can see them (the orb owner shown on the map
    stays public); a full side opens 4 orb takers on different orbs, 3 supporters, 2 hunters and 1 flexible, scaled for
    smaller sides, with humans counted but never given a role; a carrier holds the center only while supported and
    falls back on seen danger, returning with hysteresis; no deliberate orb drops; one bot leader per side. Leader
    stance changes and call-outs are still to come.
  - Why: Roadmap 0.2: Kotmogu ran on shared tactics and a random split and read every enemy carrier from the whole map.
- **Temple of Kotmogu playbook, draft** (2026-11-24)
  - Summary: `docs/bg-sheets/temple-of-kotmogu.md`: map facts from the core and the patches with sources, roles by side
    size, opening and mid-game rules, call-outs, class jobs, failure modes, the gap list, tests and the open questions.
    Not approved yet; no code changes.
  - Why: Roadmap 0.2 needs a playbook for every battleground before the bots' Kotmogu play is built and proven.
- **Bank errands that deposit, part 1** (2026-11-24)
  - Summary: A random bot on its bank errand now deposits whole stacks at the banker: profession materials it keeps
    for later (within the existing keep caps) and rare or better items it neither wears nor sells. Quest items, bags,
    consumables, tools, gear it could use and soulbound gear stay in its bags. At most 8 moves per visit; banked
    materials count toward the auction and vendor keep rules; one `[BANK]` line per visit and counters in the auction
    summary. Withdrawals wait for a craft goal. Off by default (`AiPlayerbot.BankErrands.*`).
  - Why: Roadmap 0.3: the bank errand was only a walk and a pause, and bots' bags filled up with things they keep.
- **Crafters buy recipes on their vendor trips** (2026-11-24)
  - Summary: A random bot with a crafting profession that stands at a vendor or repairer on its usual trip may buy one
    recipe it can learn now (skill high enough, not known, sold for gold), paid from the reserve's recipe share, then
    learns it by using the item, as a player does. At most one per visit and three per day; one `[RECIPE]` line per
    purchase and counters in the auction summary. Off by default (`AiPlayerbot.RecipeBuying.*`).
  - Why: Roadmap 0.3: crafters kept a recipe share in their reserve but never spent it, so their recipe books never grew.
- **Trade chatter that names real goods** (2026-11-24)
  - Summary: In a capital city with a human in the zone, a random bot with the trader style may name one of its own
    active auction listings with its buyout in the Trade channel, and a gatherer-crafter may name items it can craft
    (known recipes whose product is in the price table). A world-thread cache follows each bot's listings through the
    auction hooks; at most once per bot per hour and two lines per faction per minute, never in the first 5 minutes
    after login; counted in the social summary. Off by default (`AiPlayerbot.Social.TradeChatter.*`).
  - Why: Roadmap 0.3: city chat should reflect what bots really sell and make, not canned lines.
- **The last soak measurements** (2026-11-24)
  - Summary: The social summary adds the quest-offer pass cost (passes, average and peak ms, candidates scanned) and a
    playstyle census of the loaded random bots; the auction summary adds the largest mailbox backlog a visit found.
    Offer acceptance is not measured: the offer invites a human who already has the quest, so no quest push can be
    observed.
  - Why: The default-on gates need the pass cost on the world thread, the style mix and the mail backlog, not only
    event counts.
- **A diagnostic line for battleground bots that stand still** (2026-11-24)
  - Summary: With `AiPlayerbot.BgIdleLog = 1`, a battleground bot that stays within 10 yd of one spot for more than
    30 s while alive, out of combat and without a stationary duty logs `[BGIDLE]` (at most every 30 s) with its
    objective, target, last movement action and why the move failed (no objective, too far, no ground height, all hops
    rejected). Off by default.
  - Why: Bots were seen idling near their base on node maps, and the logs showed where they stood but not why.
- **The social summary counts the interval, by cause** (2026-11-24)
  - Summary: `[SOCIAL] summary (interval)` now reports each interval's liking changes up and down by cause (group
    time, a finished dungeon, a trade, a duel, guild time, thanks, insults, kicks, ninja looting), how many loaded
    pairs sit at each liking tier, quest offers made and held back by the pair cooldown or the rolling limit, guild
    offers, emote answers and queued store writes, with running totals for offers and writes. Counters are lock-free;
    the default interval is 1800 s.
  - Why: The old line held totals since the start, so a soak could not show whether the features stay active and
    inside their caps over time.
- **Random bots pick the role their level band is short of** (2026-11-24)
  - Summary: A random bot that chooses its first specialization prefers tank or healer when free bots of its faction
    in its finder level window are short of that role (target shares 25 / 20 / 55, at least four free tanks and four
    free healers per band from level 25 to 54). Existing specializations are never changed. Keys:
    `AiPlayerbot.SpecRoleBalance.Enable`, `.TankShare`, `.HealerShare`, `.DamageShare`.
  - Why: The finder often had no free tank in a band, so dungeon groups waited or never formed.
- **Auction soak measurements** (2026-11-24)
  - Summary: The interval `[AUCTION] summary` line reports auction work per world tick (average and maximum ms),
    waiting visits, timeouts, snapshots that hit their cap and the listings they left out (one warning per interval at
    most), and reconciliation totals (deposits returned, sale gold mailed and collected). Bot and human buyouts of a
    bot's listing now reach the sale counters: the core patch 0010 fires the auction script's sale hook on a buyout.
  - Why: The economy can only be switched on by default after a soak shows it stays under 100 ms per tick, never
    truncates its snapshot and loses no gold or items; without these numbers nobody could tell.
- **A log line for every vendor gear purchase** (2026-11-24)
  - Summary: `[BUYGEAR] bot=... level=... vendor=... spent=... money=...` after a random bot bought an upgrade on its
    vendor trip.
  - Why: The purchase was silent, so nobody could tell how often bots really buy and wear vendor gear.

### Changed
- **README and feature list name tonight's dungeon fixes** (2026-11-24)
  - Summary: The README feature table and `docs/FEATURES.md` list the finder's tank level rule, stalled bot runs that
    end and rest their dungeon, leftover bot groups breaking up, ledge recovery and diving to underwater targets.
  - Why: Every feature the module has is named in the README, marked Ready or In development.

### Fixed
- **Auction routing and role poll safety** (2026-11-25)
  - Summary: A bot with a due auction visit takes a higher need (repair, vendor, trainer, mail) first instead of a
    quest giver, a forced auction visit is tried again only after ten minutes, slow auction work is logged at most once
    a minute, and a bot answers a role poll only for the group that polled.
  - Why: A review before deployment found a quest-giver loop and a possible endless walk to an unreachable auctioneer,
    log lines every tick under load, and a late role answer that could land in a new group.
- **Arathi bots stand idle less** (2026-11-25)
  - Summary: A guard above its node's planned count takes its new job instead of keeping the post, and an opening
    attacker moves on to the team's next target once its capture hold has run out.
  - Why: In a 10v10 Arathi Basin the bots stood still about 9 of 24 minutes each; guard surpluses and finished
    capture holds were among the idle decisions.
- **Bots answer the group role poll, so a group with bots can queue** (2026-11-25)
  - Summary: When the core polls a group for roles before its finder join, each bot sets its own group role from its
    spec, as a player picks one in the role window.
  - Why: The core refuses a group's finder join until every member has a role and gives no join result; a group of a
    player and alt bots could never queue (seen with the level 90 test groups for Shado-Pan Monastery).
- **Healers come to a tank that fights out of their sight** (2026-11-25)
  - Summary: A healer takes its line-of-sight path to the tank when the tank is in combat, not only when the healer is;
    a healer or a resurrecting bot walks to a member or body it cannot see instead of stopping at a fixed standoff
    that could move it away again.
  - Why: In a Scarlet Halls run the tank died to six mobs while the out-of-combat healer cast nothing with full mana,
    then spent a minute circling the body before the resurrection.
- **Bots reach the auction house, and its work stays small per tick** (2026-11-25)
  - Summary: A mailbox visit no longer starts the auction cooldown; a bot whose auction visit is due heads for the
    auctioneer; the snapshot reads one listing past its cap to detect truncation instead of paging through the whole
    house; purchase needs are evaluated five per slice; slow steps and ticks are logged; social summaries fall on
    fixed interval boundaries and one is written at start-up.
  - Why: The soak showed no completed auction visit in a day (mailbox visits kept closing the auction window), an
    auction tick of 399 ms, and social summaries missing in about half the intervals.
- **A timed-out dock wait counts as a transport failure for a hand-in** (2026-11-25)
  - Summary: When a bot on a hand-in trip gives up waiting for a boat or zeppelin, the quest gets the transport
    failure retry delay, as a stalled walk to the transport already did.
  - Why: Repeated dock wait timeouts at the same dock (Outland) were retried straight away.
- **Looting and selling safety for the earned gear change** (2026-11-25)
  - Summary: The loot loop stops as soon as an item source (a lockbox, a clam, milling or prospecting) is released by
    the core after its last item; items a quest hands out to be used count as quest items; gear the bot wants is never
    junk because some other quest also lists it; a bag may be bought with the last free slot (it is worn at once);
    the dungeon release wait reads only group members in the same instance.
  - Why: A review before deployment found a possible crash (the loop read the freed loot), quest items and wanted gear
    that could be sold or destroyed, a bag purchase blocked in the case it was meant for, and reads of players on
    other maps.
- **Gilneas attackers reach the far base** (2026-11-25)
  - Summary: In the Battle for Gilneas a far objective is walked in short hops, as in Deepwind Gorge, since the map
    has no waypoint routes.
  - Why: In a 40 minute 5v5 match the waterworks attackers never left their start areas; each side kept its near
    base and the match drew at the timer.
- **Quest walks keep their progress** (2026-11-25)
  - Summary: A new trip to the same spot starts with a fresh stall clock; progress counts while a movement is still
    running and along the current movement leg (a detour can refresh the clock for at most three minutes); trips over
    2000 yards try the travel graph (boats, flights, portals) first; the stand-still watch resets on a task change;
    exploration trips follow the no-teleport rule of the other quest trips.
  - Why: About 1,070 quest walks a day ended as "stuck on the way" or "stood still", some seconds after they started
    because of the previous trip's stall state.
- **Transport walks no longer stall on a moving measuring point** (2026-11-24)
  - Summary: Walks to docks, flight masters and portals measure progress against the current route point instead of
    the look-ahead target; at a dock the timer resets when the bot switches from the land point to the dock node;
    portal and taxi walks record progress before checking the timeout; stalls log the bot, target and route entry.
  - Why: About 430 quest give-ups a day ended with "the walk to the transport stalled", many of them while the bot
    was still closing in.
- **Bots get to loot they can see** (2026-11-24)
  - Summary: After two failed walks to a visible corpse within reach, a bot tries a point just short of it; a walk
    that gets at least a yard closer clears earlier failures, and a walk that stops making progress still gives up.
  - Why: Half of the loot give-ups in a day were within 15 yards with line of sight.
- **Dead bots in a dungeon release after a fair wait** (2026-11-24)
  - Summary: In a dungeon or raid led by a human, a dead bot waits while a resurrection is plausible (a pending
    request, its own self-resurrection, or a living group member who knows a resurrection spell), at most 60 s after
    the group's fight ends and 300 s after its death (`AiPlayerbot.DungeonReleaseWaitSeconds`,
    `DungeonReleaseMaxWaitSeconds`); then it releases and makes the normal corpse run. It accepts a resurrection
    request before anything else.
  - Why: Dead bots waited forever for a resurrection whenever a human led the group in a dungeon, even with no
    healer able to bring them back.
- **Bots keep and wear the gear they earn** (2026-11-24)
  - Summary: A bot no longer skips gear it would wear, or a bag, when its bags are nearly full; with completely full
    bags it destroys its lowest-value junk to make room for such an item. On vendor trips it sells junk first (gray
    items, materials for professions it does not have, leftovers of quests it no longer has) and keeps a free slot,
    and it buys the largest bag it can afford for an empty bag slot. Gear it would wear now or within 5 levels is
    never sold or destroyed. Quest rewards pick the best item for the bot (ties: wearable, then item level, then sell
    price) instead of the first one when all score zero, and two zero-score items compare by armor or weapon damage.
  - Why: Bots near level 20 with about 240 hours played still wore their starting gear: their 16 backpack slots were
    full of materials and quest leftovers and they had no bags, so the loot rule skipped every piece of gear.

- **The flag carrier's escort stays with it** (2026-11-24)
  - Summary: In Warsong Gulch and Twin Peaks the escort and backup (and the taker after a hand-off) keep within the
    escort distance of a running carrier also in combat, using instant casts on the way; the healers the plan sends
    with the carrier follow it in combat too, and keep doing so while both flags are out; mid bots covering the
    carrier's rear may peel the nearest enemy hitting it. Other escorts still stop escorting while both flags are out,
    as before.
  - Why: In a bot-only match seven of eight carriers died 5 to 32 s after the pickup, still in the enemy base, because
    their escort fell behind once a fight started.
- **New random bot accounts are created again** (2026-11-24)
  - Summary: Random bot account passwords are 16 characters, the core's limit, instead of 20.
  - Why: The core refused every new bot account with a 20-character random password ("Cannot create random bot
    account ... (result 2)"), so raising the bot count added no bots once the existing accounts were full.
- **A flag carrier stops chasing targets** (2026-11-24)
  - Summary: In Warsong Gulch and Twin Peaks a bot carrying the enemy flag drops its offensive target, stops attacking
    and chasing, and gets no new target unless the playbook's fight-alone condition holds (exactly one attacker, no
    other enemy within the radius, no fighter of ours near, health 40 percent or more, casual 25). Its run home or its
    hiding spot is unchanged.
  - Why: A carrier kept the target it had before the pickup and chased it for 18 minutes with nobody attacking it, so
    the match ended 0 to 0.
- **The flag strike group walks through midfield fights to the enemy stand** (2026-11-24)
  - Summary: In Warsong Gulch and Twin Peaks, the taker, backup and escort whose plan is to take the enemy flag keep
    moving while in combat until they are within 30 yd of the enemy stand, as long as they are at 40 percent health or
    more (25 percent for casual bots), not rooted or controlled, and held by no more than one melee attacker; they
    still use instant casts on the way. At the stand they fight as before, until the taker has the flag.
  - Why: In a 10 against 10 bot match the strike groups met in the middle, fought there for the whole match and never
    reached a flag (0 to 0 after 25 minutes); the playbook keeps the strike group's fighting at the stand and leaves
    the middle to the mid group.
- **Arathi Basin bots count only the enemies their team can see** (2026-11-24)
  - Summary: The Arathi Basin plan counted enemies at each node from the whole roster. Like the other node maps, an
    enemy now counts only when a living teammate within 70 yd can see or detect it (stealth counts), or a call-out
    placed it there. The check is cached once per plan refresh for every node.
  - Why: Bots reacted to enemies nobody on their team could have seen, which a human team cannot do.
- **Quest retry history survives the quest log** (2026-11-24)
  - Summary: A bot's failure record for a quest is no longer wiped when it accepts the quest again or drops it, only
    when it makes progress or hands the quest in, and quest picks skip a quest that is still backing off. A stalled walk
    to a cross-map transport is reported as such and backed off like a trip stuck on the way.
  - Why: Bots retried the same unreachable quest again and again (the record went back to the first try each time it
    was picked up again), and transport walk stalls were reported as missing routes.
- **A cancelled trade no longer raises liking** (2026-11-24)
  - Summary: After the bot's accept closes a trade, liking is credited only when money or an item really moved (the
    bot's money, its offered items and the other side's items are compared before and after the accept).
  - Why: The core also closes the trade window when it cancels the trade at the last step (a full bag), and such a
    trade was counted as completed.
- **Shared-quest offer limits survive a logout and a restart** (2026-11-24)
  - Summary: The per-human limit (offers per rolling hour across all bots) and the bot and human pair cooldown were
    kept in memory only; the offers now go to a `playerbots_social_quest_offer` table (created at startup like the other
    social tables) and both limits are rebuilt from it when the human is loaded.
  - Why: After a logout or a restart a human could get offers again at once, more often than the documented limits.
- **Review fixes for the new run, sweep and teleport rules** (2026-11-24)
  - Summary: A bot-led run is given up only when every boss left was skipped twice (a retry after the skip ran out also
    failed), not after one failed pull; the leftover-group sweep leaves a member alone while it is in a battleground or
    battlefield raid; a finder bot counts only failed teleport asks in a row (a new time outside starts a new count); a
    dropped-mark timer no longer carries over time from an earlier wait.
  - Why: A code review found that a single failed pull on the last boss rested the dungeon for hours, that the sweep
    could take a bot out of its battleground raid, and that a bot could leave a finder group late in a run.
- **The auction soak bots go to the auction house when they stand in a town** (2026-11-24)
  - Summary: While `AiPlayerbot.Auction.OnlyBots` names bots, a named bot whose auction visit is allowed (cooldown passed, not human-led, no visit under way) that is idle in a town with an auctioneer within 200 yd now switches to its town errands (`BotAuction::SoakVisitDue`, logged as `[AUCTION] ... visit is due, goes there`), and the errand choice sends it to the auctioneer before the idle errands (`ErrandPolicy::Town::auctionDue`, `AuctionVisitPolicy::SoakWantsVisit`, three unit tests). Bots that are not named, and every bot while the list is empty, keep the ordinary odds. Nothing is given to a bot: it sells only what it carries and walks there itself.
  - Why: Seven named bots made two auction house visits in 15 hours. An auction errand needs an idle draw of the town errand status (about 6 percent of the draws), then a third of the idle errands, and the bots are in town only briefly; most of them also stood in saved bot groups that follow a leader far away and run no errands at all (see the stale group entry).
- **A saved bot group with no player and no leader in reach is left** (2026-11-24)
  - Summary: New `BotGroupSweep` (a pass every 30 s on the world thread): in a group of random bots with no player in it that is no finder, battleground or raid group and no party `BotLfm` gathers, a member whose leader has been offline, on another map, or over `AiPlayerbot.StaleGroups.ApartYards` (default 1500) away for `AiPlayerbot.StaleGroups.ApartMinutes` (default 5) leaves the group, logged as `[STALEGROUP]`; `AiPlayerbot.StaleGroups.Enable = 0` turns it off. The same tank that waited on a group mark now drops it (the target icon is cleared) once it has been out of reach and out of sight for five minutes and no player is in the group (`MarkPullPolicy::StaleMark`, `StaleGroupPolicy`, five unit tests).
  - Why: Three bots stayed in one saved group for days: the leader teleported to a random spot (a member may not), the members logged distances of 3000 to 4700 yd for hours, and the tank logged "waits: the mark is out of reach and out of sight" 74 times.
- **A finder bot that cannot get into its dungeon leaves the group after three asks** (2026-11-24)
  - Summary: `LfgTeleportRetryAction` asks for the dungeon teleport at most three times (it was six) at 15 s apart; the next time the trigger fires the bot says so in party chat when a player is in the group, and leaves the group like a finished run (`LfgTeleportPolicy`, four unit tests).
  - Why: A bot whose teleport never works (no valid entrance) asked for the teleport again and again, then sat outside the dungeon as a member that never arrived for the rest of the run.
- **A bot on a ledge above its leader searches further for a way down** (2026-11-24)
  - Summary: In `DungeonUnstuckAction::TryWalkOff`, when the bot has no path at all to its leader and the leader stands more than 10 yd lower, the landing search reads rings out to 30 yd (it was 12), allows a drop up to 30 yd (it was 20), and a landing within 4 yd of the leader's height counts as cheaper than any other (`LedgeWalkOffPolicy::WantsLeaderFloor`, `OnLeaderFloor`, four unit tests). The unseen-only teleport rule is unchanged: the bot still walks off the ledge in view.
  - Why: Two bots stood on a wall walk 40 to 63 yd from their leader, 16 yd above it, for more than five minutes: the 12 yd search found no landing with a path to the leader, and the teleport stays held while a player can see the bot.
- **A bot-led run that cannot reach any boss ends early and its dungeon rests** (2026-11-24)
  - Summary: When the leader of a bot-only run (finder run or hand-formed party) has skipped every boss still to do for lack of progress, and no pack was pulled or fought since the first of those skips, the crawl gives the run up: the bots of a finder run are asked to wrap up as at the time limit, and a hand-formed party is called over like a finished one. The dungeon is then left out of bot-only runs and of bot party recruiting for `AiPlayerbot.LfgBotRuns.FailureCooldownHours` (default 4, 0 for no rest; kept in memory). The rule is `LfgStallPolicy` (six unit tests). The built-in default of `AiPlayerbot.LfgBotRuns.Dungeons` now matches `conf/playerbots.conf.dist` (no 14, no 272).
  - Why: Five of six bot-only runs in a day sat out their 40 minutes without reaching a boss: the tank skipped every boss for no progress, waited for the skips to expire and did it again (eight cycles in Gnomeregan, seven skips of the last boss in the Wicked Grotto), tying up five bots for nothing.
- **Bot-only runs skip Gnomeregan and Maraudon's Wicked Grotto for now** (2026-11-24)
  - Summary: `AiPlayerbot.LfgBotRuns.Dungeons` no longer lists 14 (Gnomeregan) and 272 (Maraudon, Wicked Grotto) by default.
  - Why: Five of six bot-only runs in a day sat out their 40 minutes there without reaching a boss (the tank skipped every
    boss for "no progress"); the cause is still open, and a run that cannot reach a boss only ties up five bots.
- **The finder fills a tank no more than a level under the player** (2026-11-24)
  - Summary: `LfgFill` (`DungeonsFor`) now requires a tank it queues, by native spec, party bot or tank switch, to be at least the requesting player's level minus `AiPlayerbot.LfgFill.TankLevelBelowPlayer` (default 1) and never under the dungeon's minimum level (`LfgFillPolicy::TankLevelFloor`, `TankLevelOk`, three unit tests). Healers and damage dealers keep `LevelWindow`. The rule applies while the player is inside the dungeon's level range, like the window; the highest level of a premade group counts as the player's level.
  - Why: In the Stockade the finder filled a level 21 tank for a level 24 player with mobs at 23 to 24, and one pull chained 14 mobs and wiped (night proof run 2026-11-23).
- **The random password setting is read once** (2026-11-24)
  - Summary: `PlayerbotAIConfig.cpp` read `AiPlayerbot.RandomBotRandomPassword` twice in a row; the second read is gone.
  - Why: Leftover of the release cleanup; harmless, but it reads as a mistake.
- **Bots dive to a target deep in the water** (2026-11-23)
  - Summary: The surface swim lifted every point of a water path, the destination too, so a bot chasing a mob on a lake bed
    swam above it and could not hit it. The last point now keeps the destination's depth when it lies more than 1.5 yd
    under the surface; a destination near the surface (a swimming leader) is lifted as before.
  - Why: A melee bot hung on the surface over a Freshwater Eel it could not reach (Lake Everstill).

## [0.1.0] - 2026-11-23

### Added
- **First public release** (2026-11-23)
  - Summary: The module `mod-aware-bots-548` (player bots for SkyFire 5.4.8, client build 18414), the core patches it
    needs in `patches/`, and the optional AI sidecar `services/aware-ai`.
  - Why: The module ran privately for several weeks; this is its first public snapshot. The development history before
    this point is not published, so the entries of earlier private builds are not listed here.
