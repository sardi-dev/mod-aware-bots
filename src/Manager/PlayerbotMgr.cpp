/*
* This file is part of the Legends of Azeroth Pandaria Project. See THANKS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "PlayerbotMgr.h"
#include "ActivityWakePolicy.h"
#include "RBAC.h"

#include <cstdio>
#include <cstring>
#include <istream>
#include <string>

#include "BotFactory.h"
#include <map>
#include <mutex>
#include "Helper.h"
#include "PlayerbotAIConfig.h"
#include "CharacterCache.h"
#include "ChannelMgr.h"
#include "CharacterHandler.h"
#include "Common.h"
#include "BotAccounts.h"
#include "Chat.h"
#include "Define.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "BotGuid.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "WorldSession.h"
#include "ChannelMgr.h"
#include "Log.h"


// #161: ".npcbot addclass" used to randomize and equip the added bot from a detached std::thread, off the
// world thread. Record the request here and apply it in PlayerbotMgr::OnBotLoginInternal, which runs on the
// world thread once the bot is in the world (AC queues the same work onto its world-thread processor).
static std::mutex sAddclassMutex;
static std::map<BotGuid, uint32> sAddclassPending; // bot guid -> master level at the time of the command

PlayerbotHolder::PlayerbotHolder() : PlayerbotAIBase(false) {}
void PlayerbotHolder::AddPlayerBot(BotGuid playerGuid, uint32 masterAccountId)
{
    // bot is loading
    if (botLoading.find(playerGuid) != botLoading.end())
    {
        SF_LOG_DEBUG("playerbots", "Bot %u is already loading", playerGuid.GetCounter());
        return;
    }

    // has bot already been added?
    Player* bot = ObjectAccessor::FindPlayer(playerGuid);
    if (bot && bot->IsInWorld())
    {
        SF_LOG_DEBUG("playerbots", "Bot %u is already in game", playerGuid.GetCounter());
        return;
    }

    uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(playerGuid);
    if (!accountId)
    {
        SF_LOG_DEBUG("playerbots", "Bot %u has invalid accountid", playerGuid.GetCounter());
        return;
    }

    // #934 (AC Bot/PlayerbotMgr.cpp AddPlayerBot): decide whether the master may control this character BEFORE it is
    // loaded, with the rule HandlePlayerBotLoginCallback applies after login (same account, same guild, random-bot
    // account; addclass picks come from random accounts). Now that ".playerbots bot" is a player command, a character of
    // someone else's account is refused here and never logs in; the master is told why (it only reached the log before).
    // Bot accounts (design decision 0018) and the AllowAccountBots / AllowGuildBots settings decide too, with the
    // same rule as the callback.
    bool const ownBotAccount = sPlayerbotAIConfig->allowBotAccounts && BotAccounts::IsOwnedBy(accountId, masterAccountId);
    bool const sameAccountRefused = accountId == masterAccountId && !sPlayerbotAIConfig->allowAccountBots;
    if (masterAccountId && !ownBotAccount && !sPlayerbotAIConfig->IsInRandomAccountList(accountId) &&
        (accountId != masterAccountId || sameAccountRefused))
    {
        WorldSession* masterSession = sWorld->FindSession(masterAccountId);
        Player* masterPlayer = masterSession ? masterSession->GetPlayer() : nullptr;
        BotGuid::LowType const guildId = sCharacterCache->GetCharacterGuildIdByGuid(playerGuid);
        bool const sameGuild = sPlayerbotAIConfig->allowGuildBots && masterPlayer && guildId &&
            guildId == masterPlayer->GetGuildId();
        if (sameAccountRefused || !sameGuild)
        {
            std::string botName;
            sCharacterCache->GetCharacterNameByGuid(playerGuid, botName);
            if (masterSession)
                ChatHandler(masterSession).PSendSysMessage("Failure: You are not allowed to control bot %s", botName.c_str());
            SF_LOG_DEBUG("playerbots", "Bot %u refused: account %u is not the master's (%u), guild or a random-bot account",
                playerGuid.GetCounter(), accountId, masterAccountId);
            return;
        }
    }

    // SkyFire's bot login: a socketless session owned by this holder loads the character asynchronously;
    // PollBotLogins finishes it (LoA used a LoginQueryHolder subclass, which SkyFire keeps private).
    WorldSession* botSession = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, uint8(Expansions::EXPANSION_MISTS_OF_PANDARIA),
        0, LOCALE_enUS, 0, false, false, false, false, std::string(), SessionKey{});
    botSession->SetBot(true);
    // BT-136: a real client names its realm at login and Player::LoadFromDB copies it into the public field
    // PLAYER_FIELD_VIRTUAL_PLAYER_REALM. A bot session has no login packet, so every bot carried realm 0 there, and a
    // client that sees such a player cannot resolve its name ("Unknown" above the head and in the unit frames).
    botSession->SetVirtualRealmID(realmID);
    if (!botSession->BeginBotCharacterLogin(playerGuid))
    {
        SF_LOG_ERROR("playerbots", "Bot %u: the character login could not start", playerGuid.GetCounter());
        delete botSession;
        return;
    }

    botLoading.insert(playerGuid);
    botPendingLogins[playerGuid] = PendingBotLogin{ botSession, masterAccountId };
}

std::vector<WorldSession*> PlayerbotHolder::abandonedLogins;

void PlayerbotHolder::AbandonPendingLogins()
{
    for (auto const& entry : botPendingLogins)
        abandonedLogins.push_back(entry.second.session);

    botPendingLogins.clear();
    botLoading.clear();
}

void PlayerbotHolder::PollAbandonedLogins()
{
    for (auto itr = abandonedLogins.begin(); itr != abandonedLogins.end();)
    {
        // discard: a finished query is freed without putting the character in the world
        if ((*itr)->PollBotCharacterLogin(true) == WorldSession::BotLoginPollStatus::Pending)
        {
            ++itr;
            continue;
        }

        delete *itr;
        itr = abandonedLogins.erase(itr);
    }
}

void PlayerbotHolder::PollBotLogins()
{
    for (auto itr = botPendingLogins.begin(); itr != botPendingLogins.end();)
    {
        BotGuid const botGuid = itr->first;
        PendingBotLogin const pending = itr->second;
        WorldSession::BotLoginPollStatus const status = pending.session->PollBotCharacterLogin();
        if (status == WorldSession::BotLoginPollStatus::Pending)
        {
            ++itr;
            continue;
        }

        itr = botPendingLogins.erase(itr);
        if (status == WorldSession::BotLoginPollStatus::Failed || !pending.session->GetPlayer())
        {
            SF_LOG_DEBUG("playerbots", "Bot %u could not be loaded", botGuid.GetCounter());
            delete pending.session;
            botLoading.erase(botGuid);
            continue;
        }

        BotCheckPowerType(pending.session->GetPlayer(), "after core login", false);
        HandlePlayerBotLoginCallback(pending.session, pending.masterAccountId, botGuid);
    }
}

void PlayerbotHolder::HandlePlayerBotLoginCallback(WorldSession* botSession, uint32 masterAccountId, BotGuid botGUID)
{
    uint32 botAccountId = botSession->GetAccountId();
    Player* bot = botSession->GetPlayer();
    if (!bot)
    {
        SF_LOG_DEBUG("playerbots", "Bot player could not be loaded for account ID: %u", botAccountId);
        botSession->LogoutPlayer(true);
        delete botSession;
        botLoading.erase(botGUID);
        return;
    }

    uint32 masterAccount = masterAccountId;
    WorldSession* masterSession = masterAccount ? sWorld->FindSession(masterAccount) : nullptr;

    // Check if masterSession->GetPlayer() is valid
    Player* masterPlayer = masterSession ? masterSession->GetPlayer() : nullptr;
    if (masterSession && !masterPlayer)
    {
        SF_LOG_DEBUG("playerbots", "Master session found but no player is associated for master account ID: %u", masterAccount);
    }

    std::ostringstream out;
    // Who may control this character as a bot: random bots, a bot account owned by the master's account
    // (design decision 0018), and, only where the server allows it, the master's own account and guild members.
    // LoA allowed the same account and the guild regardless of AllowAccountBots and AllowGuildBots.
    bool const isRandomBot = sPlayerbotAIConfig->IsInRandomAccountList(botAccountId);
    bool const ownBotAccount = sPlayerbotAIConfig->allowBotAccounts && BotAccounts::IsOwnedBy(botAccountId, masterAccount);
    bool const sameAccount = sPlayerbotAIConfig->allowAccountBots && botAccountId == masterAccount;
    bool const sameGuild = sPlayerbotAIConfig->allowGuildBots && masterPlayer && bot->GetGuildId() != 0 &&
        bot->GetGuildId() == masterPlayer->GetGuildId();
    bool allowed = isRandomBot || ownBotAccount || sameAccount || sameGuild;
    if (!allowed)
        out << "Failure: You are not allowed to control bot " << bot->GetName().c_str();

    if (allowed && masterSession && masterPlayer)
    {
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(masterPlayer);
        if (!mgr)
        {
            SF_LOG_DEBUG("playerbots", "PlayerbotMgr not found for master player with GUID: %u", masterPlayer->GetGUIDLow());
        }
        else
        {
            uint32 count = mgr->GetPlayerbotsCount();
            uint32 cls_count = mgr->GetPlayerbotsCountByClass(bot->getClass());
            if (count >= sPlayerbotAIConfig->maxAddedBots)
            {
                allowed = false;
                out << "Failure: You have added too many bots";
            }
            else if (cls_count >= sPlayerbotAIConfig->maxAddedBotsPerClass)
            {
                allowed = false;
                out << "Failure: You have added too many bots for this class";
            }
        }
    }

    if (allowed)
    {
        sRandomPlayerbotMgr->OnPlayerLogin(bot);
        OnBotLogin(bot);
        BotCheckPowerType(bot, "after bot login", false);

        SF_LOG_DEBUG("playerbots", "Player logged: %s", bot->GetName().c_str());
    }
    else
    {
        SF_LOG_ERROR("playerbots", "Bot error on logging: %s", out.str().c_str());
        if (masterSession)
            ChatHandler(masterSession).SendSysMessage(out.str().c_str());
        botSession->LogoutPlayer(true);
        delete botSession;
    }

    botLoading.erase(botGUID);
}

std::vector<Player*> PlayerbotHolder::GetPlayerBotsSnapshot() const
{
    std::shared_lock<std::shared_mutex> guard(playerBotsMutex);

    std::vector<Player*> bots;
    bots.reserve(playerBots.size());
    for (auto const& itr : playerBots)
        if (itr.second)
            bots.push_back(itr.second);

    return bots;
}

void PlayerbotHolder::UpdateSessions()
{
    PollBotLogins();

    for (PlayerBotMap::const_iterator itr = GetPlayerBotsBegin(); itr != GetPlayerBotsEnd(); ++itr)
    {
        Player* const bot = itr->second;
        if (bot->IsBeingTeleported())
        {
            PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
            if (botAI)
            {
                botAI->HandleTeleportAck();
            }
        }
        else if (bot->IsInWorld())
        {
            HandleBotPackets(bot->GetSession());
        }
    }
}

void PlayerbotHolder::HandleBotPackets(WorldSession* session)
{
    /*WorldPacket* packet;
    while (session->GetPacketQueue().next(packet))
    {
        OpcodeClient opcode = static_cast<OpcodeClient>(packet->GetOpcode());
        ClientOpcodeHandler const* opHandle = opcodeTable[opcode];
        opHandle->Call(session, *packet);
        delete packet;
    }*/
}

