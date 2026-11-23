-- Removal data of the bots module (owner, 2026-11-12; module BT-154): deletes everything world_arena_ladder_npc.sql installed
-- and any creature rows of the arena ladder NPC "Pit Chronicler" (entry 9000100, guids 9300000 to 9300099). The module
-- installs and removes these rows; the core has no part in them.
--
-- Use it
--   - once on a server that still has the four database spawns of the first version of the NPC, before the build that spawns
--     the NPC from the module starts (otherwise the module adopts the database copy instead of its own, which is harmless
--     but leaves the NPC tied to the database), and
--   - when the module is taken out for good, so the core does not log a missing ScriptName line for the template at start-up.
-- It is safe to run again. Run world_arena_ladder_npc.sql afterwards to install the NPC again; restart the world server for
-- the core to pick the change up (creature templates are read at start-up).

DELETE FROM `creature_addon` WHERE `guid` BETWEEN 9300000 AND 9300099;
DELETE FROM `creature` WHERE `id` = 9000100 OR `guid` BETWEEN 9300000 AND 9300099;
DELETE FROM `creature_template` WHERE `entry` = 9000100;
DELETE FROM `gossip_menu` WHERE `MenuID` = 9000100;
DELETE FROM `npc_text` WHERE `ID` BETWEEN 9000100 AND 9000102;
