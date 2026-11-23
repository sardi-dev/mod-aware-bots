# Stubs and gaps

Places where SkyFire lacks something the module uses. Each is marked `// PORT:` in the code; this file is generated
by `tools/port/stubs.py`, so edit the comments, not this file. A feature counts as working only when its entries
here are resolved or accepted.

| Where | What is missing |
|-------|-----------------|
| `AI/PlayerbotAI.cpp:3154` | SkyFire's CreatureData does not carry its own spawn id, so this always takes LoA's fallback: the first spawn of that creature entry (this method is only used for battlemasters, and there is one of each). |
| `Factory/BotFactory.cpp:972` | SkyFire has no separate current pet id; active slots are searched in slot order. |
| `Factory/BotFactory.cpp:1798` | SkyFire does not load SkillRaceClassInfo; use class-restricted SkillLineAbility entries. The pets' skill lines are class-restricted too; learning them put the pets' passives on the bot (ClassSkillLines.h). |
| `Factory/BotFactory.cpp:2674` | SkyFire has no temporary item GUID option; this probe consumes an item GUID. |
| `Factory/RandomPlayerbotFactory.cpp:364` | SkyFire does not load CharSections.dbc; the module reads it itself (compat/BotCharSections.h). Only options a player can pick are used (SECTION_FLAG_PLAYER), Death Knight ones only for Death Knights, as the client offers. |
| `Factory/RandomPlayerbotFactory.cpp:402` | SkyFire builds and saves a new character only through WorldSession::CreateBotCharacter (CharacterCreateInfo is the session's own), which does what LoA's code did here plus the core's first-login setup and starter pet. |
| `Factory/RandomPlayerbotFactory.cpp:633` | SkyFire requires a unique email shape for every account (LoA passes ""). The reserved .invalid domain never routes anywhere; the random password above still guards the login. |
| `Factory/RandomPlayerbotFactory.cpp:642` | AC waits here for the login database queue (#627). SkyFire's AccountMgr::CreateAccount writes the account row synchronously (DirectExecute), so the lookups below already see the new accounts. |
| `Manager/BattlefieldBots.cpp:244` | SkyFire graveyard locations have no orientation; retain the bot orientation. |
| `Manager/RandomPlayerbotMgr.cpp:1019` | SkyFire has no general movement flag update; teleport sends its movement state. |
| `Manager/RandomPlayerbotMgr.cpp:3501` | SkyFire keeps the spawn store private; enumerate database GUIDs and use its loaded spawn data. |
| `Manager/RandomPlayerbotMgr.cpp:3657` | SkyFire has no separate rated battleground queue; RB is the random battleground queue. |
| `Manager/RandomPlayerbotMgr.cpp:3851` | patches 0017 and 0018 add the Silvershard Mines and Deepwind Gorge battlegrounds, but the random bots are not seeded into their queues yet. |
| `Manager/RandomPlayerbotMgr.cpp:3893` | SkyFire exposes one LFG state and dungeon selection per player, not LoA multi-queue data. |
| `Mgr/Item/StatsWeightCalculator.cpp:279` | SkyFire uses one enchantment pool without a property or suffix type column. |
| `Scenario/BotScenario.cpp:86` | LoA's BattlegroundMgr reads this to pin the random arena for the harness; SkyFire has no such hook, so the `arena pin` verb only records the choice here and the arena stays random. |
| `Scenario/BotScenario.cpp:401` | SkyFire exposes spell cooldown end times without a separate category end. |
| `Scenario/BotScenario.cpp:3756` | SkyFire has no SPLINE_CHAIN or FACE movement generators, so the 100 + value case cannot occur. |
| `Scenario/BotScenario.cpp:3936` | SkyFire has no SPLINE_CHAIN or FACE movement generators (the 100 + value case of the AC numbering). |
| `Scenario/BotScenario.cpp:4687` | SkyFire picks the difficulty from the player |
| `Scenario/BotScenarioDuel.cpp:131` | SkyFire exposes spell cooldown end times without a separate category end. |
| `Scenario/BotScenarioQueue.cpp:126` | SkyFire has no AutoBalance (instance level and player-count scaling); `abinfo` reports it as inactive. |
| `Scenario/BotScenarioQueue.cpp:145` | SkyFire has no rated battlegrounds, so `ratedbg test` only keeps this flag (the core's team size stays 10). |
| `Scenario/BotScenarioQueue.cpp:148` | SkyFire keeps no rated battleground rating row (LoA's RatedPvpMgr), so `ratedbg state` reads 0 for these. |
| `Scenario/BotScenarioQueue.cpp:320` | SkyFire's LFG keeps one active queue per player (no PlayerQueueDataMap), so there is at most one queue to leave, and LeaveLfg takes the player alone. |
| `Scenario/BotScenarioQueue.cpp:810` | SkyFire does not expose the forced threat insertion flags. |
| `Scenario/BotScenarioQueue.cpp:1849` | SkyFire's periodic hook has no SpellInfo argument, so `dot` only gates the call. |
| `Scenario/BotScenarioQueue.cpp:2052` | SkyFire has no rated battlegrounds (no CMSG_BATTLEMASTER_JOIN_RATED handler), so the group cannot be filed; the checks above still tell a test driver why a join would be refused. |
| `Scenario/BotScenarioQueue.cpp:2061` | SkyFire keeps no rated battleground rating row |
| `Scenario/BotScenarioSpellWatch.cpp:153` | stands in for the SpellInfo argument SkyFire's damage and heal hooks lack: the spell the unit is casting. |
| `Scenario/BotScenarioSpellWatch.cpp:173` | SkyFire's hooks cannot report periodic ticks or aura apply and remove; see the header comment. |
| `compat/BattlegroundAnyLevel.h:17` | SkyFire has no Battleground.AnyLevel; Enabled() is false, nothing is scaled, and the bracket is the stock one. |
| `compat/BotCompat_A.h:14` | SkyFire cannot apply graveyard conditions for the requesting player. |
| `compat/BotCompat_T.h:80` | SkyFire's MoveSpline has no IsEnteringTransport; a rider that is still stepping onto the deck is stopped too. |
| `compat/BotCompat_T.h:142` | SkyFire has no GetHitSpherePointFor; the line of sight starts at the unit's eye height above the point. |
| `compat/BotCompat_T.h:151` | SkyFire's MotionMaster cannot push LoA's EffectMovementGenerator from outside, so the spline runs on its own; the unit's movement generator is left idle while it moves. |
| `compat/BotWorld.h:36` | SkyFire has no per-pair combat state; ending a pairing drops the creature side's threat on the other. |
| `strategy/Classes/hunter/HunterActions.cpp:50` | SkyFire does not expose calculated spell costs before casting; use the focus cost from SpellPower.dbc. |
| `strategy/Classes/warrior/WarriorActions.cpp:80` | SkyFire does not expose the far-end polygon or destination Z checks; reject incomplete paths too. |
| `strategy/RPG/NewRpgAction.cpp:750` | SkyFire's PathGenerator has no PATHFIND_FARFROMPOLY flag (LoA marks a start or end far from the mesh), so such a path cannot be reported and only NORMAL and INCOMPLETE are accepted. |
| `strategy/RPG/NewRpgAction.cpp:1497` | SkyFire's PlayerTaxi has no last node index, and its worldport ack (MovementHandler.cpp, "flight fast teleport case") resumes the same generator like AC, so nothing is set here. |
| `strategy/RPG/NewRpgBaseAction.cpp:473` | SkyFire's PathGenerator has no PATHFIND_FARFROMPOLY; a start or end far from the mesh gives NOPATH, SHORTCUT or NOT_USING_PATH there, which stay refused. |
| `strategy/RPG/NewRpgBaseAction.cpp:605` | no PATHFIND_FARFROMPOLY on SkyFire (see MoveFarTo) |
| `strategy/RPG/NewRpgBaseAction.cpp:999` | SkyFire's PlayDistanceSound needs a target and plays to that player only; a bot has no client. |
| `strategy/RPG/NewRpgBaseAction.cpp:1144` | LoA tells scripts about the status change here (ScriptMgr::OnQuestStatusChange); SkyFire has no such hook and its own abandon handler (HandleQuestLogRemoveQuest) notifies nothing, so there is nothing to call. |
| `strategy/actions/AutoMaintenanceOnLevelupAction.cpp:83` | SkyFire does not expose LoA's reward display spell; use its reward spell for the message. |
| `strategy/actions/FishingAction.cpp:79` | SkyFire liquid queries do not accept a phase mask or collision height. |
| `strategy/actions/LootAction.cpp:452` | SkyFire LootItem has no separate currency slots; its loot handler supports item slots. |
| `strategy/actions/MovementActions.cpp:103` | SkyFire liquid queries do not accept a phase mask or collision height. |
| `strategy/actions/MovementActions.cpp:1080` | SkyFire has no collision height per model and BotGetMapWaterOrGroundLevel adds no probe offset of its own, so the whole lift (AC's 2 yards) is added here. |
| `strategy/actions/PetitionSignAction.cpp:130` | SkyFire does not track pending arena team invitations (LoA's Player::GetArenaTeamIdInvited), so a bot never refuses an arena charter for that reason. |
| `strategy/actions/PetsAction.cpp:91` | SkyFire has no OnPetCommand hook to clear LoA's queued pet spell state. |
| `strategy/actions/PetsAction.cpp:201` | SkyFire has no OnPetCommand hook for clearing queued pet spells. |
| `strategy/actions/PetsAction.cpp:259` | SkyFire can save only the pet's current stay position. |
| `strategy/actions/PetsAction.cpp:261` | SkyFire has no OnPetCommand hook for clearing queued pet spells. |
| `strategy/actions/XpGainAction.cpp:45` | SkyFire always writes the first bit as 0 and adds the "xp without bonus" field only with a victim (Player::SendLogXPGain), so an empty victim GUID is what tells a quest or exploration gain apart. |
| `strategy/bg/BattleGroundJoinAction.cpp:540` | LoA refuses 5v5 here while its solo queue owns that bracket; SkyFire has no solo queue. |
| `strategy/bg/BattleGroundJoinAction.cpp:1010` | LoA leaves every dungeon finder queue of the bot here; SkyFire's LFG has one queue per player, left only while it is still a queue (leaving in a dungeon would pull the bot out of its group). |
| `strategy/bg/BgRatedBg.cpp:64` | false until SkyFire can queue rated battlegrounds (see the queueing step below); otherwise bots would keep forming full groups that can never enter the queue. |
| `strategy/bg/BgRatedBg.cpp:72` | SkyFire has no rated BG test mode (LoA's BattlegroundMgr::IsRatedBgTesting), so always the real size. |
| `strategy/bg/BgRatedBg.cpp:283` | SkyFire cannot queue rated battlegrounds yet (no CMSG_BATTLEMASTER_JOIN_RATED handler), so the group never enters the queue and the check below reports it. |
| `strategy/bg/BgTacticsSA.cpp:2124` | SkyFire lacks spline chains; MovePoint follows a generated path to the same target. |
| `strategy/bg/BgTacticsTB.cpp:120` | SkyFire has no Tol Barad battlefield; these helpers need its types 0 none/contested, 1 Horde, 2 Alliance (TB_GO_ARTKIT_*); -1 = pole not found (grid not loaded) |
| `strategy/bg/BgTacticsTB.cpp:367` | no Tol Barad battlefield in SkyFire, so a bot is never in its war |
| `strategy/bg/BgTacticsTOK.cpp:169` | SkyFire VOP does not implement the LoA scoring-zone triggers. |
| `strategy/bg/BgTacticsTOK.cpp:238` | SkyFire keeps VOP orb holders private; use the pickup auras it applies. |
| `strategy/bg/BgTacticsTOK.cpp:296` | SkyFire keeps VOP orb holders private; reconstruct them from pickup auras. |
| `strategy/value/PartyMemberWithoutAuraValue.cpp:52` | SkyFire does not name LoA's consolidated raid buff attribute; use the shared bit value. |
| `strategy/value/StatsValues.cpp:45` | SkyFire has no current pet id. This reads the row Revive Pet brings back (Pet::LoadPetFromDB without an entry or slot): the active pet first, then the lowest Call Pet slot. |
