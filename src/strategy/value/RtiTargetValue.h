#ifndef _PLAYERBOT_RTITARGETVALUE_H
#define _PLAYERBOT_RTITARGETVALUE_H

#include "TargetValue.h"

class PlayerbotAI;
class Unit;

class RtiTargetValue : public TargetValue
{
public:
    RtiTargetValue(PlayerbotAI* botAI, std::string const type = "rti", std::string const name = "rti target")
        : TargetValue(botAI, name), type(type)
    {
    }

    static int32 GetRtiIndex(std::string const rti);
    Unit* Calculate() override;

    // AC Ai/Base/Value/RtiTargetValue.h raid icon indexes, which the raid strategies use
    static int8 const starIndex = 0;
    static int8 const circleIndex = 1;
    static int8 const diamondIndex = 2;
    static int8 const triangleIndex = 3;
    static int8 const moonIndex = 4;
    static int8 const squareIndex = 5;
    static int8 const crossIndex = 6;
    static int8 const skullIndex = 7;

private:
    std::string const type;
};

class RtiCcTargetValue : public RtiTargetValue
{
public:
    RtiCcTargetValue(PlayerbotAI* botAI, std::string const name = "rti cc target")
        : RtiTargetValue(botAI, "rti cc", name)
    {
    }
};

#endif