void PlayerbotHolder::LogoutAllBots()
{
    /*
    while (true)
    {
        PlayerBotMap::const_iterator itr = GetPlayerBotsBegin();
        if (itr == GetPlayerBotsEnd())
            break;

        Player* bot= itr->second;
        if (!GET_PLAYERBOT_AI(bot)->IsRealPlayer())
            LogoutPlayerBot(bot->GetGUID());
    }
    */

    PlayerBotMap bots = playerBots;
    for (auto& itr : bots)
    {
        Player* bot = itr.second;
        if (!bot)
            continue;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->IsRealPlayer())
            continue;

        LogoutPlayerBot(bot->GetGUID());
    }

    // #503: a selfbot's AI dies with its player. AC deletes it from MiscScript::OnDestructPlayer (AC Script/Playerbots.cpp:
    // 400-406) and its ~PlayerbotAI erases the map entry; this core has no player-destruct hook, so it is done here, from the
    // WorldSession::LogoutPlayer -> OnPlayerbotLogout call (m_playerLogout is already set there; a CMSG_LOGOUT_REQUEST that can
    // still be cancelled does not set it). Without this the next login of the same character would find the freed player's AI.
    if (PlayerbotMgr* mgr = dynamic_cast<PlayerbotMgr*>(this))
    {
        Player* master = mgr->GetMaster();
        if (master && master->GetSession() && master->GetSession()->PlayerLogout())
        {
            PlayerbotAI* selfAI = GET_PLAYERBOT_AI(master);
            if (selfAI && selfAI->IsRealPlayer())
            {
                sPlayerbotsMgr->RemovePlayerBotData(master->GetGUID(), true);
                delete selfAI;
            }
        }
    }
}

void PlayerbotMgr::CancelLogout()
{
    Player* master = GetMaster();
    if (!master)
        return;

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->IsRealPlayer())
            continue;

        if (bot->GetSession()->isLogingOut())
        {
            /*WorldPackets::Character::LogoutCancel data = WorldPacket(CMSG_LOGOUT_CANCEL);
            bot->GetSession()->HandleLogoutCancelOpcode(data);
            botAI->TellMaster("Logout cancelled!");*/
        }
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr->GetPlayerBotsBegin();
        it != sRandomPlayerbotMgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->IsRealPlayer())
            continue;

        if (botAI->GetMaster() != master)
            continue;

        if (bot->GetSession()->isLogingOut())
        {
            /*WorldPackets::Character::LogoutCancel data = WorldPacket(CMSG_LOGOUT_CANCEL);
            bot->GetSession()->HandleLogoutCancelOpcode(data);*/
        }
    }
}

