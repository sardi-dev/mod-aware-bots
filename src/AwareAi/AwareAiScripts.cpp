/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AwareAiClient.h"
#include "AwareAiConfig.h"
#include "AwareAiRuntime.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // Created by data/sql/db-auth/2026_11_12_01_aware_ai_rbac.sql: 1347 lets game masters read the layer's status, 1348 lets administrators
    // change it. They sit next to the module's other ids (1345, 1346 in data/sql/db-auth/2026_11_12_00_playerbots_rbac.sql).
    uint32 const RBAC_PERM_COMMAND_BOT_AI_STATUS = 1347;
    uint32 const RBAC_PERM_COMMAND_BOT_AI_CONTROL = 1348;

    // The sidecar's schema accepts caps up to this; its own environment ceiling clamps them further.
    constexpr double MAX_BUDGET_USD = 10000.0;
    constexpr int64 MS_PER_SECOND = 1000;
    // The status line shows spend to a hundredth of a cent.
    constexpr int SPEND_DECIMALS = 4;

    char const* OnOff(bool value)
    {
        return value ? "on" : "off";
    }

    std::string WhoRan(ChatHandler* handler)
    {
        return handler->GetSession() ? handler->GetSession()->GetPlayerName() : std::string("console");
    }

    bool ParseUsd(std::string const& text, double& value)
    {
        char* end = nullptr;
        errno = 0;
        double const parsed = std::strtod(text.c_str(), &end);
        if (errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed) || parsed < 0.0 ||
            parsed > MAX_BUDGET_USD)
            return false;

        value = parsed;
        return true;
    }

    std::vector<std::string> Words(char const* args)
    {
        std::istringstream stream(args ? args : "");
        std::vector<std::string> words;
        for (std::string word; stream >> word;)
            words.push_back(word);

        return words;
    }
}

// Reads the AwareAi.* keys once the world is up (playerbots.conf is merged into the configuration by the bots
// module's own OnConfigLoad, which runs earlier), then drives the sidecar link from the world tick.
class AwareAiWorldScript : public WorldScript
{
public:
    AwareAiWorldScript() : WorldScript("AwareAiWorldScript") { }

    void OnStartup() override
    {
        sAwareAiConfig.Load();
        sAwareAiRuntime.Start();
    }

    void OnUpdate(uint32 diff) override
    {
        sAwareAiRuntime.Update(diff);
    }

    void OnShutdown() override
    {
        sAwareAiRuntime.Stop();
    }
};

class aware_ai_commandscript : public CommandScript
{
public:
    aware_ai_commandscript() : CommandScript("aware_ai_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> aiCommandTable =
        {
            { "status", RBAC_PERM_COMMAND_BOT_AI_STATUS, true, &HandleStatusCommand, "" },
            { "on", RBAC_PERM_COMMAND_BOT_AI_CONTROL, true, &HandleOnCommand, "" },
            { "off", RBAC_PERM_COMMAND_BOT_AI_CONTROL, true, &HandleOffCommand, "" },
            { "budget", RBAC_PERM_COMMAND_BOT_AI_CONTROL, true, &HandleBudgetCommand, "" },
        };
        static std::vector<ChatCommand> botCommandTable =
        {
            { "ai", RBAC_PERM_COMMAND_BOT_AI_STATUS, true, nullptr, "", aiCommandTable },
        };
        static std::vector<ChatCommand> commandTable =
        {
            { "bot", RBAC_PERM_COMMAND_BOT_AI_STATUS, true, nullptr, "", botCommandTable },
        };
        return commandTable;
    }

    // .bot ai status
    static bool HandleStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        AwareAiConfig const& config = sAwareAiConfig;

        handler->PSendSysMessage("AI layer: %s (AwareAi.Enable; the commands last until restart)",
            OnOff(config.Enabled));
        handler->PSendSysMessage("Features: pvp %s; decision deadline %u ms; battleground leader asks every %u s, "
            "confidence %u%%, at most %u asks per side and match",
            OnOff(config.PvPEnabled), config.DecisionDeadlineMs.load(), config.PvpBgIntervalSeconds.load(),
            config.PvpMinConfidencePct.load(), config.PvpMaxRequestsPerMatchSide.load());
        handler->PSendSysMessage("Chat %s (AwareAi.Chat.Enable): deadline %u ms, nearby %u%%, party %u%%, guild window "
            "%u s for %u bot(s)", OnOff(config.ChatEnabled), config.ChatDeadlineMs.load(),
            config.ChatNearbyChancePct.load(), config.ChatPartyChancePct.load(), config.ChatGuildWindowSeconds.load(),
            config.ChatGuildBots.load());
        handler->PSendSysMessage("Chat timing: %u%% late, in combat %u%% wait and %u%% skip; openers %s (one per %u s, "
            "greet %u%%, level %u%%, duel ask %u%% every %u s); friendly whispers %s (%u a session, %u%% every %u s)",
            config.ChatLateChancePct.load(), config.ChatCombatWaitPct.load(), config.ChatCombatSkipPct.load(),
            OnOff(config.ChatOpenersEnabled), config.ChatOpenerCooldownSeconds.load(),
            config.ChatOpenerGreetChancePct.load(), config.ChatOpenerLevelChancePct.load(),
            config.ChatOpenerDuelAskChancePct.load(), config.ChatOpenerDuelAskCheckSeconds.load(),
            OnOff(config.ChatFriendWhisperEnabled), config.ChatFriendWhisperMaxPerSession.load(),
            config.ChatFriendWhisperChancePct.load(), config.ChatFriendWhisperCheckSeconds.load());
        handler->PSendSysMessage("Chat in battlegrounds %s: up to %u bot(s) per team at the end of a match with a human "
            "on the team", OnOff(config.ChatBattlegroundEnabled), config.ChatBattlegroundSpeakers.load());
        if (config.BudgetDailyUsd > 0.0)
            handler->PSendSysMessage("Requested budget: $%.2f a month, $%.2f a day", config.BudgetMonthlyUsd,
                config.BudgetDailyUsd);
        else
            handler->PSendSysMessage("Requested budget: $%.2f a month for decisions and chat, no daily cap",
                config.BudgetMonthlyUsd);

