-- Liking score of a random bot for a human player (design decision 0033 point 14, docs/social/social.md). One row per bot and human
-- that have shared a moment; a pair with no row sits at the neutral value. Safe to run again. Nothing reads or writes the
-- table while AiPlayerbot.Social.Liking.Enable is 0.
--
-- bot_guid and player_guid are characters.guid (the low part) of the random bot and the human. value is in [0, 1] and is
-- the score as of updated_at (unix seconds); the decay toward neutral is worked out when the row is read. gain_day is the
-- day number (unix seconds / 86400) that gain_today, the positive change already counted that day, belongs to: it keeps the
-- daily cap across restarts. The table holds no character names. The index on player_guid serves the wipe command and the
-- cleanup after a character is deleted.

CREATE TABLE IF NOT EXISTS `playerbots_social_liking` (
  `bot_guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid of the random bot',
  `player_guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid of the human player',
  `value` FLOAT NOT NULL DEFAULT 0.3 COMMENT 'liking in [0, 1] as of updated_at',
  `updated_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix seconds of the last change',
  `gain_day` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix seconds / 86400 of gain_today',
  `gain_today` FLOAT NOT NULL DEFAULT 0 COMMENT 'positive change counted on gain_day',
  PRIMARY KEY (`bot_guid`, `player_guid`),
  KEY `idx_social_liking_player` (`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Liking of a random bot for a human player';