void PlayerbotHolder::LogoutPlayerBot(BotGuid guid)
{
    if (Player* bot = GetPlayerBot(guid))
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
            return;

        Group* group = bot->GetGroup();
        if (group && !bot->InBattleground() && !bot->InBattlegroundQueue() && botAI->HasActivePlayerMaster())
        {
            //sPlayerbotDbStore->Save(botAI);
        }

        SF_LOG_INFO("playerbots", "Bot %s logging out", bot->GetName().c_str());
        bot->SaveToDB(false);

        WorldSession* botWorldSessionPtr = bot->GetSession();
        WorldSession* masterWorldSessionPtr = nullptr;

        if (botWorldSessionPtr->isLogingOut())
            return;

        Player* master = botAI->GetMaster();
        if (master)
            masterWorldSessionPtr = master->GetSession();

        // check for instant logout
        bool logout = botWorldSessionPtr->ShouldLogOut(time(nullptr));

        if (masterWorldSessionPtr && masterWorldSessionPtr->ShouldLogOut(time(nullptr)))
            logout = true;

        if (masterWorldSessionPtr && !masterWorldSessionPtr->GetPlayer())
            logout = true;

        if (bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_RESTING) || bot->HasUnitState(UNIT_STATE_IN_FLIGHT) ||
            botWorldSessionPtr->GetSecurity() >= AccountTypes::SEC_PLAYER)
        {
            logout = true;
        }

        if (master &&
            (master->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_RESTING) || master->HasUnitState(UNIT_STATE_IN_FLIGHT) ||
                (masterWorldSessionPtr &&
                    masterWorldSessionPtr->GetSecurity() >= AccountTypes::SEC_PLAYER)))
        {
            logout = true;
        }

        //TravelTarget* target = nullptr;
        //if (botAI->GetAiObjectContext())  // Maybe some day re-write to delate all pointer values.
        {
            //target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
        }

        // Peiru: Allow bots to always instant logout to see if this resolves logout crashes
        logout = true;

        // if no instant logout, request normal logout
        if (!logout)
        {
            if (bot->GetSession()->isLogingOut())
                return;
            else if (bot)
            {
                /*botAI->TellMaster("I'm logging out!");
                WorldPackets::Character::LogoutRequest data = WorldPacket(CMSG_LOGOUT_REQUEST);
                botWorldSessionPtr->HandleLogoutRequestOpcode(data);
                if (!bot)
                {
                    { std::unique_lock<std::shared_mutex> guard(playerBotsMutex); playerBots.erase(guid); }
                    delete botWorldSessionPtr;
                    if (target)
                        delete target;
                }*/
                return;
            }
            else
            {
                { std::unique_lock<std::shared_mutex> guard(playerBotsMutex); playerBots.erase(guid); }     // deletes bot player ptr inside this WorldSession PlayerBotMap
                delete botWorldSessionPtr;  // finally delete the bot's WorldSession
                //if (target)
                    //delete target;
            }
            return;
        }  // if instant logout possible, do it
        else if (bot && (logout || !botWorldSessionPtr->isLogingOut()))
        {
            botAI->TellMaster("Goodbye!");
            { std::unique_lock<std::shared_mutex> guard(playerBotsMutex); playerBots.erase(guid); }                  // deletes bot player ptr inside this WorldSession PlayerBotMap
            // The logout hook (RandomPlayerbotMgr::OnPlayerLogout -> DisablePlayerBot) looks the bot up in playerBots,
            // which no longer holds it, so it cannot delete the AI: delete it here while the Player still exists (its
            // destructor reads the bot to drop the AI map entry). Every break and scheduled logoff leaked one AI.
            delete botAI;
            botWorldSessionPtr->LogoutPlayer(true);  // this deletes the bot Player object
            delete botWorldSessionPtr;               // finally delete the bot's WorldSession
        }
    }
}

void PlayerbotHolder::DisablePlayerBot(BotGuid guid)
{
    if (Player* bot = GetPlayerBot(guid))
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
        {
            return;
        }
        botAI->TellMaster("Goodbye!");
        bot->StopMoving();
        bot->GetMotionMaster()->Clear();

        Group* group = bot->GetGroup();
        if (group && !bot->InBattleground() && !bot->InBattlegroundQueue() && botAI->HasActivePlayerMaster())
        {
            //sPlayerbotDbStore->Save(botAI);
        }

        SF_LOG_DEBUG("playerbots", "Bot %s logged out", bot->GetName().c_str());

        bot->SaveToDB(false);

        //if (botAI->GetAiObjectContext())  // Maybe some day re-write to delate all pointer values.
        //{
        //    TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
        //    if (target)
        //        delete target;
        //}

        { std::unique_lock<std::shared_mutex> guard(playerBotsMutex); playerBots.erase(guid); }  // deletes bot player ptr inside this WorldSession PlayerBotMap

        delete botAI;
    }
}

Player* PlayerbotHolder::GetPlayerBot(BotGuid::LowType lowGuid) const
{
    BotGuid playerGuid = BotGuid::Create<BotHighGuid::Player>(lowGuid);
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? 0 : it->second;
}

Player* PlayerbotHolder::GetPlayerBot(BotGuid playerGuid) const
{
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? 0 : it->second;
}

