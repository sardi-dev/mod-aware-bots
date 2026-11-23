-- Loot manners (LootEtiquetteActions.cpp): congratulations, "anyone need this?", offering an item the bot cannot use, with a
-- variant per personality. %name is the winner, %item the item name. Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` REGEXP '^voice_loot_(grats|anyone|offer|given)(_quiet|_friendly|_joker|_grumpy)?$';

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('voice_loot_grats', 'Grats %name on %item!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats', 'Nice one %name, %item!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats', 'Congrats %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_quiet', 'gz %name', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_friendly', 'Congratulations %name, well deserved!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_friendly', 'Awesome, grats on %item %name!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_joker', '%name wins again, show off!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_joker', '%item looks good on you %name, I am only a little jealous', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_grumpy', 'Grats %name. I wanted that.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_grats_grumpy', 'Fine, %name, enjoy %item', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone', 'Anyone need %item? I''ll wait for your roll', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone', 'Does anyone need %item? Go ahead first', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone_quiet', '%item, anyone?', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone_friendly', 'Does anyone need %item? Please go first, I can wait!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone_joker', '%item, going once, going twice... anyone?', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_anyone_grumpy', 'Anyone need %item? Say so before I roll.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer', 'I won %item but can''t use it. Say me if you want it', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer', 'Got %item, not for me. Want it? Say me', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer_quiet', '%item, not for me. say me', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer_friendly', 'I won %item but I can''t use it. Would you like it? Just say me!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer_joker', 'I won %item and it is useless to me. First to say me gets it', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_offer_grumpy', 'Got %item, no use to me. Say me if you want it.', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given', 'Here you go', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given', 'There, it is yours now', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given_quiet', 'here', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given_friendly', 'Here you go, enjoy it!', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given_joker', 'One gift, no receipt needed', 0, 0, '', '', '', '', '', '', '', ''),
('voice_loot_given_grumpy', 'Take it.', 0, 0, '', '', '', '', '', '', '', '');
