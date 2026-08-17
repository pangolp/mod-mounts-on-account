/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "AsyncCallbackProcessor.h"
#include "StringFormat.h"
#include "SharedDefines.h"
#if __has_include("Playerbots.h")
#include "Playerbots.h"
#define MOA_HAS_PLAYERBOTS 1
#else
#define MOA_HAS_PLAYERBOTS 0
#endif
#include <unordered_map>
#include <vector>

struct MOA
{
    uint32 message;
    bool enable, enableCast, enableLearn;
    bool enableLearnOnLogin;
    bool enableAccountCache;
    bool skipBotsOnLogin;
};

MOA moa;

// Built once at startup: mount spell ID -> team_id (0=Alliance, 1=Horde, 2=neutral)
static std::unordered_map<uint32, uint32> s_mountTeamMap;
// Built once at startup: mount spell ID -> required riding skill rank (75, 150, 225, 300)
static std::unordered_map<uint32, uint32> s_mountSkillMap;
// Account-level cache of learned mounts: account_id -> (spell_id, team_id)
static std::unordered_map<uint32, std::vector<std::pair<uint32, uint32>>> s_accountMountCache;

// Class-specific mount spell ID -> required class
static const std::unordered_map<uint32, uint8> s_mountClassMap = {
    // Death Knight
    {48778, CLASS_DEATH_KNIGHT},
    {54729, CLASS_DEATH_KNIGHT},
    // Warlock
    {5784, CLASS_WARLOCK},
    {23161, CLASS_WARLOCK},
    // Paladin
    {34769, CLASS_PALADIN},
    {13819, CLASS_PALADIN},
    {23214, CLASS_PALADIN},
    {34767, CLASS_PALADIN}
};

// Class-specific mount spell ID -> required riding skill rank (not obtainable from items)
static const std::unordered_map<uint32, uint32> s_classMountSkillMap = {
    // Death Knight
    {48778, 75},
    {54729, 225},
    // Warlock
    {5784, 75},
    {23161, 150},
    // Paladin
    {34769, 75},
    {13819, 75},
    {23214, 150},
    {34767, 150}
};

// Class-specific mount spell ID -> required team/faction (not obtainable from items)
static const std::unordered_map<uint32, uint32> s_classMountTeamMap = {
    // Alliance Paladin
    {13819, TEAM_ALLIANCE},
    {23214, TEAM_ALLIANCE},
    // Horde Paladin
    {34769, TEAM_HORDE},
    {34767, TEAM_HORDE}
};

// Processes async login queries on every world tick
static QueryCallbackProcessor s_loginQueryProcessor;

static constexpr uint32 ALLIANCE_RACE_MASK      = 1101; // Human|Dwarf|NightElf|Gnome|Draenei
static constexpr uint32 HORDE_RACE_MASK         = 690;  // Orc|Undead|Tauren|Troll|BloodElf

// Riding Skill Spells
static constexpr uint32 SPELL_RIDING_APPRENTICE = 33388; // grants riding skill 75
static constexpr uint32 SPELL_RIDING_JOURNEYMAN = 33391; // grants riding skill 150
static constexpr uint32 SPELL_RIDING_EXPERT     = 34090; // grants riding skill 225
static constexpr uint32 SPELL_RIDING_ARTISAN    = 34091; // grants riding skill 300

class MOAPlayer : public PlayerScript
{
public:
    MOAPlayer() : PlayerScript("MOAPlayer") { }

    static bool IsPlayerBot(Player* player)
    {
#if MOA_HAS_PLAYERBOTS
        return sPlayerbotsMgr.GetPlayerbotAI(player) != nullptr;
#else
        (void)player;
        return false;
#endif
    }

    static bool IsRidingSkillSpell(uint32 spellID)
    {
        SpellLearnSkillNode const* learnSkill = sSpellMgr->GetSpellLearnSkill(spellID);
        return learnSkill && learnSkill->skill == SKILL_RIDING;
    }

    static void ApplyMountsToPlayer(Player* player, std::vector<std::pair<uint32, uint32>> const& accountMounts)
    {
        uint16 playerRidingSkill = player->GetSkillValue(SKILL_RIDING);

        if (player->HasSpell(SPELL_RIDING_ARTISAN))
            playerRidingSkill = std::max<uint16>(playerRidingSkill, 300);
        else if (player->HasSpell(SPELL_RIDING_EXPERT))
            playerRidingSkill = std::max<uint16>(playerRidingSkill, 225);
        else if (player->HasSpell(SPELL_RIDING_JOURNEYMAN))
            playerRidingSkill = std::max<uint16>(playerRidingSkill, 150);
        else if (player->HasSpell(SPELL_RIDING_APPRENTICE))
            playerRidingSkill = std::max<uint16>(playerRidingSkill, 75);

        uint8 playerClass = player->getClass();
        uint32 playerTeamId = player->GetTeamId();

        for (auto const& [spellID, spellTeamId] : accountMounts)
        {
            if (spellTeamId != playerTeamId && spellTeamId != 2)
                continue;

            auto classIt = s_mountClassMap.find(spellID);
            if (classIt != s_mountClassMap.end() && playerClass != classIt->second)
                continue;

            uint32 reqSkill = 0;
            auto it = s_mountSkillMap.find(spellID);
            if (it != s_mountSkillMap.end())
                reqSkill = it->second;

            if (playerRidingSkill >= reqSkill && !player->HasSpell(spellID))
                player->learnSpell(spellID);
        }
    }