void PlayerbotHolder::OnBotLogin(Player* const bot)
{
    // Prevent duplicate login
    if (playerBots.find(bot->GetGUID()) != playerBots.end())
    {
        return;
    }

    sPlayerbotsMgr->AddPlayerbotData(bot, true);
    {
        std::unique_lock<std::shared_mutex> guard(playerBotsMutex);
        playerBots[bot->GetGUID()] = bot;
    }
    OnBotLoginInternal(bot);

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
    {
        // Log a warning here to indicate that the botAI is null
        //SF_LOG_DEBUG("playerbots", "PlayerbotAI is null for bot with GUID: %u", bot->GetGUID());
        return;
    }

    Player* master = botAI->GetMaster();
    if (!master)
    {
        // Log a warning to indicate that the master is null
        //SF_LOG_DEBUG("playerbots", "Master is null for bot with GUID: %u", bot->GetGUID());
        return;
    }

    Group* group = bot->GetGroup();
    if (group)
    {
        bool groupValid = false;
        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::MemberSlotList::const_iterator i = slots.begin(); i != slots.end(); ++i)
        {
            BotGuid member = i->guid;
            if (master)
            {
                if (master->GetGUID() == member)
                {
                    groupValid = true;
                    break;
                }
            }
            else
            {
                uint32 account = sCharacterCache->GetCharacterAccountIdByGuid(member);
                if (!sPlayerbotAIConfig->IsInRandomAccountList(account))
                {
                    groupValid = true;
                    break;
                }
            }
        }

        if (!groupValid)
        {
            bot->RemoveFromGroup();
        }
    }

    group = bot->GetGroup();
    if (group)
    {
        botAI->ResetStrategies();
    }
    else
    {
        botAI->ResetStrategies();
    }

    if (master && !master->HasUnitState(UNIT_STATE_IN_FLIGHT))
    {
        bot->GetMotionMaster()->MovementExpired();
        bot->CleanupAfterTaxiFlight();
    }

    // check activity
    botAI->AllowActivity(ALL_ACTIVITY, true);

    // set delay on login
    botAI->SetNextCheckDelay(urand(2000, 4000));

    botAI->TellMaster("Hello!");

    if (master && master->GetGroup() && !group)
    {
        Group* mgroup = master->GetGroup();
        if (mgroup->GetMembersCount() >= 5)
        {
            if (!mgroup->isRaidGroup() && !mgroup->isLFGGroup() && !mgroup->isBGGroup() && !mgroup->isBFGroup())
            {
                mgroup->ConvertToRaid();
            }
            if (mgroup->isRaidGroup())
            {
                mgroup->AddMember(bot);
            }
        }
        else
        {
            mgroup->AddMember(bot);
        }
    }
    else if (master && !group)
    {
        Group* newGroup = new Group();
        newGroup->Create(master);
        sGroupMgr->AddGroup(newGroup);
        newGroup->AddMember(bot);
    }

    if (master)
    {
        bot->TeleportTo(BotGetWorldLocation(master));
    }

    uint32 accountId = bot->GetSession()->GetAccountId();
    /*bool isRandomAccount = sPlayerbotAIConfig->IsInRandomAccountList(accountId);

    if (isRandomAccount && sPlayerbotAIConfig->randomBotFixedLevel)
    {
        bot->SetPlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
    }
    else if (isRandomAccount && !sPlayerbotAIConfig->randomBotFixedLevel)
    {
        bot->RemovePlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
    }*/

    bot->SaveToDB(false);
    /*if (master && isRandomAccount && master->getLevel() < bot->getLevel())
    {
        uint32 mixedGearScore =
            PlayerbotAI::GetMixedGearScore(master, true, false, 12) * sPlayerbotAIConfig->autoInitEquipLevelLimitRatio;
        PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_LEGENDARY, mixedGearScore);
        factory.Randomize(false);
    }

    // bots join World chat if not solo oriented
    if (bot->getLevel() >= 10 && sRandomPlayerbotMgr->IsRandomBot(bot) && GET_PLAYERBOT_AI(bot) &&
        GET_PLAYERBOT_AI(bot)->GetGrouperType() != GrouperType::SOLO)
    {
        // TODO make action/config
        // Make the bot join the world channel for chat
        WorldPacket pkt(CMSG_CHAT_JOIN_CHANNEL);
        pkt << uint32(0) << uint8(0) << uint8(0);
        pkt << std::string("World");
        pkt << "";  // Pass
        bot->GetSession()->HandleJoinChannel(pkt);
    }

    // join standard channels
    uint8 locale = BroadcastHelper::GetLocale();
    AreaTableEntry const* current_zone = GET_PLAYERBOT_AI(bot)->GetCurrentZone();
    ChannelMgr* cMgr = ChannelMgr::forTeam(bot->GetTeamId());
    std::string current_zone_name = current_zone ? GET_PLAYERBOT_AI(bot)->GetLocalizedAreaName(current_zone) : "";

    if (current_zone && cMgr)
    {
        for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
        {
            ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(i);
            if (!channel)
                continue;

            Channel* new_channel = nullptr;
            switch (channel->ChannelID)
            {
            case ChatChannelId::GENERAL:
            case ChatChannelId::LOCAL_DEFENSE:
            {
                char new_channel_name_buf[100];
                snprintf(new_channel_name_buf, 100, channel->pattern[locale], current_zone_name.c_str());
                new_channel = cMgr->GetJoinChannel(new_channel_name_buf, channel->ChannelID);
                break;
            }
            case ChatChannelId::TRADE:
            case ChatChannelId::GUILD_RECRUITMENT:
            {
                char new_channel_name_buf[100];
                //3459 is ID for a zone named "City" (only exists for the sake of using its name)
                //Currently in magons TBC, if you switch zones, then you join "Trade - <zone>" and "GuildRecruitment - <zone>"
                //which is a core bug, should be "Trade - City" and "GuildRecruitment - City" in both 1.12 and TBC
                //but if you (actual player) logout in a city and log back in - you join "City" versions
                snprintf(new_channel_name_buf, 100, channel->pattern[locale], GET_PLAYERBOT_AI(bot)->GetLocalizedAreaName(GetAreaEntryByAreaID(3459)).c_str());
                new_channel = cMgr->GetJoinChannel(new_channel_name_buf, channel->ChannelID);
                break;
            }
            case ChatChannelId::LOOKING_FOR_GROUP:
            case ChatChannelId::WORLD_DEFENSE:
            {
                new_channel = cMgr->GetJoinChannel(channel->pattern[locale], channel->ChannelID);
                break;
            }
            default:
                break;
            }

            if (new_channel)
                new_channel->JoinChannel(bot, "");
        }
    }*/
}

