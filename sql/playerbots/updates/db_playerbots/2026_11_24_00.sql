-- Shared-quest offer history, used for the rolling limit and pair cooldown.
CREATE TABLE IF NOT EXISTS `playerbots_social_quest_offer` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `player_guid` INT UNSIGNED NOT NULL,
  `bot_guid` INT UNSIGNED NOT NULL,
  `offered_at` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_quest_offer_player` (`player_guid`),
  KEY `idx_quest_offer_bot` (`bot_guid`),
  KEY `idx_quest_offer_at` (`offered_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
