-- Bot LFM (BotLfm.cpp, LfmLeadAction.cpp): what a bot leader says while it forms a dungeon group by hand, and what the
-- bots that answer whisper. %dungeon, %role, %name are filled in. Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` REGEXP '^voice_lfm(_[a-z_]+)?$';

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('voice_lfm', 'LFM %dungeon, need %role', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm', 'LFM %dungeon, looking for a %role', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm', '%dungeon, need %role. whisper me', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_quiet', 'LFM %dungeon %role', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_friendly', 'Hi all! Looking for a %role for %dungeon, anyone?', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_friendly', 'LFM %dungeon, need %role. Whisper me for an invite!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_joker', 'LFM %dungeon, %role wanted. Heroes preferred, snacks optional', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_joker', '%dungeon needs a %role. Free hugs included', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_grumpy', 'LFM %dungeon, need %role. Know your class.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_grumpy', '%dungeon, %role needed. Do not be late.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_answer', 'inv, %role', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_answer', '%role here for %dungeon', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_answer', 'inv pls, I am %role', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_invited', 'Inviting you, we meet at the %dungeon entrance', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_invited', 'Sent you an invite, meet us at the %dungeon entrance', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_refuse', 'Sorry, I can''t take you for this one', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_refuse', 'Sorry, not a fit for this group', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_full', 'Sorry, the group is full', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_full', 'We are full, sorry', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_go', 'Let''s go, meet me at the %dungeon entrance', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_go', 'Group is ready. Meet me at the %dungeon entrance', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_wait', 'I''m at the entrance, waiting for everyone', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_wait', 'At the %dungeon entrance, waiting for everyone', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_waiting', 'Still waiting for %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_waiting', 'Waiting for %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_enter', 'Everyone is here, going in', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_enter', 'All here, going in', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_enter_late', 'Can''t wait any longer, going in', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_enter_late', 'Going in, catch up when you can', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_cancel', 'Nobody else came, never mind', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_cancel', 'No luck finding a group, never mind', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_cancel_party', 'Not enough of us, calling it off', 0, 0, '', '', '', '', '', '', '', ''),
('voice_lfm_cancel_party', 'Too few of us, let''s call it off', 0, 0, '', '', '', '', '', '', '', '');
