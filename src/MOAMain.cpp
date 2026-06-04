/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 *
*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "ObjectMgr.h"

#include <unordered_map>

struct MOA
{
    uint32 message;
    bool enable, enableCast, enableLearn;
    bool enableLearnOnLogin;
};

MOA moa;

// Built once at startup: mount spell ID → team_id (0=Alliance, 1=Horde, 2=neutral)
static std::unordered_map<uint32, uint32> s_mountTeamMap;

static constexpr uint32 ALLIANCE_RACE_MASK = 1101; // Human|Dwarf|NightElf|Gnome|Draenei
static constexpr uint32 HORDE_RACE_MASK    = 690;  // Orc|Undead|Tauren|Troll|BloodElf

class MOAPlayer : public PlayerScript
{
public:
    MOAPlayer() : PlayerScript("MOAPlayer") { }

    void OnPlayerLogin(Player* player) override
    {
        if (moa.enable)
            ChatHandler(player->GetSession()).PSendSysMessage(moa.message);

        if (moa.enableLearnOnLogin)
        {
            QueryResult resultSpells = LoginDatabase.Query("SELECT `spell_id` FROM `mod_mounts_on_account` WHERE `team_id`={} OR `team_id`=2;", player->GetTeamId());

            if (resultSpells && player->HasSpell(33388) && player->HasSpell(33391))
            {
                do
                {
                    uint32 spellID = (*resultSpells)[0].Get<uint32>();
                    if (!player->HasSpell(spellID))
                        player->learnSpell(spellID);
                } while (resultSpells->NextRow());
            }
        }
    }

    void CustomLearnSpell(Player* player, uint32 spellID)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellID);
        if (!spellInfo)
            return;

        bool isMountSpell = false;
        for (SpellEffectInfo const& effect : spellInfo->Effects)
        {
            if (effect.ApplyAuraName == SPELL_AURA_MOUNTED)
            {
                isMountSpell = true;
                break;
            }
        }

        if (!isMountSpell)
            return;

        uint32 teamId = 2;
        auto it = s_mountTeamMap.find(spellID);
        if (it != s_mountTeamMap.end())
            teamId = it->second;

        uint32 accountId = player->GetSession()->GetAccountId();
        LoginDatabase.Execute("INSERT IGNORE INTO `mod_mounts_on_account` (`account_id`, `team_id`, `spell_id`) VALUES ({}, {}, {});",
            accountId, teamId, spellID);
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (moa.enableCast)
            CustomLearnSpell(player, spell->GetSpellInfo()->Id);
    }

    void OnPlayerLearnSpell(Player* player, uint32 spellID) override
    {
        if (moa.enableLearn)
            CustomLearnSpell(player, spellID);
    }
};

class MOAWorld : public WorldScript
{
public:
    MOAWorld() : WorldScript("MOAWorld") { }

    void OnStartup() override
    {
        ItemTemplateContainer const* items = sObjectMgr->GetItemTemplateStore();
        for (auto const& [entry, item] : *items)
        {
            // Spells[1].SpellId corresponds to spellid_2 in item_template:
            // the "on use" effect that teaches the mount riding spell.
            int32 spellId = item.Spells[1].SpellId;
            if (spellId <= 0)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
                continue;

            for (SpellEffectInfo const& effect : spellInfo->Effects)
            {
                if (effect.ApplyAuraName != SPELL_AURA_MOUNTED)
                    continue;

                uint32 teamId = 2;
                if (item.AllowableRace == ALLIANCE_RACE_MASK)
                    teamId = TEAM_ALLIANCE;
                else if (item.AllowableRace == HORDE_RACE_MASK)
                    teamId = TEAM_HORDE;

                s_mountTeamMap[spellId] = teamId;
                break;
            }
        }

        LOG_INFO("module", "MOA: Cached {} mount-to-faction mappings.", s_mountTeamMap.size());
    }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        sConfigMgr->LoadModulesConfigs();
        moa.enable = sConfigMgr->GetOption<bool>("moa.enable", true);
        moa.message = sConfigMgr->GetOption<uint32>("moa.message.id", 45000);
        moa.enableCast = sConfigMgr->GetOption<bool>("moa.enable.cast", true);
        moa.enableLearn = sConfigMgr->GetOption<bool>("moa.enable.learn", true);
        moa.enableLearnOnLogin = sConfigMgr->GetOption<bool>("moa.enable.learn.on.login", false);
    }
};

void AddMOAPlayerScripts()
{
    new MOAPlayer();
    new MOAWorld();
}
