INSERT INTO `rbac_permissions` (`id`,`name`) VALUES
(2000,'Command: enchant player operations'),
(2001,'Command: enchant administration')
ON DUPLICATE KEY UPDATE `name`=VALUES(`name`);

INSERT IGNORE INTO `rbac_linked_permissions` (`id`,`linkedId`) VALUES
(199,2000),
(196,2001);
