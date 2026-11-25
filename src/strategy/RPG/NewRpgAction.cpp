/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/World/Rpg/Action/NewRpgAction.cpp)
 * into the Legends of Azeroth Pandaria fork.
 */

#include "NewRpgAction.h"

#include "BotScenario.h"
#include "BotCompat_S.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"  // #201 (#612)
#include "LFGMgr.h"  // #201 (#612): sLFGMgr
#include "WaypointMovementGenerator.h"
#include "IVMapManager.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "NewRpgBaseAction.h"
#include "NewRpgDuelSpot.h"  // #201 (#612): sDuelSpotMgr
#include "BroadcastHelper.h"
#include "NewRpgStrategy.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "OutdoorPvP.h"
#include "OutdoorPvPMgr.h"
#include "PathGenerator.h"  // #399
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"  // #536
#include "Playerbots.h"
#include "Timer.h"
#include "WorldSession.h"
#include "NewRpgRoamTransport.h"  // #890 (beyond AC)
#include "BotRepair.h"  // #BT-015
#include "BankAction.h"
#include "BotSale.h"  // BT-149
#include "BotAuction.h"  // design decision 0033
#include "AuctionVisitPolicy.h"
#include "BotBreak.h"
#include "BotPvpTalents.h"  // BT-323
#include "BotTraining.h"
#include "CellImpl.h"
#include "ErrandPolicy.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "RoamPolicy.h"
#include "UseItemAction.h"

bool IsEventQuest(Quest const* quest);

namespace
{
// A named soak bot (AiPlayerbot.Auction.OnlyBots) in a town with an auction house in reach whose visit is allowed now; defined
// with the errand code below.
bool SoakAuctionVisitReady(PlayerbotAI* botAI, Player* bot);
}  // namespace

// #134 (flying bots): grind cells keep their z rounded to 50 yd (TravelMgr grind cells). A flying bot flies
// straight to that z and can end up under the terrain, where every later path fails and it freezes. So a
// flyer aims at the ground below the cell once it is within path range, and its arrival is measured in 2D.
// Latent upstream bug (AC has it too); fixed identically on both cores so 1:1 runs stay comparable.
static WorldPosition GrindMoveTarget(Player* bot, WorldPosition const& cell, float pathDis)
{
    if (!bot->IsFlying() || bot->GetExactDist2d(cell.GetPositionX(), cell.GetPositionY()) > pathDis)
        return cell;
    float const ground = BotGetMapHeight(bot, cell.GetPositionX(), cell.GetPositionY(), cell.GetPositionZ() + 25.0f, true, 50.0f);
    if (ground <= INVALID_HEIGHT)
        return cell;
    return WorldPosition(cell.GetMapId(), cell.GetPositionX(), cell.GetPositionY(), ground, cell.GetOrientation());
}

static float GrindArrivalDist(Player* bot, WorldPosition const& cell)
{
    return bot->IsFlying() ? bot->GetExactDist2d(cell.GetPositionX(), cell.GetPositionY()) : bot->GetExactDist(&cell);
}

// MoP keeps quest progress per QuestObjective, but quest_poi.ObjectiveIndex keeps the 3.3.5 layout that AC's
// GetQuestPOIPosAndObjectiveIdx uses: creature/GO objectives are slots 0-3 and item objectives are
// QUEST_OBJECTIVES_COUNT (4) + item slot. StorageIndex counts within each type (a kill and an item objective can
// both be 0), so item objectives map to 4 + StorageIndex. Without this no item-objective POI ever matched and bots
// dropped every collect-item quest right after accepting it.
static int32 LegacyPoiIndex(QuestObjective const& objective)
{
    return objective.Type == QUEST_OBJECTIVE_ITEM ? 4 + int32(objective.Index) : int32(objective.Index);
}

// #536: AC Ai/World/Rpg/Action/NewRpgAction.cpp:58-260 ("rpg status" / "rpg do quest" chat commands), 1:1.
// 5.4.8 diffs: botAI->rpgInfo -> GetRpgInfo(); sPlayerbotAIConfig. -> ->; bot->Whisper takes (text, lang, receiver).
void TellRpgStatusAction::WhisperStatusChange(Player* owner, std::string const& statusName)
{
    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        RPG_STATUS_CHANGED_KEY, RPG_STATUS_CHANGED_DEFAULT,
        {{"%status", statusName}});
    bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
}

bool TellRpgStatusAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    if (text.empty())
    {
        std::string out = botAI->GetRpgInfo().ToString();
        bot->Whisper(out, Language(LANG_UNIVERSAL), owner->GetGUID());
        return true;
    }

    Player* master = botAI->GetMaster();
    bool isMaster = master && master->GetGUID() == owner->GetGUID();
    bool isGM = owner->GetSession() && owner->GetSession()->GetSecurity() >= AccountTypes::SEC_GAMEMASTER;
    if (!isMaster && !isGM)
    {
        std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "rpg_debug_permission_error",
            "Only your master or a GM can change my rpg status.", {});
        bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
        return false;
    }

    std::string name = text;
    uint32 questId = 0;
    static std::string const doQuestPrefix = "do quest ";
    size_t doQuestPos = text.find(doQuestPrefix);
    if (doQuestPos != std::string::npos)
    {
        name = "do quest";
        std::string idStr = text.substr(doQuestPos + doQuestPrefix.length());
        try
        {
            questId = static_cast<uint32>(std::stoul(idStr));
        }
        catch (std::exception const&)
        {
            questId = 0;
        }
    }

    NewRpgStatus status = NewRpgInfo::StatusFromString(name);
    NewRpgInfo& info = botAI->GetRpgInfo();

    if (status == RPG_IDLE)
    {
        info.ChangeToIdle();
        WhisperStatusChange(owner, "IDLE");
        return true;
    }
    else if (status == RPG_REST)
    {
        info.ChangeToRest();
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        WhisperStatusChange(owner, "REST");
        return true;
    }
    else if (status == RPG_WANDER_RANDOM)
    {
        info.ChangeToWanderRandom();
        WhisperStatusChange(owner, "WANDER_RANDOM");
        return true;
    }
    else if (status == RPG_WANDER_NPC)
    {
        info.ChangeToWanderNpc();
        WhisperStatusChange(owner, "WANDER_NPC");
        return true;
    }
    else if (status == RPG_GO_GRIND)
    {
        WorldPosition pos = SelectRandomGrindPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_grind_pos_error", "No grind position available.", {});
            bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToGoGrind(pos);
        WhisperStatusChange(owner, "GO_GRIND");
        return true;
    }
    else if (status == RPG_GO_CAMP)
    {
        WorldPosition pos = SelectRandomCampPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_camp_pos_error", "No camp position available.", {});
            bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToGoCamp(pos);
        WhisperStatusChange(owner, "GO_CAMP");
        return true;
    }
    else if (status == RPG_TRAVEL_FLIGHT)
    {
        uint32 flightMasterEntry = 0;
        WorldPosition flightMasterPos;
        std::vector<uint32> path;
        if (!SelectRandomFlightTaxiNode(flightMasterEntry, flightMasterPos, path))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_flight_path_error", "No flight path available.", {});
            bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToTravelFlight(flightMasterEntry, flightMasterPos, std::move(path));
        WhisperStatusChange(owner, "TRAVEL_FLIGHT");
        return true;
    }
    else if (status == RPG_OUTDOOR_PVP)
    {
        info.ChangeToOutdoorPvp();
        WhisperStatusChange(owner, "OUTDOOR_PVP");
        return true;
    }
    else if (status == RPG_DO_GATHER)
    {
        if (!botAI->HasSkill(SKILL_HERBALISM) && !botAI->HasSkill(SKILL_MINING))
        {
            bot->Whisper("I have neither Herbalism nor Mining, so I can't gather.", Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToDoGather();
        WhisperStatusChange(owner, "DO_GATHER");
        return true;
    }
    else if (status == RPG_DUEL_SPOT)  // #201 (#612), as on the AC test tree (NewRpgAction.cpp:185-196)
    {
        int32 spotIdx = sDuelSpotMgr->SelectSpotFor(bot);
        if (spotIdx < 0)
        {
            bot->Whisper("No duel spot available.", Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToDuelSpot(static_cast<uint32>(spotIdx));
        WhisperStatusChange(owner, "DUEL_SPOT");
        return true;
    }
    else if (status == RPG_DO_QUEST)
    {
        if (!questId)
        {
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 qid = bot->GetQuestSlotQuestId(slot);
                if (!qid)
                    continue;
                std::vector<POIInfo> poi;
                if (GetQuestPOIPosAndObjectiveIdx(qid, poi, true))
                {
                    questId = qid;
                    break;
                }
            }
        }
        if (!questId)
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_quest_error", "No quest available; use 'do quest <id>'.", {});
            bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        QuestStatus questStatus = bot->GetQuestStatus(questId);
        if (!quest || (questStatus != QUEST_STATUS_INCOMPLETE && questStatus != QUEST_STATUS_COMPLETE))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_invalid_quest_error", "Invalid quest %quest_id",
                {{"%quest_id", std::to_string(questId)}});
            bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
            return false;
        }
        info.ChangeToDoQuest(questId, quest);
        WhisperStatusChange(owner, "DO_QUEST " + std::to_string(questId));
        return true;
    }

    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "rpg_unknown_status_error",
        "Unknown rpg status. Options: idle, rest, wander random, wander npc, "
        "go grind, go camp, do quest [<id>], travel flight, outdoor pvp.", {});
    bot->Whisper(msg, Language(LANG_UNIVERSAL), owner->GetGUID());
    return false;
}

