-- Updates are applied before base files on a fresh install, so ensure the table exists first.
CREATE TABLE IF NOT EXISTS `mod_mounts_on_account` (
  `account_id` int UNSIGNED NOT NULL COMMENT 'id of the account that learned the spell',
  `team_id` int UNSIGNED NOT NULL COMMENT '0 = Alliance, 1= Horde, 2 = All',
  `spell_id` int UNSIGNED NOT NULL COMMENT 'id of the learned spell',
  `date` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'date learned',
  UNIQUE KEY `uq_account_spell` (`account_id`, `spell_id`)
);

SET @index_exists := (
    SELECT COUNT(1)
    FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'mod_mounts_on_account'
      AND INDEX_NAME = 'idx_account_team_spell'
);

SET @sql := IF(
    @index_exists = 0,
    'ALTER TABLE `mod_mounts_on_account` ADD INDEX `idx_account_team_spell` (`account_id`, `team_id`, `spell_id`);',
    'SELECT 1;'
);

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
