/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldStrategy.h"

void RaidUlduarStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Flame Leviathan
    //
    triggers.push_back(new TriggerNode(
        "flame leviathan vehicle near",
        NextAction::array(0, new NextAction("flame leviathan enter vehicle", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "flame leviathan on vehicle",
        NextAction::array(0, new NextAction("flame leviathan vehicle", ACTION_RAID + 1), nullptr)));

    //
    // Razorscale
    //
    triggers.push_back(new TriggerNode(
        "razorscale avoid devouring flames",
        NextAction::array(0, new NextAction("razorscale avoid devouring flames", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale avoid sentinel",
        NextAction::array(0, new NextAction("razorscale avoid sentinel", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale flying alone",
    NextAction::array(0, new NextAction("razorscale ignore flying alone", ACTION_MOVE + 5), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale avoid whirlwind",
        NextAction::array(0, new NextAction("razorscale avoid whirlwind", ACTION_RAID + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale grounded",
        NextAction::array(0, new NextAction("razorscale grounded", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale harpoon trigger",
        NextAction::array(0, new NextAction("razorscale harpoon action", ACTION_MOVE), nullptr)));

    triggers.push_back(new TriggerNode(
        "razorscale fuse armor trigger",
        NextAction::array(0, new NextAction("razorscale fuse armor action", ACTION_RAID + 2), nullptr)));

    //
    // Ignis
    //
    //
    // Iron Assembly
    //
    triggers.push_back(new TriggerNode(
        "iron assembly lightning tendrils trigger",
        NextAction::array(0, new NextAction("iron assembly lightning tendrils action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "iron assembly overload trigger",
        NextAction::array(0, new NextAction("iron assembly overload action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "iron assembly rune of power trigger",
        NextAction::array(0, new NextAction("iron assembly rune of power action", ACTION_RAID), nullptr)));

    //
    // Kologarn
    //
    triggers.push_back(new TriggerNode(
        "kologarn rti target trigger",
        NextAction::array(0, new NextAction("kologarn rti target action", ACTION_RAID + 1), nullptr)));
    triggers.push_back(new TriggerNode(
        "kologarn attack dps target trigger",
        NextAction::array(0, new NextAction("attack rti target", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "kologarn mark dps target trigger",
        NextAction::array(0, new NextAction("kologarn mark dps target action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "kologarn rubble slowdown trigger",
        NextAction::array(0, new NextAction("kologarn rubble slowdown action", ACTION_RAID), nullptr)));
    //
    // Auriaya
    //
    //
    // Hodir
    //
    triggers.push_back(new TriggerNode(
        "hodir near snowpacked icicle",
        NextAction::array(0, new NextAction("hodir move snowpacked icicle", ACTION_RAID + 1), nullptr)));
    //
    // Freya
    //
    triggers.push_back(new TriggerNode(
        "freya near nature bomb",
        NextAction::array(0, new NextAction("freya move away nature bomb", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "freya mark dps target trigger",
        NextAction::array(0, new NextAction("freya mark dps target action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "freya move to healing spore trigger",
        NextAction::array(0, new NextAction("freya move to healing spore action", ACTION_RAID), nullptr)));

    //
    // Thorim
    //
    triggers.push_back(new TriggerNode(
        "thorim mark dps target trigger",
        NextAction::array(0, new NextAction("thorim mark dps target action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "thorim gauntlet positioning trigger",
        NextAction::array(0, new NextAction("thorim gauntlet positioning action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "thorim arena positioning trigger",
        NextAction::array(0, new NextAction("thorim arena positioning action", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode(
        "thorim phase 2 positioning trigger",
        NextAction::array(0, new NextAction("thorim phase 2 positioning action", ACTION_RAID), nullptr)));

    //
    // Mimiron
    //
    triggers.push_back(new TriggerNode(
        "mimiron p3wx2 laser barrage trigger",
        NextAction::array(0, new NextAction("mimiron p3wx2 laser barrage action", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron shock blast trigger",
        NextAction::array(0, new NextAction("mimiron shock blast action", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron phase 1 positioning trigger",
        NextAction::array(0, new NextAction("mimiron phase 1 positioning action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron rapid burst trigger",
        NextAction::array(0, new NextAction("mimiron rapid burst action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron aerial command unit trigger",
        NextAction::array(0, new NextAction("mimiron aerial command unit action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron rocket strike trigger",
        NextAction::array(0, new NextAction("mimiron rocket strike action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "mimiron phase 4 mark dps trigger",
        NextAction::array(0, new NextAction("mimiron phase 4 mark dps action", ACTION_RAID), nullptr)));
    //
    // General Vezax
    //
    triggers.push_back(new TriggerNode(
        "vezax shadow crash trigger",
        NextAction::array(0, new NextAction("vezax shadow crash action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "vezax mark of the faceless trigger",
        NextAction::array(0, new NextAction("vezax mark of the faceless action", ACTION_RAID), nullptr)));

    //
    // Yogg-Saron
    //
    triggers.push_back(new TriggerNode(
        "yogg-saron guardian positioning trigger",
        NextAction::array(0, new NextAction("yogg-saron guardian positioning action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron sanity trigger",
        NextAction::array(0, new NextAction("yogg-saron sanity action", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron death orb trigger",
        NextAction::array(0, new NextAction("yogg-saron death orb action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron malady of the mind trigger",
        NextAction::array(0, new NextAction("yogg-saron malady of the mind action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron mark target trigger",
        NextAction::array(0, new NextAction("yogg-saron mark target action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron brain link trigger",
        NextAction::array(0, new NextAction("yogg-saron brain link action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron move to enter portal trigger",
        NextAction::array(0, new NextAction("yogg-saron move to enter portal action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron use portal trigger",
        NextAction::array(0, new NextAction("yogg-saron use portal action", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode(
        "yogg-saron illusion room trigger",
        NextAction::array(0, new NextAction("yogg-saron illusion room action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron move to exit portal trigger",
        NextAction::array(0, new NextAction("yogg-saron move to exit portal action", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron lunatic gaze trigger",
        NextAction::array(0, new NextAction("yogg-saron lunatic gaze action", ACTION_EMERGENCY), nullptr)));

    triggers.push_back(new TriggerNode(
        "yogg-saron phase 3 positioning trigger",
        NextAction::array(0, new NextAction("yogg-saron phase 3 positioning action", ACTION_RAID), nullptr)));
}
