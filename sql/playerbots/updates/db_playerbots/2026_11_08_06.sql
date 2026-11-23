-- Walk into instances (TravelToGroupAction.cpp): what a bot says at a dungeon's entrance, with a variant per personality.
-- Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` REGEXP '^travel_group_(entering|cant_enter)(_quiet|_friendly|_joker|_grumpy)?$';

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('travel_group_entering', 'I''m at the entrance, coming in', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering', 'At the entrance, going in now', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering', 'Coming in', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering_quiet', 'coming in', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering_friendly', 'I''m at the entrance, coming in to join you!', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering_joker', 'Knock knock, I''m at the door', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_entering_grumpy', 'At the entrance. Going in.', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter', 'I can''t get in there. Can you summon me?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter', 'The entrance won''t let me in. Could you summon me?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter_quiet', 'can''t get in, summon?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter_friendly', 'Sorry, I can''t get in. Could you summon me?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter_joker', 'The door and I disagree. Summon me?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_cant_enter_grumpy', 'Cannot enter. Summon me.', 0, 0, '', '', '', '', '', '', '', '');
