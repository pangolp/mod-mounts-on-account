CREATE TABLE IF NOT EXISTS `mod_mounts_on_account` (
  `account_id` int UNSIGNED NOT NULL COMMENT 'id of the account that learned the spell',
  `team_id` int UNSIGNED NOT NULL COMMENT '0 = Alliance, 1= Horde, 2 = All',
  `spell_id` int UNSIGNED NOT NULL COMMENT 'id of the learned spell',
  `date` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'date learned'
);
