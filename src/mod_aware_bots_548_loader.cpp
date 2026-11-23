/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8.
 * Licensed under the GNU General Public License v2; see LICENSE.
 */

void AddSC_mod_playerbots();
void AddSC_aware_ai();  // src/AwareAi/AwareAiScripts.cpp
void AddSC_pvp_awareness();  // src/PvP/PvpAwareness.cpp

// SkyFire calls Add<module folder>Scripts() for every module (modules/CMakeLists.txt).
void Addmod_aware_bots_548Scripts()
{
    AddSC_mod_playerbots();
    AddSC_aware_ai();
    AddSC_pvp_awareness();
}
