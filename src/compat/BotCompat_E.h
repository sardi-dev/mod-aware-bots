#ifndef MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_E_H
#define MOD_AWARE_BOTS_COMPAT_BOTCOMPAT_E_H

#include "WorldPacket.h"

// Copy the bytes into a fresh reader: SkyFire copies the source bit cursor as well.
inline WorldPacket BotFreshPacket(WorldPacket const& source)
{
    WorldPacket packet(source.GetOpcode(), source.size());
    if (!source.empty())
        packet.append(source.contents(), source.size());
    return packet;
}

#endif
