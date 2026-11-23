-- Install data of the bots module (owner, 2026-11-12; module BT-154, design decision 0027): the arena ladder NPC "Pit Chronicler" is
-- part of mod-aware-bots-548, not of the core. This file holds the NPC's template, its gossip menu and its three static texts.
-- It holds no creature rows: the module spawns the NPC itself, at run time, beside the arena battlemasters (src/Arena/
-- ArenaNpcSpots.h), only while AiPlayerbot.ArenaTeams.Enable and AiPlayerbot.ArenaTeams.LadderNpc are 1, and never saves it to
-- the database. Apply this file by hand to the world database like the other files in sql/world/; it is safe to run again (it
-- deletes its own rows first). The file world_arena_ladder_npc_remove.sql undoes it.
--
-- The NPC is a gossip NPC that reads the books of the bot arena ladder: the module's script `npc_bot_arena_ladder` builds the
-- pages (ranking, team page, fights in progress) from its own tables. Model 26391 is the one of the Dalaran arena battlemaster
-- (already in the client). Nothing to do on the client side.
--
-- Without the module compiled in, nothing spawns, so the NPC appears nowhere; the template row alone is invisible. The core
-- then logs one missing ScriptName line for entry 9000100 at start-up, which is harmless, and the removal file clears it.
--
-- Ids (reserved custom ranges): creature_template entry 9000100; gossip_menu and npc_text 9000100 to 9000102. The creature
-- guid range 9300000 to 9300099 stays reserved for this NPC and is only ever cleaned up by the removal file (the first
-- version of this file inserted four database spawns there).
--
-- The DELETE of creature rows below is on purpose: a database copy of the NPC would be a second copy next to the module's.

DELETE FROM `creature` WHERE `id` = 9000100 OR `guid` BETWEEN 9300000 AND 9300099;
DELETE FROM `creature_template` WHERE `entry` = 9000100;
DELETE FROM `gossip_menu` WHERE `MenuID` = 9000100;
DELETE FROM `npc_text` WHERE `ID` BETWEEN 9000100 AND 9000102;

INSERT INTO `creature_template` (`entry`, `modelid1`, `name`, `subname`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction_A`, `faction_H`, `npcflag`, `speed_walk`, `speed_run`, `unit_class`, `unit_flags`, `unit_flags2`, `type`, `InhabitType`, `RegenHealth`, `ScriptName`, `WDBVerified`) VALUES
(9000100, 26391, 'Pit Chronicler', 'Arena Ladder', 9000100, 90, 90, 2, 35, 35, 1, 1, 1.14286, 1, 33024, 2048, 7, 3, 1, 'npc_bot_arena_ladder', 15595);

INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES
(9000100, 9000100);

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `lang0`, `prob0`) VALUES
(9000100, 'I keep the books of the pit. Which ladder do you want to read?', 'I keep the books of the pit. Which ladder do you want to read?', 0, 1),
(9000101, 'Here is what the books say.', 'Here is what the books say.', 0, 1),
(9000102, 'The books are empty for now. The teams are still finding their feet.', 'The books are empty for now. The teams are still finding their feet.', 0, 1);
