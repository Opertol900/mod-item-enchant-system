-- Persistent per-item levels and the write-ahead operation journal.

CREATE TABLE IF NOT EXISTS `mod_item_enchant` (
  `item_guid` int unsigned NOT NULL,
  `owner_guid` int unsigned NOT NULL,
  `item_entry` int unsigned NOT NULL,
  `enchant_level` tinyint unsigned NOT NULL DEFAULT '0',
  `revision` int unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`item_guid`),
  KEY `idx_mod_item_enchant_owner` (`owner_guid`),
  KEY `idx_mod_item_enchant_entry_level` (`item_entry`,`enchant_level`),
  CONSTRAINT `fk_mod_item_enchant_item` FOREIGN KEY (`item_guid`) REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_item_enchant_pending` (
  `operation_id` bigint unsigned NOT NULL,
  `owner_guid` int unsigned NOT NULL,
  `item_guid` int unsigned NOT NULL,
  `item_entry` int unsigned NOT NULL,
  `scroll_guid` int unsigned NOT NULL,
  `scroll_entry` int unsigned NOT NULL,
  `scroll_type` tinyint unsigned NOT NULL,
  `old_level` tinyint unsigned NOT NULL,
  `new_level` tinyint unsigned NOT NULL,
  `destroy_item` tinyint unsigned NOT NULL DEFAULT '0',
  `success` tinyint unsigned NOT NULL DEFAULT '0',
  `chance_bp` int unsigned NOT NULL DEFAULT '0',
  `roll_value` int unsigned NOT NULL DEFAULT '0',
  `status` varchar(16) NOT NULL DEFAULT 'pending',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `completed_at` timestamp NULL DEFAULT NULL,
  PRIMARY KEY (`operation_id`),
  KEY `idx_mod_item_enchant_pending_owner_status` (`owner_guid`,`status`),
  KEY `idx_mod_item_enchant_pending_item` (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_item_enchant_history` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `operation_id` bigint unsigned NOT NULL,
  `owner_guid` int unsigned NOT NULL,
  `item_guid` int unsigned NOT NULL,
  `item_entry` int unsigned NOT NULL,
  `scroll_entry` int unsigned NOT NULL,
  `scroll_type` tinyint unsigned NOT NULL,
  `old_level` tinyint unsigned NOT NULL,
  `new_level` tinyint unsigned NOT NULL,
  `chance_bp` int unsigned NOT NULL DEFAULT '0',
  `roll_value` int unsigned NOT NULL DEFAULT '0',
  `result` varchar(32) NOT NULL,
  `reason` varchar(64) NOT NULL DEFAULT '',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_mod_item_enchant_history_operation` (`operation_id`),
  KEY `idx_mod_item_enchant_history_owner` (`owner_guid`,`created_at`),
  KEY `idx_mod_item_enchant_history_item` (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
