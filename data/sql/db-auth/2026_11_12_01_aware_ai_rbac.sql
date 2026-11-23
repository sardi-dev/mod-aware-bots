-- Permissions for the AI layer commands (auth database), rerunnable. 1347 lets game masters read the status
-- (.bot ai status); 1348 lets administrators change the layer (.bot ai on, off, budget). They follow the ids 1345 and
-- 1346 of playerbots_rbac.sql. Roles: 197 = Gamemaster Commands, 196 = Administrator Commands.
-- Undo: DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1347, 1348); DELETE FROM `rbac_permissions` WHERE `id` IN (1347, 1348);
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1347, 1348);
DELETE FROM `rbac_permissions` WHERE `id` IN (1347, 1348);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1347, 'Command: bot ai status (AI layer)'),
(1348, 'Command: bot ai on, off, budget (AI layer)');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1347),
(196, 1348);
