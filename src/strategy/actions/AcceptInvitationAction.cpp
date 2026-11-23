#include "AcceptInvitationAction.h"

#include "BotVoice.h"
#include "Config.h"
#include "Event.h"
#include "Group.h"
#include "LikingStore.h"
#include "LFGMgr.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "TravelToGroupAction.h"
#include "SocialMgr.h"
#include "WorldPacket.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace
{
    // Goal-based invites: a random bot weighs an invite from a player against what it is doing, like a player
    // would, instead of joining every group it is asked into. See AcceptInvitationAction::Execute.
    struct GoalInviteConfig
    {
        bool enabled;
        int32 baseChance;       // percent, before the modifiers below
        int32 friendBonus;      // the inviter is on the bot's or the inviter's friend list
        int32 guildBonus;       // the inviter is in the bot's guild
        int32 levelTolerance;   // levels apart from the group that cost nothing
        int32 levelPenalty;     // percent per level beyond the tolerance
        int32 maxLevelBelow;    // a bot this many levels or more below the group always declines
        int32 busyPenalty;      // a quest objective is close by
        int32 busyDistance;     // yards
        int32 newTaskPenalty;   // the bot just set out on a trip, a grind or a quest
        uint32 newTaskSeconds;
    };

    GoalInviteConfig const& InviteSettings()
    {
        static GoalInviteConfig const config = []
        {
            GoalInviteConfig c;
            c.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.GoalInvites.Enable", true);
            c.baseChance = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.BaseChance", 70);
            c.friendBonus = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.FriendBonus", 15);
            c.guildBonus = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.GuildBonus", 20);
            c.levelTolerance = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.LevelTolerance", 4);
            c.levelPenalty = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.LevelPenalty", 8);
            c.maxLevelBelow = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.MaxLevelBelow", 10);
            c.busyPenalty = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.BusyPenalty", 40);
            c.busyDistance = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.BusyDistance", 100);
            c.newTaskPenalty = sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.NewTaskPenalty", 15);
            c.newTaskSeconds = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.GoalInvites.NewTaskSeconds", 90));
            return c;
        }();
        return config;
    }

    // Never certain either way: even a guild mate can be busy, and a stranger can be in a good mood.
    int32 const MIN_CHANCE = 5;
    int32 const MAX_CHANCE = 95;
    // A bot keeps the same lean for good (derived from its guid): some are more sociable than others.
    int32 const PERSONAL_BIAS_RANGE = 10;
    // The same level-fit bonus for an inviter in the bot's own zone at about its level.
    int32 const SAME_ZONE_BONUS = 10;
    // A "join" command is an explicit request and skips the goal check for this long.
    time_t const JOIN_REQUEST_SECONDS = 60;

    std::mutex joinRequestLock;
    std::unordered_map<uint32, time_t> joinRequests;

    bool ConsumeJoinRequest(Player* bot)
    {
        std::lock_guard<std::mutex> guard(joinRequestLock);
        auto itr = joinRequests.find(bot->GetGUIDLow());
        if (itr == joinRequests.end())
            return false;

        bool const fresh = time(nullptr) - itr->second <= JOIN_REQUEST_SECONDS;
        joinRequests.erase(itr);
        return fresh;
    }

    enum class DeclineReason
    {
        GENERIC,
        QUEST,
        NEW_TASK,
        LOW_LEVEL,
        HIGH_LEVEL,
        COMBAT,
        DEAD,
        QUEUED,
        DUNGEON,
        BATTLEGROUND
    };

    struct Verdict
    {
        bool accept = true;
        DeclineReason reason = DeclineReason::GENERIC;
    };

    // Text table keys and fallback lines; %level is the bot's own level.
    char const* ReasonKey(DeclineReason reason)
    {
        switch (reason)
        {
            case DeclineReason::QUEST: return "invite_decline_quest";
            case DeclineReason::NEW_TASK: return "invite_decline_new_task";
            case DeclineReason::LOW_LEVEL: return "invite_decline_low_level";
            case DeclineReason::HIGH_LEVEL: return "invite_decline_high_level";
            case DeclineReason::COMBAT: return "invite_decline_combat";
            case DeclineReason::DEAD: return "invite_decline_dead";
            case DeclineReason::QUEUED: return "invite_decline_queued";
            case DeclineReason::DUNGEON: return "invite_decline_dungeon";
            case DeclineReason::BATTLEGROUND: return "invite_decline_battleground";
            default: return "invite_decline_generic";
        }
    }

    char const* ReasonDefault(DeclineReason reason)
    {
        switch (reason)
        {
            case DeclineReason::QUEST: return "Sorry, almost done with this quest chain";
            case DeclineReason::NEW_TASK: return "Just got going, ask me again later";
            case DeclineReason::LOW_LEVEL: return "Too low for that, I'm %level";
            case DeclineReason::HIGH_LEVEL: return "I'm %level, a bit far from your level for a group";
            case DeclineReason::COMBAT: return "In a fight right now, sorry";
            case DeclineReason::DEAD: return "I'm dead, running back to my body";
            case DeclineReason::QUEUED: return "I'm queued for a dungeon, sorry";
            case DeclineReason::DUNGEON: return "I'm in the middle of a dungeon";
            case DeclineReason::BATTLEGROUND: return "In a battleground, sorry";
            default: return "Not right now, sorry";
        }
    }

    uint32 GroupLevel(Group* group, Player* inviter, Player* bot)
    {
        uint32 total = 0;
        uint32 count = 0;
        if (group)
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (!member || member == bot)
                    continue;

                total += member->getLevel();
                ++count;
            }
        }

        return count ? total / count : inviter->getLevel();
    }

    // The hard "no" cases: the bot cannot or will not leave what it is doing at all.
    bool CannotJoinNow(Player* bot, Player* inviter, DeclineReason& reason)
    {
        if (!bot->IsAlive())
            reason = DeclineReason::DEAD;
        else if (bot->IsInCombat())
            reason = DeclineReason::COMBAT;
        else if (bot->InBattleground())
            reason = DeclineReason::BATTLEGROUND;
        else if (sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE)
            reason = DeclineReason::QUEUED;
        else if (Map* map = bot->GetMap(); map && map->IsDungeon() && bot->GetMapId() != inviter->GetMapId())
            reason = DeclineReason::DUNGEON;
        else
            return false;

        return true;
    }

    Verdict DecideInvite(PlayerbotAI* botAI, Player* bot, Player* inviter, Group* group)
    {
        GoalInviteConfig const& config = InviteSettings();
        Verdict verdict;

        if (CannotJoinNow(bot, inviter, verdict.reason))
        {
            verdict.accept = false;
            return verdict;
        }

        int32 const levelGap = int32(bot->getLevel()) - int32(GroupLevel(group, inviter, bot));
        if (levelGap <= -config.maxLevelBelow)
        {
            verdict.accept = false;
            verdict.reason = DeclineReason::LOW_LEVEL;
            return verdict;
        }

        int32 chance = config.baseChance + int32(bot->GetGUIDLow() % (2 * PERSONAL_BIAS_RANGE + 1)) - PERSONAL_BIAS_RANGE;

        // The factor that costs the most is what the bot names when it declines.
        int32 worstPenalty = 0;
        auto penalize = [&](int32 penalty, DeclineReason reason)
        {
            if (penalty <= 0)
                return;

            chance -= penalty;
            if (penalty > worstPenalty)
            {
                worstPenalty = penalty;
                verdict.reason = reason;
            }
        };

        int32 const levelOver = std::abs(levelGap) - config.levelTolerance;
        if (levelOver > 0)
            penalize(levelOver * config.levelPenalty, levelGap < 0 ? DeclineReason::LOW_LEVEL : DeclineReason::HIGH_LEVEL);

        NewRpgInfo const& rpg = botAI->GetRpgInfo();
        if (auto const* quest = std::get_if<NewRpgInfo::DoQuest>(&rpg.data))
        {
            WorldPosition const& pos = quest->pos;
            if (pos.GetMapId() == bot->GetMapId() &&
                bot->GetExactDist2d(pos.GetPositionX(), pos.GetPositionY()) <= float(config.busyDistance))
                penalize(config.busyPenalty, DeclineReason::QUEST);
        }

        NewRpgStatus const status = rpg.GetStatus();
        bool const onATask = status == RPG_DO_QUEST || status == RPG_GO_GRIND || status == RPG_GO_CAMP ||
                             status == RPG_TRAVEL_FLIGHT;
        if (onATask && !rpg.HasStatusPersisted(config.newTaskSeconds * IN_MILLISECONDS))
            penalize(config.newTaskPenalty, DeclineReason::NEW_TASK);

        if (inviter->GetSocial() && inviter->GetSocial()->HasFriend(bot->GetGUIDLow()))
            chance += config.friendBonus;
        else if (bot->GetSocial() && bot->GetSocial()->HasFriend(inviter->GetGUIDLow()))
            chance += config.friendBonus;

        if (bot->GetGuildId() && bot->GetGuildId() == inviter->GetGuildId())
            chance += config.guildBonus;

        // The bot's liking score for this human (0 while AiPlayerbot.Social.Liking.Enable is off); the clamp below still applies.
        chance += LikingStore::InviteDelta(bot, inviter);

        if (levelOver <= 0 && bot->GetZoneId() == inviter->GetZoneId() && bot->GetMapId() == inviter->GetMapId())
            chance += SAME_ZONE_BONUS;

        chance = std::max(MIN_CHANCE, std::min(MAX_CHANCE, chance));
        int32 const roll = int32(urand(1, 100));
        verdict.accept = roll <= chance;

        SF_LOG_DEBUG("playerbots", "[INVITE] %s from %s: chance %d, roll %d, level gap %d -> %s", bot->GetName().c_str(),
                     inviter->GetName().c_str(), chance, roll, levelGap, verdict.accept ? "accept" : "decline");
        return verdict;
    }

    // The client's own "decline" answer: the core removes the invite and tells the inviter.
    void SendDecline(Player* bot)
    {
        WorldPacket p;
        uint8 unk = 0;
        p << unk;
        p.WriteBit(true);
        p.WriteBit(false);
        p.FlushBits();
        bot->GetSession()->HandleGroupInviteResponseOpcode(p);
    }

    void Decline(PlayerbotAI* botAI, Player* bot, Player* inviter, DeclineReason reason, bool whisper)
    {
        // The whisper goes first: the decline can end the inviter's pending group.
        if (whisper && bot->IsInWorld() && inviter->IsInWorld())
            BotVoice::Say(botAI, ReasonKey(reason), ReasonDefault(reason), { { "%level", std::to_string(bot->getLevel()) } },
                          BotVoice::Channel::WHISPER, BotVoice::Priority::REPLY, 100, inviter);

        SendDecline(bot);
    }
}

