-- The whisper of a shared-quest group offer (design decision 0033 point 16, docs/social/social.md section 12): a random bot that has the
-- same unfinished quest as a human nearby and likes them offers a group. Rows 'social_quest_offer' with a variant per
-- personality 'social_quest_offer_<personality>'; the code carries one line as the fallback. Placeholders: %name (the human) and
-- %quest (a quest link). Nothing says anything while AiPlayerbot.Social.QuestOffer.Enable is 0. Safe to run again: the rows of
-- these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` LIKE 'social\_quest\_offer%';

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('social_quest_offer', 'Hey %name, we are both working on %quest. Want to group up?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer', '%name, you have %quest too, right? Let''s do it together', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer', 'Hi %name, I am on %quest as well. Group?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_quiet', '%name, %quest too? group up', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_quiet', 'same quest, %quest. want to team up?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_friendly', 'Hey %name! Good to see you, I am on %quest too. Want to do it together?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_friendly', '%name! We are on the same quest, %quest. Come along, it is more fun in a group', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_joker', 'Two heroes, one quest: %quest. %name, shall we split the glory?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_joker', '%name, I will take the left half of %quest if you take the right half. Deal?', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_grumpy', '%name, you are on %quest as well. Fine, let''s group, it goes faster', 0, 0, '', '', '', '', '', '', '', ''),
('social_quest_offer_grumpy', 'Same quest again, %quest. Group up if you want, %name', 0, 0, '', '', '', '', '', '', '', '');
