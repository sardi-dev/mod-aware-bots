#include "Formations.h"

#include "Arrow.h"
#include "Config.h"
#include "Event.h"
#include "Map.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "ServerFacade.h"
#include "TrailMath.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace
{
// Trail follow (BT-115): in a dungeon behind a real player the bots walk where the leader walked, one behind the
// other with a little spread, instead of standing in a ring 1.5 yards around the leader (some of them ahead of
// the leader, next to the pack the leader is looking at).
using TrailPoint = TrailMath::Point;

constexpr float TRAIL_STEP = 2.0f;            // a new point when the leader moved this far
constexpr float TRAIL_RESET_DISTANCE = 6.0f;  // a gap (a teleport, a jump down): start again
constexpr float TRAIL_RESET_HEIGHT = 3.0f;    // the same for another floor, and for a place off the trail's level
constexpr size_t TRAIL_CATCH_UP_POINTS = 3;   // a bot this many points behind its place walks the trail itself
constexpr size_t TRAIL_CATCH_UP_STEP = 2;     // towards the point this many ahead of the one nearest to it
constexpr size_t TRAIL_MAX_POINTS = 16;       // 32 yards, three times the last slot
constexpr float TRAIL_FIRST_SLOT = 3.0f;      // the first bot walks this far behind the leader
constexpr float TRAIL_SLOT_SPACING = 2.0f;
constexpr float TRAIL_SIDE_SPREAD = 1.2f;     // a bot's fixed step to the left or right of the trail
constexpr float TRAIL_TOLERANCE = 1.5f;       // a bot starts walking when its place is this far away
constexpr float TRAIL_REAR_ARC_STEP = 0.45f;  // radians between the bots while the leader has not walked yet
constexpr uint32 TRAIL_HASH = 2654435761u;

bool TrailEnabled()
{
    // Read once on first use, like the other local settings (MarkTriggers.cpp).
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.DungeonTrailFollow", true);
    return enabled;
}

// A fixed value in [0, 1) per bot: the same bot keeps its side and its distance for the whole run.
float BotFraction(Player* bot, uint32 shift)
{
    return float(((bot->GetGUIDLow() * TRAIL_HASH) >> shift) % 1000) / 1000.0f;
}

uint8 TrailRole(Player* member)
{
    if (PlayerBotSpec::IsTank(member))
        return 0;
    if (PlayerBotSpec::IsHeal(member))
        return 3;
    return PlayerBotSpec::IsRanged(member) ? 2 : 1;
}

// The bot's place in the line and the length of the line: tanks first, healers last.
void TrailSlot(Player* bot, Player* master, uint32& slot, uint32& count)
{
    slot = 0;
    count = 1;
    Group* group = bot->GetGroup();
    if (!group)
        return;

    std::vector<Player*> line;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != master && member->FindMap() == bot->FindMap())
            line.push_back(member);
    }
    std::sort(line.begin(), line.end(), [](Player* a, Player* b)
    {
        uint8 const roleA = TrailRole(a);
        uint8 const roleB = TrailRole(b);
        return roleA != roleB ? roleA < roleB : a->GetGUIDLow() < b->GetGUIDLow();
    });

    auto const itr = std::find(line.begin(), line.end(), bot);
    if (itr == line.end())
        return;

    slot = uint32(std::distance(line.begin(), itr));
    count = uint32(line.size());
}
}  // namespace

WorldLocation Formation::NullLocation = WorldLocation();

bool IsSameLocation(WorldLocation const& a, WorldLocation const& b)
{
    return a.GetPositionX() == b.GetPositionX() && a.GetPositionY() == b.GetPositionY() &&
           a.GetPositionZ() == b.GetPositionZ() && a.GetMapId() == b.GetMapId();
}

bool Formation::IsNullLocation(WorldLocation const& loc) { return IsSameLocation(loc, Formation::NullLocation); }

