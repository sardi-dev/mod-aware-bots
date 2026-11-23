-- Guild offer times of a human player (design decision 0033 point 16, docs/social/social.md). One row per human and kind of offer:
-- the time (unix seconds) of the last guild offer a bot made, so the cooldown per human survives a restart. Safe to run again.
-- Nothing reads or writes the table while AiPlayerbot.Social.Liking.Enable is 0.
--
-- player_guid is characters.guid (the low part) of the human; kind 1 is a guild offer. The table holds no character names.
-- The wipe command and the deletion of a character remove a human's rows (docs/social/social.md, section 7).

CREATE TABLE IF NOT EXISTS `playerbots_social_offer` (
  `player_guid` INT UNSIGNED NOT NULL COMMENT 'characters.guid of the human player',
  `kind` TINYINT UNSIGNED NOT NULL COMMENT '1 = guild offer',
  `last_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix seconds of the last offer of this kind',
  PRIMARY KEY (`player_guid`, `kind`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Last guild offer made to a human player';
