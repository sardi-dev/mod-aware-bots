#ifndef MOD_AWARE_BOTS_STALECOMBAT_H
#define MOD_AWARE_BOTS_STALECOMBAT_H

class Player;

// BT-168 (policy: StaleCombatPolicy.h): true when `member` is flagged in combat but nothing has attacked it for a
// while. Every caller of one thread shares the clock per member, so the group's ready test and the dungeon drink
// agree. Call it only for a member that is flagged (it reads the member's attackers).
namespace StaleCombat
{
bool IsStale(Player* member);

// BT-179: a living creature with `member` as its victim that `member` can see. Only such an attacker stops the quiet
// clock: live 2026-11-13 (Deadmines run 4) a Defias Envoker, which stops on aggro and casts only at what it sees,
// stood out of the tank's sight as its "attacker" and held the tank flagged for 116 s with the clock never started.
// A creature the bot cannot select (a hostile stalker that swings at the group) does not count either (BT-376).
// The defend and rescue paths read the attacker lists themselves and still go for such an attacker.
bool AttackedInSight(Player* member);
}

#endif
