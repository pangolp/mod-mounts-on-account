-- Deduplicate rows keeping the earliest entry per (account_id, spell_id)
DELETE n1 FROM `mod_mounts_on_account` n1, `mod_mounts_on_account` n2
WHERE n1.account_id = n2.account_id
  AND n1.spell_id   = n2.spell_id
  AND n1.date       > n2.date;

-- Add unique constraint (DROP IF EXISTS makes this idempotent)
ALTER TABLE `mod_mounts_on_account` DROP INDEX IF EXISTS `uq_account_spell`;
ALTER TABLE `mod_mounts_on_account` ADD UNIQUE KEY `uq_account_spell` (`account_id`, `spell_id`);
