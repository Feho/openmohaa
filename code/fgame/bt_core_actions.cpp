// Added in OPM - Phase 2B Task 2B.2
// bt_core_actions.cpp: Registration of core behavior tree actions and conditions

#include "bt_action_registry.h"
#include "bt_blackboard_keys.h"
#include "perception.h"
#include "playerbot.h"
#include "player.h"
// Added in OPM - Phase 3 Task 3.1c
#include "bt_actions_movement.h"
#include "bt_conditions_range.h"
// Added in OPM - Phase 3 Task 3.1f
//  Include all combat action headers for complete combat tree
#include "bt_actions_target.h"
#include "bt_actions_aim.h"
#include "bt_actions_fire.h"
#include "bt_conditions_combat.h"
// Added in OPM - Phase 3 Task 3.1h
#include "bt_actions_weapon.h"
#include "bt_conditions_weapon.h"
// Added in OPM - Phase 3 Task 3.1e
#include "bt_actions_tactical.h"
#include "bt_conditions_tactical.h"
// Added in OPM - Phase 3 Task 3.1d
#include "bt_actions_cover.h"
// Added in OPM - Phase 3 Task 3.1g
#include "bt_actions_grenade.h"
#include "bt_conditions_grenade.h"
// Added in OPM - Phase 3 Task 3.2
#include "bt_actions_investigation.h"
#include "bt_conditions_investigation.h"

// Changed in OPM
//  Added bt_blackboard_keys.h to use consistent key constants

/**
 * Register all core actions and conditions.
 * Called during game initialization.
 *
 * Registered functions have access to:
 * - Blackboard key "bot" (BotController*)
 * - Blackboard key "perception" (PerceptionSnapshot*)
 * - Blackboard key "player" (Player*)
 */
