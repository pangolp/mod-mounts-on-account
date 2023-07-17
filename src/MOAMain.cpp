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

struct MOA
{
    uint32 message;
    bool enable, enableCast, enableLearn;
};

MOA moa;

class MOAPlayer : public PlayerScript
{
public:
    MOAPlayer() : PlayerScript("MOAPlayer") { }

    void OnLogin(Player* player) override
    {
        if (moa.enable)
            ChatHandler(player->GetSession()).PSendSysMessage(moa.message);
    }

    void CustomLearnSpell(Player* player, uint64 spellID)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellID);

        if (spellInfo->Mechanic & MECHANIC_MOUNT)
        {
            result = WorldDatabase.Query("SELECT `entry` FROM `item_template` WHERE `spellid_2`={};", spellID);

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate((*result)[0].Get<int32>());

            if (!itemTemplate)
                return;

            uint32 playerTeam = 2;

            if ((int)itemTemplate->AllowableRace == 690 || (int)itemTemplate->AllowableRace == 1101)
                playerTeam = player->GetTeamId();

            uint32 accountID = player->GetSession()->GetAccountId();

            result = LoginDatabase.Query("SELECT `spell_id` FROM `mod_mounts_on_account` WHERE `account_id`={} AND `spell_id`={};", accountID, spellID);

            if (!result)
                result = LoginDatabase.Query("INSERT INTO `mod_mounts_on_account` (`account_id`, `team_id`, `spell_id`) VALUES ({}, {}, {});", accountID, playerTeam, spellID);
        }
    }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (moa.enableCast)
        {
            spellID = spell->GetSpellInfo()->Id;
            CustomLearnSpell(player, spellID);
        }
    }

    void OnLearnSpell(Player* player, uint32 spellID) override
    {
        if (moa.enableLearn)
        {
            CustomLearnSpell(player, spellID);
        }
    }

private:
    uint64 spellID;
    QueryResult result;
};

class MOAWorld : public WorldScript
{
public:
    MOAWorld() : WorldScript("MOAWorld") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload)
        {
            sConfigMgr->LoadModulesConfigs();
            moa.enable = sConfigMgr->GetOption<bool>("moa.enable", true);
            moa.message = sConfigMgr->GetOption<uint32>("moa.message.id", 45000);
            moa.enableCast = sConfigMgr->GetOption<bool>("moa.enable.cast", true);
            moa.enableLearn = sConfigMgr->GetOption<bool>("moa.enable.learn", true);
        }
    }
};

void AddMOAPlayerScripts()
{
    new MOAPlayer();
    new MOAWorld();
}
