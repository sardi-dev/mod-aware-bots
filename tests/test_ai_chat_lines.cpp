/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "AiChatLines.h"
#include "TestHarness.h"

using AiChatLines::Canned;
using AiChatLines::ClassifyTrivial;
using AiChatLines::Tone;
using AiChatLines::ToneOf;
using AiChatLines::Trivial;
using AiChatPolicy::Archetype;

TEST(ai_chat_trivial_lines_are_found_whatever_the_case_and_punctuation)
{
    CHECK(ClassifyTrivial("hi") == Trivial::GREETING);
    CHECK(ClassifyTrivial("Hey!!") == Trivial::GREETING);
    CHECK(ClassifyTrivial("  HELLO there :) ") == Trivial::GREETING);
    CHECK(ClassifyTrivial("yo") == Trivial::GREETING);
    CHECK(ClassifyTrivial("sup?") == Trivial::GREETING);
    CHECK(ClassifyTrivial("heyyy") == Trivial::GREETING);
    CHECK(ClassifyTrivial("o/") == Trivial::GREETING);
    CHECK(ClassifyTrivial("hey Taran!", "Taran") == Trivial::GREETING);
    CHECK(ClassifyTrivial("ty") == Trivial::THANKS);
    CHECK(ClassifyTrivial("Thx!") == Trivial::THANKS);
    CHECK(ClassifyTrivial("thanks a lot man") == Trivial::THANKS);
    CHECK(ClassifyTrivial("thank you so much") == Trivial::THANKS);
    CHECK(ClassifyTrivial("thxxx") == Trivial::THANKS);
    CHECK(ClassifyTrivial("GG") == Trivial::GG);
    CHECK(ClassifyTrivial("gggg") == Trivial::GG);
    CHECK(ClassifyTrivial("gg wp") == Trivial::GG);
    CHECK(ClassifyTrivial("lol") == Trivial::LAUGH);
    CHECK(ClassifyTrivial("hahaha") == Trivial::LAUGH);
    CHECK(ClassifyTrivial("hahah") == Trivial::LAUGH);
    CHECK(ClassifyTrivial("LMAO") == Trivial::LAUGH);
    CHECK(ClassifyTrivial("np") == Trivial::NP);
    CHECK(ClassifyTrivial("you're welcome") == Trivial::NP);
    CHECK(ClassifyTrivial("k") == Trivial::OK);
    CHECK(ClassifyTrivial("okk") == Trivial::OK);
    CHECK(ClassifyTrivial("cya") == Trivial::BYE);
    CHECK(ClassifyTrivial("see you") == Trivial::BYE);
}

TEST(ai_chat_real_sentences_are_not_trivial)
{
    CHECK(ClassifyTrivial("hi, where is the flight master?") == Trivial::NONE);
    CHECK(ClassifyTrivial("thanks for the heal back there") == Trivial::NONE);
    CHECK(ClassifyTrivial("are you a bot") == Trivial::NONE);
    CHECK(ClassifyTrivial("") == Trivial::NONE);
    CHECK(ClassifyTrivial("Taran", "Taran") == Trivial::NONE);
    CHECK(ClassifyTrivial("hiking is fun") == Trivial::NONE);
    CHECK(ClassifyTrivial("okay so what now") == Trivial::NONE);
}

TEST(ai_chat_tone_finds_obvious_thanks_and_insults)
{
    CHECK(ToneOf("ty") == Tone::THANKED);
    CHECK(ToneOf("thanks for the heal back there") == Tone::THANKED);
    CHECK(ToneOf("Thank you!") == Tone::THANKED);
    CHECK(ToneOf("you noob") == Tone::INSULTED);
    CHECK(ToneOf("STFU") == Tone::INSULTED);
    CHECK(ToneOf("you suck lol") == Tone::INSULTED);
    // An insult wins over thanks; a first-person line is no insult.
    CHECK(ToneOf("thanks for nothing, idiot") == Tone::INSULTED);
    CHECK(ToneOf("sorry, I'm a noob") == Tone::NONE);
    CHECK(ToneOf("i am so dumb") == Tone::NONE);
    CHECK(ToneOf("nice weather") == Tone::NONE);
    // Whole words only.
    CHECK(ToneOf("future plans") == Tone::NONE);
    CHECK(ToneOf("typical") == Tone::NONE);
}

