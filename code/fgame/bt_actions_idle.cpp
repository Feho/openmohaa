// bt_actions_idle.cpp
// Behavior tree actions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#include "bt_actions_idle.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "perception.h"
#include "idle_helpers.h"
#include "investigation_helpers.h"
#include "g_local.h"

// Constants for curious investigation
namespace CuriousConstants
{
    constexpr float REACHED_DISTANCE = 64.0f; // Distance to consider location "reached"
} // namespace CuriousConstants

// Curious Investigation Actions

/**
 * Action_SetCuriousTarget_Execute
 *
 * Sets the target position for curious investigation based on ambient sounds.
 * Curious investigation is triggered by minor ambient sounds like footsteps
 * or misc sounds (not weapon fire or grenades).
 */
BTNode::Status Action_SetCuriousTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return BTNode::Status::FAILURE;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return BTNode::Status::FAILURE;
    }

    // Find curious sound (footsteps, misc ambient)
    // NOTE: Footstep and MISC are curious sounds, not weapon fire or grenades
    const AudioEvent *curiousSound = nullptr;
    for (const auto& sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP || sound.type == AI_EVENT_MISC) {
            curiousSound = &sound;
            break;
        }
    }

    if (!curiousSound) {
        return BTNode::Status::FAILURE;
    }

    // Set blackboard values
    blackboard.Set<Vector>(BlackboardKeys::CURIOUS_TARGET, curiousSound->position);
    blackboard.Set<float>(BlackboardKeys::CURIOUS_START_TIME, static_cast<float>(level.svsTime));
    blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, false);

    return BTNode::Status::SUCCESS;
}

/**
 * Action_MoveToCuriousLocation_Execute
 *
 * Moves the bot to the curious sound location. Times out after 5 seconds
 * (shorter than full investigation). If the location is unreachable, tries
 * to find a nearby reachable position.
 */
BTNode::Status Action_MoveToCuriousLocation_Execute(Blackboard& blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::CURIOUS_TARGET);

    if (!botOpt || !playerOpt || !targetOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    Vector         target = *targetOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Check if reached (within threshold)
    float distance = (target - player->origin).length();
    if (distance < CuriousConstants::REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, true);
        return BTNode::Status::SUCCESS;
    }

    // Check timeout (5 seconds, shorter than investigation)
    auto startTimeOpt = blackboard.TryGet<float>(BlackboardKeys::CURIOUS_START_TIME);
    if (startTimeOpt) {
        float startTime = *startTimeOpt;
        float elapsed   = static_cast<float>(level.svsTime) - startTime;
        if (elapsed > static_cast<float>(BotConstants::CURIOUS_INVESTIGATION_TIMEOUT)) {
            return BTNode::Status::FAILURE;
        }
    }

    // Check if path exists to target
    if (!IsPositionReachable(player->origin, target)) {
        // Try alternative nearby position
        Vector alternative = FindNearbyReachablePosition(player->origin, target);
        if (alternative != vec_zero) {
            blackboard.Set<Vector>(BlackboardKeys::CURIOUS_TARGET, alternative);
            target = alternative; // Update for movement
        } else {
            return BTNode::Status::FAILURE; // Can't reach
        }
    }

    // Move to target
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

/**
 * Action_QuickLookAround_Execute
 *
 * Performs a quick look-around behavior at the curious location.
 * Only looks left and right (2 directions) for 0.5 seconds each,
 * much faster than the full investigation search pattern.
 */
