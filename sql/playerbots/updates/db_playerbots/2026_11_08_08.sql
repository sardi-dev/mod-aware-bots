-- Bot guilds: the lines random bots say in guild chat and the offer a recruiting guild leader whispers to a player
-- (Manager/BotGuilds.cpp, through BotVoice). Each name has a few variants, one is picked at random; the placeholders are
-- %name (the member or the player) and %guildname. The code carries a default for each name, so the bots talk without these
-- rows too. Safe to run again: the rows of these names are replaced.

DELETE FROM `ai_playerbot_texts` WHERE `name` IN ('guild_welcome', 'guild_promoted', 'guild_kick', 'guild_leave', 'guild_invite_offer');

INSERT INTO `ai_playerbot_texts` (`name`, `text`, `say_type`, `reply_type`, `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`, `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
('guild_welcome', 'Welcome to the guild, %name', 0, 0, '', '', '', '', '', '', '', ''),
('guild_welcome', 'Hey %name, glad to have you here', 0, 0, '', '', '', '', '', '', '', ''),
('guild_welcome', 'welcome %name!', 0, 0, '', '', '', '', '', '', '', ''),
('guild_welcome', '%name, make yourself at home', 0, 0, '', '', '', '', '', '', '', ''),
('guild_promoted', 'Congratulations %name, you earned it', 0, 0, '', '', '', '', '', '', '', ''),
('guild_promoted', 'Nice work %name, you are promoted', 0, 0, '', '', '', '', '', '', '', ''),
('guild_promoted', 'grats %name, new rank', 0, 0, '', '', '', '', '', '', '', ''),
('guild_kick', '%name was gone too long, so I removed them', 0, 0, '', '', '', '', '', '', '', ''),
('guild_kick', 'Clearing the roster: %name has been away for weeks', 0, 0, '', '', '', '', '', '', '', ''),
('guild_leave', 'I''m going to try something else, bye all', 0, 0, '', '', '', '', '', '', '', ''),
('guild_leave', 'Time for me to move on, thanks for everything', 0, 0, '', '', '', '', '', '', '', ''),
('guild_leave', 'Leaving the guild, take care', 0, 0, '', '', '', '', '', '', '', ''),
('guild_invite_offer', 'Hey %name, we have a friendly guild, %guildname. Want to join?', 0, 0, '', '', '', '', '', '', '', ''),
('guild_invite_offer', 'Hi %name, looking for a guild? %guildname has room', 0, 0, '', '', '', '', '', '', '', ''),
('guild_invite_offer', '%name, would you like to join %guildname?', 0, 0, '', '', '', '', '', '', '', '');
