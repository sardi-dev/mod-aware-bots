-- Permissions for the playerbots commands (auth database). 1346 lets players control their own bots
-- (.playerbots bot); 1345 is for game masters (.pmon, .playerbots rndbot, .botscenario, .botprobe).
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1345, 1346);
DELETE FROM `rbac_permissions` WHERE `id` IN (1345, 1346);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1345, 'Command: pmon, rndbot, botscenario, botprobe (playerbots)'),
(1346, 'Command: playerbots bot');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1345),
(199, 1346);
