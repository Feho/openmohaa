// bt_conditions_idle.cpp
// Behavior tree conditions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#include "bt_conditions_idle.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "perception.h"
#include "g_local.h"

// Curious Investigation Conditions

/**
 * Condition_HasCuriousSound_Check
 *
 * Checks if there are any curious sounds (footsteps, misc ambient sounds)
 * in the perception snapshot. These are minor sounds that warrant a quick
 * investigation but not a full search.
 */
bool Condition_HasCuriousSound_Check(Blackboard& blackboard)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    // Check for curious sounds (footsteps, misc ambient sounds)
    // NOTE: Not weapon fire, grenades, or other high-priority sounds
    for (const auto& sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP || sound.type == AI_EVENT_MISC) {
            return true;
        }
    }

    return false;
}

/**
 * Condition_ReachedCuriousLocation_Check
 *
 * Checks if the bot has reached the curious sound location.
 */
bool Condition_ReachedCuriousLocation_Check(Blackboard& blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION);
    return reachedOpt && *reachedOpt;
}

// Patrol System Conditions

bool Condition_HasPatrolRoute_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 3
    return false;
}

bool Condition_ReachedPatrolWaypoint_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 3
    return false;
}

// Wander System Conditions

bool Condition_ShouldWander_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 3
    return false;
}

bool Condition_ReachedWanderTarget_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 3
    return false;
}

// Attractive Node Conditions

bool Condition_HasNearbyAttractiveNode_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 4
    return false;
}

bool Condition_ReachedAttractiveNode_Check(Blackboard& blackboard)
{
    // To be implemented in Commit 4
    return false;
}