void RegisterCoreBTActions()
{
    // === CONDITIONS ===

    REGISTER_BT_CONDITION("HasVisibleEnemy", [](Blackboard &bb) {
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        if (!snapshot) {
            return false;
        }
        return (*snapshot)->HasVisibleEnemy();
    });

    REGISTER_BT_CONDITION("HasKnownEnemy", [](Blackboard &bb) {
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        if (!snapshot) {
            return false;
        }
        return (*snapshot)->HasKnownEnemy();
    });

    REGISTER_BT_CONDITION("LowHealth", [](Blackboard &bb) {
        auto player = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
        if (!player || !(*player)) {
            return false;
        }

        const float healthPercent = (*player)->health / (*player)->max_health;
        return healthPercent < 0.25f;
    });

    REGISTER_BT_CONDITION("HasAmmo", [](Blackboard &bb) {
        auto player = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
        if (!player || !(*player)) {
            return false;
        }

        // Check if player has active weapon with ammo
        Weapon *weapon = (*player)->GetActiveWeapon(WEAPON_MAIN);
        if (!weapon) {
            return false;
        }

        return weapon->HasAmmo(FIRE_PRIMARY) != qfalse;
    });

    REGISTER_BT_CONDITION("HeardRecentSound", [](Blackboard &bb) {
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        if (!snapshot) {
            return false;
        }

        return !(*snapshot)->recentSounds.empty();
    });

    // === ACTIONS ===

    REGISTER_BT_ACTION("AimAtEnemy", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>(BlackboardKeys::BOT);
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);

        if (!bot || !(*bot) || !snapshot) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
            return BTNode::Status::FAILURE;
        }

        // Changed in OPM
        //  Added entity validation - verify entity is still valid and in use
        Sentient *enemy = closestEnemy->entity;
        if (!enemy || !enemy->edict->inuse) {
            return BTNode::Status::FAILURE;
        }

        // Set enemy for aiming system
        (*bot)->SetEnemy(enemy);

        return BTNode::Status::SUCCESS;
    });

    REGISTER_BT_ACTION("ShootEnemy", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>(BlackboardKeys::BOT);
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        auto player   = bb.TryGet<Player *>(BlackboardKeys::PLAYER);

        if (!bot || !(*bot) || !snapshot || !player || !(*player)) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
            return BTNode::Status::FAILURE;
        }

        // Changed in OPM
        //  Added entity validation - verify entity is still valid and in use
        Sentient *enemy = closestEnemy->entity;
        if (!enemy || !enemy->edict->inuse) {
            return BTNode::Status::FAILURE;
        }

        // Only shoot if enemy is within reasonable range
        constexpr float maxShootDistance = 1024.0f;
        if (closestEnemy->distance < maxShootDistance) {
            // Check if we have ammo
            Weapon *weapon = (*player)->GetActiveWeapon(WEAPON_MAIN);
            if (weapon && weapon->HasAmmo(FIRE_PRIMARY)) {
                // Press the fire button
                (*bot)->PressFireButton();
                return BTNode::Status::SUCCESS;
            }
        }

        return BTNode::Status::FAILURE;
    });

    REGISTER_BT_ACTION("Idle", [](Blackboard & /* bb */, float /* dt */) {
        // Stand still, do nothing
        // The bot controller will handle default stance/animation
        return BTNode::Status::SUCCESS;
    });

    REGISTER_BT_ACTION("Retreat", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>(BlackboardKeys::BOT);
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        auto player   = bb.TryGet<Player *>(BlackboardKeys::PLAYER);

        if (!bot || !(*bot) || !snapshot || !player || !(*player)) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
            return BTNode::Status::FAILURE;
        }

        // Changed in OPM
        //  Added entity validation - verify entity is still valid and in use
        Sentient *enemy = closestEnemy->entity;
        if (!enemy || !enemy->edict->inuse) {
            return BTNode::Status::FAILURE;
        }

        // Move away from closest enemy
        Vector botPos   = (*player)->origin;
        Vector enemyPos = closestEnemy->position;
        Vector awayDir  = botPos - enemyPos;

        // Normalize and move 512 units away
        awayDir.normalize();
        Vector retreatTarget = botPos + awayDir * 512.0f;

        (*bot)->GetMovement().MoveTo(retreatTarget);

        // This is a multi-frame action
        return BTNode::Status::RUNNING;
    });

    REGISTER_BT_ACTION("MoveToSound", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>(BlackboardKeys::BOT);
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);

        if (!bot || !(*bot) || !snapshot) {
            return BTNode::Status::FAILURE;
        }

        const AudioEvent *loudestSound = (*snapshot)->GetLoudestSound();
        if (!loudestSound) {
            return BTNode::Status::FAILURE;
        }

        (*bot)->GetMovement().MoveTo(loudestSound->position);

        // Multi-frame action
        return BTNode::Status::RUNNING;
    });

    REGISTER_BT_ACTION("PatrolWaypoints", [](Blackboard &bb, float /* dt */) {
        auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);

        if (!bot || !(*bot)) {
            return BTNode::Status::FAILURE;
        }

        // TODO: Implement waypoint system
        // For now, just succeed (bot will idle)
        return BTNode::Status::SUCCESS;
    });

    // Added in OPM - Phase 3 Task 3.1c
    //  Combat movement actions
    REGISTER_BT_ACTION("ApproachEnemy", [](Blackboard &bb, float /* dt */) {
        return Action_ApproachEnemy(bb);
    });

    REGISTER_BT_ACTION("RetreatFromEnemy", [](Blackboard &bb, float /* dt */) {
        return Action_RetreatFromEnemy(bb);
    });

    REGISTER_BT_ACTION("MaintainDistance", [](Blackboard &bb, float /* dt */) {
        return Action_MaintainDistance(bb);
    });

    // Added in OPM - Phase 3 Task 3.1c
    //  Range-based combat conditions
    REGISTER_BT_CONDITION("EnemyTooClose", [](Blackboard &bb) {
        return Condition_EnemyTooClose(bb) == BTNode::Status::SUCCESS;
    });

    REGISTER_BT_CONDITION("EnemyTooFar", [](Blackboard &bb) {
        return Condition_EnemyTooFar(bb) == BTNode::Status::SUCCESS;
    });

    REGISTER_BT_CONDITION("InOptimalRange", [](Blackboard &bb) {
        return Condition_InOptimalRange(bb) == BTNode::Status::SUCCESS;
    });

    // Added in OPM - Phase 3 Task 3.1f
    //  Register all combat system actions for complete combat tree assembly

    // === Task 3.1a: Target Selection ===
    RegisterTargetActions();

    // === Task 3.1b: Aiming & Fire Control (Actions) ===
    // Changed in OPM - Phase 3 Task 3.1f (Gemini review)
    //  Refactored to use blackboard-based state management instead of thread_local
    //  This prevents state-sharing bugs when multiple bots run on the same thread
    REGISTER_BT_ACTION("AimAtTarget", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_AimAtTarget_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("FireWeapon", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_FireWeapon_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("MeleeAttack", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_MeleeAttack_Execute(bb, dt);
    });

    // === Task 3.1b: Combat Conditions ===
    REGISTER_BT_CONDITION("WeaponReady", [](Blackboard &bb) -> bool {
        return Condition_WeaponReady_Check(bb);
    });

    REGISTER_BT_CONDITION("IsAimedAtTarget", [](Blackboard &bb) -> bool {
        return Condition_IsAimedAtTarget_Check(bb);
    });

    REGISTER_BT_CONDITION("InMeleeRange", [](Blackboard &bb) -> bool {
        return Condition_InMeleeRange_Check(bb);
    });

    // === Task 3.1h: Weapon Switching ===
    // Added in OPM - Phase 3 Task 3.1h
    REGISTER_BT_ACTION("SelectBestWeapon", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SelectBestWeapon_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("SwitchWeapon", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SwitchWeapon_Execute(bb, dt);
    });

    REGISTER_BT_CONDITION("CurrentWeaponEmpty", [](Blackboard &bb) -> bool {
        return Condition_CurrentWeaponEmpty_Check(bb);
    });

    REGISTER_BT_CONDITION("BetterWeaponAvailable", [](Blackboard &bb) -> bool {
        return Condition_BetterWeaponAvailable_Check(bb);
    });

    REGISTER_BT_CONDITION("WeaponSwitchReady", [](Blackboard &bb) -> bool {
        return Condition_WeaponSwitchReady_Check(bb);
    });

    // === Task 3.1e: Tactical Combat & Retreat ===
    // Added in OPM - Phase 3 Task 3.1e
    
    // Tactical actions
    REGISTER_BT_ACTION("TacticalRetreat", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_TacticalRetreat(bb, dt);
    });

    REGISTER_BT_ACTION("SafeReload", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SafeReload(bb, dt);
    });

    REGISTER_BT_ACTION("SuppressFire", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SuppressFire(bb, dt);
    });

    // Tactical conditions
    REGISTER_BT_CONDITION("ShouldRetreat", [](Blackboard &bb) -> bool {
        return Condition_ShouldRetreat(bb);
    });

    REGISTER_BT_CONDITION("UnderHeavyFire", [](Blackboard &bb) -> bool {
        return Condition_UnderHeavyFire(bb);
    });

    REGISTER_BT_CONDITION("AmmoLow", [](Blackboard &bb) -> bool {
        return Condition_AmmoLow(bb);
    });

    REGISTER_BT_CONDITION("SafeToReload", [](Blackboard &bb) -> bool {
        return Condition_SafeToReload(bb);
    });

    // === Task 3.1d: Cover System ===
    // Added in OPM - Phase 3 Task 3.1d
    
    REGISTER_BT_ACTION("FindCover", [](Blackboard &bb, float /* dt */) -> BTNode::Status {
        return Action_FindCover_Execute(bb);
    });

    REGISTER_BT_ACTION("MoveToCover", [](Blackboard &bb, float /* dt */) -> BTNode::Status {
        return Action_MoveToCover_Execute(bb);
    });

    REGISTER_BT_ACTION("PeekFromCover", [](Blackboard &bb, float /* dt */) -> BTNode::Status {
        return Action_PeekFromCover_Execute(bb);
    });

    REGISTER_BT_ACTION("ReturnToCover", [](Blackboard &bb, float /* dt */) -> BTNode::Status {
        return Action_ReturnToCover_Execute(bb);
    });

    // === Task 3.1g: Grenade System ===
    // Added in OPM - Phase 3 Task 3.1g
    
    // Grenade actions
    REGISTER_BT_ACTION("CalculateGrenadeTarget", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_CalculateGrenadeTarget(bb, dt);
    });

    REGISTER_BT_ACTION("ThrowGrenade", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_ThrowGrenade(bb, dt);
    });

    // Grenade conditions
    REGISTER_BT_CONDITION("HasGrenades", [](Blackboard &bb) -> bool {
        return Condition_HasGrenades(bb);
    });

    REGISTER_BT_CONDITION("EnemiesClustered", [](Blackboard &bb) -> bool {
        return Condition_EnemiesClustered(bb);
    });

    REGISTER_BT_CONDITION("ShouldThrowGrenade", [](Blackboard &bb) -> bool {
        return Condition_ShouldThrowGrenade(bb);
    });

    // === Task 3.2: Investigation System ===
    // Added in OPM - Phase 3 Task 3.2
    
    // Investigation actions
    REGISTER_BT_ACTION("SetInvestigationTarget", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SetInvestigationTarget_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("MoveToInvestigationTarget", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_MoveToInvestigationTarget_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("SearchArea", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SearchArea_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("SetSoundInvestigationTarget", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_SetSoundInvestigationTarget_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("MoveToSoundLocation", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_MoveToSoundLocation_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("LookAround", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_LookAround_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("MarkSoundInvestigated", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_MarkSoundInvestigated_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("AbandonInvestigation", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_AbandonInvestigation_Execute(bb, dt);
    });

    REGISTER_BT_ACTION("ReturnToIdle", [](Blackboard &bb, float dt) -> BTNode::Status {
        return Action_ReturnToIdle_Execute(bb, dt);
    });

    // Investigation conditions
    REGISTER_BT_CONDITION("HasHighConfidenceMemory", [](Blackboard &bb) -> bool {
        return Condition_HasHighConfidenceMemory_Check(bb);
    });

    REGISTER_BT_CONDITION("HasInterestingSound", [](Blackboard &bb) -> bool {
        return Condition_HasInterestingSound_Check(bb);
    });

    REGISTER_BT_CONDITION("ReachedInvestigationTarget", [](Blackboard &bb) -> bool {
        return Condition_ReachedInvestigationTarget_Check(bb);
    });

    REGISTER_BT_CONDITION("InvestigationTimedOut", [](Blackboard &bb) -> bool {
        return Condition_InvestigationTimedOut_Check(bb);
    });

    REGISTER_BT_CONDITION("ReachedSoundLocation", [](Blackboard &bb) -> bool {
        return Condition_ReachedSoundLocation_Check(bb);
    });
}
