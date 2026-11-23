-- Arena ladder of the bot teams (BT-154, design decision 0027): named teams of random bots, their rating and the history of
-- their matches. The module creates these tables itself at start-up with CREATE TABLE IF NOT EXISTS (it has no SQL
-- updater); this file holds the same statements for a fresh install. Safe to run again.
--
-- team.skill_tier: 0 expert, 1 good, 2 casual (the weakest member's tier). team.level_band: the core's PvP bracket index
-- of the members' levels (0 = 10 to 14 ... 10 = 60 to 64 ... 16 = 90). team_member.spec: the specialisation id of the core
-- (62 to 270), so a SMALLINT. match.winner: 0 draw, 1 team_a, 2 team_b, 3 voided.

CREATE TABLE IF NOT EXISTS `playerbots_arena_team` (
  `team_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(24) NOT NULL,
  `bracket` TINYINT UNSIGNED NOT NULL,
  `faction` TINYINT UNSIGNED NOT NULL,
  `level_band` TINYINT UNSIGNED NOT NULL,
  `skill_tier` TINYINT UNSIGNED NOT NULL,
  `rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,
  `peak_rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,
  `games` INT UNSIGNED NOT NULL DEFAULT 0,
  `wins` INT UNSIGNED NOT NULL DEFAULT 0,
  `losses` INT UNSIGNED NOT NULL DEFAULT 0,
  `draws` INT UNSIGNED NOT NULL DEFAULT 0,
  `streak` SMALLINT NOT NULL DEFAULT 0,
  `best_streak` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `status` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `created` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_played` TIMESTAMP NULL DEFAULT NULL,
  `retired` TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (`team_id`),
  UNIQUE KEY `uq_arena_team_name` (`name`),
  KEY `idx_arena_ladder` (`bracket`, `status`, `rating`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playerbots_arena_team_member` (
  `team_id` INT UNSIGNED NOT NULL,
  `guid` INT UNSIGNED NOT NULL,
  `bracket` TINYINT UNSIGNED NOT NULL,
  `class` TINYINT UNSIGNED NOT NULL,
  `spec` SMALLINT UNSIGNED NOT NULL,
  `role` TINYINT UNSIGNED NOT NULL,
  `joined` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`team_id`, `guid`),
  UNIQUE KEY `uq_arena_member_bracket` (`guid`, `bracket`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `playerbots_arena_match` (
  `match_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `bracket` TINYINT UNSIGNED NOT NULL,
  `team_a` INT UNSIGNED NOT NULL,
  `team_b` INT UNSIGNED NOT NULL,
  `human_name` VARCHAR(40) NULL,
  `winner` TINYINT UNSIGNED NOT NULL,
  `rating_a` SMALLINT UNSIGNED NOT NULL,
  `rating_a_new` SMALLINT UNSIGNED NOT NULL,
  `rating_b` SMALLINT UNSIGNED NOT NULL,
  `rating_b_new` SMALLINT UNSIGNED NOT NULL,
  `map_id` SMALLINT UNSIGNED NOT NULL,
  `seconds` SMALLINT UNSIGNED NOT NULL,
  `ended` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`match_id`),
  KEY `idx_arena_match_ended` (`ended`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
