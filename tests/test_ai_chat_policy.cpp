/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "AiChatPolicy.h"
#include "TestHarness.h"

#include <set>

using AiChatPolicy::Channel;
using AiChatPolicy::CommandSyntax;
using AiChatPolicy::Gate;
using AiChatPolicy::Kind;
using AiChatPolicy::Source;

namespace
{
    Gate OpenGate()
    {
        Gate gate;
        gate.layerOn = gate.chatOn = gate.linkUp = true;
        gate.nearbyChancePct = 25;
        gate.partyChancePct = 30;
        return gate;
    }

    // A few of the module's real trigger names (ChatTriggerContext.h).
    std::function<bool(std::string const&)> Triggers()
    {
        static std::set<std::string> const names = { "who", "follow", "stay", "attack", "i need", "help", "t", "talk" };
        return [](std::string const& name) { return names.count(name) > 0; };
    }

    // A few action names (ActionContext.h).
    std::function<bool(std::string const&)> Actions()
    {
        static std::set<std::string> const names = { "follow", "drink", "attack duel opponent" };
        return [](std::string const& name) { return names.count(name) > 0; };
    }

    CommandSyntax DefaultSyntax()
    {
        return { "\\\\", "" };
    }
}

TEST(ai_chat_kind_and_channel_names_match_the_sidecar)
{
    CHECK(std::string(AiChatPolicy::KindName(Kind::DUEL_CHALLENGE)) == "duel_challenge");
    CHECK(std::string(AiChatPolicy::KindName(Kind::DUEL_WIN)) == "duel_win");
    CHECK(std::string(AiChatPolicy::KindName(Kind::DUEL_LOSS)) == "duel_loss");
    CHECK(std::string(AiChatPolicy::KindName(Kind::WHISPER_REPLY)) == "whisper_reply");
    CHECK(std::string(AiChatPolicy::KindName(Kind::NEARBY_REPLY)) == "nearby_reply");
    CHECK(std::string(AiChatPolicy::KindName(Kind::PARTY_MOMENT)) == "party_moment");
    CHECK(std::string(AiChatPolicy::KindName(Kind::GUILD_REPLY)) == "guild_reply");
    CHECK(std::string(AiChatPolicy::KindName(Kind::OPENER)) == "opener");
    CHECK(std::string(AiChatPolicy::KindName(Kind::FRIEND_WHISPER)) == "friend_whisper");
    CHECK(std::string(AiChatPolicy::KindName(Kind::BG_WIN)) == "bg_win");
    CHECK(std::string(AiChatPolicy::KindName(Kind::BG_LOSS)) == "bg_loss");
    CHECK(std::string(AiChatPolicy::KindName(Kind::BG_CAPTURE)) == "bg_capture");
    CHECK(std::string(AiChatPolicy::ChannelName(Channel::BATTLEGROUND)) == "battleground");
    CHECK(std::string(AiChatPolicy::ChannelName(Channel::SAY)) == "say");
    CHECK(std::string(AiChatPolicy::ChannelName(Channel::RAID)) == "raid");
    CHECK(std::string(AiChatPolicy::SourceName(Source::FAKE)) == "fake");
}

TEST(ai_chat_gate_needs_the_layer_the_chat_switch_and_the_link)
{
    Gate gate = OpenGate();
    CHECK(AiChatPolicy::ShouldAsk(gate, Kind::WHISPER_REPLY, false, 100));
    gate.chatOn = false;
    CHECK(!AiChatPolicy::ShouldAsk(gate, Kind::WHISPER_REPLY, false, 1));
    gate = OpenGate();
    gate.layerOn = false;
    CHECK(!AiChatPolicy::ShouldAsk(gate, Kind::DUEL_WIN, true, 1));
    gate = OpenGate();
    gate.linkUp = false;
    CHECK(!AiChatPolicy::ShouldAsk(gate, Kind::GUILD_REPLY, true, 1));
}

