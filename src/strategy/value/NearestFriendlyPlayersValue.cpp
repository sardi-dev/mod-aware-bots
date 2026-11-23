#include "NearestFriendlyPlayersValue.h"

#include "GridObject.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"

void NearestFriendlyPlayersValue::FindUnits(std::list<Unit*>& targets)
{
    Skyfire::AnyFriendlyUnitInObjectRangeCheck u_check(bot, bot, range);
    Skyfire::UnitListSearcher<Skyfire::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    bot->VisitNearbyObject(range, searcher);
}

bool NearestFriendlyPlayersValue::AcceptUnit(Unit* unit)
{
    BotGuid guid = unit->GetGUID();
    return (guid.GetTypeId() == TypeID::TYPEID_PLAYER) && guid != botAI->GetBot()->GetGUID();
}
