// bt_conditions_idle.cpp
// Behavior tree conditions for idle behaviors
// Added in OPM - Phase 3 Task 3.3

#include "bt_conditions_idle.h"
#include "bt_blackboard_keys.h"
#include "bt_action_registry.h"
#include "perception.h"
#include "playerbot.h"
#include "player.h"
#include "navigate.h"
#include "g_local.h"

// Constants
namespace IdleConditionConstants
{
    constexpr float WANDER_COOLDOWN = 5000.0f; // 5 seconds between wanders (milliseconds)
}

// ============================================================================
// Curious Behavior Conditions
// ============================================================================

bool Condition_HasCuriousSound(Blackboard &blackboard)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    // Check for curious sounds (footsteps, misc - NOT weapon fire or urgent)
    for (const auto &sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP || sound.type == AI_EVENT_MISC) {
            return true;
        }
    }

    return false;
}

bool Condition_ReachedCuriousLocation(Blackboard &blackboard)
{
    return blackboard.GetOrDefault<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, false);
}

// ============================================================================
// Patrol System Conditions
// ============================================================================

bool Condition_HasPatrolRoute(Blackboard &blackboard)
{
    auto patrolRouteOpt = blackboard.TryGet<std::vector<PathNode *> *>(BlackboardKeys::PATROL_ROUTE);

    if (!patrolRouteOpt) {
        return false;
    }

    std::vector<PathNode *> *patrolRoute = *patrolRouteOpt;

    return patrolRoute && !patrolRoute->empty();
}

bool Condition_ReachedPatrolWaypoint(Blackboard &blackboard)
{
    return blackboard.GetOrDefault<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT, false);
}

// ============================================================================
// Wander System Conditions
// ============================================================================

bool Condition_ShouldWander(Blackboard &blackboard)
{
    // Don't wander if we have a patrol route
    if (Condition_HasPatrolRoute(blackboard)) {
        return false;
    }

    // Check cooldown - don't wander too frequently
    float lastWanderTime = blackboard.GetOrDefault<float>(BlackboardKeys::LAST_WANDER_TIME, 0.0f);
    float currentTime    = level.svsTime;

    return (currentTime - lastWanderTime) > IdleConditionConstants::WANDER_COOLDOWN;
}

bool Condition_ReachedWanderTarget(Blackboard &blackboard)
{
    return blackboard.GetOrDefault<bool>(BlackboardKeys::REACHED_WANDER_TARGET, false);
}

// ============================================================================
// Attractive Node Conditions
// ============================================================================

bool Condition_HasNearbyAttractiveNode(Blackboard &blackboard)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!playerOpt) {
        return false;
    }

    Player *player = *playerOpt;

    if (!player) {
        return false;
    }

    // Search for nearby attractive nodes
    const float     searchRadius   = 1024.0f;
    const PathNode *allNodes       = AI_GetPathNodeList();
    int             numNodes       = AI_NumPathNodes();

    for (int i = 0; i < numNodes; i++) {
        const PathNode *node = &allNodes[i];

        // Check if node has attractive flags
        if (node->nodeflags & (AI_SNIPER | AI_CORNER | AI_COVER)) {
            float distance = (node->origin - player->origin).length();
            if (distance < searchRadius) {
                // Store the node for later use
                blackboard.Set<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE, const_cast<PathNode *>(node));
                return true;
            }
        }
    }

    return false;
}

bool Condition_ReachedAttractiveNode(Blackboard &blackboard)
{
    return blackboard.GetOrDefault<bool>(BlackboardKeys::REACHED_ATTRACTIVE_NODE, false);
}

// ============================================================================
// Registration Function
// ============================================================================

void RegisterIdleConditions()
{
    auto &registry = BTActionRegistry::Instance();

    // Curious behavior
    registry.RegisterCondition("HasCuriousSound", Condition_HasCuriousSound,
        BTNodeMetadata("HasCuriousSound", "Checks for curious sounds (footsteps, ambient)",
            {"perception"}, "Idle"));

    registry.RegisterCondition("ReachedCuriousLocation", Condition_ReachedCuriousLocation,
        BTNodeMetadata("ReachedCuriousLocation", "Checks if reached curious location",
            {"reachedCuriousLocation"}, "Idle"));

    // Patrol system
    registry.RegisterCondition("HasPatrolRoute", Condition_HasPatrolRoute,
        BTNodeMetadata("HasPatrolRoute", "Checks if bot has patrol route",
            {"patrolRoute"}, "Idle"));

    registry.RegisterCondition("ReachedPatrolWaypoint", Condition_ReachedPatrolWaypoint,
        BTNodeMetadata("ReachedPatrolWaypoint", "Checks if reached patrol waypoint",
            {"reachedPatrolWaypoint"}, "Idle"));

    // Wander system
    registry.RegisterCondition("ShouldWander", Condition_ShouldWander,
        BTNodeMetadata("ShouldWander", "Checks if bot should wander",
            {"lastWanderTime"}, "Idle"));

    registry.RegisterCondition("ReachedWanderTarget", Condition_ReachedWanderTarget,
        BTNodeMetadata("ReachedWanderTarget", "Checks if reached wander target",
            {"reachedWanderTarget"}, "Idle"));

    // Attractive node
    registry.RegisterCondition("HasNearbyAttractiveNode", Condition_HasNearbyAttractiveNode,
        BTNodeMetadata("HasNearbyAttractiveNode", "Checks for nearby attractive nodes",
            {"player"}, "Idle"));

    registry.RegisterCondition("ReachedAttractiveNode", Condition_ReachedAttractiveNode,
        BTNodeMetadata("ReachedAttractiveNode", "Checks if reached attractive node",
            {"reachedAttractiveNode"}, "Idle"));
}