TEST(ai_chat_nearby_and_party_lines_are_answered_only_sometimes)
{
    Gate const gate = OpenGate();
    CHECK(AiChatPolicy::ShouldAsk(gate, Kind::NEARBY_REPLY, false, 25));
    CHECK(!AiChatPolicy::ShouldAsk(gate, Kind::NEARBY_REPLY, false, 26));
    CHECK(AiChatPolicy::ShouldAsk(gate, Kind::PARTY_MOMENT, false, 30));
    CHECK(!AiChatPolicy::ShouldAsk(gate, Kind::PARTY_MOMENT, false, 31));
    // A line that names the bot is always answered.
    CHECK(AiChatPolicy::ShouldAsk(gate, Kind::NEARBY_REPLY, true, 100));
    // Whispers, duels and guild lines have no chance of their own.
    CHECK(AiChatPolicy::ChancePct(gate, Kind::WHISPER_REPLY, false) == 100);
    CHECK(AiChatPolicy::ChancePct(gate, Kind::DUEL_LOSS, false) == 100);
    Gate wide = gate;
    wide.nearbyChancePct = 250;
    CHECK(AiChatPolicy::ChancePct(wide, Kind::NEARBY_REPLY, false) == 100);
}

TEST(ai_chat_commands_never_get_a_chat_reply)
{
    CommandSyntax const syntax = DefaultSyntax();
    auto const triggers = Triggers();
    CHECK(AiChatPolicy::IsCommandLike("follow", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("  Follow!  ", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("attack", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("who are you", syntax, triggers));  // the module's "who" answers it
    CHECK(AiChatPolicy::IsCommandLike("i need help", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("inv", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("inv pls", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("can you inv me?", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("come here", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("follow\\\\stay", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("#w stats", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("@tank attack", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("do follow", syntax, triggers, Actions()));
    CHECK(AiChatPolicy::IsCommandLike("d drink water", syntax, triggers, Actions()));
    CHECK(AiChatPolicy::IsCommandLike("reset", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("|cff1eff00|Hitem:2589|h[Linen Cloth]|h|r", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("   ", syntax, triggers));
}

TEST(ai_chat_small_talk_is_chat)
{
    CommandSyntax const syntax = DefaultSyntax();
    auto const triggers = Triggers();
    CHECK(!AiChatPolicy::IsCommandLike("hey, how is it going?", syntax, triggers));
    CHECK(!AiChatPolicy::IsCommandLike("nice gear", syntax, triggers));
    CHECK(!AiChatPolicy::IsCommandLike("gg, that was close", syntax, triggers));
    // A request word late in the line is only an action when it can stand anywhere ("inv", "trade").
    CHECK(!AiChatPolicy::IsCommandLike("where do you come from", syntax, triggers));
    CHECK(AiChatPolicy::IsCommandLike("where do you trade", syntax, triggers));
    // "do" only commands with an action after it.
    CHECK(!AiChatPolicy::IsCommandLike("do you duel?", syntax, triggers, Actions()));
    CHECK(!AiChatPolicy::IsCommandLike("d you there", syntax, triggers, Actions()));
    // Without a bot context only the syntax and the request words count.
    CHECK(!AiChatPolicy::IsCommandLike("who are you", syntax, {}));
}

TEST(ai_chat_command_prefix_marks_commands)
{
    CommandSyntax const syntax{ "", "!bot " };
    CHECK(AiChatPolicy::IsCommandLike("!bot follow", syntax, {}));
    CHECK(!AiChatPolicy::IsCommandLike("hello there", syntax, {}));
}

TEST(ai_chat_name_mentions_are_whole_words)
{
    CHECK(AiChatPolicy::MentionsName("hey taran, duel?", "Taran"));
    CHECK(AiChatPolicy::MentionsName("TARAN", "Taran"));
    CHECK(!AiChatPolicy::MentionsName("taranis is here", "Taran"));
    CHECK(!AiChatPolicy::MentionsName("hello", ""));
}

TEST(ai_chat_line_falls_back_to_the_rules)
{
    AiChatPolicy::Line line = AiChatPolicy::ChooseLine("llm", "gg, again?", "Ouch. Let's try again");
    CHECK(line.text == "gg, again?" && line.source == Source::AI);
    line = AiChatPolicy::ChooseLine("fake", "(test reply) hi", "");
    CHECK(line.source == Source::FAKE);
    // A hint (disabled, rate_limited, budget_exhausted, no_api_key ...), an error or a timeout: the rule line.
    line = AiChatPolicy::ChooseLine("rules_hint", "", "Ouch. Let's try again");
    CHECK(line.text == "Ouch. Let's try again" && line.source == Source::RULES);
    line = AiChatPolicy::ChooseLine("error", "", "");
    CHECK(line.text.empty() && line.source == Source::RULES);
    // An AI reply that is empty once cleaned is no reply.
    line = AiChatPolicy::ChooseLine("llm", " | \n ", "fallback");
    CHECK(line.text == "fallback" && line.source == Source::RULES);
}

TEST(ai_chat_reply_is_made_safe_for_one_chat_line)
{
    CHECK(AiChatPolicy::SanitizeReply("  hi\nthere |cffff0000red|r  ") == "hi there cffff0000redr");
    std::string const longText(300, 'a');
    CHECK(AiChatPolicy::SanitizeReply(longText).size() == AiChatPolicy::MAX_LINE_BYTES);
    std::string words;
    while (words.size() < 300)
        words += "word ";
    std::string const cut = AiChatPolicy::SanitizeReply(words);
    CHECK(cut.size() <= AiChatPolicy::MAX_LINE_BYTES);
    CHECK(cut.back() != ' ');
    CHECK(cut.substr(cut.size() - 4) == "word");
    // Never in the middle of a UTF-8 character.
    std::string accents;
    while (accents.size() < 300)
        accents += "\xC3\xA9";
    std::string const cutAccents = AiChatPolicy::SanitizeReply(accents);
    CHECK(cutAccents.size() == 254);
    CHECK(AiChatPolicy::TruncateUtf8("ab\xC3\xA9", 3) == "ab");
}

TEST(ai_chat_delay_reads_and_types_like_a_player)
{
    // A moment without a human line and a short answer: the floor.
    CHECK(AiChatPolicy::ReplyDelayMs(0, 2, 50) == AiChatPolicy::MIN_DELAY_MS);
    // Longer lines take longer, within the cap.
    uint32_t const shortReply = AiChatPolicy::ReplyDelayMs(20, 20, 50);
    uint32_t const longReply = AiChatPolicy::ReplyDelayMs(20, 60, 50);
    CHECK(longReply > shortReply);
    CHECK(shortReply == 500 + 20 * 20 + 20 * 55);
    CHECK(AiChatPolicy::ReplyDelayMs(400, 400, 100) == AiChatPolicy::MAX_DELAY_MS);
    // The owner's 1 to 5 s.
    CHECK(AiChatPolicy::MIN_DELAY_MS == 1000 && AiChatPolicy::MAX_DELAY_MS == 5000);
    // The jitter spreads +-20 %.
    CHECK(AiChatPolicy::ReplyDelayMs(20, 20, 0) == (500 + 20 * 20 + 20 * 55) * 80 / 100);
    CHECK(AiChatPolicy::ReplyDelayMs(20, 20, 100) == (500 + 20 * 20 + 20 * 55) * 120 / 100);
}

TEST(ai_chat_guild_answerers_and_window)
{
    CHECK(AiChatPolicy::GuildAnswerers(0) == 1);
    CHECK(AiChatPolicy::GuildAnswerers(2) == 2);
    CHECK(AiChatPolicy::GuildAnswerers(9) == 2);
    CHECK(AiChatPolicy::StillFresh(60000, 60000));
    CHECK(!AiChatPolicy::StillFresh(60001, 60000));
}

TEST(ai_chat_context_tokens_fit_the_sidecar_schema)
{
    CHECK(std::string(AiChatPolicy::ClassToken(6)) == "death_knight");
    CHECK(std::string(AiChatPolicy::ClassToken(10)) == "monk");
    CHECK(std::string(AiChatPolicy::ClassToken(0)) == "adventurer");
    CHECK(std::string(AiChatPolicy::RaceToken(4)) == "night_elf");
    CHECK(std::string(AiChatPolicy::RaceToken(26)) == "pandaren");
    CHECK(std::string(AiChatPolicy::RaceToken(99)) == "unknown");
    AiChatPolicy::Traits const grumpy = AiChatPolicy::PersonalityTraits(3);
    CHECK(grumpy.sociability <= 33 && grumpy.aggression >= 66);
    AiChatPolicy::Traits const quiet = AiChatPolicy::PersonalityTraits(0);
    CHECK(quiet.chattiness <= 33);
    CHECK(AiChatPolicy::PersonalityTraits(7).honor == 50);
}

TEST(ai_chat_archetypes_follow_the_shared_trait_rule)
{
    using AiChatPolicy::Archetype;
    using AiChatPolicy::ArchetypeOf;
    using AiChatPolicy::PersonalityTraits;
    // Each voice personality lands in its own archetype: quiet, friendly, joker as chatty, grumpy as rough.
    CHECK(ArchetypeOf(PersonalityTraits(0)) == Archetype::QUIET);
    CHECK(ArchetypeOf(PersonalityTraits(1)) == Archetype::FRIENDLY);
    CHECK(ArchetypeOf(PersonalityTraits(2)) == Archetype::CHATTY);
    CHECK(ArchetypeOf(PersonalityTraits(3)) == Archetype::ROUGH);
    // The edges of the contract.
    CHECK(ArchetypeOf({ 90, 50, 25, 10 }) == Archetype::QUIET);
    CHECK(ArchetypeOf({ 70, 50, 50, 40 }) == Archetype::ROUGH);
    CHECK(ArchetypeOf({ 70, 50, 50, 41 }) == Archetype::FRIENDLY);
    CHECK(ArchetypeOf({ 69, 50, 80, 50 }) == Archetype::CHATTY);
    CHECK(ArchetypeOf({ 70, 50, 90, 50 }) == Archetype::FRIENDLY);
    CHECK(std::string(AiChatPolicy::ArchetypeName(Archetype::ROUGH)) == "rough");
}

TEST(ai_chat_random_bots_get_the_spec_personality_mix)
{
    uint32_t counts[4] = {};
    uint32_t constexpr bots = 20000;
    for (uint64_t guid = 1; guid <= bots; ++guid)
        ++counts[AiChatPolicy::PersonalityIndex(guid, true)];

    // 25 quiet, 50 friendly, 10 chatty, 15 rough, within 1.5 points.
    uint32_t const expected[4] = { 25, 50, 10, 15 };
    for (int i = 0; i < 4; ++i)
    {
        uint32_t const share = counts[i] * 1000 / bots;
        CHECK(share + 15 >= expected[i] * 10 && share <= expected[i] * 10 + 15);
    }

    // Stable per guid, and alt bots keep the older split.
    CHECK(AiChatPolicy::PersonalityIndex(4242, true) == AiChatPolicy::PersonalityIndex(4242, true));
    uint32_t altQuiet = 0;
    for (uint64_t guid = 1; guid <= bots; ++guid)
        altQuiet += AiChatPolicy::PersonalityIndex(guid, false) == 0;
    CHECK(altQuiet * 1000 / bots + 15 >= 300 && altQuiet * 1000 / bots <= 315);
}

TEST(ai_chat_nearby_chance_reads_name_distance_and_personality)
{
    using AiChatPolicy::Archetype;
    using AiChatPolicy::NearbyChancePct;
    AiChatPolicy::NearbyCue cue;
    cue.basePct = 25;
    // Unprompted: the base scaled by the personality.
    cue.archetype = Archetype::FRIENDLY;
    CHECK(NearbyChancePct(cue) == 25);
    cue.archetype = Archetype::QUIET;
    CHECK(NearbyChancePct(cue) == 12);
    cue.archetype = Archetype::ROUGH;
    CHECK(NearbyChancePct(cue) == 20);
    cue.archetype = Archetype::CHATTY;
    CHECK(NearbyChancePct(cue) == 37);
    // Very close doubles it.
    cue.veryClose = true;
    CHECK(NearbyChancePct(cue) == 74);
    cue.archetype = Archetype::FRIENDLY;
    CHECK(NearbyChancePct(cue) == 50);
    // Named (or targeted): 75 to 90, rough low, chatty high, never certain; close keeps the cap of 90.
    cue.veryClose = false;
    cue.named = true;
    cue.archetype = Archetype::ROUGH;
    CHECK(NearbyChancePct(cue) == 75);
    cue.archetype = Archetype::CHATTY;
    CHECK(NearbyChancePct(cue) == 90);
    cue.archetype = Archetype::QUIET;
    cue.veryClose = true;
    CHECK(NearbyChancePct(cue) == 90);
    // A base of 0 keeps unprompted lines silent even up close; a huge base stays a percentage.
    cue.named = false;
    cue.basePct = 0;
    CHECK(NearbyChancePct(cue) == 0);
    cue.basePct = 500;
    cue.archetype = Archetype::CHATTY;
    cue.veryClose = false;
    CHECK(NearbyChancePct(cue) == 100);
}

TEST(ai_chat_timing_is_late_now_and_then_and_holds_in_combat)
{
    using AiChatPolicy::PlanTiming;
    using AiChatPolicy::Timing;
    AiChatPolicy::TimingCue cue;
    // Moments always come now.
    CHECK(PlanTiming(cue, 1) == Timing::NOW);
    cue.humanLine = true;
    // Out of combat: 7 percent late.
    CHECK(PlanTiming(cue, 7) == Timing::LATE);
    CHECK(PlanTiming(cue, 8) == Timing::NOW);
    cue.lateAllowed = false;
    CHECK(PlanTiming(cue, 1) == Timing::NOW);
    // In combat: 20 skip, then 60 wait for the end of the fight, 20 answer anyway.
    cue.inCombat = true;
    CHECK(PlanTiming(cue, 20) == Timing::SKIP);
    CHECK(PlanTiming(cue, 21) == Timing::AFTER_COMBAT);
    CHECK(PlanTiming(cue, 80) == Timing::AFTER_COMBAT);
    CHECK(PlanTiming(cue, 81) == Timing::NOW);
    // A whisper is never skipped: it waits for its late line.
    cue.whisper = true;
    CHECK(PlanTiming(cue, 1) == Timing::AFTER_COMBAT);
    // Chances that add past 100 are clamped.
    cue.whisper = false;
    cue.combatSkipPct = 70;
    cue.combatWaitPct = 70;
    CHECK(PlanTiming(cue, 70) == Timing::SKIP);
    CHECK(PlanTiming(cue, 100) == Timing::AFTER_COMBAT);
    CHECK(std::string(AiChatPolicy::TimingName(Timing::AFTER_COMBAT)) == "after_combat");
}

TEST(ai_chat_conversation_mode_merges_rapid_lines)
{
    using AiChatPolicy::JoinLines;
    // Two minutes of silence end a conversation.
    CHECK(AiChatPolicy::InConversation(true, 119999));
    CHECK(!AiChatPolicy::InConversation(true, 120000));
    CHECK(!AiChatPolicy::InConversation(false, 10));
    // Merged lines are joined with " / ", the last four kept.
    CHECK(JoinLines({ "hey" }) == "hey");
    CHECK(JoinLines({ "hey", "u there?" }) == "hey / u there?");
    CHECK(JoinLines({ "1", "2", "3", "4", "5" }) == "2 / 3 / 4 / 5");
    CHECK(JoinLines({}).empty());
    // The request waits a reading time for more lines, up to 1.5 s, and a merged line holds it up to 2.5 s.
    CHECK(AiChatPolicy::FirstGatherMs(0) == AiChatPolicy::REACTION_MS);
    CHECK(AiChatPolicy::FirstGatherMs(20) == 500 + 20 * 20);
    CHECK(AiChatPolicy::FirstGatherMs(400) == AiChatPolicy::MAX_FIRST_GATHER_MS);
    CHECK(AiChatPolicy::ExtendedGatherMs(900, 600) == 1300);
    CHECK(AiChatPolicy::ExtendedGatherMs(1500, 100) == 1500);
    CHECK(AiChatPolicy::ExtendedGatherMs(1500, 2400) == AiChatPolicy::MAX_GATHER_MS);
}

TEST(ai_chat_memory_events_and_the_ninja_rule)
{
    using AiChatPolicy::IsOffArmorNeed;
    using AiChatPolicy::MemoryEvent;
    CHECK(std::string(AiChatPolicy::MemoryEventName(MemoryEvent::BOSS_KILLED_TOGETHER)) == "boss_killed_together");
    CHECK(std::string(AiChatPolicy::MemoryEventName(MemoryEvent::DUEL_LOST)) == "duel_lost");
    CHECK(std::string(AiChatPolicy::MemoryEventName(MemoryEvent::LOOT_PASSED)) == "loot_passed");
    // A level 90 warrior needing cloth or leather robs a bot; plate is his own.
    CHECK(IsOffArmorNeed(1, 90, 4, 1, 20));
    CHECK(IsOffArmorNeed(1, 90, 4, 2, 7));
    CHECK(!IsOffArmorNeed(1, 90, 4, 4, 5));
    // Before 40 a warrior wears mail, so mail is fair then.
    CHECK(!IsOffArmorNeed(1, 30, 4, 3, 5));
    CHECK(IsOffArmorNeed(1, 30, 4, 2, 5));
    // A priest needing cloth is fine; cloaks, weapons and shields are no armor type.
    CHECK(!IsOffArmorNeed(5, 90, 4, 1, 20));
    CHECK(!IsOffArmorNeed(1, 90, 4, 1, 16));
    CHECK(!IsOffArmorNeed(1, 90, 2, 7, 13));
    CHECK(!IsOffArmorNeed(1, 90, 4, 6, 14));
    // Monks wear leather, hunters mail from 40.
    CHECK(IsOffArmorNeed(10, 90, 4, 1, 5));
    CHECK(IsOffArmorNeed(3, 90, 4, 2, 5));
    CHECK(!IsOffArmorNeed(3, 20, 4, 2, 5));
    CHECK(!IsOffArmorNeed(0, 90, 4, 1, 5));
}

TEST(ai_chat_openers_are_rare_and_cooled_down)
{
    using AiChatPolicy::MayOpen;
    AiChatPolicy::OpenerCue cue;
    cue.chancePct = 60;
    cue.cooldownMs = 120000;
    CHECK(MayOpen(cue, 60));
    CHECK(!MayOpen(cue, 61));
    // Inside the cooldown of the human's last opener nothing opens.
    cue.hadOpener = true;
    cue.sinceLastOpenerMs = 119999;
    CHECK(!MayOpen(cue, 1));
    cue.sinceLastOpenerMs = 120000;
    CHECK(MayOpen(cue, 1));
    cue.chancePct = 0;
    CHECK(!MayOpen(cue, 1));
    // Only rough and chatty bots ask for duels.
    CHECK(AiChatPolicy::AsksForDuels(AiChatPolicy::Archetype::ROUGH));
    CHECK(AiChatPolicy::AsksForDuels(AiChatPolicy::Archetype::CHATTY));
    CHECK(!AiChatPolicy::AsksForDuels(AiChatPolicy::Archetype::QUIET));
    CHECK(!AiChatPolicy::AsksForDuels(AiChatPolicy::Archetype::FRIENDLY));
}

TEST(ai_chat_friendly_whispers_are_capped_per_session)
{
    using AiChatPolicy::MayFriendWhisper;
    AiChatPolicy::FriendWhisperCue cue;
    cue.sessionCap = 2;
    cue.chancePct = 15;
    CHECK(MayFriendWhisper(cue, 15));
    CHECK(!MayFriendWhisper(cue, 16));
    cue.saidThisSession = 1;
    CHECK(MayFriendWhisper(cue, 1));
    cue.saidThisSession = 2;
    CHECK(!MayFriendWhisper(cue, 1));
    // Never more than two, whatever the config says.
    cue.sessionCap = 9;
    CHECK(!MayFriendWhisper(cue, 1));
    // Never in combat or in an instance.
    cue.saidThisSession = 0;
    cue.humanInCombat = true;
    CHECK(!MayFriendWhisper(cue, 1));
    cue.humanInCombat = false;
    cue.humanInInstance = true;
    CHECK(!MayFriendWhisper(cue, 1));
    cue.humanInInstance = false;
    cue.sessionCap = 0;
    CHECK(!MayFriendWhisper(cue, 1));
}

namespace
{
    AiChatPolicy::BgChatCue OpenBgCue()
    {
        AiChatPolicy::BgChatCue cue;
        cue.chatOn = cue.battlegroundOn = cue.humanOnTeam = true;
        return cue;
    }
}

TEST(ai_chat_battleground_result_reads_the_core_winner)
{
    using AiChatPolicy::BgResult;
    using AiChatPolicy::ResultFor;
    CHECK(ResultFor(AiChatPolicy::WINNER_ALLIANCE_ID, true) == BgResult::WIN);
    CHECK(ResultFor(AiChatPolicy::WINNER_ALLIANCE_ID, false) == BgResult::LOSS);
    CHECK(ResultFor(AiChatPolicy::WINNER_HORDE_ID, false) == BgResult::WIN);
    CHECK(ResultFor(AiChatPolicy::WINNER_HORDE_ID, true) == BgResult::LOSS);
    // A draw (the core writes 3) or no result says nothing.
    CHECK(ResultFor(2, true) == BgResult::NONE);
    CHECK(ResultFor(3, false) == BgResult::NONE);
}

TEST(ai_chat_battleground_lines_need_a_human_on_the_team)
{
    using AiChatPolicy::MayBgChat;
    AiChatPolicy::BgChatCue cue = OpenBgCue();
    CHECK(MayBgChat(cue, Kind::BG_WIN));
    CHECK(MayBgChat(cue, Kind::BG_LOSS));
    CHECK(MayBgChat(cue, Kind::BG_CAPTURE));
    CHECK(!MayBgChat(cue, Kind::PARTY_MOMENT));
    cue.humanOnTeam = false;
    CHECK(!MayBgChat(cue, Kind::BG_WIN));
    CHECK(!MayBgChat(cue, Kind::BG_CAPTURE));
    cue = OpenBgCue();
    cue.arena = true;
    CHECK(!MayBgChat(cue, Kind::BG_LOSS));
    cue = OpenBgCue();
    cue.chatOn = false;
    CHECK(!MayBgChat(cue, Kind::BG_WIN));
    cue = OpenBgCue();
    cue.battlegroundOn = false;
    CHECK(!MayBgChat(cue, Kind::BG_WIN));
    CHECK(!MayBgChat(cue, Kind::BG_CAPTURE));

}

TEST(ai_chat_battleground_has_one_or_two_speakers_per_team)
{
    using AiChatPolicy::BgSpeakerCount;
    CHECK(BgSpeakerCount(1, 1) == 1);
    CHECK(BgSpeakerCount(1, 100) == 1);
    CHECK(BgSpeakerCount(2, 1) == 2);
    CHECK(BgSpeakerCount(2, AiChatPolicy::SECOND_BG_SPEAKER_PCT) == 2);
    CHECK(BgSpeakerCount(2, AiChatPolicy::SECOND_BG_SPEAKER_PCT + 1) == 1);
    CHECK(BgSpeakerCount(0, 1) == 1);
    CHECK(BgSpeakerCount(-3, 1) == 1);
    CHECK(BgSpeakerCount(9, 1) == 2);
}

TEST(ai_chat_battleground_speakers_are_distinct_and_weighted)
{
    using AiChatPolicy::PickSpeakers;
    // Weights 40, 100, 120 (quiet, friendly, joker): roll 0 is the first, 40 the second, 140 the third.
    std::vector<uint32_t> const weights = { 40, 100, 120 };
    CHECK(PickSpeakers(weights, 1, { 0 }) == std::vector<size_t>{ 0 });
    CHECK(PickSpeakers(weights, 1, { 40 }) == std::vector<size_t>{ 1 });
    CHECK(PickSpeakers(weights, 1, { 140 }) == std::vector<size_t>{ 2 });
    CHECK(PickSpeakers(weights, 1, { 260 }) == std::vector<size_t>{ 0 });
    // The second pick never repeats the first: with the first gone, roll 0 lands on the next one.
    CHECK((PickSpeakers(weights, 2, { 40, 0 }) == std::vector<size_t>{ 1, 0 }));
    CHECK((PickSpeakers(weights, 2, { 0, 0 }) == std::vector<size_t>{ 0, 1 }));
    // Never more than there are, never a weight of 0, nothing from nothing.
    CHECK(PickSpeakers(weights, 5, { 7, 7, 7, 7, 7 }).size() == 3);
    CHECK((PickSpeakers({ 0, 50 }, 2, { 3, 3 }) == std::vector<size_t>{ 1 }));
    CHECK(PickSpeakers({}, 2, { 1 }).empty());
    CHECK(PickSpeakers(weights, 0, { 1 }).empty());
    CHECK(PickSpeakers(weights, 1, {}) == std::vector<size_t>{ 0 });
    // Over many rolls each candidate is picked about in proportion to its weight.
    uint32_t counts[3] = { 0, 0, 0 };
    for (uint32_t roll = 0; roll < 2600; ++roll)
        ++counts[PickSpeakers(weights, 1, { roll })[0]];
    CHECK(counts[0] == 400 && counts[1] == 1000 && counts[2] == 1200);
}

TEST(ai_chat_battleground_match_end_is_filed_once_per_team)
{
    AiChatPolicy::MatchEndBook book;
    CHECK(book.Claim(7, 0, 1000));
    CHECK(!book.Claim(7, 0, 1500));
    CHECK(book.Claim(7, 1, 1500));
    CHECK(book.Claim(8, 0, 1500));
    CHECK(!book.Claim(7, 1, 1000 + AiChatPolicy::MATCH_END_KEEP_MS - 1));
    // A reused instance id much later is a new match.
    CHECK(book.Claim(7, 0, 1000 + AiChatPolicy::MATCH_END_KEEP_MS));
    // The clock wraps.
    AiChatPolicy::MatchEndBook wrapped;
    CHECK(wrapped.Claim(3, 0, 0xFFFFFF00u));
    CHECK(!wrapped.Claim(3, 0, 0x100u));
    // Old claims are trimmed once the book fills up.
    AiChatPolicy::MatchEndBook full;
    for (uint32_t i = 0; i < AiChatPolicy::MATCH_END_TRIM_SIZE; ++i)
        CHECK(full.Claim(i, 0, 0));
    CHECK(full.Size() == AiChatPolicy::MATCH_END_TRIM_SIZE);
    CHECK(full.Claim(1000, 0, AiChatPolicy::MATCH_END_KEEP_MS));
    CHECK(full.Size() == 1);
}

TEST(ai_chat_battleground_speaker_waits_for_its_line_then_leaves)
{
    using AiChatPolicy::BgDeliveryBlock;
    CHECK(AiChatPolicy::HoldsLeave(true, 0));
    CHECK(AiChatPolicy::HoldsLeave(true, AiChatPolicy::BG_END_HOLD_MS - 1));
    CHECK(!AiChatPolicy::HoldsLeave(true, AiChatPolicy::BG_END_HOLD_MS));
    CHECK(!AiChatPolicy::HoldsLeave(false, 0));
    // The longest end line (deadline 5 s, the lost-answer grace 2 s, typing 5 s, openers' 3 s, a second speaker's 4 s)
    // fits the hold, and the hold fits the core's 120 s before it closes the match.
    CHECK(5000 + 2000 + AiChatPolicy::MAX_DELAY_MS + 3000 + 4000 < AiChatPolicy::BG_END_HOLD_MS);
    CHECK(BgDeliveryBlock(true, 12, 12, true) == nullptr);
    CHECK(std::string(BgDeliveryBlock(false, 12, 12, true)) == "left_battleground");
    CHECK(std::string(BgDeliveryBlock(true, 13, 12, true)) == "left_battleground");
    CHECK(std::string(BgDeliveryBlock(true, 0, 0, true)) == "left_battleground");
    CHECK(std::string(BgDeliveryBlock(true, 12, 12, false)) == "human_gone");
}

TEST(ai_chat_capture_observation_tracks_each_team_and_baselines_late_joins)
{
    AiChatPolicy::BgCaptureBook book;
    CHECK(!book.Observe(1, 0, 1, 10));
    CHECK(!book.Observe(1, 1, 0, 10));
    CHECK(book.Observe(1, 0, 2, 11));
    CHECK(!book.Observe(1, 0, 2, 12));
    CHECK(book.Observe(1, 1, 1, 13));
    CHECK(!book.Observe(2, 0, 2, 13));
}
