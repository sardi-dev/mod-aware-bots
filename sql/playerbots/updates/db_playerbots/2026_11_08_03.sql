-- Summons (AcceptSummonAction.cpp): the lines a bot says when it accepts or declines a summon and when it summons a
-- group mate at a meeting stone, with a variant per personality. Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` REGEXP '^voice_(summon_thanks|summon_busy|stone_summon)(_quiet|_friendly|_joker|_grumpy)?$';

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('voice_summon_thanks', 'Thanks for the summon', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks', 'Thanks, that saved me a walk', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks', 'Got the summon, thanks', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_quiet', 'ty', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_friendly', 'Thank you so much for the summon!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_friendly', 'Thanks a lot, you are the best!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_joker', 'Teleported by magic again, my favourite', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_joker', 'Summoned like a pet, thanks', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_grumpy', 'About time. Thanks.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_thanks_grumpy', 'Fine, thanks', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy', 'Can''t right now, I''m busy', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy', 'In a fight, try again in a moment', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy', 'Not now, ask me again in a bit', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy_quiet', 'busy', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy_friendly', 'Sorry, I''m in the middle of something, try again soon!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy_joker', 'Currently fighting for my life, call back later', 0, 0, '', '', '', '', '', '', '', ''),
('voice_summon_busy_grumpy', 'Not now. Busy.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon', 'Summoning %name at the stone, hang on', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon', '%name is far away, summoning at the stone', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon', 'Summoning %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon_quiet', 'summoning %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon_friendly', 'Hang on %name, I am summoning you at the stone!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon_joker', '%name, your carriage awaits at the stone', 0, 0, '', '', '', '', '', '', '', ''),
('voice_stone_summon_grumpy', 'Summoning %name. Be quick about it.', 0, 0, '', '', '', '', '', '', '', '');
