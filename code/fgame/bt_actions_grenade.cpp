// bt_actions_grenade.cpp
// Grenade throwing actions for behavior trees
// Added in OPM - Phase 3 Task 3.1g: Grenade System

#include "bt_actions_grenade.h"
#include "bt_blackboard_keys.h"
#include "bt_combat_helpers.h"
#include "playerbot.h"
#include "player.h"
#include "perception.h"

// ============================================================================
// Action_CalculateGrenadeTarget
// ============================================================================

BTNode::Status Action_CalculateGrenadeTarget(Blackboard &bb, float deltaTime)
{
    auto perceptionOpt = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return BTNode::Status::FAILURE;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return BTNode::Status::FAILURE;
    }

    // Need at least 2 enemies for grenade
    if (perception->visibleEnemies.size() < 2) {
        return BTNode::Status::FAILURE;
    }

    // Calculate cluster center as grenade target
    Vector targetPosition = BT::Combat::CalculateClusterCenter(perception->visibleEnemies);

    // Store target position in blackboard
    bb.Set<Vector>(BlackboardKeys::GRENADE_TARGET_POSITION, targetPosition);

    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Action_ThrowGrenade
// ============================================================================

BTNode::Status Action_ThrowGrenade(Blackboard &bb, float deltaTime)
{
    auto playerOpt = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetPosOpt = bb.TryGet<Vector>(BlackboardKeys::GRENADE_TARGET_POSITION);

    if (!playerOpt || !targetPosOpt) {
        return BTNode::Status::FAILURE;
    }

    Player *player = *playerOpt;
    Vector targetPosition = *targetPosOpt;

    if (!player) {
        return BTNode::Status::FAILURE;
    }

    // Verify bot still has grenades
    if (!player->HasGrenades()) {
        return BTNode::Status::FAILURE;
    }

    // Throw grenade at target position
    player->ThrowGrenade(targetPosition);

    // Update last grenade time
    bb.Set<float>(BlackboardKeys::LAST_GRENADE_TIME, level.svsTime);

    return BTNode::Status::SUCCESS;
}
