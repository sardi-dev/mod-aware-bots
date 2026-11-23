#ifndef PLAYERBOT_DPS_THREAT_POLICY_H
#define PLAYERBOT_DPS_THREAT_POLICY_H

#include <string>

namespace DpsThreatPolicy
{
// BT-161: utility spells must remain available when damage is held for the tank.
constexpr unsigned SingleLimit = 80;
constexpr unsigned AreaLimit = 50;
inline bool IsInterruptOrControl(std::string const& spell)
{
    return spell == "kick" || spell == "pummel" || spell == "rebuke" || spell == "wind shear" ||
        spell == "counterspell" || spell == "mind freeze" || spell == "strangulate" ||
        spell == "skull bash" || spell == "silence" || spell == "counter shot" ||
        spell == "silencing shot" || spell == "spear hand strike" || spell == "hammer of justice" ||
        spell == "solar beam" || spell == "spell lock";
}
inline bool Block(bool exempt, bool victimIsBot, bool area, unsigned singleThreat, unsigned areaThreat)
{
    return !exempt && !victimIsBot && (singleThreat >= SingleLimit || (area && areaThreat >= AreaLimit));
}
}
#endif
