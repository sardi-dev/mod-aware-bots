/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidRoles.h"

#include <cctype>
#include <functional>

#include "Group.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

namespace
{
    // AC's shared shape of IsAssist{Tank,Heal,RangedDps}OfIndex: group assistants of the role come first, in group
    // order, then the other members of the role.
    bool IsRoleMemberOfIndex(Player* player, uint8 index, bool indexLivingOnly,
                             std::function<bool(Player*)> const& inRole)
    {
        if (!player || !inRole(player))
            return false;

        if (indexLivingOnly && !player->IsAlive())
            return false;

        Group* group = player->GetGroup();
        if (!group)
            return false;

        uint8 totalAssistants = 0;
        uint8 assistantsBeforePlayer = 0;
        uint8 nonAssistantsBeforePlayer = 0;
        bool playerFound = false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || (indexLivingOnly && !member->IsAlive()) || !inRole(member))
                continue;

            bool const isAssistant = group->IsAssistant(member->GetGUID());
            if (isAssistant)
                totalAssistants++;

            if (member == player)
                playerFound = true;
            else if (!playerFound)
            {
                if (isAssistant)
                    assistantsBeforePlayer++;
                else
                    nonAssistantsBeforePlayer++;
            }
        }

        if (!playerFound)
            return false;

        uint8 const playerIndex = group->IsAssistant(player->GetGUID()) ?
            assistantsBeforePlayer : uint8(totalAssistants + nonAssistantsBeforePlayer);
        return playerIndex == index;
    }

    int32 GetIndexAmong(Player* player, std::function<bool(Player*)> const& inKind)
    {
        if (!player || !inKind(player))
            return -1;

        Group* group = player->GetGroup();
        if (!group)
            return -1;

        int32 counter = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
                continue;

            if (member == player)
                return counter;

            if (inKind(member))
                counter++;
        }
        return 0;
    }
}

// AC Bot/PlayerbotAI.cpp IsAssistTankOfIndex: the main tank is never an assist tank
bool RaidRoles::IsAssistTankOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsRoleMemberOfIndex(player, index, indexLivingOnly,
                               [](Player* member) { return PlayerBotSpec::IsAssistTank(member); });
}

// AC Bot/PlayerbotAI.cpp IsAssistHealOfIndex
bool RaidRoles::IsAssistHealOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsRoleMemberOfIndex(player, index, indexLivingOnly,
                               [](Player* member) { return PlayerBotSpec::IsHeal(member); });
}

// AC Bot/PlayerbotAI.cpp IsAssistRangedDpsOfIndex
bool RaidRoles::IsAssistRangedDpsOfIndex(Player* player, uint8 index, bool indexLivingOnly)
{
    return IsRoleMemberOfIndex(player, index, indexLivingOnly,
                               [](Player* member) { return PlayerBotSpec::IsRangedDps(member); });
}

// AC Bot/PlayerbotAI.cpp GetRangedIndex
int32 RaidRoles::GetRangedIndex(Player* player)
{
    return GetIndexAmong(player, [](Player* member) { return PlayerBotSpec::IsRanged(member); });
}

// AC Bot/PlayerbotAI.cpp GetClassIndex
int32 RaidRoles::GetClassIndex(Player* player, uint8 cls)
{
    return GetIndexAmong(player, [cls](Player* member) { return member->getClass() == cls; });
}

// AC Bot/PlayerbotAI.cpp GetRangedDpsIndex
int32 RaidRoles::GetRangedDpsIndex(Player* player)
{
    return GetIndexAmong(player, [](Player* member) { return PlayerBotSpec::IsRangedDps(member); });
}

// AC Bot/PlayerbotAI.cpp IsBotMainTank, without AC's assist tank index ties: the main tank when it is a bot, else the
// first bot assist tank in the assist tank order.
bool RaidRoles::IsBotMainTank(Player* player)
{
    if (!player || !player->IsInWorld() || !GET_PLAYERBOT_AI(player) || !PlayerBotSpec::IsTank(player))
        return false;

    if (PlayerBotSpec::IsMainTank(player))
        return true;

    Group* group = player->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && PlayerBotSpec::IsMainTank(member) && GET_PLAYERBOT_AI(member))
            return false;
    }

    uint8 constexpr MAX_ASSIST_TANKS = 8;
    for (uint8 i = 0; i < MAX_ASSIST_TANKS; ++i)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && GET_PLAYERBOT_AI(member) && IsAssistTankOfIndex(member, i))
                return member == player;
        }
    }

    return false;
}

// AC Bot/PlayerbotAI.cpp EqualLowercaseName
bool RaidRoles::EqualLowercaseName(std::string const& s1, std::string const& s2)
{
    if (s1.length() != s2.length())
        return false;

    for (std::string::size_type i = 0; i < s1.length(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(s1[i])) != std::tolower(static_cast<unsigned char>(s2[i])))
            return false;
    }
    return true;
}
