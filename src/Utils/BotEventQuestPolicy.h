#ifndef BOT_EVENT_QUEST_POLICY_H
#define BOT_EVENT_QUEST_POLICY_H

namespace BotEventQuestPolicy
{
// BT-152: the caller supplies the core's cooldown result; unknown daily state fails closed.
inline bool Allowed(bool seasonal, bool active, bool repeatable, bool daily, bool weekly, bool cooldownClear)
{
    if (seasonal)
        return active && (!repeatable || daily || weekly) && (!(daily || weekly) || cooldownClear);
    return !repeatable && !daily && !weekly;
}
}
#endif
