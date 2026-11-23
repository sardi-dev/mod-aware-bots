#ifndef _PLAYERBOT_BOTFACTORY_H
#define _PLAYERBOT_BOTFACTORY_H

#include "Player.h"
#include "PlayerbotAI.h"

class BotFactory
{
public:
    BotFactory(Player* bot, uint32 level, uint32 itemQuality = 0, uint32 gearScoreLimit = 0);

    static BotGuid GetRandomBot();
    // AC PlayerbotFactory::CalcMixedGearScore (Bot/Factory/PlayerbotFactory.cpp:3912)
    static uint32 CalcMixedGearScore(uint32 gs, uint32 quality);
    static void Init();
    void Refresh();
    void Randomize(bool incremental);
    void ClearEverything();

    void InitEquipment(bool incremental, bool second_chance = false);
    /// BT-421: a hunter fights with a bow, gun or crossbow in the main hand; give it the best one it can wear when it
    /// holds anything else (a dagger picked up as a bad-equip into an empty hand, an old pass that kept a melee weapon).
    void EnsureHunterRangedWeapon();
    void InitBags(bool destroyOld = true);
    void InitPet();
    void InitTalentsTree(bool reset);
    /// Change a bot that already has a specialization to another one (ChrSpecialization TabPage, 0-3), the way a player
    /// does and the way the bot picked its first spec: reset the talents, set the spec through the core's handler, learn
    /// its spells, then talents, glyphs, gear and the strategies for the new role. World thread, bot in the world.
    static void SwitchSpecialization(Player* bot, uint32 tab);
    /// Design decision 0020: an alt bot picks its specialization at level 10 and a talent in each unlocked tier, as a player
    /// does in the talent pane. Unlike InitTalentsTree it learns no other spells: alts buy those at a trainer.
    void InitAltSpecAndTalents();
    /// Design decision 0020/0021: learn the glyph items in the bags and fill empty unlocked slots with known glyphs.
    void InitAltGlyphs();
    /// AiPlayerbot.AltBotsPickTalents (default 1).
    static bool AltBotsPickTalents();
    /// #734: learn one talent per unlocked 5.4.8 tier (AiPlayerbot.PremadeSpecLink); idempotent.
    void InitTalentTiers();
    /// #734: AC PlayerbotFactory::InitGlyphs - fill the unlocked glyph slots (AiPlayerbot.PremadeSpecGlyph).
    void InitGlyphs(bool increment = false);
    /// BT-320: the index of the specialization of talent group `group` in the AiPlayerbot.PremadeSpec* arrays
    /// (RandomClassSpecIndex applied), or MAX_SPECIALIZATIONS when that group has no specialization.
    static uint32 PremadeSpecIndexOf(Player* bot, uint8 group);
    /// BT-323: the ChrSpecialization TabPage of talent group `group`, or MAX_SPECIALIZATIONS.
    static uint32 SpecTabOf(Player* bot, uint8 group);
    /// BT-320: what a glyph item puts in a slot, for a bot of class `cls`: the GlyphProperties id, the item's apply
    /// spell, the slot type (0 major, 1 minor) and who may use it. False for a retired or foreign item.
    struct GlyphItem
    {
        uint32 glyphId = 0;
        uint32 applySpell = 0;
        uint32 typeFlags = 0;
        uint32 requiredLevel = 0;
        int32 allowableRace = -1;
    };
    static bool ResolveGlyph(uint32 itemId, uint8 cls, GlyphItem& out);
    /// BT-321: puts `glyphId` (0 clears) in `slot` of the active talent group as the glyph spell does: Player::SetGlyph
    /// only stores the id, so the old glyph's aura is removed and the new one cast here (in the world only; at login
    /// Player::_LoadGlyphAuras casts them).
    static void PutGlyph(Player* bot, uint8 slot, uint32 glyphId);
    void InitClassSpells();
    /// issue #62: give the bot a ground mount so the mount strategy has something to cast.
    void InitMountSpells();
    /// #745: AC PlayerbotFactory::InitSkills / InitTradeSkills (Bot/Factory/PlayerbotFactory.cpp:3275 / 2933): riding,
    /// profession pair + secondary skills, gathering tools. Public like AC's: the level-up maintenance action calls InitSkills.
    void InitSkills();
    void InitTradeSkills();
    /// BT-197: whether a random bot may learn this trade skill trainer spell (its chosen profession pair or a secondary
    /// skill). Non-random bots and other trainers: true. The bot pays at the trainer (BotTraining::LearnAffordable).
    static bool MayLearnTrainerSpell(Player* bot, TrainerSpellData const* trainer, TrainerSpell const* trainerSpell);
    /// #534: AC PlayerbotFactory::InitFood / InitReagents / InitPotions / InitConsumables / CleanupConsumables
    /// (Bot/Factory/PlayerbotFactory.cpp:4164 / 4229 / 4084 / 1091 / 4351). Public like AC's: the level-up maintenance
    /// action (AutoMaintenanceOnLevelupAction::AutoUpgradeEquip, AC :160-177) calls them too.
    void InitFood();
    void InitReagents();
    void InitPotions();
    void InitConsumables();
    void CleanupConsumables();
private:
    void PickSpecialization();
    void Prepare();
    void CancelAuras();

    // -- GEARING
    bool CanEquipItem(ItemTemplate const* proto);
    bool CanEquipUnseenItem(uint8 slot, uint16& dest, uint32 item);
    std::vector<InventoryType> GetPossibleInventoryTypeListBySlot(EquipmentSlots slot);

    uint32 level;
    uint32 itemQuality;
    uint32 gearScoreLimit;
protected:
    Player* bot;
    PlayerbotAI* botAI;
};

#endif