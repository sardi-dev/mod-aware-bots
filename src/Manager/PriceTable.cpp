/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The price table of the bots' auction house (PriceTable.h).
 */

#include "PriceTable.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"
#include "Timer.h"

#include <atomic>
#include <utility>
#include <vector>

namespace
{
    char const* const TABLE_NAME = "playerbots_item_price";

    // Built once on the world thread before the first bot update, never changed afterwards; the flag publishes it.
    std::vector<PricePolicy::PriceRow> sRows;
    std::atomic<bool> sLoaded{ false };
}

uint32 PriceTable::Load()
{
    if (!sPlayerbotAIConfig->auctionEnable || sLoaded.load())
        return 0;

    uint32 const oldMSTime = getMSTime();

    // A query on a table that does not exist makes the core abort, so look first: the file in data/sql/db-world is
    // applied by the core's updater, which an older core or a hand-managed database may not run.
    QueryResult exists = WorldDatabase.PQuery("SHOW TABLES LIKE '%s'", TABLE_NAME);
    if (!exists)
    {
        SF_LOG_ERROR("playerbots", "[AUCTION] table %s is missing in the world database: the auction house trades nothing "
                     "(apply data/sql/db-world/*_playerbots_item_price.sql)", TABLE_NAME);
        return 0;
    }

    QueryResult result = WorldDatabase.PQuery("SELECT item, price_class, min_price, max_price FROM %s", TABLE_NAME);
    if (!result)
    {
        SF_LOG_ERROR("playerbots", "[AUCTION] table %s is empty: the auction house trades nothing", TABLE_NAME);
        return 0;
    }

    std::vector<PricePolicy::PriceRow> rows;
    rows.reserve(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();
        PricePolicy::PriceRow row;
        row.item = fields[0].GetUInt32();
        row.range.priceClass = PricePolicy::PriceClass(fields[1].GetUInt8());
        row.range.minCopper = fields[2].GetUInt32();
        row.range.maxCopper = fields[3].GetUInt32();
        rows.push_back(row);
    } while (result->NextRow());

    uint32 const read = uint32(rows.size());
    uint32 const dropped = PricePolicy::Normalize(rows);
    if (rows.empty())
    {
        SF_LOG_ERROR("playerbots", "[AUCTION] table %s has no usable row (%u read): the auction house trades nothing", TABLE_NAME, read);
        return 0;
    }

    sRows = std::move(rows);
    sLoaded.store(true);
    SF_LOG_INFO("playerbots", ">> Loaded %u auction price ranges (%u rows dropped) in %u ms", uint32(sRows.size()), dropped,
                GetMSTimeDiffToNow(oldMSTime));
    return uint32(sRows.size());
}

bool PriceTable::Find(uint32 itemEntry, PricePolicy::PriceRange& out)
{
    return sLoaded.load() && PricePolicy::FindRange(sRows, itemEntry, out);
}

uint32 PriceTable::Size()
{
    return sLoaded.load() ? uint32(sRows.size()) : 0;
}
