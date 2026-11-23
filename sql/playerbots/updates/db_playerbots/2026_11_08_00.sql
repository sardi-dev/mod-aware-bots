-- Goal-based invites: the lines a random bot whispers when it declines a group invite (AcceptInvitationAction.cpp).
-- Each name has a few variants, one is picked at random; %level is the bot's own level. The code carries the first
-- variant of each name as its fallback, so the bots work without these rows too (and log a missing-text error).
-- Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` IN ('invite_decline_quest', 'invite_decline_new_task',
  'invite_decline_low_level', 'invite_decline_high_level', 'invite_decline_combat', 'invite_decline_dead',
  'invite_decline_queued', 'invite_decline_dungeon', 'invite_decline_battleground', 'invite_decline_generic');

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('invite_decline_quest', 'Sorry, almost done with this quest chain', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_quest', 'Can''t right now, I''m in the middle of a quest', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_quest', 'Maybe later, I''m nearly done with this quest', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_new_task', 'Just got going, ask me again later', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_new_task', 'Not now, I just set off', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_new_task', 'I only just started something, maybe in a bit', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_low_level', 'Too low for that, I''m %level', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_low_level', 'I''m only %level, that would be too much for me', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_low_level', 'Thanks, but I''m %level and would just die', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_high_level', 'I''m %level, a bit far from your level for a group', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_high_level', 'I''m %level, you would level faster with someone closer to your level', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_combat', 'In a fight right now, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_combat', 'Busy fighting, ask me again in a bit', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_dead', 'I''m dead, running back to my body', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_dead', 'Dead at the moment, give me a minute', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_queued', 'I''m queued for a dungeon, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_queued', 'Waiting on a dungeon queue, maybe later', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_dungeon', 'I''m in the middle of a dungeon', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_dungeon', 'Stuck in an instance right now, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_battleground', 'In a battleground, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_generic', 'Not right now, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_generic', 'Maybe later', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_generic', 'Thanks, but I have things to do', 0, 0, '', '', '', '', '', '', '', ''),
('invite_decline_generic', 'Sorry, I''m busy at the moment', 0, 0, '', '', '', '', '', '', '', '');