bool StartRpgDoQuestAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    PlayerbotChatHandler ch(owner);
    uint32 questId = ch.extractQuestId(text);
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest)
    {
        botAI->GetRpgInfo().ChangeToDoQuest(questId, quest);
        bot->Whisper("Start to do quest " + std::to_string(questId), Language(LANG_UNIVERSAL), owner->GetGUID());
        return true;
    }
    bot->Whisper("Invalid quest " + text, Language(LANG_UNIVERSAL), owner->GetGUID());
    return false;
}

bool NewRpgStatusUpdateAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->GetRpgInfo();
    // #201 (#615): a seeded bot is handed to RPG_DUEL_SPOT first, before the scenario lock (SPEC 2).
    {
        uint32 pendingSpot = 0;
        if (sDuelSpotMgr->TakePending(bot->GetGUID(), pendingSpot))
        {
            info.ChangeToDuelSpot(pendingSpot, true);
            return true;
        }
    }
    // .botscenario pins the status it set until the scenario arrives or times out.
    if (BotScenario::IsStatusLocked(bot))
        return false;
    NewRpgStatus status = info.GetStatus();

    // BT-254 (ported from mod-playerbots' "move stuck" trigger, with a player's recovery instead of its reset): a bot
    // whose status wants it moving and that stood within 5 yd for 3 minutes out of combat steps aside and picks a new task.
    // The 3 minutes are active time (BT-230 clock): a bot on the activity rotation stands still between its windows by
    // design, and the wall clock flagged 42 of them in the first 20 minutes after build 4 (BT-257).
    {
        if (info.stillWatchSerial != info.serial)
        {
            info.stillWatch = RoamPolicy::StillWatch{};
            info.stillWatchSerial = info.serial;
        }
        bool const wantsToMove = status == RPG_GO_GRIND || status == RPG_GO_CAMP || status == RPG_WANDER_RANDOM ||
                                 status == RPG_DO_QUEST || status == RPG_DO_GATHER;
        bool const watching = wantsToMove && bot->IsAlive() && !bot->IsInCombat() && bot->IsStandState() &&
                              !bot->IsInFlight() && !bot->GetTransport() && !bot->GetVehicle() &&
                              !BotBreak::IsOnBreak(bot->GetGUIDLow()) && !RoamTransport::HoldsForTransportLeg(bot) &&
                              !bot->IsNonMeleeSpellCasted(false);
        if (RoamPolicy::StillTooLong(info.stillWatch, watching, bot->GetPositionX(), bot->GetPositionY(),
                                     bot->GetPositionZ(), botAI->GetQuestClockMs()))
        {
            std::string statusName = info.ToString().substr(8);
            statusName = statusName.substr(0, statusName.find('\n'));
            // BT-381: on a quest the work is written to the retry book first (its log line names the quest), so the
            // new pick does not walk back to the same spot: Ocassa stood at one ledge ten times in four hours.
            uint32 const questId = status == RPG_DO_QUEST ? FailQuestWorkStoodStill() : 0;
            std::string const questNote = questId ? " quest " + std::to_string(questId) : std::string();
            SF_LOG_INFO("playerbots", "[STUCK] %s stood still %u s on %s%s at (%.1f,%.1f,%.1f) map %u, steps aside and picks a new task",
                        bot->GetName().c_str(), RoamPolicy::STILL_MS / IN_MILLISECONDS, statusName.c_str(),
                        questNote.c_str(),
                        bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
            bot->StopMoving();
            bot->GetMotionMaster()->Clear();
            AI_VALUE(LastMovement&, "last movement").clear();
            WorldPosition const here(bot);
            info.ChangeToIdle();
            PatrolNear(here, 20.0f);
            return true;
        }
    }
    switch (status)
    {
        case RPG_IDLE:
        {
            // #BT-015: worn gear and no repair NPC in sight: the next trip is to a camp hub, where repairers stand;
            // the "repair" strategy's trigger takes the bot to one when it sees it
            // BT-149: the same trip when the bags are full or the food or drink is low (BT-178) and no vendor is in
            // sight; vendors stand at camps too ("sell trip" strategy takes the bot to one it sees)
            if ((BotRepair::NeedsRepair(bot) || BotSale::TripWanted(botAI, bot)) &&
                (!info.lastRepairTripMs || GetMSTimeDiffToNow(info.lastRepairTripMs) > repairTripCooldown) &&
                !BotRepair::FindNearbyRepairer(botAI, bot) && !BotSale::FindNearbyVendor(botAI, bot))
            {
                WorldPosition const camp = SelectRandomCampPos(bot);
                if (camp != WorldPosition())
                {
                    info.lastRepairTripMs = getMSTime();
                    info.ChangeToGoCamp(camp);
                    return true;
                }
            }

            // BT-210: a completed quest whose ender is far away comes before any new pick (new quests included).
            if (StartQuestHandInTrip())
                return true;

            // The soak bots are few and the odds of an errand at the auction house are small (an idle draw, a town, a
            // third of the idle errands): a named bot that stands in a town with an auctioneer goes there when its visit is due.
            if (SoakAuctionVisitReady(botAI, bot))
            {
                SF_LOG_INFO("playerbots", "[AUCTION] %s stands in a town with an auction house and its visit is due, goes there",
                            bot->GetName().c_str());
                info.ChangeToWanderNpc();
                return true;
            }

            return RandomChangeStatus({RPG_GO_CAMP, RPG_GO_GRIND, RPG_WANDER_RANDOM, RPG_WANDER_NPC, RPG_DO_QUEST,
                                       RPG_TRAVEL_FLIGHT, RPG_REST, RPG_OUTDOOR_PVP, RPG_DUEL_SPOT, RPG_DO_GATHER});
        }

        case RPG_GO_GRIND:
        {
            auto* data = std::get_if<NewRpgInfo::GoGrind>(&info.data);
            if (!data)
                return false;
            // GO_GRIND -> WANDER_RANDOM
            if (GrindArrivalDist(bot, data->pos) < 10.0f)
            {
                info.ChangeToWanderRandom();
                return true;
            }
            break;
        }
        case RPG_GO_CAMP:
        {
            auto* data = std::get_if<NewRpgInfo::GoCamp>(&info.data);
            if (!data)
                return false;
            // GO_CAMP -> WANDER_NPC
            if (bot->GetExactDist(&data->pos) < 10.0f)
            {
                info.ChangeToWanderNpc();
                return true;
            }
            break;
        }
        case RPG_WANDER_RANDOM:
        {
            // WANDER_RANDOM -> IDLE (BT-371: durations of these statuses are minutes of play)
            uint32 const activeMs = info.StatusActiveMs(botAI->GetQuestClockMs());
            if (activeMs > uint32(statusWanderRandomDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            // BT-252: no experience for two minutes means nothing to fight here; a new pick gives the bot a real task.
            auto* wander = std::get_if<NewRpgInfo::WanderRandom>(&info.data);
            if (wander && wander->anchor != WorldPosition() && bot->getLevel() < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) &&
                RoamPolicy::WanderHasNoWork(activeMs,
                                            bot->getLevel() != wander->startLevel ||
                                                bot->GetUInt32Value(PLAYER_FIELD_XP) != wander->startXp))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_WANDER_NPC:
        {
            if (info.HasStatusPersistedActive(statusWanderNpcDuration, botAI->GetQuestClockMs()))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_DO_QUEST:
        {
            // DO_QUEST -> IDLE
            if (info.HasStatusPersistedActive(statusDoQuestDuration, botAI->GetQuestClockMs()))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_TRAVEL_FLIGHT:
        {
            auto* data = std::get_if<NewRpgInfo::TravelFlight>(&info.data);
            if (!data)
                return false;

            if (data->inFlight && !bot->IsInFlight())
            {
                // flight arrival
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_REST:
        {
            // REST -> IDLE
            if (info.HasStatusPersistedActive(statusRestDuration, botAI->GetQuestClockMs()))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_OUTDOOR_PVP:
        {
            if (info.HasStatusPersisted(statusOutDoorPvPDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_DO_GATHER:
        {
            // DO_GATHER -> IDLE (AC NewRpgAction.cpp)
            if (info.HasStatusPersistedActive(statusDoGatherDuration, botAI->GetQuestClockMs()))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_DUEL_SPOT:
        {
            // #201 (#612): DUEL_SPOT -> IDLE (AC NewRpgAction.cpp:343-350 outdoor-pvp pattern). BT-044 (owner
            // 2026-11-08): a seeded bot also leaves after the visit's 30 minutes, like a natural one; before, it stayed
            // until the spot was disabled, a BG / arena / queue, or a group with a real player, and stopped levelling.
            auto* data = std::get_if<NewRpgInfo::DuelSpot>(&info.data);
            if (!data)
                return false;
            bool leave = info.HasStatusPersisted(statusDuelSpotDuration) ||
                         data->spotIndex >= sPlayerbotAIConfig->duelSpots.size() || !sPlayerbotAIConfig->duelSpotEnable ||
                         bot->InBattleground() || bot->InArena() || bot->InBattlegroundQueue() ||
                         sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE;
            if (!leave)
            {
                if (Group* group = bot->GetGroup())
                {
                    for (GroupReference* ref = group->GetFirstMember(); ref != nullptr; ref = ref->next())
                    {
                        Player* member = ref->GetSource();
                        if (!member || member == bot)
                            continue;
                        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
                        if (!memberAI || memberAI->IsRealPlayer())
                        {
                            leave = true;
                            break;
                        }
                    }
                }
            }
            if (leave)
            {
                // #201 (wave 30): why a seeded bot left its spot (t8 saw seeded bots back on RPG statuses minutes later)
                if (data->seeded)
                    SF_LOG_DEBUG("playerbots",
                                 "[DUELSPOT] leave %s spot=%u bg=%d arena=%d bgq=%d lfg=%d disabled=%d badidx=%d",
                                 bot->GetName().c_str(), data->spotIndex, bot->InBattleground() ? 1 : 0,
                                 bot->InArena() ? 1 : 0, bot->InBattlegroundQueue() ? 1 : 0,
                                 sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE ? 1 : 0,
                                 sPlayerbotAIConfig->duelSpotEnable ? 0 : 1,
                                 data->spotIndex >= sPlayerbotAIConfig->duelSpots.size() ? 1 : 0);
                info.ChangeToIdle();
                sDuelSpotMgr->Leave(bot->GetGUID());
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

bool NewRpgGoGrindAction::Execute(Event /*event*/)
{
    // #890 (BEYOND AC): no quest-giver detour while the bot's roam trip holds it for a transport leg - the detour to an NPC on
    // the Orgrimmar tower walked the waiting bot up and down the ramp and reset its dock wait (wait-again x12).
    if (!RoamTransport::HoldsForTransportLeg(bot) && SearchQuestGiverAndAcceptOrReward())
        return true;

    if (auto* data = std::get_if<NewRpgInfo::GoGrind>(&botAI->GetRpgInfo().data))
    {
        if (MoveFarTo(GrindMoveTarget(bot, data->pos, pathFinderDis)))
            return true;
        // Small nudge so the next tick's MoveFarTo starts from a
        // slightly different position. Kept small so it doesn't look
        // like the bot is abandoning its destination.
        return MoveRandomNear(10.0f);
    }

    return false;
}

bool NewRpgGoCampAction::Execute(Event /*event*/)
{
    // #890 (BEYOND AC): no quest-giver detour while the bot's roam trip holds it for a transport leg - the detour to an NPC on
    // the Orgrimmar tower walked the waiting bot up and down the ramp and reset its dock wait (wait-again x12).
    if (!RoamTransport::HoldsForTransportLeg(bot) && SearchQuestGiverAndAcceptOrReward())
        return true;

    if (auto* data = std::get_if<NewRpgInfo::GoCamp>(&botAI->GetRpgInfo().data))
    {
        if (MoveFarTo(data->pos))
            return true;
        return MoveRandomNear(10.0f);
    }

    return false;
}

namespace
{
// MoP 5.4.8 ships no src/server/game/Maps/AreaDefines.h, so AC's AREA_NAGRAND is not available
// by name; 3518 is the Nagrand zone id (Halaa, OutdoorPvPNA).
constexpr uint32 AREA_NAGRAND = 3518;
} // namespace

bool NewRpgOutdoorPvpAction::Execute(Event /*event*/)
{
    if (!bot->IsPvP())
    {
        botAI->GetRpgInfo().ChangeToIdle();
        return false;
    }
    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL) || !bot->IsOutdoorPvPActive())
        return false;

    uint32 zoneId = bot->GetZoneId();
    OutdoorPvP* outdoorPvP = sOutdoorPvPMgr->GetOutdoorPvPToZoneId(zoneId);
    if (!outdoorPvP || zoneId == AREA_NAGRAND)
    {
        botAI->GetRpgInfo().ChangeToIdle();
        return false;
    }

    OutdoorPvP::OPvPCapturePointMap const& capturePointMap = BotGetCapturePoints(outdoorPvP);

    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* dataPtr = std::get_if<NewRpgInfo::OutdoorPvP>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;
    // Re-resolve stored lowguid from the capture point map each tick (avoids dangling pointers).
    // MoP keys OPvPCapturePointMap by that same lowguid, so the stored id is the map key itself.
    OPvPCapturePoint* objective = nullptr;
    if (data.capturePointSpawnId && !capturePointMap.empty())
    {
        auto it = capturePointMap.find(data.capturePointSpawnId);
        if (it != capturePointMap.end())
        {
            OPvPCapturePoint* capturePoint = it->second;
            if (capturePoint && capturePoint->m_capturePoint)
            {
                float threshold = BotGetCapturePointMinValue(capturePoint);
                float slider = BotGetCapturePointSlider(capturePoint);
                TeamId faction = bot->GetTeamId();
                SF_LOG_DEBUG("playerbots", "[New RPG] Bot %s with faction %u is evaluating existing RPG objective %s with threshold %f and slider value %f", bot->GetName().c_str(), uint32(faction), capturePoint->m_capturePoint->GetName().c_str(), threshold, slider);
                if ((faction == TEAM_HORDE && slider >= -threshold) ||
                    (faction == TEAM_ALLIANCE && slider <= threshold))
                    objective = capturePoint;
            }
        }
        if (!objective)
            data.capturePointSpawnId = 0;
    }

    if (!objective)
    {
        objective = SelectNewObjective(capturePointMap);
        if (!objective)
        {
            botAI->GetRpgInfo().ChangeToIdle();
            return true;
        }
        // AC reads objective->m_capturePointSpawnId here; SkyFire carries the same lowguid on
        // m_capturePointGUID (a plain uint32), which is what the map is keyed by.
        data.capturePointSpawnId = objective->m_capturePointGUID;
        SF_LOG_DEBUG("playerbots", "[New RPG] Bot %s selected outdoor PvP target capturePointSpawnId %u", bot->GetName().c_str(), uint32(data.capturePointSpawnId));
    }

    GameObject* objectiveGO = objective->m_capturePoint;
    if (!objectiveGO)
        return false;

    if (objectiveGO->GetGoType() != GAMEOBJECT_TYPE_CAPTURE_POINT)
        return false;

    float radius = objectiveGO->GetGOInfo()->capturePoint.radius / 2.0f;
    if (!objectiveGO->IsWithinDistInMap(bot, radius))
        return MoveFarTo(WorldPosition(objectiveGO));

    return PatrolCapturePoint(objectiveGO, radius);
}

OPvPCapturePoint* NewRpgOutdoorPvpAction::SelectNewObjective(OutdoorPvP::OPvPCapturePointMap const& capturePointMap)
{
    OPvPCapturePoint* objective = nullptr;
    TeamId faction = bot->GetTeamId();
    std::vector<OPvPCapturePoint*> candidateObjectives;

    if (capturePointMap.empty())
    {
        botAI->GetRpgInfo().ChangeToIdle();
        return objective;
    }
    for (auto const& [lowguid, point] : capturePointMap)
    {
        GameObject* capturePointObject = point->m_capturePoint;
        if (!capturePointObject)
            continue;

        float threshold = BotGetCapturePointMinValue(point);
        float slider = BotGetCapturePointSlider(point);
        if (faction == TEAM_HORDE && slider > -threshold)
            candidateObjectives.push_back(point);
        else if (faction == TEAM_ALLIANCE && slider < threshold)
            candidateObjectives.push_back(point);
    }
    if (candidateObjectives.empty())
    {
        SF_LOG_DEBUG("playerbots", "[New RPG] Bot %s found no valid outdoor PvP objectives to capture", bot->GetName().c_str());
        botAI->GetRpgInfo().ChangeToIdle();
        return objective;
    }
    int randomIndex = urand(0, candidateObjectives.size() - 1);
    objective = candidateObjectives[randomIndex];
    return objective;
}

bool NewRpgOutdoorPvpAction::PatrolCapturePoint(GameObject* objectiveGO, float radius)
{
    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL))
        return false;

    // Randomly pause at the current spot before picking a new patrol point
    if (urand(0, 2) == 0)
        return ForceToWait(urand(3000, 6000));

    float patrolRadius = radius * 0.8f;

    // #399: AC calls MoveRandomNear(patrolRadius, MOVEMENT_NORMAL, objectiveGO) (NewRpgOutdoorPvP.cpp:130), but its
    // MoveRandomNear never reads that centre (NewRpgBaseAction.cpp:243, unused since upstream #2217): it hops around
    // the bot and validates each 13-32 yd hop as ONE straight segment with Map::CanReachPositionAndGetValidCoords, whose
    // slope test (PathGenerator::IsWalkableClimb, Map.cpp:4629) refuses any segment that rises or falls more than about
    // the bot's collision height (~2 yd). On the hill of a Hellfire tower that refuses nearly every hop (the one hop
    // made rose 1.3 yd), so the bot waited for good (MoP, Stadium, 2026-10-03: 1 hop in 120 s).
    // Extending AC's intent (the centre it already passes): sample the hop around the capture point itself, within
    // patrolRadius, and validate the navmesh path the bot will actually walk segment by segment instead of one
    // straight line. Same guards as MoveRandomNear (path type, reachability, water, the #61 longest-short-hop
    // fallback); the destination always stays inside the capture radius.
    Map* map = bot->GetMap();
    if (!map)
        return ForceToWait(urand(3000, 6000));

    float const cx = objectiveGO->GetPositionX();
    float const cy = objectiveGO->GetPositionY();
    float const cz = objectiveGO->GetPositionZ();
    float const minHop = 0.25f * patrolRadius;
    float bestHop = 0.0f, bestX = 0.0f, bestY = 0.0f, bestZ = 0.0f;
    uint32 rejPath = 0, rejReach = 0, rejWater = 0, rejOut = 0;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        float distance, angle;
        // Deterministic scenarios (HARNESS-SPEC-botscenario-v2 G3.4) get the same golden-angle sweep as MoveRandomNear.
        if (!BotScenario::NextRandomNearAngle(bot, patrolRadius, angle, distance))
        {
            distance = static_cast<float>(rand_norm()) * patrolRadius;  // uniform in distance, like the duel-spot patrol
            angle = static_cast<float>(rand_norm()) * 2.0f * static_cast<float>(M_PI);
        }
        float dx = cx + distance * std::cos(angle);
        float dy = cy + distance * std::sin(angle);
        float dz = cz;
        float const h = map->GetHeight(bot->GetPhaseMask(), dx, dy, cz + 10.0f);
        if (h > INVALID_HEIGHT && std::fabs(h - cz) <= 30.0f)
            dz = h + 0.2f;

        PathGenerator path(bot);
        path.CalculatePath(dx, dy, dz);
        // PORT: SkyFire's PathGenerator has no PATHFIND_FARFROMPOLY flag (LoA marks a start or end far from the mesh), so
        // such a path cannot be reported and only NORMAL and INCOMPLETE are accepted.
        uint32 const typeOk = PATHFIND_NORMAL | PATHFIND_INCOMPLETE;
        if ((path.GetPathType() & ~typeOk) || path.GetPath().empty())
        {
            ++rejPath;
            continue;
        }

        // Walk the path's own ~4 yd smooth-path segments (SMOOTH_PATH_STEP_SIZE) through the same per-segment check
        // MoveRandomNear applies to its one straight segment; a refused segment ends the hop at the last one that passed.
        // Done here rather than with the Map.cpp:4547 path overload: that one also tests the degenerate first segment
        // (bot position -> first path point, same x/y), where getSlopeAngle divides by a zero floor distance and the
        // slope test fails.
        bool fullPath = true;
        float px = bot->GetPositionX(), py = bot->GetPositionY(), pz = bot->GetPositionZ();
        for (G3D::Vector3 const& point : path.GetPath())
        {
            float sx = point.x, sy = point.y, sz = point.z;
            if ((sx - px) * (sx - px) + (sy - py) * (sy - py) < 0.25f)
                continue;
            if (!BotCanReachPositionAndGetValidCoords(map, bot, px, py, pz, sx, sy, sz))
            {
                fullPath = false;
                break;
            }
            px = sx;
            py = sy;
            pz = sz;
        }
        dx = px;
        dy = py;
        dz = pz;
        if (map->IsInWater(dx, dy, dz))
        {
            ++rejWater;
            continue;
        }
        if (objectiveGO->GetExactDist2d(dx, dy) > radius)
        {
            ++rejOut;
            continue;
        }

        float const hop = bot->GetExactDist2d(dx, dy);
        if (!fullPath || hop < minHop)
        {
            if (!fullPath)
                ++rejReach;
            if (hop >= 3.0f && hop > bestHop)
            {
                bestHop = hop;
                bestX = dx;
                bestY = dy;
                bestZ = dz;
            }
            continue;
        }

        if (MoveTo(bot->GetMapId(), dx, dy, dz, false, false, false, true, MovementPriority::MOVEMENT_NORMAL))
        {
            BotScenario::LogTarget(bot, "rnd", nullptr, dx, dy, dz, patrolRadius);
            return true;
        }
    }

    if (bestHop > 0.0f && MoveTo(bot->GetMapId(), bestX, bestY, bestZ, false, false, false, true, MovementPriority::MOVEMENT_NORMAL))
    {
        BotScenario::LogTarget(bot, "rnd", nullptr, bestX, bestY, bestZ, patrolRadius);
        return true;
    }

    if (sPlayerbotAIConfig->debugBotNames.count(bot->GetName()) || BotScenario::WantsTrail(bot))
        SF_LOG_DEBUG("playerbots", "[OPVP] %s patrol: no hop around %s (%.1f,%.1f,%.1f) r=%.1f from (%.1f,%.1f,%.1f) rej path=%u reach=%u water=%u out=%u",
                     bot->GetName().c_str(), objectiveGO->GetName().c_str(), cx, cy, cz, patrolRadius, bot->GetPositionX(),
                     bot->GetPositionY(), bot->GetPositionZ(), rejPath, rejReach, rejWater, rejOut);

    return ForceToWait(urand(3000, 6000));
}

bool NewRpgWanderRandomAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* data = std::get_if<NewRpgInfo::WanderRandom>(&info.data);
    if (!data)
        return false;

    // BT-253: nobody wanders the streets of a town without a reason; a bot there runs errands instead.
    if (bot->HasFlag(PLAYER_FIELD_PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        info.ChangeToWanderNpc();
        return true;
    }

    // BT-252: the patrol stays around where it began instead of drifting step by step over the hills.
    if (data->anchor == WorldPosition())
    {
        data->anchor = WorldPosition(bot);
        data->startXp = bot->GetUInt32Value(PLAYER_FIELD_XP);
        data->startLevel = bot->getLevel();
    }
    return PatrolNear(data->anchor, RoamPolicy::WANDER_LEASH_YARDS);
}

namespace
{
// BT-253: errand targets within this range; a town's bank, auction house and inn are rarely further from its centre.
constexpr float ERRAND_RANGE = 200.0f;
// Stand this far in front of the NPC or mailbox, as a player does.
constexpr float ERRAND_STAND_YARDS = 2.5f;

using ErrandPolicy::Errand;
using ErrandTargets = std::array<WorldObject*, size_t(Errand::Inn) + 1>;

void Keep(ErrandTargets& targets, Errand errand, WorldObject* object, Player* bot)
{
    WorldObject*& slot = targets[size_t(errand)];
    if (!slot || bot->GetExactDist(object) < bot->GetExactDist(slot))
        slot = object;
}

// The nearest friendly target of every errand, and what the bot needs, in one scan (once per errand, not per tick).
ErrandTargets FindErrandTargets(PlayerbotAI* botAI, Player* bot, ErrandPolicy::Town& town)
{
    ErrandTargets targets{};
    // BT-323: Dual Talent Specialization for its PvP set is bought at a class trainer, like a player.
    bool const wantsDualSpec = BotPvpTalents::WantsDualSpec(bot);
    std::list<Unit*> units;
    Skyfire::AnyUnitInObjectRangeCheck unitCheck(bot, ERRAND_RANGE);
    Skyfire::UnitListSearcher<Skyfire::AnyUnitInObjectRangeCheck> unitSearcher(bot, units, unitCheck);
    bot->VisitNearbyObject(ERRAND_RANGE, unitSearcher);
    for (Unit* unit : units)
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsInWorld() || !creature->IsAlive() || creature->IsHostileTo(bot) ||
            creature->IsInCombat())
            continue;
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR))
            Keep(targets, Errand::Repair, creature, bot);
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR))
            Keep(targets, Errand::Vendor, creature, bot);
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_BANKER))
            Keep(targets, Errand::Bank, creature, bot);
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER))
            Keep(targets, Errand::Auction, creature, bot);
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_INNKEEPER))
            Keep(targets, Errand::Inn, creature, bot);
        if (creature->HasFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_TRAINER_CLASS))
        {
            CreatureTemplate const* info = creature->GetCreatureTemplate();
            if (info && info->trainer_type == TRAINER_TYPE_CLASS && info->trainer_class == bot->getClass() &&
                (wantsDualSpec || BotTraining::CountLearnableAt(bot, creature)))
                Keep(targets, Errand::Trainer, creature, bot);
        }
    }

    std::list<GameObject*> objects;
    Skyfire::GameObjectInRangeCheck objectCheck(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), ERRAND_RANGE);
    Skyfire::GameObjectListSearcher<Skyfire::GameObjectInRangeCheck> objectSearcher(bot, objects, objectCheck);
    bot->VisitNearbyGridObject(ERRAND_RANGE, objectSearcher);
    for (GameObject* object : objects)
        if (object && object->IsInWorld() && object->isSpawned() && object->GetGoType() == GAMEOBJECT_TYPE_MAILBOX)
            Keep(targets, Errand::Mailbox, object, bot);

    town.needsRepair = BotRepair::NeedsRepair(bot);
    town.needsVendor = BotSale::TripWanted(botAI, bot);
    town.needsTraining = targets[size_t(Errand::Trainer)] != nullptr;
    town.hasMail = bot->unReadMails > 0;
    town.repairer = targets[size_t(Errand::Repair)] != nullptr;
    town.vendor = targets[size_t(Errand::Vendor)] != nullptr;
    town.trainer = town.needsTraining;
    town.mailbox = targets[size_t(Errand::Mailbox)] != nullptr;
    town.banker = targets[size_t(Errand::Bank)] != nullptr;
    town.auctioneer = targets[size_t(Errand::Auction)] != nullptr;
    town.innkeeper = targets[size_t(Errand::Inn)] != nullptr;
    town.auctionDue = town.auctioneer && BotAuction::SoakVisitDue(botAI, bot);
    return targets;
}

