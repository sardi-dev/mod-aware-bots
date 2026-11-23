-- Travel to the group: what a random bot says in party chat while it travels to a player's group
-- (TravelToGroupAction.cpp). %zone is the bot's zone, %minutes a rough estimate. Each name has a few variants, one
-- is picked at random; the code carries the first variant of each name as its fallback.
-- Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` IN ('travel_group_start', 'travel_group_start_unknown',
  'travel_group_update', 'travel_group_arrived', 'travel_group_lost');

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('travel_group_start', 'I''m in %zone, about %minutes min away. Wait for me', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_start', 'Coming from %zone, around %minutes min. Wait for me!', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_start', 'I''m way out in %zone, about %minutes min to you', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_start_unknown', 'I''m in %zone, a long way from you. Give me a while', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_start_unknown', 'Far away in %zone, I''ll be a while', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_update', 'Still on my way, about %minutes min', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_update', 'Getting closer, around %minutes min', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_update', 'Almost there, %minutes min or so', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_arrived', 'I''m here', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_arrived', 'Here', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_arrived', 'Made it', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_lost', 'I can''t find a way to you from here. Can you summon me?', 0, 0, '', '', '', '', '', '', '', ''),
('travel_group_lost', 'I''m stuck and can''t get to you. Could you summon me?', 0, 0, '', '', '', '', '', '', '', '');
