/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Generated from SkyFire's SharedDefines.h (enum class ChatMsg). The playerbots code keeps chat types in uint32 variables,
 * as TrinityCore's plain enum allowed; calls into SkyFire's chat API cast back explicitly.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCHATCONSTANTS_H
#define MOD_AWARE_BOTS_COMPAT_BOTCHATCONSTANTS_H

#include "SharedDefines.h"

// ChatMsg names as uint32: the module keeps chat types and languages in uint32 variables.
constexpr uint32 CHAT_MSG_ADDON = uint32(ChatMsg::CHAT_MSG_ADDON);
constexpr uint32 CHAT_MSG_SYSTEM = uint32(ChatMsg::CHAT_MSG_SYSTEM);
constexpr uint32 CHAT_MSG_SAY = uint32(ChatMsg::CHAT_MSG_SAY);
constexpr uint32 CHAT_MSG_PARTY = uint32(ChatMsg::CHAT_MSG_PARTY);
constexpr uint32 CHAT_MSG_RAID = uint32(ChatMsg::CHAT_MSG_RAID);
constexpr uint32 CHAT_MSG_GUILD = uint32(ChatMsg::CHAT_MSG_GUILD);
constexpr uint32 CHAT_MSG_OFFICER = uint32(ChatMsg::CHAT_MSG_OFFICER);
constexpr uint32 CHAT_MSG_YELL = uint32(ChatMsg::CHAT_MSG_YELL);
constexpr uint32 CHAT_MSG_WHISPER = uint32(ChatMsg::CHAT_MSG_WHISPER);
constexpr uint32 CHAT_MSG_SMART_WHISPER = uint32(ChatMsg::CHAT_MSG_SMART_WHISPER);
constexpr uint32 CHAT_MSG_WHISPER_INFORM = uint32(ChatMsg::CHAT_MSG_WHISPER_INFORM);
constexpr uint32 CHAT_MSG_EMOTE = uint32(ChatMsg::CHAT_MSG_EMOTE);
constexpr uint32 CHAT_MSG_TEXT_EMOTE = uint32(ChatMsg::CHAT_MSG_TEXT_EMOTE);
constexpr uint32 CHAT_MSG_MONSTER_SAY = uint32(ChatMsg::CHAT_MSG_MONSTER_SAY);
constexpr uint32 CHAT_MSG_MONSTER_PARTY = uint32(ChatMsg::CHAT_MSG_MONSTER_PARTY);
constexpr uint32 CHAT_MSG_MONSTER_YELL = uint32(ChatMsg::CHAT_MSG_MONSTER_YELL);
constexpr uint32 CHAT_MSG_MONSTER_WHISPER = uint32(ChatMsg::CHAT_MSG_MONSTER_WHISPER);
constexpr uint32 CHAT_MSG_MONSTER_EMOTE = uint32(ChatMsg::CHAT_MSG_MONSTER_EMOTE);
constexpr uint32 CHAT_MSG_CHANNEL = uint32(ChatMsg::CHAT_MSG_CHANNEL);
constexpr uint32 CHAT_MSG_CHANNEL_JOIN = uint32(ChatMsg::CHAT_MSG_CHANNEL_JOIN);
constexpr uint32 CHAT_MSG_CHANNEL_LEAVE = uint32(ChatMsg::CHAT_MSG_CHANNEL_LEAVE);
constexpr uint32 CHAT_MSG_CHANNEL_LIST = uint32(ChatMsg::CHAT_MSG_CHANNEL_LIST);
constexpr uint32 CHAT_MSG_CHANNEL_NOTICE = uint32(ChatMsg::CHAT_MSG_CHANNEL_NOTICE);
constexpr uint32 CHAT_MSG_CHANNEL_NOTICE_USER = uint32(ChatMsg::CHAT_MSG_CHANNEL_NOTICE_USER);
constexpr uint32 CHAT_MSG_AFK = uint32(ChatMsg::CHAT_MSG_AFK);
constexpr uint32 CHAT_MSG_DND = uint32(ChatMsg::CHAT_MSG_DND);
constexpr uint32 CHAT_MSG_IGNORED = uint32(ChatMsg::CHAT_MSG_IGNORED);
constexpr uint32 CHAT_MSG_SKILL = uint32(ChatMsg::CHAT_MSG_SKILL);
constexpr uint32 CHAT_MSG_LOOT = uint32(ChatMsg::CHAT_MSG_LOOT);
constexpr uint32 CHAT_MSG_MONEY = uint32(ChatMsg::CHAT_MSG_MONEY);
constexpr uint32 CHAT_MSG_OPENING = uint32(ChatMsg::CHAT_MSG_OPENING);
constexpr uint32 CHAT_MSG_TRADESKILLS = uint32(ChatMsg::CHAT_MSG_TRADESKILLS);
constexpr uint32 CHAT_MSG_PET_INFO = uint32(ChatMsg::CHAT_MSG_PET_INFO);
constexpr uint32 CHAT_MSG_COMBAT_MISC_INFO = uint32(ChatMsg::CHAT_MSG_COMBAT_MISC_INFO);
constexpr uint32 CHAT_MSG_COMBAT_XP_GAIN = uint32(ChatMsg::CHAT_MSG_COMBAT_XP_GAIN);
constexpr uint32 CHAT_MSG_COMBAT_HONOR_GAIN = uint32(ChatMsg::CHAT_MSG_COMBAT_HONOR_GAIN);
constexpr uint32 CHAT_MSG_COMBAT_FACTION_CHANGE = uint32(ChatMsg::CHAT_MSG_COMBAT_FACTION_CHANGE);
constexpr uint32 CHAT_MSG_BG_SYSTEM_NEUTRAL = uint32(ChatMsg::CHAT_MSG_BG_SYSTEM_NEUTRAL);
constexpr uint32 CHAT_MSG_BG_SYSTEM_ALLIANCE = uint32(ChatMsg::CHAT_MSG_BG_SYSTEM_ALLIANCE);
constexpr uint32 CHAT_MSG_BG_SYSTEM_HORDE = uint32(ChatMsg::CHAT_MSG_BG_SYSTEM_HORDE);
constexpr uint32 CHAT_MSG_RAID_LEADER = uint32(ChatMsg::CHAT_MSG_RAID_LEADER);
constexpr uint32 CHAT_MSG_RAID_WARNING = uint32(ChatMsg::CHAT_MSG_RAID_WARNING);
constexpr uint32 CHAT_MSG_RAID_BOSS_EMOTE = uint32(ChatMsg::CHAT_MSG_RAID_BOSS_EMOTE);
constexpr uint32 CHAT_MSG_RAID_BOSS_WHISPER = uint32(ChatMsg::CHAT_MSG_RAID_BOSS_WHISPER);
constexpr uint32 CHAT_MSG_FILTERED = uint32(ChatMsg::CHAT_MSG_FILTERED);
constexpr uint32 CHAT_MSG_RESTRICTED = uint32(ChatMsg::CHAT_MSG_RESTRICTED);
constexpr uint32 CHAT_MSG_ACHIEVEMENT = uint32(ChatMsg::CHAT_MSG_ACHIEVEMENT);
constexpr uint32 CHAT_MSG_GUILD_ACHIEVEMENT = uint32(ChatMsg::CHAT_MSG_GUILD_ACHIEVEMENT);
constexpr uint32 CHAT_MSG_COMBAT_ARENA_POINTS_GAIN = uint32(ChatMsg::CHAT_MSG_COMBAT_ARENA_POINTS_GAIN);
constexpr uint32 CHAT_MSG_PARTY_LEADER = uint32(ChatMsg::CHAT_MSG_PARTY_LEADER);
constexpr uint32 CHAT_MSG_TARGET_ICONS = uint32(ChatMsg::CHAT_MSG_TARGET_ICONS);
constexpr uint32 CHAT_MSG_BN_WHISPER = uint32(ChatMsg::CHAT_MSG_BN_WHISPER);
constexpr uint32 CHAT_MSG_BN_WHISPER_INFORM = uint32(ChatMsg::CHAT_MSG_BN_WHISPER_INFORM);
constexpr uint32 CHAT_MSG_BN_CONVERSATION = uint32(ChatMsg::CHAT_MSG_BN_CONVERSATION);
constexpr uint32 CHAT_MSG_BN_CONVERSATION_NOTICE = uint32(ChatMsg::CHAT_MSG_BN_CONVERSATION_NOTICE);
constexpr uint32 CHAT_MSG_BN_CONVERSATION_LIST = uint32(ChatMsg::CHAT_MSG_BN_CONVERSATION_LIST);
constexpr uint32 CHAT_MSG_BN_INLINE_TOAST_ALERT = uint32(ChatMsg::CHAT_MSG_BN_INLINE_TOAST_ALERT);
constexpr uint32 CHAT_MSG_BN_INLINE_TOAST_BROADCAST = uint32(ChatMsg::CHAT_MSG_BN_INLINE_TOAST_BROADCAST);
constexpr uint32 CHAT_MSG_BN_INLINE_TOAST_BROADCAST_INFORM = uint32(ChatMsg::CHAT_MSG_BN_INLINE_TOAST_BROADCAST_INFORM);
constexpr uint32 CHAT_MSG_BN_INLINE_TOAST_CONVERSATION = uint32(ChatMsg::CHAT_MSG_BN_INLINE_TOAST_CONVERSATION);
constexpr uint32 CHAT_MSG_BN_WHISPER_PLAYER_OFFLINE = uint32(ChatMsg::CHAT_MSG_BN_WHISPER_PLAYER_OFFLINE);
constexpr uint32 CHAT_MSG_COMBAT_GUILD_XP_GAIN = uint32(ChatMsg::CHAT_MSG_COMBAT_GUILD_XP_GAIN);
constexpr uint32 CHAT_MSG_CURRENCY = uint32(ChatMsg::CHAT_MSG_CURRENCY);
constexpr uint32 CHAT_MSG_QUEST_BOSS_EMOTE = uint32(ChatMsg::CHAT_MSG_QUEST_BOSS_EMOTE);
constexpr uint32 CHAT_MSG_PET_BATTLE_COMBAT_LOG = uint32(ChatMsg::CHAT_MSG_PET_BATTLE_COMBAT_LOG);
constexpr uint32 CHAT_MSG_PET_BATTLE_INFO = uint32(ChatMsg::CHAT_MSG_PET_BATTLE_INFO);
constexpr uint32 CHAT_MSG_INSTANCE = uint32(ChatMsg::CHAT_MSG_INSTANCE);
constexpr uint32 CHAT_MSG_INSTANCE_LEADER = uint32(ChatMsg::CHAT_MSG_INSTANCE_LEADER);
constexpr uint32 MSG_NULL_ACTION = uint32(ChatMsg::MSG_NULL_ACTION);

#endif