WorldLocation MoveAheadFormation::GetLocation()
{
    Player* master = GetMaster();
    if (!master || master == bot)
        return WorldLocation();

    WorldLocation loc = GetLocationInternal();
    if (Formation::IsNullLocation(loc))
        return loc;

    float x = loc.GetPositionX();
    float y = loc.GetPositionY();
    float z = loc.GetPositionZ();

    return WorldLocation(master->GetMapId(), x, y, z);
}

class MeleeFormation : public FollowFormation
{
public:
    MeleeFormation(PlayerbotAI* botAI) : FollowFormation(botAI, "melee") {}

    std::string const GetTargetName() override { return "master target"; }
};

class QueueFormation : public FollowFormation
{
public:
    QueueFormation(PlayerbotAI* botAI) : FollowFormation(botAI, "queue") {}

    std::string const GetTargetName() override { return "line target"; }
};

class NearFormation : public MoveAheadFormation
{
public:
    NearFormation(PlayerbotAI* botAI) : MoveAheadFormation(botAI, "near") {}

    WorldLocation GetLocationInternal() override
    {
        Player* master = GetMaster();
        if (!master)
            return WorldLocation();
        float range = sPlayerbotAIConfig->followDistance;
        float angle = GetFollowAngle();
        float x = master->GetPositionX() + cos(angle) * range;
        float y = master->GetPositionY() + sin(angle) * range;
        float z = master->GetPositionZ() + BotGetHoverHeight(master);
        if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), master, master->GetPositionX(), master->GetPositionY(),
                                                               master->GetPositionZ(), x, y, z))
        {
            x = master->GetPositionX() + cos(angle) * range;
            y = master->GetPositionY() + sin(angle) * range;
            z = master->GetPositionZ() + BotGetHoverHeight(master);
            master->UpdateAllowedPositionZ(x, y, z);
        }
        return WorldLocation(master->GetMapId(), x, y, z);
    }

    float GetMaxDistance() override { return sPlayerbotAIConfig->followDistance; }
};

class ChaosFormation : public MoveAheadFormation
{
public:
    ChaosFormation(PlayerbotAI* botAI) : MoveAheadFormation(botAI, "chaos"), lastChangeTime(0) {}

    WorldLocation GetLocationInternal() override
    {
        Player* master = GetMaster();
        if (!master)
            return WorldLocation();

        trailActive = UsesTrail(master);
        if (trailActive)
            return TrailLocation(master);

        // A fight or a ride ends the trail: the leader may come back to the same place on another way.
        trail.clear();

        float range = sPlayerbotAIConfig->followDistance;
        float angle = GetFollowAngle();

        time_t now = time(nullptr);
        if (!lastChangeTime || now - lastChangeTime >= 3)
        {
            Player* master = GetMaster();
            if (!master)
                return WorldLocation();

            float range = sPlayerbotAIConfig->followDistance;
            float angle = GetFollowAngle();

            time_t now = time(nullptr);
            if (!lastChangeTime || now - lastChangeTime >= 3)
            {
                lastChangeTime = now;
                dx = (urand(0, 10) / 10.0 - 0.5) * sPlayerbotAIConfig->tooCloseDistance;
                dy = (urand(0, 10) / 10.0 - 0.5) * sPlayerbotAIConfig->tooCloseDistance;
                dr = sqrt(dx * dx + dy * dy);
            }

            float x = master->GetPositionX() + cos(angle) * range + dx;
            float y = master->GetPositionY() + sin(angle) * range + dy;
            float z = master->GetPositionZ() + BotGetHoverHeight(master);
            if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), 
                    master, master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), x, y, z))
            {
                x = master->GetPositionX() + cos(angle) * range + dx;
                y = master->GetPositionY() + sin(angle) * range + dy;
                z = master->GetPositionZ() + BotGetHoverHeight(master);
                master->UpdateAllowedPositionZ(x, y, z);
            }
            // bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
            // bot->GetPositionZ(), x, y, z);
            return WorldLocation(master->GetMapId(), x, y, z);
        }

        float x = master->GetPositionX() + cos(angle) * range + dx;
        float y = master->GetPositionY() + sin(angle) * range + dy;
        float z = master->GetPositionZ() + BotGetHoverHeight(master);
        if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), master, master->GetPositionX(), master->GetPositionY(),
                                                               master->GetPositionZ(), x, y, z))
        {
            x = master->GetPositionX() + cos(angle) * range + dx;
            y = master->GetPositionY() + sin(angle) * range + dy;
            z = master->GetPositionZ() + BotGetHoverHeight(master);
            master->UpdateAllowedPositionZ(x, y, z);
        }
        return WorldLocation(master->GetMapId(), x, y, z);
    }

    float GetMaxDistance() override
    {
        // On the trail every bot has its own tolerance, so the line does not start and stop in one instant.
        if (trailActive)
            return TRAIL_TOLERANCE + BotFraction(bot, 4);

        return sPlayerbotAIConfig->followDistance + dr;
    }