        if (!sAwareAiClient.IsRunning())
        {
            handler->PSendSysMessage("Sidecar: not contacted, the client is not started (%s). Turn the layer on "
                "with .bot ai on.", config.SidecarUrl.c_str());
            return true;
        }

        // The answer arrives on the IO thread after this command returns: run the command again for the new numbers.
        AwareAiStatus const status = sAwareAiRuntime.Status();
        int64 const ageMs = sAwareAiRuntime.StatusAgeMs();
        sAwareAiRuntime.RefreshStatus();

        if (ageMs < 0)
            handler->SendSysMessage("Sidecar: asked just now, no answer yet. Run the command again.");
        else if (!status.Reachable)
            handler->PSendSysMessage("Sidecar: unavailable at %s (checked %lld s ago)%s", config.SidecarUrl.c_str(),
                static_cast<long long>(ageMs / MS_PER_SECOND),
                sAwareAiClient.IsBreakerOpen() ? "; client paused after repeated failures" : "");
        else
        {
            handler->PSendSysMessage("Sidecar: reachable (v%s, checked %lld s ago), mode %s, has_api_key %s, "
                "provider breaker %s, %u in flight", status.Version.c_str(),
                static_cast<long long>(ageMs / MS_PER_SECOND), status.Mode.c_str(),
                status.HasApiKey ? "true" : "false", status.Breaker.c_str(), status.InFlight);
            handler->PSendSysMessage("Sidecar settings: layer %s. Spend: $%.4f of $%.2f this month, %s%s%s",
                OnOff(status.Enabled), status.MonthSpentUsd, status.MonthCapUsd,
                DaySpendText(status.DaySpentUsd, status.DayCapUsd, SPEND_DECIMALS).c_str(),
                status.Exhausted ? " - cap reached, bots use their rules" : (status.Warning ? " - near the cap" : ""),
                status.CapClamped ? " (the request was above the sidecar's ceiling and was clamped)" : "");
            handler->PSendSysMessage("Hard monthly ceiling set in the sidecar's environment: $%.2f",
                status.CeilingUsd);
        }

        handler->PSendSysMessage("Requests in flight from worldserver: %u", sAwareAiClient.InFlight());
        return true;
    }

    // .bot ai on
    static bool HandleOnCommand(ChatHandler* handler, char const* /*args*/)
    {
        return SetEnabled(handler, true);
    }

    // .bot ai off
    static bool HandleOffCommand(ChatHandler* handler, char const* /*args*/)
    {
        return SetEnabled(handler, false);
    }

    // .bot ai budget <monthly dollars> [daily dollars]
    static bool HandleBudgetCommand(ChatHandler* handler, char const* args)
    {
        std::vector<std::string> const words = Words(args);
        double monthly = 0.0;
        double daily = 0.0;
        if (words.empty() || words.size() > 2 || !ParseUsd(words[0], monthly) ||
            (words.size() == 2 && !ParseUsd(words[1], daily)))
        {
            handler->PSendSysMessage("Usage: .bot ai budget <monthly dollars> [daily dollars], each between 0 and "
                "%.0f. Without a daily cap there is none (one monthly pool).", MAX_BUDGET_USD);
            return false;
        }

        sAwareAiConfig.BudgetMonthlyUsd = monthly;
        sAwareAiConfig.BudgetDailyUsd = words.size() == 2 ? daily : 0.0;
        sAwareAiRuntime.LogEvent("command", "budget " + (args ? std::string(args) : std::string()) + " by " +
            WhoRan(handler));
        if (sAwareAiClient.IsRunning())
            sAwareAiRuntime.SyncNow();

        std::string const dailyText = words.size() == 2 && daily > 0.0 ? "$" + words[1] + " a day" : "no daily cap";
        handler->PSendSysMessage("Budget requested: $%.2f a month, %s. The sidecar never goes above its own ceiling "
            "(see .bot ai status). Lasts until restart.", monthly, dailyText.c_str());
        return true;
    }

private:
    static bool SetEnabled(ChatHandler* handler, bool enabled)
    {
        sAwareAiConfig.Enabled = enabled;
        sAwareAiRuntime.LogEvent("command", std::string(enabled ? "on" : "off") + " by " + WhoRan(handler));
        // Turning off only needs the sidecar told when the link exists; turning on starts it.
        if (enabled || sAwareAiClient.IsRunning())
            sAwareAiRuntime.SyncNow();

        if (enabled && !sAwareAiClient.IsRunning())
        {
            handler->SendSysMessage("AI layer on, but the sidecar client could not start (check AwareAi.SidecarUrl "
                "and the Server log).");
            return false;
        }

        handler->PSendSysMessage("AI layer %s. Lasts until restart; set AwareAi.Enable in playerbots.conf to make it "
            "permanent. Run .bot ai status in a moment.", OnOff(enabled));
        return true;
    }
};

void AddSC_aware_ai()
{
    new AwareAiWorldScript();
    new aware_ai_commandscript();
}
