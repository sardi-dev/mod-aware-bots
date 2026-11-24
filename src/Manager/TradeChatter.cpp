#include "TradeChatter.h"

#include "BotAuction.h"
#include "BotAuctionPolicy.h"
#include "BotVoice.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "Map.h"
#include "SpellInfo.h"
#include "BroadcastHelper.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PlaystylePolicy.h"
#include "PriceTable.h"
#include "RandomPlayerbotMgr.h"
#include "SocialLog.h"
#include "SpellMgr.h"
#include "TradeChatterPolicy.h"
#include "WorldSession.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using Clock = std::chrono::steady_clock;
    uint64 NowSeconds() { return std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count(); }
    struct ChatterState { uint64 login = 0; uint64 nextCheck = 0; uint64 lastSent = 0; };
    std::mutex lock;
    std::unordered_map<uint32, ChatterState> bots;
    std::deque<uint64> channels[2];

    std::vector<std::string> Recipes(Player* bot)
    {
        std::vector<std::string> result;
        std::unordered_set<uint32> seen;
        for (auto const& known : bot->GetSpellMap())
        {
            if (!known.second || known.second->state == PLAYERSPELL_REMOVED || !known.second->active) continue;
            auto bounds = sSpellMgr->GetSkillLineAbilityMapBounds(known.first);
            bool profession = false;
            for (auto it = bounds.first; it != bounds.second; ++it)
                if (it->second && bot->HasSkill(it->second->skillId) &&
                    (BotAuctionPolicy::IsCraftingSkill(it->second->skillId) ||
                     BotAuctionPolicy::IsSecondarySkill(it->second->skillId) || it->second->skillId == BotAuctionPolicy::Skill::MINING))
                    { profession = true; break; }
            if (!profession) continue;
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(known.first);
            if (!spell) continue;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                uint32 item = spell->Effects[i].ItemType;
                PricePolicy::PriceRange range;
                if (spell->Effects[i].Effect != SPELL_EFFECT_CREATE_ITEM || !item || !seen.insert(item).second || !PriceTable::Find(item, range)) continue;
                if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item)) result.push_back(proto->Name1);
            }
        }
        return result;
    }
}

void TradeChatter::Login(Player* player)
{
    if (!player || !player->GetSession() || !player->GetSession()->IsBot() ||
        !sPlayerbotAIConfig->IsInRandomAccountList(player->GetSession()->GetAccountId())) return;
    std::lock_guard<std::mutex> guard(lock);
    ChatterState& state = bots[player->GetGUIDLow()];
    state.login = NowSeconds();
    state.nextCheck = 0;
}

void TradeChatter::Logout(Player* player)
{
    if (!player) return;
    std::lock_guard<std::mutex> guard(lock);
    auto it = bots.find(player->GetGUIDLow());
    if (it != bots.end()) it->second.login = NowSeconds();
}

void TradeChatter::Update(PlayerbotAI* ai, Player* bot)
{
    if (!sPlayerbotAIConfig->socialTradeChatterEnable || !ai || !bot || !bot->GetSession() ||
        !bot->GetSession()->IsBot() || ai->IsRealPlayer() ||
        !sPlayerbotAIConfig->IsInRandomAccountList(bot->GetSession()->GetAccountId()) || !SocialLog::InScope(bot)) return;
    AreaTableEntry const* zone = GetAreaEntryByAreaID(bot->GetZoneId());
    if (!zone || !(zone->m_flags & AREA_FLAG_CAPITAL)) return;
    auto style = PlaystylePolicy::StyleFor(bot->GetGUIDLow(), sPlayerbotAIConfig->socialPlaystyleShares);
    TradeChatterPolicy::Style kind = style == PlaystylePolicy::Style::Trader ? TradeChatterPolicy::Style::Trader :
        style == PlaystylePolicy::Style::GathererCrafter ? TradeChatterPolicy::Style::GathererCrafter : TradeChatterPolicy::Style::Other;
    if (kind == TradeChatterPolicy::Style::Other) return;
    uint64 const now = NowSeconds();
    uint32 const guid = bot->GetGUIDLow();
    {
        std::lock_guard<std::mutex> guard(lock);
        auto it = bots.find(guid);
        if (it == bots.end()) it = bots.emplace(guid, ChatterState{now, 0, 0}).first;
        ChatterState& state = it->second;
        if (now - state.login < 300 || now < state.nextCheck) return;
        state.nextCheck = now + 30;

    }
    bool present = false;
    for (auto const& ref : bot->GetMap()->GetPlayers())
    {
        Player* human = ref.GetSource();
        if (human && human->IsInWorld() && human->GetZoneId() == bot->GetZoneId() && human->GetSession() &&
            !human->GetSession()->IsBot() && !GET_PLAYERBOT_AI(human)) { present = true; break; }
    }
    if (!present) return;
    {
        std::lock_guard<std::mutex> guard(lock);
        auto it = bots.find(guid);
        if (it == bots.end()) return;
        if (it->second.lastSent && now - it->second.lastSent < uint64(sPlayerbotAIConfig->socialTradeChatterBotMinutes) * 60)
        {
            SocialLog::Count(SocialSoak::Counter::TradeChatterBotCap);
            return;
        }
    }
    std::vector<TradeChatterPolicy::Listing> listings;
    if (kind == TradeChatterPolicy::Style::Trader)
        for (auto const& entry : BotAuction::ActiveListings(guid))
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry.itemEntry))
                listings.push_back({proto->Name1 + (entry.count > 1 ? " x" + std::to_string(entry.count) : ""), entry.buyout, true, true});
    auto ad = TradeChatterPolicy::Choose(kind, listings, kind == TradeChatterPolicy::Style::GathererCrafter ? Recipes(bot) : std::vector<std::string>{}, 3);
    if (ad.kind == TradeChatterPolicy::Kind::None) return;
    std::lock_guard<std::mutex> guard(lock);
    auto it = bots.find(guid);
    if (it == bots.end()) return;
    if (it->second.lastSent && now - it->second.lastSent < uint64(sPlayerbotAIConfig->socialTradeChatterBotMinutes) * 60)
    {
        SocialLog::Count(SocialSoak::Counter::TradeChatterBotCap);
        return;
    }
    auto& channel = channels[bot->GetTeam() == ALLIANCE ? 0 : 1];
    TradeChatterPolicy::Prune(channel, now, 60);
    if (channel.size() >= sPlayerbotAIConfig->socialTradeChatterChannelPerMinute)
    {
        SocialLog::Count(SocialSoak::Counter::TradeChatterChannelCap);
        return;
    }
    std::string text = TradeChatterPolicy::Text(ad, uint32(BotVoice::PersonalityOf(bot)));
    if (BroadcastHelper::SayToChannel(bot, text, BroadcastHelper::TO_TRADE))
    {
        channel.push_back(now);
        it->second.lastSent = now;
        SocialLog::Count(SocialSoak::Counter::TradeChatterSent);
    }
}
