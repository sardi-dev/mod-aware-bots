/*
* This file is part of the Legends of Azeroth Pandaria Project. See THANKS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ReviveFromCorpseAction.h"

#include "DBCStores.h"
#include "Event.h"
#include "MapManager.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ServerFacade.h"
#include "TravelToGroupAction.h"
#include "Corpse.h"
#include "WorldPosition.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "Group.h"
#include "FleeManager.h"

#include <mutex>
#include <unordered_map>

bool ReviveFromCorpseAction::Execute(Event event)
{
    Player* master = botAI->GetGroupMaster();
    Corpse* corpse = bot->GetCorpse();

    // follow master when master revives
    WorldPacket& p = event.getPacket();
    if (!p.empty() && p.GetOpcode() == CMSG_RECLAIM_CORPSE && master && !corpse && bot->IsAlive())
    {
        if (sServerFacade->IsDistanceLessThan(AI_VALUE2(float, "distance", "master target"),
            sPlayerbotAIConfig->farDistance))
        {
            if (!botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT))
            {
                botAI->TellMasterNoFacing("Welcome back!");
                botAI->ChangeStrategy("+follow,-stay", BOT_STATE_NON_COMBAT);
                return true;
            }
        }
    }

    if (!corpse)
        return false;

    if (master)
    {
        if (!GET_PLAYERBOT_AI(master) && master->isDead() && master->GetCorpse() &&
            sServerFacade->IsDistanceLessThan(AI_VALUE2(float, "distance", "master target"),
                sPlayerbotAIConfig->farDistance))
            return false;
    }

    if (!botAI->HasRealPlayerMaster())
    {
        uint32 dCount = AI_VALUE(uint32, "death count");

        if (dCount >= 5)
        {
            return botAI->DoSpecificAction("spirit healer");
        }
    }

    SF_LOG_DEBUG("playerbots", "Bot %s %s:%u <%s> revives at body", BotGuid(bot->GetGUID()).ToString().c_str(),
        bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());

    bot->GetMotionMaster()->Clear();
    bot->StopMoving();

    WorldPacket packet(CMSG_RECLAIM_CORPSE);

    BotGuid guid = bot->GetGUID();
    packet.WriteBit(guid[1]);
    packet.WriteBit(guid[5]);
    packet.WriteBit(guid[7]);
    packet.WriteBit(guid[2]);
    packet.WriteBit(guid[6]);
    packet.WriteBit(guid[3]);
    packet.WriteBit(guid[0]);
    packet.WriteBit(guid[4]);

    packet.WriteByteSeq(guid[2]);
    packet.WriteByteSeq(guid[5]);
    packet.WriteByteSeq(guid[4]);
    packet.WriteByteSeq(guid[6]);
    packet.WriteByteSeq(guid[1]);
    packet.WriteByteSeq(guid[0]);
    packet.WriteByteSeq(guid[7]);
    packet.WriteByteSeq(guid[3]);

    bot->GetSession()->HandleReclaimCorpseOpcode(packet);

    return true;
}

namespace
{
    // The ghost's tries at the entrance trigger, per bot (map threads, hence the lock): a try is a second or more after the
    // last one, the count starts over after a quiet minute.
    struct EntranceTries
    {
        uint32 count = 0;
        uint32 lastMs = 0;
    };

    std::mutex entranceLock;
    std::unordered_map<uint32, EntranceTries> entranceTries;
    uint32 const MAX_ENTRANCE_TRIES = 3;
    uint32 const ENTRANCE_RETRY_MS = 5 * IN_MILLISECONDS;
    uint32 const ENTRANCE_FORGET_MS = 60 * IN_MILLISECONDS;
}

// BT-018, and the author's earlier private bot module: a corpse inside a dungeon or raid cannot be reached from outside
// (the distance across maps reads as zero, so the ghost stood still). Run to the instance's entrance and walk in like a
// player; entering the map that holds the corpse resurrects the ghost at the entrance. Same walk-in as a bot joining a
// group inside a dungeon (GroupTravel). False when this does not apply or gave up: the old logic follows.
bool FindCorpseAction::RunToDungeonEntrance(Corpse* corpse)
{
    if (corpse->GetMapId() == bot->GetMapId() || !bot->GetMap() || bot->GetMap()->Instanceable())
        return false;

    MapEntry const* corpseMap = sMapStore.LookupEntry(corpse->GetMapId());
    if (!corpseMap || !(corpseMap->IsDungeon() || corpseMap->IsRaid()))
        return false;

    GroupTravel::EntranceInfo entrance;
    if (!GroupTravel::FindEntranceFor(corpse->GetMapId(), bot, entrance) || entrance.pos.GetMapId() != bot->GetMapId())
        return false;

    if (bot->GetExactDist(entrance.pos.GetPositionX(), entrance.pos.GetPositionY(), entrance.pos.GetPositionZ()) >
        GroupTravel::EnterDistance(entrance.radius))
        return bot->isMoving() ||
            MoveTo(entrance.pos.GetMapId(), entrance.pos.GetPositionX(), entrance.pos.GetPositionY(),
                   entrance.pos.GetPositionZ(), false, false);

    uint32 const now = getMSTime();
    bool turnedAway = false;
    {
        std::lock_guard<std::mutex> guard(entranceLock);
        EntranceTries& tries = entranceTries[bot->GetGUIDLow()];
        if (tries.count && uint32(now - tries.lastMs) >= ENTRANCE_FORGET_MS)
            tries.count = 0;

        if (tries.count >= MAX_ENTRANCE_TRIES && uint32(now - tries.lastMs) >= ENTRANCE_RETRY_MS)
        {
            entranceTries.erase(bot->GetGUIDLow());
            turnedAway = true;
        }
        else if (tries.count >= MAX_ENTRANCE_TRIES)
            return true;
        else if (tries.count && uint32(now - tries.lastMs) < ENTRANCE_RETRY_MS)
            return true;
        else
        {
            ++tries.count;
            tries.lastMs = now;
        }
    }

    // The core turned the ghost away three times (a lockout, a raid it is not in): like a player turned away from the
    // instance that holds its corpse, it is revived at the entrance instead of standing there until the manager rescues it.
    if (turnedAway)
    {
        SF_LOG_DEBUG("playerbots", "Bot %s <%s> is turned away from map %u and revives at the entrance",
                     BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetName().c_str(), corpse->GetMapId());
        bot->ResurrectPlayer(0.5f);
        bot->SpawnCorpseBones();
        return true;
    }

    SF_LOG_DEBUG("playerbots", "Bot %s <%s> enters map %u on the way to its corpse", BotGuid(bot->GetGUID()).ToString().c_str(),
                 bot->GetName().c_str(), corpse->GetMapId());
    GroupTravel::SendEntranceTrigger(bot, entrance.triggerId);
    return true;
}

bool FindCorpseAction::Execute(Event event)
{
    if (bot->InBattleground())
        return false;

    Player* master = botAI->GetGroupMaster();
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
        return false;

    if (RunToDungeonEntrance(corpse))
        return true;

    // #BT-016: AC revives a bot through the manager (resurrection and a teleport) after five deaths. Here a bot
    // runs back or uses a spirit healer like a player (design decision 0016, 0017); the manager's revive timer only rescues a
    // ghost still dead after 10 to 15 minutes.
    uint32 dCount = AI_VALUE(uint32, "death count");

    WorldPosition botPos(bot);
    WorldPosition corpsePos(corpse);
    WorldPosition moveToPos = corpsePos;
    WorldPosition masterPos(master);

    float reclaimDist = CORPSE_RECLAIM_RADIUS - 5.0f;
    float corpseDist = botPos.distance(corpsePos);
    int64 deadTime = time(nullptr) - corpse->GetGhostTime();

    bool moveToMaster = master && master != bot && masterPos.fDist(corpsePos) < reclaimDist;

    // Should we ressurect? If so, return false.
    if (corpseDist < reclaimDist)
    {
        if (moveToMaster)  // We are near master.
        {
            if (botPos.fDist(masterPos) < sPlayerbotAIConfig->spellDistance)
                return false;
        }
        else if (deadTime > 8 * MINUTE)  // We have walked too long already.
            return false;
        else
        {
            GuidVector units = AI_VALUE(GuidVector, "possible targets no los");
            if (botPos.getUnitsAggro(units, bot) == 0)  // There are no mobs near.
                return false;
        }
    }

    // If we are getting close move to a save ressurrection spot instead of just the corpse.
    if (corpseDist < sPlayerbotAIConfig->reactDistance)
    {
        if (moveToMaster)
            moveToPos = masterPos;
        else
        {
            // #721: AC ReviveFromCorpseAction.cpp:143-155 - with mobs in aggro range of the corpse, pick a
            // safe spot inside the reclaim radius. This block was commented out at the module import
            // (dbcb9e52, before FleeManager existed; ported in #177). Without it a MoP ghost whose corpse
            // lies among the mobs that killed it walked onto the corpse, MoveTo had nowhere to go, it fell
            // through to "spirit healer" and shuttled corpse <-> graveyard until the 8-minute rule, or
            // revived at 50% health next to the same pack and died again (escalating the 30/60/120 s
            // reclaim delay, hence the revive retries).
            FleeManager manager(bot, reclaimDist, 0.0, urand(0, 1), moveToPos);

            if (manager.isUseful())
            {
                float rx, ry, rz;
                if (manager.CalculateDestination(&rx, &ry, &rz))
                    moveToPos = WorldPosition(moveToPos.getMapId(), rx, ry, rz, 0.0);
                else if (!moveToPos.GetReachableRandomPointOnGround(bot, reclaimDist, urand(0, 1)))
                    moveToPos = corpsePos;
            }
        }
    }

    // Actual mobing part.
    bool moved = false;

    // BT-019 (owner 2026-11-08): the shortcut is only for a ghost no real player can see; a seen one walks.
    if (!botAI->AllowActivity(ALL_ACTIVITY) && !botAI->HasPlayerNearby(bot->GetMap()->GetVisibilityRange()))
    {
        uint32 delay = sServerFacade->GetDistance2d(bot, corpse) /
            bot->GetSpeed(MOVE_RUN);        // Time a bot would take to travel to it's corpse.
        delay = std::min(delay, uint32(10 * MINUTE));  // Cap time to get to corpse at 10 minutes.

        if (deadTime > delay)
        {
            bot->GetMotionMaster()->Clear();
            bot->TeleportTo(moveToPos.getMapId(), moveToPos.getX(), moveToPos.getY(), moveToPos.getZ(), 0);
        }

        moved = true;
    }
    else
    {
        if (bot->isMoving())
            moved = true;
        else
        {
            if (deadTime < 10 * MINUTE && dCount < 5)  // Look for corpse up to 30 minutes.
            {
                moved = MoveTo(moveToPos.getMapId(), moveToPos.getX(), moveToPos.getY(), moveToPos.getZ(), false, false);
            }

            if (!moved)
            {
                moved = botAI->DoSpecificAction("spirit healer", Event(), true);
            }
        }
    }

    return moved;
}

bool FindCorpseAction::isUseful()
{
    if (bot->InBattleground())
        return false;

    return bot->GetCorpse();
}

static bool InGroupWithRealPlayer(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource())
            if (member != bot && !GET_PLAYERBOT_AI(member))
                return true;

    return false;
}

WorldSafeLocsEntry const* SpiritHealerAction::GetGrave(bool startZone)
{
    WorldSafeLocsEntry const* ClosestGrave = nullptr;
    WorldSafeLocsEntry const* NewGrave = nullptr;

    // AC finds BG graveyards in game_graveyard_zone; this core keeps the newer BGs' graveyards
    // (EotS, IoC, ...) in the BG class, so look up the way Player::RepopAtGraveyard does
    if (Battleground* bg = bot->GetBattleground())
        ClosestGrave = bg->GetClosestGraveYard(bot);
    else if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(bot->GetZoneId()))
        ClosestGrave = bf->GetClosestGraveYard(bot);
    else
        ClosestGrave = BotGetClosestGraveYard(BotGetWorldLocation(bot), bot->GetTeam());

    if (!startZone && ClosestGrave)
        return ClosestGrave;

    if (botAI->HasStrategy("follow", BOT_STATE_NON_COMBAT) && botAI->GetGroupMaster() && botAI->GetGroupMaster() != bot)
    {
        Player* master = botAI->GetGroupMaster();
        if (master && master != bot)
        {
            ClosestGrave = BotGetClosestGraveYard(BotGetWorldLocation(master), bot->GetTeam());

            if (ClosestGrave)
                return ClosestGrave;
        }
    }
    /*else if (startZone && AI_VALUE(uint8, "durability"))
    {
        TravelTarget* travelTarget = AI_VALUE(TravelTarget*, "travel target");

        if (travelTarget->getPosition())
        {
            WorldPosition travelPos = *travelTarget->getPosition();
            if (travelPos.GetMapId() != uint32(-1))
            {
                uint32 areaId = 0;
                uint32 zoneId = 0;
                sMapMgr->GetZoneAndAreaId(bot->GetPhaseMask(), zoneId, areaId, travelPos.getMapId(), travelPos.getX(),
                                          travelPos.getY(), travelPos.getZ());
                ClosestGrave = sGraveyard->GetClosestGraveyard(travelPos.getMapId(), travelPos.getX(), travelPos.getY(),
                                                               travelPos.getZ(), bot->GetTeamId(), areaId, zoneId,
                                                               bot->getClass() == CLASS_DEATH_KNIGHT);

                if (ClosestGrave)
                    return ClosestGrave;
            }
        }
    }*/

    // Owner decision 2026-09-27, a deliberate deviation from AC: a bot is never sent to a starting-zone
    // graveyard out of a battleground, arena, dungeon or raid, or while grouped with a real player (AC's loop
    // below teleports BG ghosts off the map once the per-tick death count passes 10).
    if (bot->GetMap()->Instanceable() || InGroupWithRealPlayer(bot))
    {
        if (!ClosestGrave)
            SF_LOG_ERROR("playerbots", "Graveyard nullptr");
        return ClosestGrave;
    }

    // AC parity (AC ReviveFromCorpseAction.cpp:250-291): with startZone (many deaths, a long ghost run or
    // broken gear) take the nearest starting-zone graveyard of the bot's faction. AC's race list, plus the
    // Cata/MoP races.
    std::vector<uint32> races;
    if (bot->GetTeamId() == TEAM_ALLIANCE)
        races = {RACE_HUMAN, RACE_DWARF, RACE_GNOME, RACE_NIGHTELF, RACE_DRAENEI, RACE_WORGEN, RACE_PANDAREN_ALLIANCE};
    else
        races = {RACE_ORC, RACE_TROLL, RACE_TAUREN, RACE_UNDEAD_PLAYER, RACE_BLOODELF, RACE_GOBLIN, RACE_PANDAREN_HORDE};

    float graveDistance = -1;
    WorldPosition botPos(bot);

    for (uint32 race : races)
    {
        for (uint32 cls = 0; cls < MAX_CLASSES; cls++)
        {
            PlayerInfo const* info = sObjectMgr->GetPlayerInfo(race, cls);
            if (!info)
                continue;

            NewGrave = BotGetClosestGraveYard(
                WorldLocation(info->mapId, info->positionX, info->positionY, info->positionZ), bot->GetTeam());
            if (!NewGrave)
                continue;

            WorldPosition gravePos(NewGrave->map_id, NewGrave->x, NewGrave->y, NewGrave->z);
            float newDist = botPos.fDist(gravePos);

            if (graveDistance < 0 || newDist < graveDistance)
            {
                ClosestGrave = NewGrave;
                graveDistance = newDist;
            }
        }
    }

    if (!ClosestGrave)
        SF_LOG_ERROR("playerbots", "Graveyard nullptr");
    return ClosestGrave;
}

