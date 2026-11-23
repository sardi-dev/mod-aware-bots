/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * SkyFire has no Tol Barad battlefield (BattlefieldTB). The Tol Barad tactics compile their battlefield-specific code
 * only with BOT_HAVE_BF_TB; without it a bot is never in that war, while Wintergrasp keeps working (docs/stubs.md).
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BATTLEFIELDTB_H
#define MOD_AWARE_BOTS_COMPAT_BATTLEFIELDTB_H

#include "Define.h"

constexpr uint32 BATTLEFIELD_TB_MAPID = 732;  // Tol Barad (Map.dbc)

#endif