private:
    bool UsesTrail(Player* master)
    {
        if (!TrailEnabled() || master == bot || GET_PLAYERBOT_AI(master) || bot->IsInCombat() || !bot->IsAlive() ||
            !master->IsAlive() || master->IsInWater() || bot->GetTransport() || master->GetTransport())
            return false;

        Map* map = bot->FindMap();
        return map && map == master->FindMap() && map->IsDungeon() && !map->IsRaid();
    }

    WorldLocation TrailLocation(Player* master)
    {
        TrailPoint const head = { master->GetPositionX(), master->GetPositionY(), master->GetPositionZ() };
        if (trailMaster != master->GetGUIDLow() || trailInstance != master->GetInstanceId() ||
            trailMap != master->GetMapId())
        {
            trail.clear();
            trailMaster = master->GetGUIDLow();
            trailInstance = master->GetInstanceId();
            trailMap = master->GetMapId();
        }

        // The fan keeps the direction the leader had when the trail began, or it would swing around a leader who
        // only turns to look.
        if (TrailMath::Record(trail, head, { TRAIL_STEP, TRAIL_RESET_DISTANCE, TRAIL_RESET_HEIGHT, TRAIL_MAX_POINTS }))
            fanOrientation = master->GetOrientation();

        uint32 slot = 0;
        uint32 count = 1;
        TrailSlot(bot, master, slot, count);
        float const behind = TRAIL_FIRST_SLOT + slot * TRAIL_SLOT_SPACING + BotFraction(bot, 12);
        float const side = (BotFraction(bot, 20) * 2.0f - 1.0f) * TRAIL_SIDE_SPREAD;

        // The bot's place along the trail (TrailMath.h). A bot far behind its place walks the trail point by point, as
        // the leader did, and does not take the shortest way to its place (around a corner that can lead through a pack).
        TrailMath::Place const place = TrailMath::Find(trail, head, behind, bot->GetPositionX(), bot->GetPositionY(),
                                                       TRAIL_CATCH_UP_POINTS, TRAIL_CATCH_UP_STEP);
        if (place.kind == TrailMath::Place::CATCH_UP)
            return WorldLocation(master->GetMapId(), place.point.x, place.point.y, place.point.z);

        if (place.kind == TrailMath::Place::SPOT)
        {
            // The step to the side, unless a wall or a ledge is in the way: then the bot stays on the trail.
            TrailPoint const& spot = place.point;
            float x = spot.x + place.sideX * side;
            float y = spot.y + place.sideY * side;
            float z = spot.z;
            if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), master, spot.x, spot.y, spot.z, x, y, z) ||
                std::fabs(z - spot.z) > TRAIL_RESET_HEIGHT)
            {
                x = spot.x;
                y = spot.y;
                z = spot.z;
            }
            return WorldLocation(master->GetMapId(), x, y, z);
        }

        // The leader has not walked far enough yet (the entrance, or after a fight): a fan behind the leader. When the
        // bot's own place is behind a wall or down a ledge it tries the place straight behind the leader, then the
        // mirrored one, then the same three at half the distance, before it gives up for the leader's own position
        // (BT-121: the tank of a finder group stood on the leader at the Shadowfang Keep entrance).
        float const offset = (float(slot) - float(count - 1) / 2.0f) * TRAIL_REAR_ARC_STEP;
        float const offsets[] = { offset, 0.0f, -offset };
        float const distances[] = { behind, behind * 0.5f };
        for (float distance : distances)
        {
            for (float tryOffset : offsets)
            {
                float const angle = fanOrientation + float(M_PI) + tryOffset;
                float x = head.x + std::cos(angle) * distance;
                float y = head.y + std::sin(angle) * distance;
                float z = head.z;
                if (BotCheckCollisionAndGetValidCoords(master->GetMap(), master, head.x, head.y, head.z, x, y, z) &&
                    std::fabs(z - head.z) <= TRAIL_RESET_HEIGHT)
                    return WorldLocation(master->GetMapId(), x, y, z);
            }
        }

        return WorldLocation(master->GetMapId(), head.x, head.y, head.z);
    }

    float fanOrientation = 0.0f;
    std::deque<TrailPoint> trail;
    uint32 trailMaster = 0;
    uint32 trailInstance = 0;
    uint32 trailMap = 0;
    bool trailActive = false;
    time_t lastChangeTime;
    float dx = 0.f;
    float dy = 0.f;
    float dr = 0.f;
};

