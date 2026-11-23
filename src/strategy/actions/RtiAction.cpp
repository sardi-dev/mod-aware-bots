#include "RtiAction.h"

#include "Event.h"
#include "ObjectAccessor.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"

bool RtiAction::Execute(Event event)
{
    std::string text = event.getParam();
    std::string type = "rti";
    if (text.find("cc ") == 0)
    {
        type = "rti cc";
        text = text.substr(3);
    }

    if (text.empty() || text == "?")
    {
        std::ostringstream outRti;
        outRti << "rti"
               << ": ";
        AppendRti(outRti, "rti");
        botAI->TellMaster(outRti);

        std::ostringstream outRtiCc;
        outRtiCc << "rti cc"
                 << ": ";
        AppendRti(outRtiCc, "rti cc");
        botAI->TellMaster(outRtiCc);
        return true;
    }

    context->GetValue<std::string>(type)->Set(text);

    std::ostringstream out;
    out << type << " set to: ";
    AppendRti(out, type);
    botAI->TellMaster(out);
    return true;
}

void RtiAction::AppendRti(std::ostringstream& out, std::string const type)
{
    out << AI_VALUE(std::string, type);

    std::ostringstream n;
    n << type << " target";

    if (Unit* target = AI_VALUE(Unit*, n.str()))
        out << " (" << target->GetName() << ")";
}

bool MarkRtiAction::Execute(Event event)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (bot->InBattleground())
        return false;

    // From the author's earlier private bot module: with a player as master only the main tank marks, so several tank
    // bots never fight over the skull.
    if (botAI->HasGameClientMaster() && !PlayerBotSpec::IsMainTank(bot))
        return false;

    Unit* target = nullptr;
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (BotGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        // do not mark players
        if ((unit->GetTypeId() == TypeID::TYPEID_PLAYER))
            continue;

        bool marked = false;
        for (uint8 i = 0; i < 8; i++)
        {
            BotGuid iconGUID = group->GetTargetIcon(i);
            if (iconGUID == unit->GetGUID())
            {
                marked = true;
                break;
            }
        }

        if (marked)
            continue;

        if (!target || target->GetHealth() > unit->GetHealth())
            target = unit;
    }

    if (!target)
        return false;

    std::string const rti = AI_VALUE(std::string, "rti");
    uint8 index = RtiTargetValue::GetRtiIndex(rti);

    // From the author's earlier private bot module: with a player as master never move an icon that still sits on a
    // living unit (a player's mark or an encounter's), even one this bot cannot see or reach.
    if (botAI->HasGameClientMaster())
    {
        Unit* markedUnit = ObjectAccessor::GetUnit(*bot, group->GetTargetIcon(index));
        if (markedUnit && markedUnit->IsAlive())
            return false;
    }

    group->SetTargetIcon(index, bot->GetGUID(), target->GetGUID(), 0);
    return true;
}
