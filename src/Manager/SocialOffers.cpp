/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * See SocialOffers.h and docs/social/social.md section 12.
 */

#include "SocialOffers.h"

#include "BotVoice.h"
#include "ChatHelper.h"
#include "Group.h"
#include "InviteToGroupAction.h"
#include "LikingStore.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "QuestOfferPolicy.h"
#include "RandomPlayerbotMgr.h"
#include "SocialLog.h"
#include "SocialSoakPolicy.h"
#include "WorldSession.h"

#include <ctime>
#include <string>
#include <vector>

namespace
{
    constexpr uint32 PERCENT = 100;
    // A human below this level is not offered: the first quests of the start zones are shared by crowds anyway.
    constexpr uint8 MIN_HUMAN_LEVEL = 5;

    uint32 nextPassAt = 0;

    bool IsHuman(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot() && !GET_PLAYERBOT_AI(player);
    }

    bool InInstanceOrBattleground(Player* player)
    {
        return player->InBattleground() || player->InBattlegroundQueue() || player->InArena() ||
            (player->GetMap() && player->GetMap()->IsDungeon());
    }

    bool Free(Player* player)
    {
        return player->IsInWorld() && player->IsAlive() && !player->IsInCombat() && !player->IsInFlight() &&
            !player->IsBeingTeleported() && !InInstanceOrBattleground(player);
    }

    // Quests in the log that are not done yet. A quest that is only "none" (an item that starts it) does not count here:
    // the offer is about work both are doing.
    std::vector<uint32> IncompleteQuests(Player* player)
    {
        std::vector<uint32> quests;
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (uint32 const questId = player->GetQuestSlotQuestId(slot))
                if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                    quests.push_back(questId);

        return quests;
    }

    uint32 GroupSize(Player* player)
    {
        Group* group = player->GetGroup();
        return group ? group->GetMembersCount() : 1u;
    }

    struct Candidate
    {
        Player* bot = nullptr;
        uint32 questId = 0;
    };

    // The bots near the human that pass the policy, with the quest they share, nearest first.
    std::vector<Candidate> CandidatesFor(QuestOfferPolicy::Tuning const& tuning, Player* human,
        std::vector<uint32> const& humanQuests, std::vector<Player*> const& bots)
    {
        std::vector<QuestOfferPolicy::Candidate> order;
        std::vector<Candidate> found;
        for (Player* bot : bots)
        {
            if (!bot || !bot->IsInWorld() || bot->GetMapId() != human->GetMapId() || bot->GetTeamId() != human->GetTeamId() ||
                !sRandomPlayerbotMgr->IsRandomBot(bot) || !GET_PLAYERBOT_AI(bot))
                continue;

            float const yards = bot->GetExactDist2d(human);
            if (yards > tuning.radiusYards)
                continue;

            QuestOfferPolicy::Facts facts;
            facts.botBusy = !Free(bot) || bot->GetGroupInvite() != nullptr;
            facts.botGrouped = GroupSize(bot) > 1;
            facts.humanBusy = false;
            facts.humanGrouped = false;
            facts.sameMap = true;
            facts.sameZone = bot->GetZoneId() == human->GetZoneId();
            facts.yards = yards;
            facts.botLevel = bot->getLevel();
            facts.humanLevel = human->getLevel();
            facts.inScope = SocialLog::InScope(bot);
            uint32 const shared = QuestOfferPolicy::FirstSharedQuest(humanQuests, IncompleteQuests(bot));
            facts.sharedQuest = shared != 0;
            QuestOfferPolicy::Verdict const verdict = QuestOfferPolicy::Decide(tuning, facts);
            if (verdict == QuestOfferPolicy::Verdict::OutOfScope)
                SocialLog::Count(SocialSoak::Counter::OutOfScope);
            if (verdict != QuestOfferPolicy::Verdict::Offer)
                continue;

            order.push_back({ bot->GetGUIDLow(), yards });
            found.push_back({ bot, shared });
        }

        QuestOfferPolicy::OrderNearestFirst(order);
        std::vector<Candidate> sorted;
        for (QuestOfferPolicy::Candidate const& entry : order)
            for (Candidate const& candidate : found)
                if (candidate.bot->GetGUIDLow() == entry.bot)
                {
                    sorted.push_back(candidate);
                    break;
                }

        return sorted;
    }

    // The whisper and the invite. The invite goes first: when it cannot be sent (the human got an invite meanwhile) nothing is said.
    bool Offer(Player* bot, Player* human, uint32 questId)
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
            return false;

        InviteToGroupAction invite(botAI);
        if (!invite.Invite(bot, human))
            return false;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        std::map<std::string, std::string> const placeholders = {
            { "%name", human->GetName() },
            { "%quest", quest ? ChatHelper::FormatQuest(quest) : std::string("the same quest") } };
        BotVoice::Say(botAI, "social_quest_offer", "Hey %name, we are both working on %quest. Want to group up?", placeholders,
            BotVoice::Channel::WHISPER, BotVoice::Priority::NOTICE, PERCENT, human);
        SocialLog::Count(SocialSoak::Counter::QuestOffer);
        SocialLog::Offer(SocialSoak::OfferLine("quest", bot->GetName(), human->GetGUIDLow(), questId, true));
        return true;
    }
}  // namespace

void SocialOffers::Update()
{
    // The soak summary line rides on this call: it comes from the world thread every update, even with the offers off.
    SocialLog::Update();

    if (!sPlayerbotAIConfig->socialQuestOfferEnable || !LikingStore::Enabled() || !sPlayerbotAIConfig->enabled)
        return;

    uint32 const now = uint32(time(nullptr));
    if (now < nextPassAt)
        return;

    QuestOfferPolicy::Tuning const& tuning = sPlayerbotAIConfig->socialQuestOffer;
    nextPassAt = now + tuning.passSeconds;

    std::vector<Player*> const humans = sRandomPlayerbotMgr->GetPlayers();
    if (humans.empty())
        return;

    std::vector<Player*> const bots = sRandomPlayerbotMgr->GetPlayerBotsSnapshot();
    for (Player* human : humans)
    {
        if (!IsHuman(human) || human->getLevel() < MIN_HUMAN_LEVEL || !Free(human) || human->isDND() ||
            human->GetGroupInvite() || GroupSize(human) > 1)
            continue;

        std::vector<uint32> const quests = IncompleteQuests(human);
        if (quests.empty())
            continue;

        std::vector<Candidate> const candidates = CandidatesFor(tuning, human, quests, bots);
        bool taken = false;
        for (Candidate const& candidate : candidates)
        {
            if (!LikingStore::MayOfferQuestGroup(candidate.bot, human, tuning.pairCooldownSeconds))
                continue;

            // One offer per human per pass; a failed invite (the human was invited meanwhile) ends their pass too.
            Offer(candidate.bot, human, candidate.questId);
            taken = true;
            break;
        }

        // Counted once per human and pass, not per candidate: the gate says no to most bots on most passes.
        if (!taken && !candidates.empty())
            SocialLog::Count(SocialSoak::Counter::QuestOfferRefused);
    }
}