class CircleFormation : public MoveFormation
{
public:
    CircleFormation(PlayerbotAI* botAI) : MoveFormation(botAI, "circle") {}

    WorldLocation GetLocation() override
    {
        float range = 2.0f;

        Unit* target = AI_VALUE(Unit*, "current target");
        Player* master = GetMaster();
        if (!target && target != bot)
            target = master;

        if (!target)
            return Formation::NullLocation;

        switch (bot->getClass())
        {
            case CLASS_HUNTER:
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                range = botAI->GetRange("flee");
                break;
            case CLASS_DRUID:
                if (!PlayerBotSpec::IsTank(bot))
                    range = botAI->GetRange("flee");
                break;
            case CLASS_SHAMAN:
                if (PlayerBotSpec::IsHeal(bot))
                    range = botAI->GetRange("flee");
                break;
        }

        float angle = GetFollowAngle();
        float x = target->GetPositionX() + cos(angle) * range;
        float y = target->GetPositionY() + sin(angle) * range;
        float z = target->GetPositionZ();
        if (!BotCheckCollisionAndGetValidCoords(target->GetMap(), target, target->GetPositionX(), target->GetPositionY(),
                                                               target->GetPositionZ(), x, y, z))
        {
            x = target->GetPositionX() + cos(angle) * range;
            y = target->GetPositionY() + sin(angle) * range;
            z = target->GetPositionZ();
            target->UpdateAllowedPositionZ(x, y, z);
        }
        return WorldLocation(bot->GetMapId(), x, y, z);
    }
};

class LineFormation : public MoveAheadFormation
{
public:
    LineFormation(PlayerbotAI* botAI) : MoveAheadFormation(botAI, "line") {}

    WorldLocation GetLocationInternal() override
    {
        Group* group = bot->GetGroup();
        if (!group)
            return Formation::NullLocation;

        float range = 2.0f;

        Player* master = GetMaster();
        if (!master)
            return Formation::NullLocation;

        float x = master->GetPositionX();
        float y = master->GetPositionY();
        float z = master->GetPositionZ();
        float orientation = master->GetOrientation();

        std::vector<Player*> players;
        GroupReference* gref = group->GetFirstMember();
        while (gref)
        {
            Player* member = gref->GetSource();
            if (member != master)
                players.push_back(member);

            gref = gref->next();
        }

        players.insert(players.begin() + group->GetMembersCount() / 2, master);

        return MoveLine(players, 0.0f, x, y, z, orientation, range);
    }
};

class ShieldFormation : public MoveFormation
{
public:
    ShieldFormation(PlayerbotAI* botAI) : MoveFormation(botAI, "shield") {}