AcceptInvitationAction::AcceptInvitationAction(PlayerbotAI* botAI, const std::string name)
    : SummonAction(botAI, name)
{
}

void AcceptInvitationAction::NoteJoinRequest(Player* bot)
{
    if (!bot)
        return;

    std::lock_guard<std::mutex> guard(joinRequestLock);
    joinRequests[bot->GetGUIDLow()] = time(nullptr);
}

bool AcceptInvitationAction::Execute(Event event)
{
    Group* grp = bot->GetGroupInvite();
    if (!grp)
        return false;

    // #161 AC parity (AC AcceptInvitationAction.cpp:26): the inviter is the pending invite's group leader.
    // The old code parsed the 5.4.8 invite packet with a hard-coded read_skip(27) and silently refused the
    // invite whenever the name came out wrong.
    Player* inviter = ObjectAccessor::FindPlayer(grp->GetLeaderGUID());
    if (!inviter)
        return false;

    if (!botAI->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter))
    {
        Decline(botAI, bot, inviter, DeclineReason::GENERIC, false);
        return false;
    }

    // A random bot answers a player's invite from its own goals. Alt bots, a "join" command, and invites from
    // other bots (which pick their group mates themselves) are accepted as before.
    PlayerbotAI* inviterAI = GET_PLAYERBOT_AI(inviter);
    bool const inviterIsPlayer = !inviterAI || inviterAI->IsRealPlayer();
    bool const joinRequested = ConsumeJoinRequest(bot);
    if (InviteSettings().enabled && inviterIsPlayer && !joinRequested && sRandomPlayerbotMgr->IsRandomBot(bot))
    {
        Verdict const verdict = DecideInvite(botAI, bot, inviter, grp);
        if (!verdict.accept)
        {
            Decline(botAI, bot, inviter, verdict.reason, true);
            return false;
        }
    }

    if (bot->isAFK())
        bot->ToggleAFK();

    WorldPacket p;
    uint8 unk = 0;
    p << unk;
    p.WriteBit(true);
    p.WriteBit(true);
    p.FlushBits();
    bot->GetSession()->HandleGroupInviteResponseOpcode(p);

    if (!bot->GetGroup() || !bot->GetGroup()->IsMember(inviter->GetGUID()))
        return false;

    if (sRandomPlayerbotMgr->IsRandomBot(bot))
        botAI->SetMaster(inviter);

    botAI->ResetStrategies();
    botAI->ChangeStrategy("+follow,-lfg,-bg", BOT_STATE_NON_COMBAT);
    botAI->Reset();

    // A random bot greets in party chat with its own manner; alt bots keep the plain hello.
    if (!(BotVoice::Enabled() && sRandomPlayerbotMgr->IsRandomBot(bot)))
        botAI->TellMaster(PlayerbotTextMgr::instance().GetBotTextOrDefault("hello", "Hello", {}));
    else
        BotVoice::Say(botAI, BotVoice::Moment::JOIN_GROUP);

    // AC AcceptInvitationAction.cpp:61-64 (#177): summon the bot to the inviter if it accepted from
    // out of sight range. A random bot travels there instead (TravelToGroupAction), into a dungeon through its
    // entrance, unless the entrance is unknown or AiPlayerbot.TravelToGroup.SummonIntoDungeons is on. Alt bots are
    // always summoned.
    if (sPlayerbotAIConfig->summonWhenGroup && bot->GetDistance(inviter) > sPlayerbotAIConfig->sightDistance)
    {
        Map* inviterMap = inviter->GetMap();
        bool const intoInstance = inviterMap && inviterMap->Instanceable();
        bool const walksIn = !intoInstance || (!GroupTravel::SummonIntoDungeons() && GroupTravel::HasEntrance(inviter->GetMapId()));
        bool const travels = GroupTravel::Enabled() && sRandomPlayerbotMgr->IsRandomBot(bot) && walksIn;
        if (!travels)
            Teleport(inviter, bot, true);
    }
    return true;
}