bool SoakAuctionVisitReady(PlayerbotAI* botAI, Player* bot)
{
    // The cheap gate first: the town scan runs only for a bot whose visit is due.
    if (!BotAuction::SoakVisitDue(botAI, bot))
        return false;

    ErrandPolicy::Town town;
    FindErrandTargets(botAI, bot, town);
    return town.auctionDue;
}

// A gesture of a player waiting at a counter.
uint32 IdleGesture()
{
    static uint32 const gestures[] = {EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_QUESTION, EMOTE_ONESHOT_LAUGH,
                                      EMOTE_ONESHOT_POINT, EMOTE_ONESHOT_YES, EMOTE_ONESHOT_NO};
    return gestures[urand(0, std::size(gestures) - 1)];
}
}  // namespace

bool NewRpgWanderNpcAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* dataPtr = std::get_if<NewRpgInfo::WanderNpc>(&info.data);
    if (!dataPtr)
        return false;

    auto& data = *dataPtr;
    bool const auctionErrand = data.npcOrGo && Errand(data.errand) == Errand::Auction;
    if (!auctionErrand && (data.npcOrGo || !SoakAuctionVisitReady(botAI, bot)) && SearchQuestGiverAndAcceptOrReward())
        return true;

    if (!data.npcOrGo)
    {
        // A quest giver with something to offer first, then an errand (BT-253), then AC's random NPC visit.
        BotGuid npcOrGo = BotGuid::Empty;
        Errand errand = Errand::None;
        uint32 const nowMs = getMSTime();
        bool const forcePaused = info.lastForcedAuctionMs && getMSTimeDiff(info.lastForcedAuctionMs, nowMs) < 10 * MINUTE * IN_MILLISECONDS;
        if (!forcePaused && BotAuction::SoakVisitDue(botAI, bot))
        {
            ErrandPolicy::Town town;
            ErrandTargets const targets = FindErrandTargets(botAI, bot, town);
            // A higher need (repair, vendor, trainer, mail) still comes first; otherwise the due auction visit.
            Errand const chosen = town.auctionDue ? ErrandPolicy::Choose(town, 0) : Errand::None;
            if (chosen != Errand::None && targets[size_t(chosen)])
            {
                errand = chosen;
                npcOrGo = targets[size_t(chosen)]->GetGUID();
                if (chosen == Errand::Auction)
                    info.lastForcedAuctionMs = nowMs ? nowMs : 1;
            }
        }
        if (npcOrGo.IsEmpty())
            npcOrGo = ChooseNpcOrGameObjectToInteract(true);
        if (npcOrGo.IsEmpty() && !BotScenario::PickNearest(bot))
        {
            ErrandPolicy::Town town;
            ErrandTargets const targets = FindErrandTargets(botAI, bot, town);
            errand = ErrandPolicy::Choose(town, urand(0, 99));
            if (errand != Errand::None)
                npcOrGo = targets[size_t(errand)]->GetGUID();
        }
        if (npcOrGo.IsEmpty())
            npcOrGo = ChooseNpcOrGameObjectToInteract();
        if (npcOrGo.IsEmpty())
        {
            // No npc can be found, switch to IDLE
            BotScenario::LogTarget(bot, "none", nullptr, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            info.ChangeToIdle();
            return true;
        }

        data.npcOrGo = npcOrGo;
        data.lastReach = 0;
        data.errand = uint8(errand);
        data.dwellMs = errand != Errand::None ? ErrandPolicy::DwellMs(errand, urand(0, 99)) : npcStayTime;
        if (WorldObject* s = ObjectAccessor::GetWorldObject(*bot, npcOrGo))
        {
            BotScenario::LogTarget(bot, "npc", s, s->GetPositionX(), s->GetPositionY(), s->GetPositionZ());
            if (sPlayerbotAIConfig->debugBotNames.count(bot->GetName()))
                SF_LOG_DEBUG("playerbots", "[ERRAND] %s goes to %s (%s, %.0f yd), stays %u s", bot->GetName().c_str(),
                             s->GetName().c_str(), ErrandPolicy::Name(errand), bot->GetExactDist(s),
                             data.dwellMs / IN_MILLISECONDS);
        }
        return true;
    }

    bool const debugBot = !sPlayerbotAIConfig->debugBotNames.empty() && sPlayerbotAIConfig->debugBotNames.count(bot->GetName());

    WorldObject* object = ObjectAccessor::GetWorldObject(*bot, data.npcOrGo);
    if (!object || !object->IsInWorld() || object->GetMapId() != bot->GetMapId())
    {
        // The target despawned or left: pick the next one.
        data.npcOrGo = BotGuid::Empty;
        data.lastReach = 0;
        return false;
    }
    bool const within = IsWithinInteractionDist(object);
    if (debugBot)
        SF_LOG_DEBUG("playerbots", "[RPGNPC] %s npc=%s object=%d withinDist=%d waitingMove=%d dist=%.1f",
                     bot->GetName().c_str(), data.npcOrGo.ToString().c_str(), object ? 1 : 0, within ? 1 : 0,
                     IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL) ? 1 : 0,
                     object ? bot->GetExactDist(object) : -1.0f);

    Errand const errand = Errand(data.errand);
    if (within)
    {
        if (!data.lastReach)
        {
            // Arrived: stop at the counter, face it, and stay (seated at the inn) like a player doing business.
            data.lastReach = getMSTime();
            data.nextGestureMs = ErrandPolicy::NextGestureMs(urand(0, 99));
            if (bot->isMoving())
                bot->StopMoving();
            bot->SetFacingToObject(object);
            if (ErrandPolicy::SitsThere(errand))
                bot->SetStandState(UNIT_STAND_STATE_SIT);
            if (bot->CanInteractWithQuestGiver(object))
                InteractWithNpcOrGameObjectForQuest(data.npcOrGo);
            if (errand == Errand::Bank)
                BankErrand::OnArrival(botAI, bot, object);
            if (errand == Errand::Trainer)
                BotPvpTalents::BuyDualSpec(bot, object->ToCreature());  // BT-323
            // Design decision 0033: at the auctioneer or a mailbox a random bot does business; the work runs on the world thread.
            if (errand == Errand::Auction || errand == Errand::Mailbox)
                BotAuction::OnErrandArrival(botAI, bot, object, errand == Errand::Auction);
            return true;
        }

        uint32 const stayed = GetMSTimeDiffToNow(data.lastReach);
        // A visit that is still working keeps the bot at the counter; it ends by itself within AuctionVisitPolicy::VISIT_TIMEOUT_MS.
        // The bound is a guard only: a visit that never ends (the world thread stalled) must not hold a bot for good.
        bool const doingBusiness = (errand == Errand::Auction || errand == Errand::Mailbox) &&
                                   stayed < data.dwellMs + AuctionVisitPolicy::VISIT_TIMEOUT_MS + 15000 && BotAuction::VisitWorking(bot);
        if (stayed < data.dwellMs || doingBusiness)
        {
            if (errand != Errand::None && stayed >= data.nextGestureMs && bot->IsStandState())
            {
                bot->HandleEmoteCommand(IdleGesture());
                data.nextGestureMs = stayed + ErrandPolicy::NextGestureMs(urand(0, 99));
            }
            return false;
        }

        // Done here: one more errand, or leave town (the next status is drawn as usual).
        BotAuction::OnErrandEnd(bot);
        if (!bot->IsStandState())
            bot->SetStandState(UNIT_STAND_STATE_STAND);
        data.npcOrGo = BotGuid::Empty;
        data.lastReach = 0;
        if (errand != Errand::None)
        {
            ++data.errandsDone;
            if (!ErrandPolicy::AnotherErrand(data.errandsDone, urand(0, 99)))
            {
                info.ChangeToIdle();
                return true;
            }
        }
    }
    else
    {
        // Walk straight to a spot in front of the target, the whole way (MoveFarTo follows the real route).
        float const angle = object->GetAngle(bot);
        WorldPosition const stand(object->GetMapId(), object->GetPositionX() + std::cos(angle) * ERRAND_STAND_YARDS,
                                  object->GetPositionY() + std::sin(angle) * ERRAND_STAND_YARDS, object->GetPositionZ());
        if (errand != Errand::None ? MoveFarTo(stand) : MoveWorldObjectTo(data.npcOrGo))
        {
            if (debugBot)
                SF_LOG_DEBUG("playerbots", "[RPGNPC] %s walks to the target", bot->GetName().c_str());
            return true;
        }
        if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL) || bot->isMoving())
            return false;

        // NPC pathing failed (random offset in a wall, mmap hiccup, etc).
        // Take a small random step so the next tick retries from a
        // different spot instead of staring at the NPC from afar.
        bool const moved = MoveRandomNear(15.0f);
        if (debugBot)
            SF_LOG_DEBUG("playerbots", "[RPGNPC] %s MoveWorldObjectTo FAILED, MoveRandomNear=%d", bot->GetName().c_str(), moved ? 1 : 0);
        return moved;
    }

    return true;
}