// Ported from AC mod-playerbots Bot/PlayerbotMgr.cpp:675-708,721-822 (#177), the ".playerbots bot
// add|remove|login|logout|list|init" family (aliased to ".npcbot" on this fork, cs_playerbots.cpp).
// Trimmed from AC's version:
//  - "addaccount"'s security gate (AiPlayerbot.AllowAccountBots/AllowTrustedAccountBots) is kept;
//    the "linked accounts" half of it (IsAccountLinked) is dropped - this fork has no account-linking
//    feature (group-functionality-parity.md section 5.2 calls it out as optional/not built).
//  - "init=auto" (gearscore-matched init) needs a BotInitGuard concurrency lock and a
//    resetInstanceIdForAltBots/autoInitEquipLevelLimitRatio config pair that do not exist on this
//    fork; left out rather than guessed at (falls through to "unknown command").
//  - "refresh=raid"/"levelup"/"refresh"/"random"/"quests" are out of #177's scope (add/remove/login/
//    logout/list/init only) and are not ported here.
//  - the "addclass"-only restriction AC applies before init= (IsAddclassBot()) has no equivalent
//    tracking on this fork; init works on any of the master's own logged-in bots instead.
//  - the "bot not found" fallback that AC gets from sRandomPlayerbotMgr.GetPlayerBot(guid) (a lookup
//    this fork's RandomPlayerbotMgr does not expose) is replaced with ObjectAccessor::
//    FindConnectedPlayer(guid), equivalent for an already-logged-in bot.
std::string const PlayerbotHolder::ProcessBotCommand(std::string const cmd, BotGuid guid, BotGuid /*masterguid*/,
    bool /*admin*/, uint32 masterAccountId, uint32 /*masterGuildId*/)
{
    if (!sPlayerbotAIConfig->enabled || guid.IsEmpty())
        return "bot system is disabled";

    if (cmd == "add" || cmd == "addaccount" || cmd == "login")
    {
        if (ObjectAccessor::FindPlayer(guid))
            return "player already logged in";

        // Refuse before logging the character in when its account rules it out; the guild rule needs the loaded
        // character, so HandlePlayerBotLoginCallback still checks everything.
        if (uint32 const botAccountId = sCharacterCache->GetCharacterAccountIdByGuid(guid))
        {
            bool const accountAllowed = sPlayerbotAIConfig->IsInRandomAccountList(botAccountId) ||
                (sPlayerbotAIConfig->allowBotAccounts && BotAccounts::IsOwnedBy(botAccountId, masterAccountId)) ||
                (sPlayerbotAIConfig->allowAccountBots && botAccountId == masterAccountId);
            if (!accountAllowed && !sPlayerbotAIConfig->allowGuildBots)
                return "not allowed: only characters of your bot accounts (.playerbots account botmode)";
        }

        if (cmd == "addaccount")
        {
            uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
            if (!accountId)
                return "character not found";

            if (accountId == masterAccountId ? !sPlayerbotAIConfig->allowAccountBots
                                             : !(sPlayerbotAIConfig->allowBotAccounts && BotAccounts::IsOwnedBy(accountId, masterAccountId)))
                return "you can only add characters of your bot accounts";
        }

        AddPlayerBot(guid, masterAccountId);
        return "ok";
    }
    else if (cmd == "remove" || cmd == "logout" || cmd == "rm")
    {
        if (!ObjectAccessor::FindPlayer(guid))
            return "player is offline";

        if (!GetPlayerBot(guid))
            return "not your bot";

        LogoutPlayerBot(guid);
        return "ok";
    }

    Player* bot = GetPlayerBot(guid);
    if (!bot)
        bot = ObjectAccessor::FindPlayer(guid);

    if (!bot)
        return "bot not found";

    if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
    {
        if (Player* master = botAI->GetMaster())
        {
            if (cmd == "init=white" || cmd == "init=common")
            {
                BotFactory(bot, master->getLevel(), ITEM_QUALITY_NORMAL).Randomize(false);
                return "ok";
            }
            else if (cmd == "init=green" || cmd == "init=uncommon")
            {
                BotFactory(bot, master->getLevel(), ITEM_QUALITY_UNCOMMON).Randomize(false);
                return "ok";
            }
            else if (cmd == "init=blue" || cmd == "init=rare")
            {
                BotFactory(bot, master->getLevel(), ITEM_QUALITY_RARE).Randomize(false);
                return "ok";
            }
            else if (cmd == "init=epic" || cmd == "init=purple")
            {
                BotFactory(bot, master->getLevel(), ITEM_QUALITY_EPIC).Randomize(false);
                return "ok";
            }
            else if (cmd == "init=legendary" || cmd == "init=yellow")
            {
                BotFactory(bot, master->getLevel(), ITEM_QUALITY_LEGENDARY).Randomize(false);
                return "ok";
            }
            else
            {
                int gs;
                if (sscanf(cmd.c_str(), "init=%d", &gs) == 1)
                {
                    BotFactory(bot, master->getLevel(), ITEM_QUALITY_LEGENDARY, gs).Randomize(false);
                    // AC PlayerbotMgr.cpp:807-811 reports the limit divided by the epic multiplier.
                    return "ok, gear score limit: " + std::to_string(gs / PlayerbotAI::GetItemScoreMultiplier(ItemQualities(ITEM_QUALITY_EPIC))) + "(for epic)";
                }
            }
        }
    }

    return "unknown command";
}

bool PlayerbotMgr::HandlePlayerbotMgrCommand(ChatHandler* handler, char const* args)
{
    if (!sPlayerbotAIConfig->enabled)
    {
        handler->PSendSysMessage("|cffff0000Playerbot system is currently disabled!");
        return false;
    }

    WorldSession* m_session = handler->GetSession();
    if (!m_session)
    {
        handler->PSendSysMessage("You may only add bots from an active session");
        return false;
    }

    Player* player = m_session->GetPlayer();
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
    if (!mgr)
    {
        handler->PSendSysMessage("You cannot control bots yet");
        return false;
    }

    std::vector<std::string> messages = mgr->HandlePlayerbotCommand(args, player);
    if (messages.empty())
        return true;

    for (std::vector<std::string>::iterator i = messages.begin(); i != messages.end(); ++i)
    {
        handler->PSendSysMessage("%s", i->c_str());
    }

    return true;
}

