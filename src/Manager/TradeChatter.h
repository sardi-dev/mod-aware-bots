#ifndef AWARE_BOTS_TRADE_CHATTER_H
#define AWARE_BOTS_TRADE_CHATTER_H

class Player;
class PlayerbotAI;
namespace TradeChatter
{
    void Login(Player* player);
    void Logout(Player* player);
    void Update(PlayerbotAI* ai, Player* bot);
}
#endif