bool NewRpgDoQuestAction::Execute(Event /*event*/)
{
    // #890 pattern: no quest-giver detour while a roam trip holds the bot for a transport leg (a hand-in trip's route).
    if (!RoamTransport::HoldsForTransportLeg(bot) && SearchQuestGiverAndAcceptOrReward())
        return true;

    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* dataPtr = std::get_if<NewRpgInfo::DoQuest>(&info.data);
    if (!dataPtr)
        return false;

    // BT-211: an item fetch ends with its quest's objectives (or when another quest is picked), so the grind pick stops
    // preferring its mobs.
    QuestItemSourcePolicy::Fetch& fetch = botAI->GetItemFetch();
    if (fetch.active && (fetch.questId != dataPtr->questId ||
                         bot->GetQuestStatus(fetch.questId) != QUEST_STATUS_INCOMPLETE))
        fetch = QuestItemSourcePolicy::Fetch();
    // BT-212: the same for an interaction trip.
    QuestInteractPolicy::Trip& interactTrip = botAI->GetInteractTrip();
    if (interactTrip.active && (interactTrip.questId != dataPtr->questId ||
                                bot->GetQuestStatus(interactTrip.questId) != QUEST_STATUS_INCOMPLETE))
        interactTrip = QuestInteractPolicy::Trip();

    switch (bot->GetQuestStatus(dataPtr->questId))
    {
        case QUEST_STATUS_INCOMPLETE:
            return DoIncompleteQuest(*dataPtr);
        case QUEST_STATUS_COMPLETE:
            return DoCompletedQuest(*dataPtr);
        default:
            break;
    }

    info.ChangeToIdle();
    return true;
}