std::vector<std::string> PlayerbotHolder::HandlePlayerbotCommand(char const* args, Player* master)
{
    std::vector<std::string> messages;

    if (!*args)
    {
        messages.push_back("usage: list/reload/tweak/self or add/init/remove PLAYERNAME\n");
        messages.push_back("usage: addclass CLASSNAME");
        return messages;
    }

    char* cmd = strtok((char*)args, " ");
    char* charname = strtok(nullptr, " ");
    if (!cmd)
    {
        messages.push_back("usage: list/reload/tweak/self or add/init/remove PLAYERNAME or addclass CLASSNAME");
        return messages;
    }

    if (!strcmp(cmd, "addspec") && master)
    {
        if (!charname || !strcmp(charname, "tank") || !strcmp(charname, "dps") || !strcmp(charname, "heal"))
        {
            messages.push_back(
                "addclass: invalid SPECNAME(tank/heal/dps)");
            return messages;
        }
        std::map<std::string, std::vector<uint8>> clazs = {
            {"tank", { 1, 2, 11, 6, 10 }},
            {"heal", { 2, 5, 7, 11, 10 }},
            {"dps", { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }}
        };
    }
    if (!strcmp(cmd, "setspec") && master && master->GetTarget())
    {
        uint32 tab = std::atoi(charname);

        auto bot = ObjectAccessor::FindPlayer(master->GetTarget());
        if (bot)
            BotFactory::SwitchSpecialization(bot, tab);
        return StringVector();
    }

    if (!strcmp(cmd, "addclass"))
    {
        if (!charname)
        {
            messages.push_back(
                "addclass: invalid CLASSNAME(warrior/paladin/hunter/rogue/priest/shaman/mage/warlock/druid/dk)");
            return messages;
        }
        uint8 claz;
        if (!strcmp(charname, "warrior"))
        {
            claz = 1;
        }
        else if (!strcmp(charname, "paladin"))
        {
            claz = 2;
        }
        else if (!strcmp(charname, "hunter"))
        {
            claz = 3;
        }
        else if (!strcmp(charname, "rogue"))
        {
            claz = 4;
        }
        else if (!strcmp(charname, "priest"))
        {
            claz = 5;
        }
        else if (!strcmp(charname, "shaman"))
        {
            claz = 7;
        }
        else if (!strcmp(charname, "mage"))
        {
            claz = 8;
        }
        else if (!strcmp(charname, "warlock"))
        {
            claz = 9;
        }
        else if (!strcmp(charname, "druid"))
        {
            claz = 11;
        }
        else if (!strcmp(charname, "dk"))
        {
            claz = 6;
        }
        else if (!strcmp(charname, "monk"))
        {
            claz = 10;
        }
        else
        {
            messages.push_back("Error: Invalid Class. Try again.");
            return messages;
        }
        TeamId teamId = master->GetTeamId();
        GuidVector guidCache = sRandomPlayerbotMgr->AddclassCache()[RandomPlayerbotMgr::GetTeamClassIdx(teamId == TEAM_ALLIANCE, claz)];
        
        std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
        std::shuffle(guidCache.begin(), guidCache.end(), gen);
        for (size_t i = 0; i < guidCache.size(); i++)
        {
            BotGuid guid = guidCache[i];
            if (botLoading.find(guid) != botLoading.end())
                continue;
            if (ObjectAccessor::FindPlayer(guid))
                continue;

            {
                std::lock_guard<std::mutex> lock(sAddclassMutex);
                sAddclassPending[guid] = master->getLevel();
            }
            AddPlayerBot(guid, master->GetSession()->GetAccountId());
            
            messages.push_back("Add class " + std::string(charname));
            return messages;
        }
        messages.push_back("Add class failed, no available characters!");
        return messages;
    }

    // #177 (AC PlayerbotMgr.cpp:1022-1025,1181-1320): "list", then the generic add/remove/login/
    // logout/init dispatch onto ProcessBotCommand() above. Trimmed from AC's version: no "*" (whole
    // group) / "!" (GM: every logged-in bot) / comma-separated multi-target expansion - one bot name
    // (or the master's current target, same fallback AC itself falls back to) per command. The
    // "addaccount"-lists-every-character-on-that-account expansion is dropped for the same reason.
    if (!strcmp(cmd, "list"))
    {
        messages.push_back(ListBots(master));
        return messages;
    }

    // #503 (AC Bot/PlayerbotMgr.cpp:1053-1072): "self" hands the player's own character to the bot AI, or takes it back.
    // Fork mappings:
    //  - AC CanBeGameMaster() -> HasPermission(rbac::RBAC_PERM_COMMAND_GM), the mapping PlayerbotAI.cpp/PlayerbotSecurity.cpp use.
    //  - AC's ~PlayerbotAI erases the AI-map entry (AC PlayerbotAI.cpp:357-358). This fork's destructor does not, so the
    //    entry is erased here before the delete; otherwise GET_PLAYERBOT_AI(master) would return a freed AI.
    //  - AC PlayerbotRepository::Load (saved strategies) is skipped: this fork's bot DB store is disabled
    //    (PlayerbotOperations.h:11).
    if (!strcmp(cmd, "self") && master)
    {
        if (PlayerbotAI* selfAI = GET_PLAYERBOT_AI(master))
        {
            messages.push_back("Disable player botAI");
            sPlayerbotsMgr->RemovePlayerBotData(master->GetGUID(), true);
            delete selfAI;
        }
        else if (sPlayerbotAIConfig->selfBotLevel == 0)
            messages.push_back("Self-bot is disabled");
        else if (sPlayerbotAIConfig->selfBotLevel == 1 && !master->GetSession()->HasPermission(rbac::RBAC_PERM_COMMAND_GM))
            messages.push_back("You do not have permission to enable player botAI");
        else
        {
            messages.push_back("Enable player botAI");
            sPlayerbotsMgr->AddPlayerbotData(master, true);
            if (PlayerbotAI* newAI = GET_PLAYERBOT_AI(master))
                newAI->SetMaster(master);
        }

        return messages;
    }

    if (!master)
    {
        messages.push_back("usage: list/reload/tweak/self or add/init/remove PLAYERNAME");
        return messages;
    }

    std::string charnameStr;
    if (!charname)
    {
        std::string name;
        if (master && sCharacterCache->GetCharacterNameByGuid(master->GetTarget(), name))
            charnameStr = name;
        else
        {
            messages.push_back("usage: list/reload/tweak/self or add/init/remove PLAYERNAME");
            return messages;
        }
    }
    else
        charnameStr = charname;

    if (!normalizePlayerName(charnameStr))
    {
        messages.push_back("Character '" + charnameStr + "' not found");
        return messages;
    }

    BotGuid member = sCharacterCache->GetCharacterGuidByName(charnameStr);
    if (!member)
    {
        messages.push_back("Character '" + charnameStr + "' not found");
        return messages;
    }

    std::string const cmdStr = cmd;
    std::ostringstream out;
    out << cmdStr << ": " << charnameStr << " - ";
    if (member == master->GetGUID())
        out << "cannot target yourself";
    else
        out << ProcessBotCommand(cmdStr, member, master->GetGUID(), master->GetSession()->HasPermission(rbac::RBAC_PERM_COMMAND_GM),
                                 master->GetSession()->GetAccountId(), master->GetGuildId());

    messages.push_back(out.str());
    return messages;
}

uint32 PlayerbotHolder::GetAccountId(std::string const name) { return AccountMgr::GetId(name); }

uint32 PlayerbotHolder::GetAccountId(BotGuid guid)
{
    if (!(guid.GetTypeId() == TypeID::TYPEID_PLAYER))
        return 0;

    // prevent DB access for online player
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        return player->GetSession()->GetAccountId();

    BotGuid::LowType lowguid = guid.GetCounter();
    if (QueryResult result = CharacterDatabase.PQuery("SELECT account FROM characters WHERE guid = {}", lowguid))
    {
        uint32 acc = (*result)[0].GetUInt32();
        return acc;
    }

    return 0;
}

std::string const PlayerbotHolder::ListBots(Player* master)
{
    std::set<std::string> bots;
    std::map<uint8, std::string> classNames;

    classNames[CLASS_DEATH_KNIGHT] = "Death Knight";
    classNames[CLASS_DRUID] = "Druid";
    classNames[CLASS_HUNTER] = "Hunter";
    classNames[CLASS_MAGE] = "Mage";
    classNames[CLASS_PALADIN] = "Paladin";
    classNames[CLASS_PRIEST] = "Priest";
    classNames[CLASS_ROGUE] = "Rogue";
    classNames[CLASS_SHAMAN] = "Shaman";
    classNames[CLASS_WARLOCK] = "Warlock";
    classNames[CLASS_WARRIOR] = "Warrior";
    classNames[CLASS_DEATH_KNIGHT] = "DeathKnight";
    classNames[CLASS_MONK] = "Monk";

    std::map<std::string, std::string> online;
    std::vector<std::string> names;
    std::map<std::string, std::string> classes;

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        std::string const name = bot->GetName();
        bots.insert(name);

        names.push_back(name);
        online[name] = "+";
        classes[name] = classNames[bot->getClass()];
    }

    /*if (master)
    {
        QueryResult results = CharacterDatabase.Query("SELECT class, name FROM characters WHERE account = {}",
            master->GetSession()->GetAccountId());
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                uint8 cls = fields[0].Get<uint8>();
                std::string const name = fields[1].Get<std::string>();
                if (bots.find(name) == bots.end() && name != master->GetSession()->GetPlayerName())
                {
                    names.push_back(name);
                    online[name] = "-";
                    classes[name] = classNames[cls];
                }
            } while (results->NextRow());
        }
    }*/

    std::sort(names.begin(), names.end());

    if (Group* group = master->GetGroup())
    {
        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player* member = ObjectAccessor::FindPlayer(itr->guid);
            if (member && sRandomPlayerbotMgr->IsRandomBot(member))
            {
                std::string const name = member->GetName();

                names.push_back(name);
                online[name] = "+";
                classes[name] = classNames[member->getClass()];
            }
        }
    }

    std::ostringstream out;
    bool first = true;
    out << "Bot roster: ";
    for (std::vector<std::string>::iterator i = names.begin(); i != names.end(); ++i)
    {
        if (first)
            first = false;
        else
            out << ", ";

        std::string const name = *i;
        out << online[name] << name << " " << classes[name];
    }

    return out.str();
}

