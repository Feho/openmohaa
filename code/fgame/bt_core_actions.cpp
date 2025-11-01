// Added in OPM - Phase 2B Task 2B.2
// bt_core_actions.cpp: Registration of core behavior tree actions and conditions

#include "bt_action_registry.h"
#include "perception.h"
#include "playerbot.h"
#include "player.h"

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
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");
        if (!snapshot) {
            return false;
        }
        return (*snapshot)->HasVisibleEnemy();
    });

    REGISTER_BT_CONDITION("HasKnownEnemy", [](Blackboard &bb) {
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");
        if (!snapshot) {
            return false;
        }
        return (*snapshot)->HasKnownEnemy();
    });

    REGISTER_BT_CONDITION("LowHealth", [](Blackboard &bb) {
        auto player = bb.TryGet<Player *>("player");
        if (!player || !(*player)) {
            return false;
        }

        const float healthPercent = (*player)->health / (*player)->max_health;
        return healthPercent < 0.25f;
    });

    REGISTER_BT_CONDITION("HasAmmo", [](Blackboard &bb) {
        auto player = bb.TryGet<Player *>("player");
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
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");
        if (!snapshot) {
            return false;
        }

        return !(*snapshot)->recentSounds.empty();
    });

    // === ACTIONS ===

    REGISTER_BT_ACTION("AimAtEnemy", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>("bot");
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");

        if (!bot || !(*bot) || !snapshot) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
            return BTNode::Status::FAILURE;
        }

        // Set enemy for aiming system
        (*bot)->SetEnemy(closestEnemy->entity);

        return BTNode::Status::SUCCESS;
    });

    REGISTER_BT_ACTION("ShootEnemy", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>("bot");
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");
        auto player   = bb.TryGet<Player *>("player");

        if (!bot || !(*bot) || !snapshot || !player || !(*player)) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
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
        auto bot      = bb.TryGet<BotController *>("bot");
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");
        auto player   = bb.TryGet<Player *>("player");

        if (!bot || !(*bot) || !snapshot || !player || !(*player)) {
            return BTNode::Status::FAILURE;
        }

        const EnemyInfo *closestEnemy = (*snapshot)->GetClosestEnemy();
        if (!closestEnemy || !closestEnemy->entity) {
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
        auto bot      = bb.TryGet<BotController *>("bot");
        auto snapshot = bb.TryGet<PerceptionSnapshot *>("perception");

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
        auto bot = bb.TryGet<BotController *>("bot");

        if (!bot || !(*bot)) {
            return BTNode::Status::FAILURE;
        }

        // TODO: Implement waypoint system
        // For now, just succeed (bot will idle)
        return BTNode::Status::SUCCESS;
    });
}
