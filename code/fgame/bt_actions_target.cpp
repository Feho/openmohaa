// Added in OPM - Phase 3 Task 3.1a
// bt_actions_target.cpp: Target selection and tracking for behavior trees

#include "bt_actions_target.h"
#include "bt_action_registry.h"
#include "bt_blackboard_keys.h"
#include "bt_combat_helpers.h"
#include "perception.h"
#include "playerbot.h"
#include "player.h"
#include "bot_profile.h"

// Added in OPM - Phase 3 Task 3.1a
//  Register all target selection actions and conditions
void RegisterTargetActions()
{
    // === CONDITIONS ===

    // Added in OPM - Phase 3 Task 3.1a
    //  Check if current target is valid and attackable
    REGISTER_BT_CONDITION("HasValidTarget", [](Blackboard &bb) {
        auto player = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
        auto target = bb.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);

        if (!player || !(*player) || !target || !(*target)) {
            return false;
        }

        // Validate target is still valid
        Sentient *targetEntity = *target;
        if (!targetEntity->edict->inuse) {
            return false;
        }

        // Use combat helper to validate enemy
        return BT::Combat::IsValidEnemy(*player, targetEntity);
    });

    // Added in OPM - Phase 3 Task 3.1a
    //  Check if current target is visible
    REGISTER_BT_CONDITION("TargetVisible", [](Blackboard &bb) {
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        auto target   = bb.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);

        if (!snapshot || !target || !(*target)) {
            return false;
        }

        Sentient *targetEntity = *target;

        // Check if target is in visible enemies list
        for (const auto &enemyInfo : (*snapshot)->visibleEnemies) {
            if (enemyInfo.entity == targetEntity) {
                return true;
            }
        }

        return false;
    });

    // === ACTIONS ===

    // Added in OPM - Phase 3 Task 3.1a
    //  Scan visible enemies and select best target based on distance and stickiness
    REGISTER_BT_ACTION("SelectTarget", [](Blackboard &bb, float /* dt */) {
        auto bot      = bb.TryGet<BotController *>(BlackboardKeys::BOT);
        auto player   = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
        auto snapshot = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
        auto profile  = bb.TryGet<BotProfile *>(BlackboardKeys::PROFILE);

        if (!bot || !(*bot) || !player || !(*player) || !snapshot || !profile) {
            return BTNode::Status::FAILURE;
        }

        // No visible enemies
        if (!(*snapshot)->HasVisibleEnemy()) {
            return BTNode::Status::FAILURE;
        }

        // Get current target if any
        auto currentTargetOpt = bb.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
        Sentient *currentTarget = (currentTargetOpt && *currentTargetOpt) ? *currentTargetOpt : nullptr;

        // Get target lock time
        auto lockTimeOpt = bb.TryGet<float>(BlackboardKeys::TARGET_LOCK_TIME);
        float lockTime = lockTimeOpt ? *lockTimeOpt : 0.0f;

        // Get profile parameters
        float lockDuration = (*profile)->GetTargetLockTime();
        float switchThreshold = (*profile)->GetTargetSwitchThreshold();

        // Evaluate all visible enemies and find best target
        Sentient *bestTarget = nullptr;
        float bestScore = -1.0f;
        float bestDistance = 0.0f;

        for (const auto &enemyInfo : (*snapshot)->visibleEnemies) {
            Sentient *enemy = enemyInfo.entity;
            if (!enemy || !enemy->edict->inuse) {
                continue;
            }

            // Validate enemy is attackable
            if (!BT::Combat::IsValidEnemy(*player, enemy)) {
                continue;
            }

            // Calculate target score (higher = better)
            float score = BT::Combat::CalculateTargetScore(
                enemy,
                currentTarget,
                enemyInfo.distance,
                lockTime,
                lockDuration,
                switchThreshold
            );

            if (score > bestScore) {
                bestScore = score;
                bestTarget = enemy;
                bestDistance = enemyInfo.distance;
            }
        }

        // No valid target found
        if (!bestTarget) {
            return BTNode::Status::FAILURE;
        }

        // Changed in OPM - Phase 3 Task 3.1a Code Review
        //  Simplified target selection logic per Gemini review
        //  CalculateTargetScore is now the single source of truth for stickiness
        //  Removed redundant lock time checking after scoring
        bool switched = (currentTarget != nullptr && currentTarget != bestTarget);

        // Update blackboard with selected target
        bb.Set<Sentient *>(BlackboardKeys::SELECTED_TARGET, bestTarget);
        bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, bestDistance);
        bb.Set<bool>(BlackboardKeys::TARGET_SWITCHED, switched);
        bb.Set<Sentient *>(BlackboardKeys::PREVIOUS_TARGET, currentTarget);

        // Update lock time if switching or no previous target
        if (switched || !currentTarget) {
            bb.Set<float>(BlackboardKeys::TARGET_LOCK_TIME, static_cast<float>(level.svsTime));
        }

        // Update bot controller's enemy (for legacy compatibility)
        (*bot)->SetEnemy(bestTarget);

        // Note: Memory system is updated automatically by PerceptionSystem
        // when the perception snapshot is created

        return BTNode::Status::SUCCESS;
    });
}