std::string const PlayerbotHolder::LookupBots(Player* master)
{
    std::list<std::string> messages;
    messages.push_back("Classes Available:");
    messages.push_back("|TInterface\\icons\\INV_Sword_27.png:25:25:0:-1|t Warrior");
    messages.push_back("|TInterface\\icons\\INV_Hammer_01.png:25:25:0:-1|t Paladin");
    messages.push_back("|TInterface\\icons\\INV_Weapon_Bow_07.png:25:25:0:-1|t Hunter");
    messages.push_back("|TInterface\\icons\\INV_ThrowingKnife_04.png:25:25:0:-1|t Rogue");
    messages.push_back("|TInterface\\icons\\INV_Staff_30.png:25:25:0:-1|t Priest");
    messages.push_back("|TInterface\\icons\\inv_jewelry_talisman_04.png:25:25:0:-1|t Shaman");
    messages.push_back("|TInterface\\icons\\INV_staff_30.png:25:25:0:-1|t Mage");
    messages.push_back("|TInterface\\icons\\INV_staff_30.png:25:25:0:-1|t Warlock");
    messages.push_back("|TInterface\\icons\\Ability_Druid_Maul.png:25:25:0:-1|t Druid");
    messages.push_back("DK");
    messages.push_back("Monk");
    messages.push_back("(Usage: .bot lookup CLASS)");
    std::string ret_msg;
    for (std::string msg : messages)
    {
        ret_msg += msg + "\n";
    }
    return ret_msg;
}

uint32 PlayerbotHolder::GetPlayerbotsCountByClass(uint32 cls)
{
    uint32 count = 0;
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot && bot->IsInWorld() && bot->getClass() == cls)
        {
            count++;
        }
    }
    return count;
}

PlayerbotMgr::PlayerbotMgr(Player* const master) : PlayerbotHolder(), master(master), lastErrorTell(0) {}

PlayerbotMgr::~PlayerbotMgr()
{
    AbandonPendingLogins();

    if (master)
        sPlayerbotsMgr->RemovePlayerBotData(master->GetGUID(), false);
}

void PlayerbotMgr::UpdateAIInternal(uint32 elapsed, bool /*minimal*/)
{
    SetNextCheckDelay(sPlayerbotAIConfig->reactDelay);
    CheckTellErrors(elapsed);
    WakeNearbyBots();
}

void PlayerbotMgr::WakeNearbyBots()
{
    uint32 const now = getMSTime();
    uint32 const radius = sPlayerbotAIConfig->botActiveAloneForceWhenInRadius;
    if (!ActivityWakePolicy::ScanDue(lastWakeScanMs, now ? now : 1, radius > 0))
        return;
    lastWakeScanMs = now ? now : 1;

    // The same exceptions as PlayerbotAI::AllowActive's radius rule: an invisible game master wakes nobody.
    if (!master || !master->IsInWorld() || (master->IsGameMaster() && !master->isGMVisible()))
        return;

    // Bots on the player's map are updated by this map thread, as in the duel wake (mod_playerbots.cpp WakeDuelant).
    std::list<Player*> players;
    master->GetPlayerListInGrid(players, float(radius));
    for (Player* player : players)
    {
        PlayerbotAI* botAI = player != master ? GET_PLAYERBOT_AI(player) : nullptr;
        if (!botAI || botAI->IsRealPlayer())
            continue;
        bool const cached = botAI->AllowActivity(ALL_ACTIVITY);
        if (cached)
            continue;
        bool const activeNow = botAI->AllowActivity(ALL_ACTIVITY, true);
        if (ActivityWakePolicy::Wake(cached, activeNow, player->IsNonMeleeSpellCasted(false, false, true)))
            botAI->SetNextCheckDelay(0);
    }
}

void PlayerbotMgr::HandleCommand(uint32 type, std::string const text)
{
    Player* master = GetMaster();
    if (!master)
        return;

    if (text.find(sPlayerbotAIConfig->commandSeparator) != std::string::npos)
    {
        std::vector<std::string> commands;
        split(commands, text, sPlayerbotAIConfig->commandSeparator.c_str());
        for (std::vector<std::string>::iterator i = commands.begin(); i != commands.end(); ++i)
        {
            HandleCommand(type, *i);
        }

        return;
    }

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI)
            botAI->HandleCommand(type, text, master);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr->GetPlayerBotsBegin();
        it != sRandomPlayerbotMgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI && botAI->GetMaster() == master)
            botAI->HandleCommand(type, text, master);
    }
}

void PlayerbotMgr::HandleMasterIncomingPacket(WorldPacket const& packet)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (!bot)
            continue;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI)
            botAI->HandleMasterIncomingPacket(packet);
    }

    // Snapshot rather than walking sRandomPlayerbotMgr's live map, for the same reason as the
    // outgoing path below. This one is defensive - the proven hang was on the outgoing hook - but
    // the map is shared and mutated as bots log in and out, so iterating it directly is unsound.
    for (Player* const bot : sRandomPlayerbotMgr->GetPlayerBotsSnapshot())
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI && botAI->GetMaster() == GetMaster())
            botAI->HandleMasterIncomingPacket(packet);
    }

    switch (packet.GetOpcode())
    {
        // if master is logging out, log out all bots
    case CMSG_LOGOUT_REQUEST:
    {
        LogoutAllBots();
        break;
    }
    // if master cancelled logout, cancel too
    case CMSG_LOGOUT_CANCEL:
    {
        CancelLogout();
        break;
    }
    }
}

void PlayerbotMgr::HandleMasterOutgoingPacket(WorldPacket const& packet)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI)
            botAI->HandleMasterOutgoingPacket(packet);
    }

    // Iterate a snapshot, not the live map. This runs from WorldSession::SendPacket, which the
    // map-update worker calls while the world/session thread is still logging random bots in and
    // out; walking sRandomPlayerbotMgr's std::map directly races with those inserts and can leave
    // the worker spinning in ++it forever (see GetPlayerBotsSnapshot).
    for (Player* const bot : sRandomPlayerbotMgr->GetPlayerBotsSnapshot())
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (botAI && botAI->GetMaster() == GetMaster())
            botAI->HandleMasterOutgoingPacket(packet);
    }
}

void PlayerbotMgr::SaveToDB()
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->SaveToDB(false);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr->GetPlayerBotsBegin();
        it != sRandomPlayerbotMgr->GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (GET_PLAYERBOT_AI(bot) && GET_PLAYERBOT_AI(bot)->GetMaster() == GetMaster())
            bot->SaveToDB(false);
    }
}

