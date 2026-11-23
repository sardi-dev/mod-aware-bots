#ifndef _PLAYERBOT_DPSTARGETVALUE_H
#define _PLAYERBOT_DPSTARGETVALUE_H

#include "RtiTargetValue.h"

class PlayerbotAI;

class DpsTargetValue : public RtiTargetValue
{
public:
    DpsTargetValue(PlayerbotAI* botAI, std::string const type = "rti", std::string const name = "dps target")
        : RtiTargetValue(botAI, type, name)
    {
    }

    Unit* Calculate() override;
private:
    uint64 stickyGuid = 0;
    uint32 stickySince = 0;
};

class DpsAoeTargetValue : public RtiTargetValue
{
public:
    DpsAoeTargetValue(PlayerbotAI* botAI, std::string const type = "rti", std::string const name = "dps aoe target")
        : RtiTargetValue(botAI, type, name)
    {
    }

    Unit* Calculate() override;
};

#endif