    WorldLocation GetLocation() override
    {
        Group* group = bot->GetGroup();
        if (!group)
            return Formation::NullLocation;

        float range = sPlayerbotAIConfig->followDistance;

        Player* master = GetMaster();
        if (!master)
            return Formation::NullLocation;

        float x = master->GetPositionX();
        float y = master->GetPositionY();
        float z = master->GetPositionZ();
        float orientation = master->GetOrientation();

        std::vector<Player*> tanks;
        std::vector<Player*> dps;
        GroupReference* gref = group->GetFirstMember();
        while (gref)
        {
            Player* member = gref->GetSource();
            if (member != master)
            {
                if (PlayerBotSpec::IsTank(member))
                    tanks.push_back(member);
                else
                    dps.push_back(member);
            }

            gref = gref->next();
        }

        if (PlayerBotSpec::IsTank(master))
            tanks.insert(tanks.begin() + (tanks.size() + 1) / 2, master);
        else
            dps.insert(dps.begin() + (dps.size() + 1) / 2, master);

        if (PlayerBotSpec::IsTank(bot) && PlayerBotSpec::IsTank(master))
        {
            return MoveLine(tanks, 0.0f, x, y, z, orientation, range);
        }

        if (!PlayerBotSpec::IsTank(bot) && !PlayerBotSpec::IsTank(master))
        {
            return MoveLine(dps, 0.0f, x, y, z, orientation, range);
        }

        if (PlayerBotSpec::IsTank(bot) && !PlayerBotSpec::IsTank(master))
        {
            float diff = tanks.size() % 2 == 0 ? -sPlayerbotAIConfig->tooCloseDistance / 2.0f : 0.0f;
            return MoveLine(tanks, diff, x + cos(orientation) * range, y + sin(orientation) * range, z, orientation,
                            range);
        }

        if (!PlayerBotSpec::IsTank(bot) && PlayerBotSpec::IsTank(master))
        {
            float diff = dps.size() % 2 == 0 ? -sPlayerbotAIConfig->tooCloseDistance / 2.0f : 0.0f;
            return MoveLine(dps, diff, x - cos(orientation) * range, y - sin(orientation) * range, z, orientation,
                            range);
        }

        return Formation::NullLocation;
    }
};

class FarFormation : public FollowFormation
{
public:
    FarFormation(PlayerbotAI* botAI) : FollowFormation(botAI, "far") {}

    WorldLocation GetLocation() override
    {
        float range = sPlayerbotAIConfig->farDistance;
        float followRange = sPlayerbotAIConfig->followDistance;

        Player* master = GetMaster();
        if (!master)
            return Formation::NullLocation;

        if (sServerFacade->GetDistance2d(bot, master) <= range)
            return Formation::NullLocation;

        float angle = master->GetAngle(bot);
        float followAngle = GetFollowAngle();

        float x = master->GetPositionX() + cos(angle) * range + cos(followAngle) * followRange;
        float y = master->GetPositionY() + sin(angle) * range + sin(followAngle) * followRange;
        float z = master->GetPositionZ();

        float ground = BotGetMapHeight(master, x, y, z + 30.0f);
        if (ground <= INVALID_HEIGHT)
        {
            float minDist = 0, minX = 0, minY = 0;
            for (float angle = 0.0f; angle <= 2 * M_PI; angle += M_PI / 16.0f)
            {
                x = master->GetPositionX() + cos(angle) * range + cos(followAngle) * followRange;
                y = master->GetPositionY() + sin(angle) * range + sin(followAngle) * followRange;
                float dist = sServerFacade->GetDistance2d(bot, x, y);
                float ground = BotGetMapHeight(master, x, y, z + 30.0f);
                if (ground > INVALID_HEIGHT && (!minDist || minDist > dist))
                {
                    minDist = dist;
                    minX = x;
                    minY = y;
                }
            }

            if (minDist)
            {
                if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), 
                        master, master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), x, y, z))
                {
                    x = master->GetPositionX() + cos(angle) * range + cos(followAngle) * followRange;
                    y = master->GetPositionY() + sin(angle) * range + sin(followAngle) * followRange;
                    z = master->GetPositionZ() + BotGetHoverHeight(master);
                    master->UpdateAllowedPositionZ(x, y, z);
                }
                return WorldLocation(bot->GetMapId(), minX, minY, z);
            }

            return Formation::NullLocation;
        }

        if (!BotCheckCollisionAndGetValidCoords(master->GetMap(), master, master->GetPositionX(), master->GetPositionY(),
                                                               master->GetPositionZ(), x, y, z))
        {
            x = master->GetPositionX() + cos(angle) * range + cos(followAngle) * followRange;
            y = master->GetPositionY() + sin(angle) * range + sin(followAngle) * followRange;
            z = master->GetPositionZ() + BotGetHoverHeight(master);
            master->UpdateAllowedPositionZ(x, y, z);
        }
        return WorldLocation(bot->GetMapId(), x, y, z);
    }
};

