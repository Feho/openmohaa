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

BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_ContinuePatrol_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

// Wander System Actions

BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_PauseAfterWander_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_ClearWanderTarget_Execute(Blackboard& blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
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
