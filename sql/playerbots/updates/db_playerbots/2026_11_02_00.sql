-- Bot accounts: an account its player turned into a bot account for one owner account (".playerbots account botmode").
CREATE TABLE IF NOT EXISTS `playerbots_bot_accounts` (
  `bot_account` INT UNSIGNED NOT NULL COMMENT 'auth.account.id of the bot account',
  `owner_account` INT UNSIGNED NOT NULL COMMENT 'auth.account.id of the account that may use its characters as bots',
  `created` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`bot_account`),
  KEY `idx_owner` (`owner_account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Playerbot bot accounts';