float Formation::GetFollowAngle()
{
    Player* master = GetMaster();
    Group* group = bot->GetGroup();
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    // If there's no master and no group
    if (!master && !group)
        return 0.0f;

    uint32 index = 1;
    uint32 total = 1;

    std::vector<Player*> roster;

    if (group)
    {
        bool left = true;  // Used for alternating tanks' positions

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();

            // Skip invalid, dead, or out-of-map members
            if (!member || !member->IsAlive() || bot->GetMapId() != member->GetMapId())
                continue;

            // Skip the master
            if (member == master)
                continue;

            // Put DPS in the middle
            if (!PlayerBotSpec::IsTank(member) && !PlayerBotSpec::IsHeal(member))
            {
                roster.insert(roster.begin() + roster.size() / 2, member);
            }

            // Put Healers in the middle
            else if (PlayerBotSpec::IsHeal(member))
            {
                roster.insert(roster.begin() + roster.size() / 2, member);
            }

            // Handle tanks (alternate between front and back)
            else if (PlayerBotSpec::IsTank(member))
            {
                if (left)
                    roster.push_back(member);  // Place tank at the back
                else
                    roster.insert(roster.begin(), member);  // Place tank at the front

                left = !left;  // Alternate for the next tank
            }

            total++;
        }
    }
    else if (master)
    {
        // If the bot is following a master, look up the bot's position in the master's list
        PlayerbotMgr* masterBotMgr = GET_PLAYERBOT_MGR(master);
        if (masterBotMgr && !GET_PLAYERBOT_AI(master))
        {
            for (auto it = masterBotMgr->GetPlayerBotsBegin(); it != masterBotMgr->GetPlayerBotsEnd(); ++it)
            {
                if (it->second == bot)
                {
                    index = total;  // Found bot in master's list, set the index
                    break;
                }
                ++total;
            }
        }
    }

    // Find the bot's position in the roster
    auto it = std::find(roster.begin(), roster.end(), bot);
    if (it != roster.end())
    {
        index = std::distance(roster.begin(), it) + 1;  // Find bot's index in the roster
    }

    // Return
    float start = (master ? master->GetOrientation() : 0.0f);
    return start + (0.125f + 1.75f * index / total + (total == 2 ? 0.125f : 0.0f)) * M_PI;
}

FormationValue::FormationValue(PlayerbotAI* botAI)
    : ManualSetValue<Formation*>(botAI, new ChaosFormation(botAI), "formation")
{
}

FormationValue::~FormationValue()
{
    if (value)
    {
        delete value;
        value = nullptr;
    }
}

std::string const FormationValue::Save() { return value ? value->getName() : "?"; }