bool NewRpgDoQuestAction::DoIncompleteQuest(NewRpgInfo::DoQuest& data)
{
    uint32 const questId = data.questId;
    Quest const* quest = data.quest ? data.quest : sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        botAI->GetRpgInfo().ChangeToIdle();
        return true;
    }

    // On MoP, objective progress is per QuestObjective rather than the 3.3.5
    // CreatureOrGOCount/ItemCount arrays; match the POI's objective index against
    // QuestObjective::StorageIndex.
    auto objectiveComplete = [this, quest](int32 storageIndex) -> bool
    {
        for (QuestObjective const& objective : BotQuestObjectives(quest))
            if (LegacyPoiIndex(objective) == storageIndex)
                return BotIsQuestObjectiveComplete(bot, quest, objective);

        return true;
    };

    auto progress = [this, quest]()
    {
        std::vector<uint32> counters;
        for (QuestObjective const& objective : BotQuestObjectives(quest))
            counters.push_back(bot->GetQuestObjectiveCounter(objective.Id));
        return counters;
    };

    if (data.pos != WorldPosition())
    {
        // the current objective is completed, clear it and pick a new one below
        if (objectiveComplete(data.objectiveIdx))
        {
            if (data.lastReachPOI && QuestRetryPolicy::Advanced(data.objectiveProgress, progress()))
                QuestRetryPolicy::OnQuestEvent(botAI->GetQuestRetries(), questId, QuestRetryPolicy::QuestEvent::Progress);
            data.lastReachPOI = 0;
            data.pos = WorldPosition();
            data.objectiveIdx = 0;
        }
    }

    // BT-372: an open exploration objective goes into its area trigger, where a client would report it.
    bool exploreHandled = false;
    bool const explored = DoQuestExploration(quest, data, exploreHandled);
    if (exploreHandled)
        return explored;

    // BT-211: an open item objective with a usable source (a mob, a chest, a vendor, a recipe) goes there instead of
    // the quest POI. A fetch starts only where a new POI would be picked, so the POI walk keeps the quests whose items
    // have no source.
    bool fetchHandled = false;
    bool const fetched = DoQuestItemFetch(quest, data.pos == WorldPosition(), fetchHandled);
    if (fetchHandled)
        return fetched;

    // BT-212: an open creature objective whose credit comes from talking, clicking, a quest item or an object (and not
    // from a kill) goes to that creature or object; the core gives the credit for the act.
    bool interactHandled = false;
    bool const interacted = DoQuestInteraction(quest, data.pos == WorldPosition(), interactHandled);
    if (interactHandled)
        return interacted;

    if (data.pos == WorldPosition())
    {
        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo))
        {
            // can't find a poi pos to go, stop doing quest for now
            botAI->GetRpgInfo().ChangeToIdle();
            return true;
        }

        uint32 rndIdx = urand(0, poiInfo.size() - 1);
        float dx = poiInfo[rndIdx].pos.x;
        float dy = poiInfo[rndIdx].pos.y;

        // z = MAX_HEIGHT as we do not know the accurate z
        float dz = std::max(bot->GetMap()->GetHeight(bot->GetPhaseMask(), dx, dy, MAX_HEIGHT),
                            bot->GetMap()->GetWaterLevel(dx, dy));

        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        data.lastReachPOI = 0;
        data.pos = WorldPosition(bot->GetMapId(), dx, dy, dz);
        data.objectiveIdx = poiInfo[rndIdx].objectiveIdx;
    }

    if (bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI)
    {
        if (MoveFarTo(data.pos))
            return true;

        // Long-range sampler couldn't land a candidate - nudge the bot a short distance so the
        // next tick retries from a different position instead of sitting idle.
        return MoveRandomNear(10.0f);
    }

    // Now we are near the quest objective. Killing mobs and looting quest items is done by the
    // grind strategy.
    if (!data.lastReachPOI)
    {
        // BT-230: the stay at the POI is measured in active time, as the quest trips are.
        data.lastReachPOI = botAI->GetQuestClockMs();
        data.objectiveProgress = progress();
        // BT-152: source items with self-use spells can be needed at the objective location.
        // The existing item path validates usability and invokes the core item script and spell.
        if (IsEventQuest(quest) && quest->GetSrcItemId())
            if (Item* item = bot->GetItemByEntry(quest->GetSrcItemId()))
            {
                UseItemAction useItem(botAI, "event quest source item");
                useItem.UseQuestSourceItem(item);
            }
        return true;
    }

    if (botAI->GetQuestClockMs() - data.lastReachPOI >= poiStayTime)
    {
        if (!QuestRetryPolicy::Advanced(data.objectiveProgress, progress()))
        {
            auto& failure = botAI->GetQuestRetries()[questId];
            bool const abandon = QuestRetryPolicy::RecordFailure(failure, getMSTime(), bot->GetZoneId(), bot->getLevel(), false);
            uint32 const tries = failure.tries;
            if (abandon && bot->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE && DropQuest(questId))
            {
                botAI->GetRpgStatistic().questAbandoned++;
                SF_LOG_INFO("playerbots", "[QUEST] %s abandons %u %s after %u failed tries",
                    bot->GetName().c_str(), questId, quest->GetTitle().c_str(), tries);
            }
            botAI->GetRpgInfo().ChangeToIdle();
            return true;
        }
        QuestRetryPolicy::OnQuestEvent(botAI->GetQuestRetries(), questId, QuestRetryPolicy::QuestEvent::Progress);

        // clear and select another poi later
        data.lastReachPOI = 0;
        data.pos = WorldPosition();
        data.objectiveIdx = 0;
        return true;
    }

    // At the POI: search the area around it like a player looking for the quest's mobs and objects, in steps of 12 yd
    // or more within 30 yd of the POI (BT-252). The 8 yd random shuffles read as pacing on the spot.
    return PatrolNear(data.pos, RoamPolicy::POI_LEASH_YARDS);
}