    // Helper function to sync mounts from DB based on current skill
    void SyncAccountMounts(Player* player)
    {
        uint32 accountId = player->GetSession()->GetAccountId();

        if (moa.enableAccountCache)
        {
            auto cacheIt = s_accountMountCache.find(accountId);
            if (cacheIt != s_accountMountCache.end())
            {
                ApplyMountsToPlayer(player, cacheIt->second);
                return;
            }
        }

        ObjectGuid guid = player->GetGUID();

        s_loginQueryProcessor.AddCallback(
            LoginDatabase.AsyncQuery(
                Acore::StringFormat("SELECT `spell_id`, `team_id` FROM `mod_mounts_on_account` WHERE `account_id`={};", accountId)
            ).WithCallback([guid](QueryResult result)
            {
                Player* p = ObjectAccessor::FindConnectedPlayer(guid);
                if (!p || !result)
                    return;

                std::vector<std::pair<uint32, uint32>> accountMounts;
                accountMounts.reserve(result->GetRowCount());

                do
                {
                    uint32 spellID = (*result)[0].Get<uint32>();
                    uint32 teamID = (*result)[1].Get<uint32>();
                    accountMounts.emplace_back(spellID, teamID);
                } while (result->NextRow());

                if (moa.enableAccountCache)
                {
                    uint32 accountId = p->GetSession()->GetAccountId();
                    s_accountMountCache[accountId] = accountMounts;
                }

                MOAPlayer::ApplyMountsToPlayer(p, accountMounts);
            })
        );
    }

    void OnPlayerLogin(Player* player) override
    {
        if (moa.enable)
            ChatHandler(player->GetSession()).PSendSysMessage(moa.message);

        if (moa.skipBotsOnLogin && IsPlayerBot(player))
            return;

        if (moa.enableLearnOnLogin)
            SyncAccountMounts(player);
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

        if (moa.enableAccountCache)
        {
            auto cacheIt = s_accountMountCache.find(accountId);
            if (cacheIt != s_accountMountCache.end())
            {
                bool exists = false;
                for (auto const& [cachedSpellId, _] : cacheIt->second)
                {
                    if (cachedSpellId == spellID)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                    cacheIt->second.emplace_back(spellID, teamId);
            }
        }
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        uint32 const spellID = spell->GetSpellInfo()->Id;

        if (moa.enableCast)
            CustomLearnSpell(player, spellID);
    }

    void OnPlayerLearnSpell(Player* player, uint32 spellID) override
    {
        // 1. Check if the learned spell is a mount and save it to the DB
        if (moa.enableLearn)
            CustomLearnSpell(player, spellID);

        // 2. Check if the learned spell is a Riding Skill upgrade. 
        // If yes, dynamically trigger a sync to grant available account mounts immediately.
        if (IsRidingSkillSpell(spellID))
        {
            SyncAccountMounts(player);
        }
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

                // Cache the required riding skill from the item template
                if (item.RequiredSkill == SKILL_RIDING)
                    s_mountSkillMap[spellId] = item.RequiredSkillRank;

                break;
            }
        }

        // Populate class-specific mounts that do not have associated items
        for (auto const& [spellId, skill] : s_classMountSkillMap)
            s_mountSkillMap[spellId] = skill;

        for (auto const& [spellId, teamId] : s_classMountTeamMap)
            s_mountTeamMap[spellId] = teamId;

        LOG_INFO("module", "MOA: Cached {} mount-to-faction mappings.", s_mountTeamMap.size());
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        s_loginQueryProcessor.ProcessReadyCallbacks();
    }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        sConfigMgr->LoadModulesConfigs();

        moa.enable = sConfigMgr->GetOption<bool>("moa.enable", true);
        moa.message = sConfigMgr->GetOption<uint32>("moa.message.id", 45000);
        moa.enableCast = sConfigMgr->GetOption<bool>("moa.enable.cast", false);
        moa.enableLearn = sConfigMgr->GetOption<bool>("moa.enable.learn", true);
        moa.enableLearnOnLogin = sConfigMgr->GetOption<bool>("moa.enable.learn.on.login", false);
        moa.enableAccountCache = sConfigMgr->GetOption<bool>("moa.enable.account.cache", true);
        moa.skipBotsOnLogin = sConfigMgr->GetOption<bool>("moa.skip.bots.on.login", true);
    }
};

void AddMOAPlayerScripts()
{
    new MOAPlayer();
    new MOAWorld();
}