bool SpiritHealerAction::Execute(Event event)
{
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        botAI->TellError("I am not a spirit");
        return false;
    }

    uint32 dCount = AI_VALUE(uint32, "death count");
    int64 deadTime = time(nullptr) - corpse->GetGhostTime();

    WorldSafeLocsEntry const* ClosestGrave = GetGrave(dCount > 10 || deadTime > 15 * MINUTE || AI_VALUE(uint8, "durability") < 10);

    if (!ClosestGrave)
        return false;

    if (bot->GetDistance2d(ClosestGrave->x, ClosestGrave->y) < sPlayerbotAIConfig->sightDistance)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
        for (GuidVector::iterator i = npcs.begin(); i != npcs.end(); i++)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (unit && unit->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_SPIRITHEALER))
            {
                // #BT-017: a player has to reach the healer, and pays with resurrection sickness and durability
                if (!bot->IsWithinDistInMap(unit, INTERACTION_DISTANCE))
                    return MoveTo(unit, INTERACTION_DISTANCE - 1.0f);

                SF_LOG_DEBUG("playerbots", "Bot %s %s:%u <%s> revives at spirit healer", BotGuid(bot->GetGUID()).ToString().c_str(), bot->GetTeamId() == TEAM_ALLIANCE ? "A" : "H", bot->getLevel(), bot->GetName().c_str());
                bot->GetSession()->SendSpiritResurrect();
                context->GetValue<Unit*>("current target")->Set(nullptr);
                bot->SetTarget(BotGuid::Empty);
                botAI->TellMaster("Hello");

                if (dCount > 20)
                    context->GetValue<uint32>("death count")->Set(0);

                return true;
            }
        }
    }

    if (!ClosestGrave)
    {
        return false;
    }

    bool moved = false;

    if (bot->IsWithinLOS(ClosestGrave->x, ClosestGrave->y, ClosestGrave->z))
        moved = MoveNear(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, 0.0);
    else
        moved = MoveTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, false, false);

    if (moved)
        return true;

    context->GetValue<uint32>("death count")->Set(dCount + 1);
    // BT-019 (owner 2026-11-08): a ghost a real player can see does not teleport; it tries again next time, and the
    // manager's rescue takes it after 10 to 15 minutes.
    if (botAI->HasPlayerNearby(bot->GetMap()->GetVisibilityRange()))
        return false;
    return bot->TeleportTo(ClosestGrave->map_id, ClosestGrave->x, ClosestGrave->y, ClosestGrave->z, 0.f);
}

bool SpiritHealerAction::isUseful()
{
    return bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_GHOST);
}