bool NewRpgDoQuestAction::DoCompletedQuest(NewRpgInfo::DoQuest& data)
{
    uint32 const questId = data.questId;

    QuestHandInPolicy::Trip const& trip = botAI->GetHandInTrip();
    if (trip.active && trip.questId == questId)
        return DoHandInTrip();

    if (data.objectiveIdx != -1)
    {
        // quest is complete: head back to the POI with objective index -1 to hand it in
        // #322 (AC NewRpgAction.cpp:608)
        if (Quest const* quest = data.quest ? data.quest : sObjectMgr->GetQuestTemplate(questId))
            BroadcastHelper::BroadcastQuestUpdateComplete(botAI, bot, quest);
        botAI->GetRpgStatistic().questCompleted++;

        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo, true))
        {
            // can't find a poi pos to reward, stop doing quest for now
            botAI->GetRpgInfo().ChangeToIdle();
            return false;
        }

        float dx = poiInfo[0].pos.x;
        float dy = poiInfo[0].pos.y;
        float dz = std::max(bot->GetMap()->GetHeight(bot->GetPhaseMask(), dx, dy, MAX_HEIGHT),
                            bot->GetMap()->GetWaterLevel(dx, dy));

        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        data.lastReachPOI = 0;
        data.pos = WorldPosition(bot->GetMapId(), dx, dy, dz);
        data.objectiveIdx = -1;
    }

    if (data.pos == WorldPosition())
        return false;

    if (bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI)
    {
        if (MoveFarTo(data.pos))
            return true;

        return MoveRandomNear(10.0f);
    }

    // Near the hand-in POI; the quest itself is turned in by SearchQuestGiverAndAcceptOrReward.
    if (!data.lastReachPOI)
    {
        data.lastReachPOI = botAI->GetQuestClockMs();
        return true;
    }

    if (botAI->GetQuestClockMs() - data.lastReachPOI >= poiStayTime)
    {
        // A complete quest is retried later and is never abandoned here.
        QuestRetryPolicy::RecordFailure(botAI->GetQuestRetries()[questId], getMSTime(), bot->GetZoneId(), bot->getLevel(), true);
        botAI->GetRpgInfo().ChangeToIdle();
        return true;
    }

    return false;
}

