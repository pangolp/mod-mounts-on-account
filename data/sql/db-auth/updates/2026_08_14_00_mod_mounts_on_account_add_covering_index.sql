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
