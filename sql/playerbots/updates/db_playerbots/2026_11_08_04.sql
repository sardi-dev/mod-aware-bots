-- Finder manners: what a random bot that the Dungeon Finder put in a group says in party chat (LfgMannersAction.cpp).
-- Each name has a few variants, one is picked at random. The code carries variants of each name too, so the bots talk
-- without these rows as well; the rows let the wording be changed without a build.
-- Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` IN ('lfg_hello', 'lfg_quest', 'lfg_ready', 'lfg_thanks', 'lfg_bye');

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('lfg_hello', 'hi', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_hello', 'hello', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_hello', 'hey all', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_hello', 'hi all', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_hello', 'Hi!', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_quest', 'anyone need a quest in here?', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_quest', 'does anybody have a quest for this place?', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_quest', 'need to do any quests in here?', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_ready', 'ready when you are', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_ready', 'ready', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_ready', 'rdy', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_ready', 'good to go', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_thanks', 'thanks all', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_thanks', 'ty everyone', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_thanks', 'thanks for the run', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_thanks', 'gg, thanks', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_bye', 'bye!', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_bye', 'cya', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_bye', 'see you around', 0, 0, '', '', '', '', '', '', '', ''),
('lfg_bye', 'gl with everything, bye', 0, 0, '', '', '', '', '', '', '', '');