bool FormationValue::Load(std::string const formation)
{
    if (formation == "melee")
    {
        if (value)
            delete value;

        value = new MeleeFormation(botAI);
    }
    else if (formation == "queue")
    {
        if (value)
            delete value;

        value = new QueueFormation(botAI);
    }
    else if (formation == "chaos" || formation == "default")
    {
        if (value)
            delete value;

        value = new ChaosFormation(botAI);
    }
    else if (formation == "circle")
    {
        if (value)
            delete value;

        value = new CircleFormation(botAI);
    }
    else if (formation == "line")
    {
        if (value)
            delete value;

        value = new LineFormation(botAI);
    }
    else if (formation == "shield")
    {
        if (value)
            delete value;

        value = new ShieldFormation(botAI);
    }
    else if (formation == "arrow")
    {
        if (value)
            delete value;

        value = new ArrowFormation(botAI);
    }
    else if (formation == "near")
    {
        if (value)
            delete value;

        value = new NearFormation(botAI);
    }
    else if (formation == "far")
    {
        if (value)
            delete value;

        value = new FarFormation(botAI);
    }
    else
        return false;

    return true;
}

bool SetFormationAction::Execute(Event event)
{
    std::string const formation = event.getParam();

    FormationValue* value = (FormationValue*)context->GetValue<Formation*>("formation");
    if (formation == "?" || formation.empty())
    {
        std::ostringstream str;
        str << "Formation: |cff00ff00" << value->Get()->getName();
        botAI->TellMaster(str);
        return true;
    }

    if (formation == "show")
    {
        WorldLocation loc = value->Get()->GetLocation();
        //if (!Formation::IsNullLocation(loc))
            //botAI->Ping(loc.GetPositionX(), loc.GetPositionY());

        return true;
    }

    if (!value->Load(formation))
    {
        std::ostringstream str;
        str << "Invalid formation: |cffff0000" << formation;
        botAI->TellMaster(str);
        botAI->TellMaster(
            "Please set to any of:|cffffffff chaos (default), near, queue, circle, line, shield, arrow, melee, far");
        return false;
    }

    std::ostringstream str;
    str << "Formation set to: " << formation;
    botAI->TellMaster(str);
    return true;
}

WorldLocation MoveFormation::MoveLine(std::vector<Player*> line, float diff, float cx, float cy, float cz,
                                      float orientation, float range)
{
    if (line.size() < 5)
    {
        return MoveSingleLine(line, diff, cx, cy, cz, orientation, range);
    }

    uint32 lines = ceil((double)line.size() / 5.0);
    for (uint32 i = 0; i < lines; i++)
    {
        float radius = range * i;
        float x = cx + cos(orientation) * radius;
        float y = cy + sin(orientation) * radius;

        std::vector<Player*> singleLine;
        for (uint32 j = 0; j < 5 && !line.empty(); j++)
        {
            singleLine.push_back(line[line.size() - 1]);
            line.pop_back();
        }

        WorldLocation loc = MoveSingleLine(singleLine, diff, x, y, cz, orientation, range);
        if (!Formation::IsNullLocation(loc))
            return loc;
    }

    return Formation::NullLocation;
}

WorldLocation MoveFormation::MoveSingleLine(std::vector<Player*> line, float diff, float cx, float cy, float cz,
                                            float orientation, float range)
{
    float count = line.size();
    float angle = orientation - M_PI / 2.0f;
    float x = cx + cos(angle) * (range * floor(count / 2.0f) + diff);
    float y = cy + sin(angle) * (range * floor(count / 2.0f) + diff);

    uint32 index = 0;
    for (Player* member : line)
    {
        if (member == bot)
        {
            float angle = orientation + M_PI / 2.0f;
            float radius = range * index;

            float lx = x + cos(angle) * radius;
            float ly = y + sin(angle) * radius;
            float lz = cz;

            Player* master = botAI->GetMaster();
            if (!master || !BotCheckCollisionAndGetValidCoords(master->GetMap(), 
                    master, master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), lx, ly, lz))
            {
                lx = x + cos(angle) * radius;
                ly = y + sin(angle) * radius;
                lz = cz;
            }

            return WorldLocation(bot->GetMapId(), lx, ly, lz);
        }

        ++index;
    }

    return Formation::NullLocation;
}
