// bt_conditions_idle.cpp
// Behavior tree conditions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#include "g_local.h"
#include "bt_conditions_idle.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "perception.h"
#include "idle_helpers.h"

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

/**
 * Condition_HasPatrolRoute_Check
 *
 * Checks if the bot has a valid patrol route with at least one waypoint.
 */
bool Condition_HasPatrolRoute_Check(Blackboard& blackboard)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!botOpt) {
        return false;
    }

    BotController *bot = *botOpt;
    if (!bot) {
        return false;
    }

    return bot->m_patrolRoute.NumObjects() > 0;
}

/**
 * Condition_ReachedPatrolWaypoint_Check
 *
 * Checks if the bot has reached the current patrol waypoint.
 */
bool Condition_ReachedPatrolWaypoint_Check(Blackboard& blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT);
    return reachedOpt && *reachedOpt;
}

// Wander System Conditions

/**
 * Condition_ShouldWander_Check
 *
 * Checks if the bot should start wandering.
 * Wander if there's no patrol route and hasn't wandered recently.
 */
bool Condition_ShouldWander_Check(Blackboard& blackboard)
{
    // Get last wander time
    float lastWanderTime = 0.0f;
    auto  timeOpt        = blackboard.TryGet<float>(BlackboardKeys::LAST_WANDER_TIME);
    if (timeOpt) {
        lastWanderTime = *timeOpt;
    }

    // Wander if it's been more than 5 seconds since last wander
    float elapsed = static_cast<float>(level.svsTime) - lastWanderTime;
    return elapsed > 5000.0f; // 5 seconds in milliseconds
}

/**
 * Condition_ReachedWanderTarget_Check
 *
 * Checks if the bot has reached the wander target position.
 */
bool Condition_ReachedWanderTarget_Check(Blackboard& blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_WANDER_TARGET);
    return reachedOpt && *reachedOpt;
}

// Attractive Node Conditions

/**
 * Condition_HasNearbyAttractiveNode_Check
 *
 * Checks if there is an attractive tactical node (sniper, cover, corner)
 * within 1024 units of the bot's current position.
 */
bool Condition_HasNearbyAttractiveNode_Check(Blackboard& blackboard)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt) {
        return false;
    }

    Player *player = *playerOpt;
    if (!player) {
        return false;
    }

    // Fixed in OPM - Add 1-second cache to avoid O(n) search every frame
    // Check cache first
    auto cachedNodeOpt = blackboard.TryGet<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE);
    auto cacheTimeOpt  = blackboard.TryGet<float>(BlackboardKeys::ATTRACTIVE_NODE_CACHE_TIME);

    if (cachedNodeOpt && cacheTimeOpt) {
        float elapsed = static_cast<float>(level.svsTime) - *cacheTimeOpt;
        if (elapsed < 1000.0f) { // 1 second cache
            return *cachedNodeOpt != nullptr;
        }
    }

    // Cache miss or expired - perform search
    PathNode *attractiveNode = FindNearbyAttractiveNode(player->origin, 1024.0f);

    // Cache the result (even if null)
    blackboard.Set<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE, attractiveNode);
    blackboard.Set<float>(BlackboardKeys::ATTRACTIVE_NODE_CACHE_TIME, static_cast<float>(level.svsTime));

    return attractiveNode != nullptr;
}

/**
 * Condition_ReachedAttractiveNode_Check
 *
 * Checks if the bot has reached the attractive node.
 */
bool Condition_ReachedAttractiveNode_Check(Blackboard& blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_ATTRACTIVE_NODE);
    return reachedOpt && *reachedOpt;
}
