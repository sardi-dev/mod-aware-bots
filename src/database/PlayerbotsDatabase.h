/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The playerbots database: its connection, prepared statements and worker pool. The statements follow Legends of
 * Azeroth 5.4.8's PlayerbotsDatabase; the connection uses SkyFire's MySQLConnection, so the module owns the pool
 * instead of the core.
 */

#ifndef MOD_AWARE_BOTS_PLAYERBOTSDATABASE_H
#define MOD_AWARE_BOTS_PLAYERBOTSDATABASE_H

#include "DatabaseWorkerPool.h"
#include "MySQLConnection.h"

enum PlayerbotsDatabaseStatements : uint32
{
    /*  Naming standard for defines:
        {DB}_{SEL/INS/UPD/DEL/REP}_{Summary of data changed}
        When updating more than one field, consider looking at the calling function
        name for a suiting suffix.
    */
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME,
    PLAYERBOTS_SEL_CUSTOM_STRATEGY_BY_OWNER_AND_NAME_AND_IDX,
    PLAYERBOTS_DEL_CUSTOM_STRATEGY,
    PLAYERBOTS_UPD_CUSTOM_STRATEGY,
    PLAYERBOTS_INS_CUSTOM_STRATEGY,
    PLAYERBOTS_SEL_DB_STORE,
    PLAYERBOTS_DEL_DB_STORE,
    PLAYERBOTS_INS_DB_STORE,
    PLAYERBOTS_SEL_ENCHANTS,
    PLAYERBOTS_SEL_EQUIP_CACHE,
    PLAYERBOTS_INS_EQUIP_CACHE,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_VALUE,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_AND_TYPE,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_DISTINCT,
    PLAYERBOTS_SEL_GUILD_TASKS_BY_OWNER_ORDERED,
    PLAYERBOTS_DEL_GUILD_TASKS,
    PLAYERBOTS_INS_GUILD_TASKS,
    PLAYERBOTS_SEL_RANDOM_BOTS_VALUE,
    PLAYERBOTS_SEL_RANDOM_BOTS_BOT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_EVENT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_OWNER_AND_BOT,
    PLAYERBOTS_SEL_RANDOM_BOTS_BY_EVENT_AND_VALUE,
    PLAYERBOTS_INS_RANDOM_BOTS,
    PLAYERBOTS_DEL_RANDOM_BOTS,
    PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER,
    PLAYERBOTS_DEL_RANDOM_BOTS_BY_OWNER_AND_EVENT,
    PLAYERBOTS_UPD_RANDOM_BOTS,
    PLAYERBOTS_SEL_RARITY_CACHE,
    PLAYERBOTS_INS_RARITY_CACHE,
    PLAYERBOTS_SEL_RNDITEM_CACHE,
    PLAYERBOTS_INS_RNDITEM_CACHE,
    PLAYERBOTS_SEL_SPEECH,
    PLAYERBOTS_SEL_SPEECH_PROBABILITY,
    PLAYERBOTS_SEL_TELE_CACHE,
    PLAYERBOTS_INS_TELE_CACHE,
    PLAYERBOTS_SEL_TEXT,
    PLAYERBOTS_SEL_DUNGEON_SUGGESTION,
    PLAYERBOTS_SEL_TRAVELNODE,
    PLAYERBOTS_INS_TRAVELNODE,
    PLAYERBOTS_DEL_TRAVELNODE,
    PLAYERBOTS_SEL_TRAVELNODE_LINK,
    PLAYERBOTS_INS_TRAVELNODE_LINK,
    PLAYERBOTS_DEL_TRAVELNODE_LINK,
    PLAYERBOTS_SEL_TRAVELNODE_PATH,
    PLAYERBOTS_INS_TRAVELNODE_PATH,
    PLAYERBOTS_DEL_TRAVELNODE_PATH,
    PLAYERBOTS_SEL_WEIGHTSCALES,
    PLAYERBOTS_SEL_WEIGHTSCALE_DATA,
    PLAYERBOTS_INS_EQUIP_CACHE_NEW,
    PLAYERBOTS_DEL_EQUIP_CACHE_NEW,
    PLAYERBOTS_SEL_BOT_ACCOUNTS,
    PLAYERBOTS_REP_BOT_ACCOUNT,
    PLAYERBOTS_DEL_BOT_ACCOUNT,
    MAX_PLAYERBOTS_STATEMENTS
};

class PlayerbotsDatabaseConnection : public MySQLConnection
{
public:
    typedef PlayerbotsDatabaseStatements Statements;

    PlayerbotsDatabaseConnection(MySQLConnectionInfo& connInfo) : MySQLConnection(connInfo) { }
    PlayerbotsDatabaseConnection(Skyfire::DatabaseQueue* q, MySQLConnectionInfo& connInfo) : MySQLConnection(q, connInfo) { }

    void DoPrepareStatements() OVERRIDE;
};

typedef DatabaseWorkerPool<PlayerbotsDatabaseConnection> PlayerbotsDatabaseWorkerPool;
typedef PreparedStatement PlayerbotsDatabasePreparedStatement;

extern PlayerbotsDatabaseWorkerPool PlayerbotsDatabase;

// Opens the pool from PlayerbotsDatabaseInfo in the server configuration; false if it fails.
bool OpenPlayerbotsDatabase();
void ClosePlayerbotsDatabase();

#endif