BTNode::Status Action_QuickLookAround_Execute(Blackboard& blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get or initialize timer and count
    float lookTimer = 0.0f;
    int   lookCount = 0;

    auto timerOpt = blackboard.TryGet<float>(BlackboardKeys::CURIOUS_LOOK_TIMER);
    if (timerOpt) {
        lookTimer = *timerOpt;
    }

    auto countOpt = blackboard.TryGet<int>(BlackboardKeys::CURIOUS_LOOK_COUNT);
    if (countOpt) {
        lookCount = *countOpt;
    }

    // Increment timer
    lookTimer += deltaTime;

    // Check if it's time to switch direction
    if (lookTimer > BotConstants::CURIOUS_LOOK_DURATION) {
        lookCount++;
        lookTimer = 0.0f;

        // Done after looking in both directions
        if (lookCount >= BotConstants::CURIOUS_LOOK_DIRECTIONS) {
            blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, 0);
            blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, 0.0f);
            return BTNode::Status::SUCCESS;
        }

        // Calculate look direction (alternate left/right: +90, -90)
        Vector currentAngles = bot->GetRotation().GetTargetAngles();
        float  yawOffset     = (lookCount % 2 == 0) ? 90.0f : -90.0f;
        Vector newAngles     = currentAngles;
        newAngles[YAW] += yawOffset;

        // Normalize yaw to 0-360
        while (newAngles[YAW] >= BotConstants::FULL_CIRCLE_DEGREES) {
            newAngles[YAW] -= BotConstants::FULL_CIRCLE_DEGREES;
        }
        while (newAngles[YAW] < 0.0f) {
            newAngles[YAW] += BotConstants::FULL_CIRCLE_DEGREES;
        }

        bot->GetRotation().SetTargetAngles(newAngles);
    }

    // Update blackboard
    blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, lookTimer);
    blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, lookCount);

    return BTNode::Status::RUNNING;
}

/**
 * Action_ClearCuriousState_Execute
 *
 * Clears all curious investigation state from the blackboard,
 * allowing the bot to start a fresh curious investigation if needed.
 */
BTNode::Status Action_ClearCuriousState_Execute(Blackboard& blackboard, float deltaTime)
{
    blackboard.Set<Vector>(BlackboardKeys::CURIOUS_TARGET, vec_zero);
    blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, false);
    blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, 0.0f);
    blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, 0);

    return BTNode::Status::SUCCESS;
}

// Patrol System Actions

/**
 * Action_MoveToNextPatrolWaypoint_Execute
 *
 * Moves the bot to the next waypoint in the patrol route.
 * Uses BotMovement to pathfind to the waypoint position.
 */
BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get patrol route from bot controller (member variable)
    if (bot->m_patrolRoute.NumObjects() == 0) {
        return BTNode::Status::FAILURE;
    }

    // Get current waypoint index
    int  waypointIndex = 0;
    auto indexOpt      = blackboard.TryGet<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX);
    if (indexOpt) {
        waypointIndex = *indexOpt;
    }

    // Ensure index is valid
    if (waypointIndex < 0 || waypointIndex >= bot->m_patrolRoute.NumObjects()) {
        waypointIndex = 0;
        blackboard.Set<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, waypointIndex);
    }

    PathNode *waypoint = bot->m_patrolRoute.ObjectAt(waypointIndex + 1);
    if (!waypoint) {
        return BTNode::Status::FAILURE;
    }

    // Move to waypoint
    bot->GetMovement().MoveTo(waypoint->origin, nullptr, 0.0f);

    // Check if reached
    float distance = (waypoint->origin - player->origin).length();
    if (distance < BotConstants::WAYPOINT_REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT, true);
        return BTNode::Status::SUCCESS;
    }

    return BTNode::Status::RUNNING;
}

/**
 * Action_PauseAtWaypoint_Execute
 *
 * Pauses the bot at a patrol waypoint for 1-3 seconds.
 * This makes patrol behavior look more natural.
 */
BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    // Get or initialize pause timer
    float pauseTimer = 0.0f;
    auto  timerOpt   = blackboard.TryGet<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER);
    if (timerOpt) {
        pauseTimer = *timerOpt;
    }

    // Increment timer
    pauseTimer += deltaTime * static_cast<float>(BotConstants::SECONDS_TO_MS); // Convert to milliseconds

    // Random pause duration between min and max
    int pauseDuration =
        BotConstants::WAYPOINT_PAUSE_MIN
        + static_cast<int>(
            G_Random(static_cast<float>(BotConstants::WAYPOINT_PAUSE_MAX - BotConstants::WAYPOINT_PAUSE_MIN))
        );

    if (pauseTimer > static_cast<float>(pauseDuration)) {
        blackboard.Set<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER, 0.0f);
        return BTNode::Status::SUCCESS;
    }

    blackboard.Set<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER, pauseTimer);
    return BTNode::Status::RUNNING;
}

/**
 * Action_AdvanceToNextWaypoint_Execute
 *
 * Advances the bot to the next waypoint in the patrol route.
 * Supports both loop and bounce patrol modes.
 *
 * Loop mode: Goes back to start after reaching end
 * Bounce mode: Reverses direction when reaching either end
 */
BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!botOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot = *botOpt;
    if (!bot) {
        return BTNode::Status::FAILURE;
    }

    int routeSize = bot->m_patrolRoute.NumObjects();
    if (routeSize == 0) {
        return BTNode::Status::FAILURE;
    }

    // Get current state
    int  waypointIndex = 0;
    bool patrolReverse = false;
    int  patrolMode    = 0; // 0=loop, 1=bounce

    auto indexOpt = blackboard.TryGet<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX);
    if (indexOpt) {
        waypointIndex = *indexOpt;
    }

    auto reverseOpt = blackboard.TryGet<bool>(BlackboardKeys::PATROL_REVERSE);
    if (reverseOpt) {
        patrolReverse = *reverseOpt;
    }

    auto modeOpt = blackboard.TryGet<int>(BlackboardKeys::PATROL_MODE);
    if (modeOpt) {
        patrolMode = *modeOpt;
    }

    // Advance waypoint index
    if (patrolReverse) {
        waypointIndex--;
        if (waypointIndex < 0) {
            waypointIndex = 1;
            patrolReverse = false; // Reached start, go forward
        }
    } else {
        waypointIndex++;
        if (waypointIndex >= routeSize) {
            if (patrolMode == 0) {
                // Loop mode: Go back to start
                waypointIndex = 0;
            } else {
                // Bounce mode: Reverse direction
                waypointIndex = routeSize - 2;
                if (waypointIndex < 0) {
                    waypointIndex = 0;
                }
                patrolReverse = true;
            }
        }
    }

    // Update blackboard
    blackboard.Set<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, waypointIndex);
    blackboard.Set<bool>(BlackboardKeys::PATROL_REVERSE, patrolReverse);
    blackboard.Set<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT, false);

    return BTNode::Status::SUCCESS;
}

/**
 * Action_ContinuePatrol_Execute
 *
 * Simple continuation action for patrol behavior.
 * Returns SUCCESS to allow the patrol sequence to continue.
 */
BTNode::Status Action_ContinuePatrol_Execute(Blackboard& blackboard, float deltaTime)
{
    return BTNode::Status::SUCCESS;
}

// Wander System Actions

/**
 * Action_PickRandomWanderTarget_Execute
 *
 * Selects a random position nearby to wander to.
 * Picks a random direction and distance (256-768 units) and checks if reachable.
 */
BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt) {
        return BTNode::Status::FAILURE;
    }

    Player *player = *playerOpt;
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    // Pick random direction (0-360 degrees)
    float angle = G_Random(BotConstants::FULL_CIRCLE_DEGREES);

    // Pick random distance (256-768 units)
    float distance = BotConstants::WANDER_DISTANCE_MIN
                   + G_Random(BotConstants::WANDER_DISTANCE_MAX - BotConstants::WANDER_DISTANCE_MIN);

    // Calculate direction vector using angle
    vec3_t angles = {0, angle, 0};
    vec3_t forward, right, up;
    AngleVectors(angles, forward, right, up);

    Vector direction    = Vector(forward);
    Vector wanderTarget = player->origin + (direction * distance);

    // Check if reachable
    if (!IsPositionReachable(player->origin, wanderTarget)) {
        // Try closer position (half distance)
        wanderTarget = player->origin + (direction * (distance * 0.5f));

        if (!IsPositionReachable(player->origin, wanderTarget)) {
            return BTNode::Status::FAILURE;
        }
    }

    // Set blackboard values
    blackboard.Set<Vector>(BlackboardKeys::WANDER_TARGET, wanderTarget);
    blackboard.Set<float>(BlackboardKeys::WANDER_START_TIME, static_cast<float>(level.svsTime));
    blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, false);

    return BTNode::Status::SUCCESS;
}

/**
 * Action_MoveToWanderTarget_Execute
 *
 * Moves the bot to the wander target position.
 * Times out after a reasonable duration to prevent getting stuck.
 */
BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::WANDER_TARGET);

    if (!botOpt || !playerOpt || !targetOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    Vector         target = *targetOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Move to wander target
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    // Check if reached
    float distance = (target - player->origin).length();
    if (distance < CuriousConstants::REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, true);
        return BTNode::Status::SUCCESS;
    }

    // Timeout check (use larger timeout for wander, 10 seconds)
    auto startTimeOpt = blackboard.TryGet<float>(BlackboardKeys::WANDER_START_TIME);
    if (startTimeOpt) {
        float startTime = *startTimeOpt;
        float elapsed   = static_cast<float>(level.svsTime) - startTime;
        if (elapsed > 10000.0f) { // 10 second timeout
            return BTNode::Status::FAILURE;
        }
    }

    return BTNode::Status::RUNNING;
}

/**
 * Action_PauseAfterWander_Execute
 *
 * Pauses the bot after reaching a wander target for 2-5 seconds.
 * This makes wander behavior look more natural and less frantic.
 */
BTNode::Status Action_PauseAfterWander_Execute(Blackboard& blackboard, float deltaTime)
{
    // Get or initialize pause timer
    float pauseTimer = 0.0f;
    auto  timerOpt   = blackboard.TryGet<float>(BlackboardKeys::WANDER_PAUSE_TIMER);
    if (timerOpt) {
        pauseTimer = *timerOpt;
    }

    // Increment timer
    pauseTimer += deltaTime * static_cast<float>(BotConstants::SECONDS_TO_MS); // Convert to milliseconds

    // Random pause duration between min and max
    int pauseDuration =
        BotConstants::WANDER_PAUSE_MIN
        + static_cast<int>(G_Random(static_cast<float>(BotConstants::WANDER_PAUSE_MAX - BotConstants::WANDER_PAUSE_MIN))
        );

    if (pauseTimer > static_cast<float>(pauseDuration)) {
        blackboard.Set<float>(BlackboardKeys::WANDER_PAUSE_TIMER, 0.0f);
        return BTNode::Status::SUCCESS;
    }

    blackboard.Set<float>(BlackboardKeys::WANDER_PAUSE_TIMER, pauseTimer);
    return BTNode::Status::RUNNING;
}

/**
 * Action_ClearWanderTarget_Execute
 *
 * Clears the wander target and updates the last wander time.
 * This allows the bot to pick a new wander target on the next iteration.
 */
BTNode::Status Action_ClearWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    blackboard.Set<Vector>(BlackboardKeys::WANDER_TARGET, vec_zero);
    blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, false);
    blackboard.Set<float>(BlackboardKeys::LAST_WANDER_TIME, static_cast<float>(level.svsTime));

    return BTNode::Status::SUCCESS;
}

// Attractive Node Actions

BTNode::Status Action_MoveToAttractiveNode_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_UseAttractiveNode_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

// Idle Standing Actions

BTNode::Status Action_StandInPlace_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_OccasionalLookAround_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}