TEST(ai_chat_template_answers_fit_the_personality)
{
    using AiChatLines::TemplateReply;
    // Quiet and rough bots answer short, in lower case.
    CHECK(TemplateReply(Trivial::THANKS, Archetype::QUIET, 0, 90, true) == "np");
    CHECK(TemplateReply(Trivial::THANKS, Archetype::ROUGH, 1, 90, true) == "whatever");
    CHECK(TemplateReply(Trivial::GG, Archetype::ROUGH, 2, 90, true) == "lol ok");
    // Friendly and chatty bots answer with fuller lines.
    CHECK(TemplateReply(Trivial::GREETING, Archetype::FRIENDLY, 0, 90, true) == "Hey there!");
    CHECK(TemplateReply(Trivial::GREETING, Archetype::CHATTY, 0, 90, true).size() > 10);
    // The pick wraps round the pool.
    CHECK(TemplateReply(Trivial::OK, Archetype::QUIET, 3, 90, true) == "k");
    // Natural variation for the polished ones: no last "!" or a lower-case start.
    CHECK(TemplateReply(Trivial::GREETING, Archetype::FRIENDLY, 0, 55, true) == "Hey there");
    CHECK(TemplateReply(Trivial::GREETING, Archetype::FRIENDLY, 0, 70, true) == "hey there!");
    // Quiet lines stay as written.
    CHECK(TemplateReply(Trivial::GREETING, Archetype::QUIET, 0, 70, true) == "hi");
    // "ok" and "np" in say or party chat are left unanswered half of the time; a whisper always gets its answer.
    CHECK(TemplateReply(Trivial::OK, Archetype::FRIENDLY, 0, 10, false).empty());
    CHECK(!TemplateReply(Trivial::OK, Archetype::FRIENDLY, 0, 10, true).empty());
    CHECK(!TemplateReply(Trivial::GREETING, Archetype::FRIENDLY, 0, 10, false).empty());
    CHECK(TemplateReply(Trivial::NONE, Archetype::FRIENDLY, 0, 90, true).empty());
    // Every pool has a line for every archetype.
    for (uint8_t kind = 1; kind <= uint8_t(Trivial::BYE); ++kind)
        for (uint8_t archetype = 0; archetype <= uint8_t(Archetype::CHATTY); ++archetype)
            CHECK(!TemplateReply(Trivial(kind), Archetype(archetype), 0, 90, true).empty());
}

TEST(ai_chat_canned_lines_cover_every_moment_but_a_friendly_whisper)
{
    using AiChatLines::CannedLine;
    CHECK(CannedLine(Canned::DUEL_WIN, Archetype::QUIET, 0) == "gg");
    CHECK(CannedLine(Canned::WHISPER, Archetype::QUIET, 0) == "busy rn");
    CHECK(CannedLine(Canned::DUEL_DECLINE, Archetype::ROUGH, 0) == "nah");
    CHECK(CannedLine(Canned::GRATS, Archetype::QUIET, 0) == "gz");
    CHECK(CannedLine(Canned::NONE, Archetype::CHATTY, 0).empty());
    CHECK(CannedLine(Canned::BG_WIN, Archetype::QUIET, 0) == "gg");
    CHECK(CannedLine(Canned::BG_WIN, Archetype::ROUGH, 0) == "ez");
    CHECK(CannedLine(Canned::BG_LOSS, Archetype::FRIENDLY, 0) == "gg all, close one");
    CHECK(CannedLine(Canned::BG_CAPTURE, Archetype::CHATTY, 1) == "Yesss great cap!");
    for (uint8_t moment = 1; moment <= uint8_t(Canned::BG_CAPTURE); ++moment)
        for (uint8_t archetype = 0; archetype <= uint8_t(Archetype::CHATTY); ++archetype)
            for (uint32_t pick = 0; pick < 4; ++pick)
            {
                std::string const line = CannedLine(Canned(moment), Archetype(archetype), pick);
                CHECK(!line.empty() && line.size() <= 40);
            }
}

TEST(ai_chat_failures_say_the_rule_line_then_the_canned_line)
{
    using AiChatPolicy::ChooseLine;
    using AiChatPolicy::Source;
    AiChatPolicy::Line line = ChooseLine("error", "", "", "busy rn");
    CHECK(line.text == "busy rn" && line.source == Source::CANNED);
    line = ChooseLine("rules_hint", "", "Ouch. Let's try again", "lol");
    CHECK(line.text == "Ouch. Let's try again" && line.source == Source::RULES);
    line = ChooseLine("llm", "", "", "gg");
    CHECK(line.text == "gg" && line.source == Source::CANNED);
    // A friendly whisper the sidecar refuses has neither: silence.
    line = ChooseLine("rules", "", "", "");
    CHECK(line.text.empty() && line.source == Source::RULES);
    // A template answer is said as it is; an empty one is silence, never canned.
    line = ChooseLine(AiChatPolicy::TEMPLATE_SOURCE, "np", "", "busy rn");
    CHECK(line.text == "np" && line.source == Source::TEMPLATE);
    line = ChooseLine(AiChatPolicy::TEMPLATE_SOURCE, "", "", "busy rn");
    CHECK(line.text.empty() && line.source == Source::TEMPLATE);
    CHECK(std::string(AiChatPolicy::SourceName(Source::CANNED)) == "canned");
}