void PlayerbotMgr::OnBotLoginInternal(Player* const bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
    {
        return;
    }
    botAI->SetMaster(master);
    //botAI->ResetStrategies();

    // #161: apply a pending ".npcbot addclass" init here, on the world thread
    uint32 addclassLevel = 0;
    {
        std::lock_guard<std::mutex> lock(sAddclassMutex);
        auto it = sAddclassPending.find(bot->GetGUID());
        if (it != sAddclassPending.end())
        {
            addclassLevel = it->second;
            sAddclassPending.erase(it);
        }
    }
    if (addclassLevel)
    {
        sRandomPlayerbotMgr->SetValue(bot, "level", addclassLevel);
        sRandomPlayerbotMgr->Randomize(bot);
        BotFactory factory(bot, bot->getLevel());
        factory.InitTalentsTree(true);
        factory.InitGlyphs(false);  // #734, AC PlayerbotFactory::Randomize order (talents, then glyphs)
        factory.InitEquipment(true);
    }
    else if (BotFactory::AltBotsPickTalents() && !sRandomPlayerbotMgr->IsRandomBot(bot))
        BotFactory(bot, bot->getLevel()).InitAltSpecAndTalents();   // an alt that levelled while the owner played it

    SF_LOG_INFO("playerbots", "Bot %s logged in - Active spec tab: %u Spec: %u ", bot->GetName().c_str(), (uint32)bot->GetActiveSpec(), (uint32)BotGetSpecialization(bot));
}

void PlayerbotMgr::OnPlayerLogin(Player* player)
{
    // #503 (AC Bot/PlayerbotMgr.cpp:1660-1661): SelfBotLevel 3 turns every real player into a selfbot at login.
    // HandlePlayerbotCommand strtok()s its argument, so it gets a writable copy.
    if (sPlayerbotAIConfig->selfBotLevel > 2)
    {
        char selfCmd[] = "self";
        HandlePlayerbotCommand(selfCmd, player);
    }

    /*
    // set locale priority for bot texts
    sPlayerbotTextMgr->AddLocalePriority(player->GetSession()->GetSessionDbcLocale());

    if (sPlayerbotAIConfig->selfBotLevel > 2)
        HandlePlayerbotCommand("self", player);

    if (!sPlayerbotAIConfig->botAutologin)
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    QueryResult results = CharacterDatabase.Query("SELECT name FROM characters WHERE account = {}", accountId);
    if (results)
    {
        std::ostringstream out;
        out << "add ";
        bool first = true;
        do
        {
            Field* fields = results->Fetch();

            if (first)
                first = false;
            else
                out << ",";

            out << fields[0].Get<std::string>();
        } while (results->NextRow());

        HandlePlayerbotCommand(out.str().c_str(), player);
    }*/
}

void PlayerbotMgr::TellError(std::string const botName, std::string const text)
{
    std::set<std::string> names = errors[text];
    if (names.find(botName) == names.end())
    {
        names.insert(botName);
    }
    SF_LOG_DEBUG("playerbots", "TellErro: %s: %s", botName.c_str(), text.c_str());
    errors[text] = names;
}

void PlayerbotMgr::CheckTellErrors(uint32 elapsed)
{
    time_t now = time(nullptr);
    if ((now - lastErrorTell) < sPlayerbotAIConfig->errorDelay / 1000)
        return;

    lastErrorTell = now;

    for (PlayerBotErrorMap::iterator i = errors.begin(); i != errors.end(); ++i)
    {
        std::string const text = i->first;
        std::set<std::string> names = i->second;

        std::ostringstream out;
        bool first = true;
        for (std::set<std::string>::iterator j = names.begin(); j != names.end(); ++j)
        {
            if (!first)
                out << ", ";
            else
                first = false;

            out << *j;
        }

        out << "|cfff00000: " << text;

        // AC PlayerbotMgr::CheckTellErrors shows these to the master (LoA had it commented out, so "help" and every bot
        // error stayed silent). Sent as text, not as a format: bot messages can contain '%'.
        if (master && master->GetSession())
            ChatHandler(master->GetSession()).SendSysMessage(out.str().c_str());
    }

    errors.clear();
}

void PlayerbotsMgr::AddPlayerbotData(Player* player, bool isBotAI)
{
    if (!player)
    {
        return;
    }
    // If the guid already exists in the map, remove it

    if (!isBotAI)
    {
        std::unordered_map<BotGuid, PlayerbotAIBase*>::iterator itr = _playerbotsMgrMap.find(player->GetGUID());
        if (itr != _playerbotsMgrMap.end())
        {
            _playerbotsMgrMap.erase(itr);
        }
        PlayerbotMgr* playerbotMgr = new PlayerbotMgr(player);
        ASSERT(_playerbotsMgrMap.emplace(player->GetGUID(), playerbotMgr).second);

        playerbotMgr->OnPlayerLogin(player);
    }
    else
    {
        std::unordered_map<BotGuid, PlayerbotAIBase*>::iterator itr = _playerbotsAIMap.find(player->GetGUID());
        if (itr != _playerbotsAIMap.end())
        {
            _playerbotsAIMap.erase(itr);
        }
        PlayerbotAI* botAI = new PlayerbotAI(player);
        ASSERT(_playerbotsAIMap.emplace(player->GetGUID(), botAI).second);
    }
}

void PlayerbotsMgr::RemovePlayerBotData(BotGuid const& guid, bool is_AI)
{
    if (is_AI)
    {
        std::unordered_map<BotGuid, PlayerbotAIBase*>::iterator itr = _playerbotsAIMap.find(guid);
        if (itr != _playerbotsAIMap.end())
        {
            _playerbotsAIMap.erase(itr);
        }
    }
    else
    {
        std::unordered_map<BotGuid, PlayerbotAIBase*>::iterator itr = _playerbotsMgrMap.find(guid);
        if (itr != _playerbotsMgrMap.end())
        {
            _playerbotsMgrMap.erase(itr);
        }
    }
}

PlayerbotAI* PlayerbotsMgr::GetPlayerbotAI(Player* player)
{
    if (!(sPlayerbotAIConfig->enabled) || !player)
    {
        return nullptr;
    }
    // if (player->GetSession()->isLogingOut() || player->IsDuringRemoveFromWorld()) {
    //     return nullptr;
    // }
    auto itr = _playerbotsAIMap.find(player->GetGUID());
    if (itr != _playerbotsAIMap.end())
    {
        if (itr->second->IsBotAI())
            return reinterpret_cast<PlayerbotAI*>(itr->second);
    }

    return nullptr;
}

PlayerbotMgr* PlayerbotsMgr::GetPlayerbotMgr(Player* player)
{
    if (!(sPlayerbotAIConfig->enabled) || !player)
    {
        return nullptr;
    }
    auto itr = _playerbotsMgrMap.find(player->GetGUID());
    if (itr != _playerbotsMgrMap.end())
    {
        if (!itr->second->IsBotAI())
            return reinterpret_cast<PlayerbotMgr*>(itr->second);
    }

    return nullptr;
}