bool NewRpgDoQuestAction::DoHandInTrip()
{
    QuestHandInPolicy::Trip& trip = botAI->GetHandInTrip();
    WorldPosition const dest(trip.ender.map, trip.ender.x, trip.ender.y, trip.ender.z);
    if (bot->GetMapId() != trip.ender.map)
    {
        // A ride or a taxi on the route is still under way.
        if (bot->IsInFlight() || bot->GetTransport() || bot->IsBeingTeleported())
            return false;
        if (!trip.crossMap)
        {
            FailHandInTrip("left the ender's map");
            return true;
        }
        // BT-375: on the way to the ender's continent; MoveFarTo drives the boat, zeppelin or portal route and goes
        // idle when the travel graph has none.
        QuestHandInPolicy::Resume(trip, botAI->GetQuestClockMs());
        if (QuestHandInPolicy::Expired(trip, botAI->GetQuestClockMs()))
        {
            FailHandInTrip(QuestHandInPolicy::StepName(QuestHandInPolicy::Step::Expired));
            return true;
        }
        bool const moved = MoveFarTo(dest);
        if (trip.active && botAI->GetRpgInfo().GetStatus() == RPG_IDLE)
            FailHandInTrip("no route to the ender's continent");
        return moved;
    }

    QuestHandInPolicy::Resume(trip, botAI->GetQuestClockMs());
    QuestHandInPolicy::Step const step = QuestHandInPolicy::Judge(trip, botAI->GetQuestClockMs(), bot->GetExactDist(&dest));
    switch (step)
    {
        case QuestHandInPolicy::Step::Expired:
        case QuestHandInPolicy::Step::NotThere:
            FailHandInTrip(QuestHandInPolicy::StepName(step));
            return true;
        case QuestHandInPolicy::Step::Wait:
            // At the spawn point: SearchQuestGiverAndAcceptOrReward (Execute) walks to the ender and hands in by talking.
            return false;
        case QuestHandInPolicy::Step::Travel:
            break;
    }

    return MoveFarOnQuestTrip(dest);
}

