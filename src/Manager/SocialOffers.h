/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The shared-quest group offer (design decision 0033 point 16, docs/social/social.md section 12): a random bot that has the same
 * unfinished quest as a human nearby and likes them enough whispers an offer and sends the ordinary group invite, which the
 * human can decline. One pass on the world thread every AiPlayerbot.Social.QuestOffer.PassSeconds, nothing per bot tick.
 * The decisions are Utils/QuestOfferPolicy.h; the liking gate, the pair cooldown and the cap per human per hour are
 * LikingStore::MayOfferQuestGroup. Needs AiPlayerbot.Social.QuestOffer.Enable and AiPlayerbot.Social.Liking.Enable; with
 * either off Update returns at once.
 *
 * Players are never touched beyond the invite and a whisper: no gold, no items, no quest sharing (design decision 0016 and 0017).
 */

#ifndef _PLAYERBOT_SOCIALOFFERS_H
#define _PLAYERBOT_SOCIALOFFERS_H

namespace SocialOffers
{
    // World thread, from RandomPlayerbotMgr::UpdateAIInternal next to LikingStore::Update; throttles itself.
    void Update();
}

#endif
