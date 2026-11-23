/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Bot accounts: a player turns an account they own into a bot account for one owner account (".playerbots account
 * botmode on <owner>"), whose characters that owner can then use as bots, and back into a human-only account
 * ("botmode off"). Being logged in to the account is the consent. Stored in playerbots_bot_accounts.
 */

#ifndef MOD_AWARE_BOTS_BOTACCOUNTS_H
#define MOD_AWARE_BOTS_BOTACCOUNTS_H

#include "Define.h"

#include <string>
#include <vector>

class ChatHandler;

namespace BotAccounts
{
    // The owner of a bot account, 0 when the account is not a bot account.
    uint32 GetOwner(uint32 botAccountId);

    // True when botAccountId is a bot account owned by ownerAccountId.
    bool IsOwnedBy(uint32 botAccountId, uint32 ownerAccountId);

    // Bot accounts this owner controls.
    std::vector<uint32> OwnedBy(uint32 ownerAccountId);

    // Logs out every character of this account that is online as a bot (for its master's PlayerbotMgr).
    void LogoutBotsOfAccount(uint32 accountId);

    // ".playerbots account botmode [on <owner account> | off [<bot account>]]".
    bool HandleBotModeCommand(ChatHandler* handler, std::string const& args);
}

#endif