bool NewRpgTravelFlightAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* dataPtr = std::get_if<NewRpgInfo::TravelFlight>(&info.data);
    if (!dataPtr)
        return false;

    auto& data = *dataPtr;

    if (bot->IsInFlight())
    {
        data.inFlight = true;
        ContinueCrossMapTaxi();
        return false;
    }

    if (bot->GetDistance(data.flightMasterPos.GetPositionX(), data.flightMasterPos.GetPositionY(),
                         data.flightMasterPos.GetPositionZ()) > INTERACTION_DISTANCE)
        return MoveFarTo(data.flightMasterPos);

    Creature* flightMaster = bot->FindNearestCreature(data.flightMasterEntry, INTERACTION_DISTANCE * 3);
    if (!flightMaster || !flightMaster->IsAlive())
    {
        SF_LOG_DEBUG("playerbots",
                     "[New RPG] %s reached the flight master spot but found no live creature %u within %.1f yd "
                     "(bot at %.1f,%.1f,%.1f map %u) - giving up",
                     bot->GetName().c_str(), data.flightMasterEntry, INTERACTION_DISTANCE * 3, bot->GetPositionX(),
                     bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
        info.ChangeToIdle();
        return true;
    }

    if (bot->GetDistance(flightMaster) > INTERACTION_DISTANCE)
        return MoveFarTo(WorldPosition(flightMaster));

    std::vector<uint32> nodes = data.path;

    botAI->RemoveShapeshift();
    // #843 (MoP-only compensation, AC NewRpgAction.cpp:739-741 `bot->Dismount()`): on this core Unit::Dismount() only
    // clears the mount display and UNIT_FLAG_MOUNT and leaves the SPELL_AURA_MOUNTED aura (Unit.cpp Unit::Dismount;
    // #134), and ActivateTaxiPathTo with a flight master does not remove it (Player.cpp, "taximaster case"). AC's
    // Dismount() drops the aura. Dismount the way CheckMountStateAction does (CMSG_CANCEL_MOUNT_AURA), as on AC.
    if (bot->IsMounted())
    {
        WorldPacket emptyPacket;
        bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    }

    // Bots have not discovered taxi nodes the way a player would, so learn them from the flight
    // master standing here first (this is what the client does on gossip).
    bot->GetSession()->SendLearnNewTaxiNode(flightMaster);

    if (!bot->ActivateTaxiPathTo(nodes, flightMaster, 0))
    {
        SF_LOG_DEBUG("playerbots", "[New RPG] %s activate taxi path %u (from %u to %u) failed", bot->GetName().c_str(),
                     flightMaster->GetEntry(), nodes.empty() ? 0 : nodes.front(), nodes.empty() ? 0 : nodes.back());
        info.ChangeToIdle();
        return true;
    }

    // Upstream logs only the failure; log the success too, so "bots actually take flights" is
    // observable in the server log rather than inferred from the absence of errors.
    SF_LOG_DEBUG("playerbots", "[New RPG] %s boarded taxi at %u: node %u -> %u (%u hops)",
                 bot->GetName().c_str(), flightMaster->GetEntry(), nodes.empty() ? 0 : nodes.front(),
                 nodes.empty() ? 0 : nodes.back(), uint32(nodes.size()));

    return true;
}

void NewRpgTravelFlightAction::ContinueCrossMapTaxi()
{
    if (bot->IsBeingTeleported())
        return;

    if (!bot->movespline->Finalized())
        return;

    MotionMaster* mm = bot->GetMotionMaster();
    if (!mm || mm->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE)
        return;

    uint32 nextDest = bot->m_taxi.GetTaxiDestination();
    if (!nextDest)
        return;

    // Only act when the next node is on a different map - otherwise the flight generator handles
    // it on its own.
    TaxiNodesEntry const* nextNode = sTaxiNodesStore.LookupEntry(nextDest);
    if (!nextNode || nextNode->map_id == bot->GetMapId())
        return;

    FlightPathMovementGenerator* flight = dynamic_cast<FlightPathMovementGenerator*>(mm->top());
    if (!flight)
        return;

    SF_LOG_DEBUG("playerbots", "[New RPG] %s continuing taxi across map boundary (next node %u on map %u)",
                 bot->GetName().c_str(), nextDest, nextNode->map_id);

    flight->SetCurrentNodeAfterTeleport();

    if (flight->HasArrived())
        return;

    // LoA's TaxiPathNodeList is an indexed Path<> whose element type is TaxiPathNodeEntry const.
    TaxiPathNodeEntry const& node = flight->GetPath()[flight->GetCurrentNode()];
    flight->SkipCurrentNode();

    // #707 (owner log 2026-10-06: Rut'theran -> Exodar, path 2476 crosses map 1 -> 530 at node 6; the bot was left at
    // node 6 in the Veiled Sea for 65 min). AC's worldport ack resumes the SAME flight generator
    // (azerothcore-wotlk Handlers/MovementHandler.cpp:218-224, top()->Initialize), so the flight continues from the node
    // after this one. LoA's ack instead rebuilt the flight from PlayerTaxi's last node index, so this code had to point
    // that index at the node teleported to.
    // PORT: SkyFire's PlayerTaxi has no last node index, and its worldport ack (MovementHandler.cpp, "flight fast
    // teleport case") resumes the same generator like AC, so nothing is set here.

    // Upstream passes TELE_TO_NOT_LEAVE_TAXI because AC's TeleportTo clears the taxi
    // destinations. SkyFire's TeleportTo does not touch taxi state (it has no such flag), and its worldport ack resumes
    // the flight, so no flag is needed here.
    bot->TeleportTo(nextNode->map_id, node.x, node.y, node.z, bot->GetOrientation());
}
