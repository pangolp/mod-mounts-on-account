# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore

## mod-mounts-on-account

[![Build Status](https://github.com/pangolp/mod-mounts-on-account/actions/workflows/core-build.yml/badge.svg)](https://github.com/pangolp/mod-mounts-on-account)

## Description

Account-wide mounts for AzerothCore (WotLK 3.3.5a). When a character on an account learns a mount spell, the module records it in the auth database and automatically teaches it to all other characters on the same account the next time they log in.

In WotLK, mounts are character-bound spells. There is no account-wide mount collection at the protocol level (that feature was introduced in Mists of Pandaria). This module achieves the same result by actually learning the spell on each character — so every alt sees the full mount roster in their own spell book.

## Features

- Automatically records mounts when a character learns them (`moa.enable.learn`).
- Teaches all account mounts to a character on login (`moa.enable.learn.on.login`).
- Respects faction restrictions: Alliance-only mounts are only shared with Alliance characters, and vice versa. Faction-neutral mounts are shared with everyone on the account.
- Mount-to-faction mappings are built from the item store at server startup — no database queries on spell-learn events.
- Login sync is fully asynchronous and does not block the world thread.

## Requirements

- AzerothCore (WotLK 3.3.5a)

## Installation

1. Copy the module folder into `modules/`.
2. Re-run CMake and recompile.
3. Apply the SQL files from `data/sql/`:
   - `db-auth/base/mod_mounts_on_account.sql` — creates the account mount table (fresh installs).
   - `db-auth/updates/` — run any update files if upgrading from a previous version.
   - `db-world/base/moa_acore_string.sql` — login notification string.
4. Copy `conf/mod-mounts-on-account.conf.dist` to your server's config directory and rename it to `mod-mounts-on-account.conf`.

## Configuration

| Option | Default | Description |
|---|---|---|
| `moa.enable` | `true` | Show a chat notification to players on login indicating the module is active. |
| `moa.message.id` | `45000` | Entry ID in `acore_string` used for the login notification. |
| `moa.enable.learn` | `true` | Record a mount in the account table when a character learns its spell. This is the primary option and should remain enabled. |
| `moa.enable.learn.on.login` | `false` | Teach all account mounts to a character when they log in. Enable this to have alts automatically receive mounts learned by other characters. |
| `moa.enable.cast` | `false` | **Migration helper only.** Records a mount whenever a character *casts* (rides) it, not just when they learn it. Useful for servers with pre-existing characters who have mounts that were never captured by `moa.enable.learn`. Disable once the initial migration is complete, as it runs on every mount cast. |

## How it works

1. **Learning** (`OnPlayerLearnSpell`): when a character learns a spell, the module checks whether any of its effects apply `SPELL_AURA_MOUNTED`. If so, it inserts the mount into `mod_mounts_on_account` with the account ID, faction (`team_id`: 0 = Alliance, 1 = Horde, 2 = neutral), and spell ID. A `UNIQUE KEY (account_id, spell_id)` prevents duplicates.

2. **Faction mapping**: at server startup (`OnStartup`), the module iterates the loaded item template store and builds an in-memory map from mount spell ID to faction. This lookup replaces the previous synchronous `WorldDatabase` query that ran on every spell-learn event.

3. **Login sync** (`OnPlayerLogin`): if `moa.enable.learn.on.login` is enabled, the module issues an async query for all mounts associated with the account and faction, then teaches any that the character does not already know. The query is non-blocking; the player GUID is captured and resolved safely inside the callback.

## Database

Table created in `acore_auth`:

```sql
CREATE TABLE `mod_mounts_on_account` (
  `account_id` int UNSIGNED NOT NULL,
  `team_id`    int UNSIGNED NOT NULL,  -- 0=Alliance, 1=Horde, 2=neutral
  `spell_id`   int UNSIGNED NOT NULL,
  `date`       TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY `uq_account_spell` (`account_id`, `spell_id`)
);
```
