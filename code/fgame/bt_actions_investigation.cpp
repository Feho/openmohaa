// bt_actions_investigation.cpp
// Behavior tree actions for investigation system
// Added in OPM - Phase 3 Task 3.2

#include "bt_actions_investigation.h"
#include "bt_blackboard_keys.h"
#include "perception.h"
#include "investigation_helpers.h"
#include "g_local.h"

// Constants
namespace InvestigationConstants
{
    constexpr float REACHED_DISTANCE        = 128.0f;  // Distance to consider target "reached"
    constexpr float INVESTIGATION_TIMEOUT   = 10000.0f; // 10 seconds in milliseconds
    constexpr float DEFAULT_SEARCH_RADIUS   = 256.0f;  // Default search radius
    constexpr float LOOK_DURATION           = 1.0f;    // Look in each direction for 1 second
    constexpr int   LOOK_DIRECTIONS         = 4;       // Look in 4 cardinal directions
    constexpr float SPIRAL_ANGLE_STEP       = 30.0f;   // Degrees per spiral step
    constexpr float SPIRAL_MAX_ANGLE        = 720.0f;  // Two full circles
    constexpr float CONFIDENCE_DECAY_FACTOR = 0.5f;    // Reduce confidence by half on failed search
}

BTNode::Status Action_SetInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return BTNode::Status::FAILURE;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return BTNode::Status::FAILURE;
    }

    // Find highest confidence enemy memory
    EnemyMemory *bestMemory       = nullptr;
    float        highestConfidence = 0.0f;

    for (auto &memory : perception->knownEnemies) {
        if (memory.confidenceLevel > highestConfidence) {
            highestConfidence = memory.confidenceLevel;
            bestMemory        = &memory;
        }
    }

    if (!bestMemory) {
        return BTNode::Status::FAILURE;
    }

    // Calculate index for safe storage (avoids dangling pointer)
    size_t memoryIndex = bestMemory - &perception->knownEnemies[0];

    // Use predicted position (accounts for last known velocity)
    blackboard.Set<Vector>(BlackboardKeys::INVESTIGATION_TARGET, bestMemory->predictedPosition);
    blackboard.Set<float>(BlackboardKeys::INVESTIGATION_START_TIME, level.svsTime);
    blackboard.Set<float>(BlackboardKeys::INVESTIGATION_RADIUS, InvestigationConstants::DEFAULT_SEARCH_RADIUS);
    blackboard.Set<size_t>(BlackboardKeys::INVESTIGATING_MEMORY_INDEX, memoryIndex);

    // Mark that investigation has started for this memory
    bestMemory->investigationStarted = true;

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_MoveToInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::INVESTIGATION_TARGET);

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
    if (distance < InvestigationConstants::REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_INVESTIGATION_TARGET, true);
        return BTNode::Status::SUCCESS;
    }

    // Check if path exists to target
    if (!IsPositionReachable(player->origin, target)) {
        // Try alternative nearby position
        Vector alternative = FindNearbyReachablePosition(player->origin, target);
        if (alternative != vec_zero) {
            blackboard.Set<Vector>(BlackboardKeys::INVESTIGATION_TARGET, alternative);
            target = alternative; // Update for movement
        } else {
            return BTNode::Status::FAILURE; // Can't reach
        }
    }

    // Move to target
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_SearchArea_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto centerPosOpt = blackboard.TryGet<Vector>(BlackboardKeys::INVESTIGATION_TARGET);

    if (!botOpt || !playerOpt || !centerPosOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot       = *botOpt;
    Player        *player    = *playerOpt;
    Vector         centerPos = *centerPosOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get search radius
    float searchRadius = InvestigationConstants::DEFAULT_SEARCH_RADIUS;
    auto radiusOpt = blackboard.TryGet<float>(BlackboardKeys::INVESTIGATION_RADIUS);
    if (radiusOpt) {
        searchRadius = *radiusOpt;
    }

    // Get or initialize search state
    int   searchPhase = 0;
    float searchAngle = 0.0f;
    float searchTimer = 0.0f;

    auto phaseOpt = blackboard.TryGet<int>(BlackboardKeys::SEARCH_PHASE);
    if (phaseOpt) {
        searchPhase = *phaseOpt;
    }

    auto angleOpt = blackboard.TryGet<float>(BlackboardKeys::SEARCH_ANGLE);
    if (angleOpt) {
        searchAngle = *angleOpt;
    }

    auto timerOpt = blackboard.TryGet<float>(BlackboardKeys::SEARCH_TIMER);
    if (timerOpt) {
        searchTimer = *timerOpt;
    }

    searchTimer += deltaTime;

    // Phase 0: Look around from center (4 cardinal directions)
    if (searchPhase == 0) {
        if (searchTimer > InvestigationConstants::LOOK_DURATION) {
            searchAngle += 90.0f; // Next cardinal direction
            searchTimer = 0.0f;

            if (searchAngle >= 360.0f) {
                searchPhase = 1; // Move to spiral search
                searchAngle = 0.0f;
            }
        }

        // Look in current cardinal direction using engine angle system
        Vector angles  = player->GetViewAngles();
        angles[YAW]    = searchAngle; // Already in degrees, engine handles conversion
        player->SetViewAngles(angles);
    }
    // Phase 1: Spiral search pattern
    else if (searchPhase == 1) {
        // Calculate spiral position using engine angle system
        float spiralRadius = (searchAngle / 360.0f) * searchRadius;
        
        // Use AngleVectors for proper engine angle handling
        vec3_t angles = {0, searchAngle, 0};
        vec3_t forward, right, up;
        AngleVectors(angles, forward, right, up);
        
        Vector spiralOffset = Vector(forward) * spiralRadius;
        Vector spiralPos = centerPos + spiralOffset;

        // Move to spiral position
        bot->GetMovement().MoveTo(spiralPos, nullptr, 0.0f);

        // Increment spiral
        searchAngle += InvestigationConstants::SPIRAL_ANGLE_STEP;

        if (searchAngle >= InvestigationConstants::SPIRAL_MAX_ANGLE) {
            searchPhase = 2; // Search complete
        }
    }
    // Phase 2: Search complete
    else {
        blackboard.Set<bool>(BlackboardKeys::SEARCH_COMPLETE, true);
        return BTNode::Status::SUCCESS;
    }

    // Update blackboard
    blackboard.Set<int>(BlackboardKeys::SEARCH_PHASE, searchPhase);
    blackboard.Set<float>(BlackboardKeys::SEARCH_ANGLE, searchAngle);
    blackboard.Set<float>(BlackboardKeys::SEARCH_TIMER, searchTimer);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_SetSoundInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return BTNode::Status::FAILURE;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return BTNode::Status::FAILURE;
    }

    const AudioEvent *sound = perception->GetLoudestSound();
    if (!sound) {
        return BTNode::Status::FAILURE;
    }

    blackboard.Set<Vector>(BlackboardKeys::INVESTIGATION_TARGET, sound->position);
    blackboard.Set<float>(BlackboardKeys::INVESTIGATION_START_TIME, level.svsTime);
    blackboard.Set<bool>(BlackboardKeys::INVESTIGATING_SOUND, true);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_MoveToSoundLocation_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::INVESTIGATION_TARGET);

    if (!botOpt || !playerOpt || !targetOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    Vector         target = *targetOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Move to target
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    // Check if reached
    float distance = (target - player->origin).length();
    if (distance < InvestigationConstants::REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_SOUND_LOCATION, true);
        return BTNode::Status::SUCCESS;
    }

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_LookAround_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get or initialize look state
    float lookTimer = 0.0f;
    int   lookCount = 0;

    auto timerOpt = blackboard.TryGet<float>(BlackboardKeys::LOOK_TIMER);
    if (timerOpt) {
        lookTimer = *timerOpt;
    }

    auto countOpt = blackboard.TryGet<int>(BlackboardKeys::LOOK_COUNT);
    if (countOpt) {
        lookCount = *countOpt;
    }

    lookTimer += deltaTime;

    // Look for 1 second in each direction
    if (lookTimer > InvestigationConstants::LOOK_DURATION) {
        lookCount++;
        lookTimer = 0.0f;

        if (lookCount >= InvestigationConstants::LOOK_DIRECTIONS) {
            blackboard.Set<bool>(BlackboardKeys::LOOK_COMPLETE, true);
            blackboard.Set<int>(BlackboardKeys::LOOK_COUNT, 0);
            return BTNode::Status::SUCCESS;
        }

        // Choose new random direction using engine angle system
        float  randomAngle = G_Random() * 360.0f;
        Vector angles      = player->GetViewAngles();
        angles[YAW]        = randomAngle; // Engine handles angle wrapping
        player->SetViewAngles(angles);
    }

    blackboard.Set<float>(BlackboardKeys::LOOK_TIMER, lookTimer);
    blackboard.Set<int>(BlackboardKeys::LOOK_COUNT, lookCount);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_MarkSoundInvestigated_Execute(Blackboard &blackboard, float deltaTime)
{
    // Clear sound investigation state
    blackboard.Set<bool>(BlackboardKeys::INVESTIGATING_SOUND, false);
    blackboard.Set<bool>(BlackboardKeys::REACHED_SOUND_LOCATION, false);
    blackboard.Set<bool>(BlackboardKeys::LOOK_COMPLETE, false);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_AbandonInvestigation_Execute(Blackboard &blackboard, float deltaTime)
{
    // Get memory being investigated (if any) - using safe index lookup
    auto memoryIndexOpt = blackboard.TryGet<size_t>(BlackboardKeys::INVESTIGATING_MEMORY_INDEX);
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    
    if (memoryIndexOpt && perceptionOpt) {
        size_t memoryIndex = *memoryIndexOpt;
        PerceptionSnapshot *perception = *perceptionOpt;
        
        if (perception && memoryIndex < perception->knownEnemies.size()) {
            EnemyMemory *memory = &perception->knownEnemies[memoryIndex];
            // Lower confidence in memory
            memory->confidenceLevel *= InvestigationConstants::CONFIDENCE_DECAY_FACTOR;
        }
    }

    // Clear investigation state (prevents broken state on abort)
    blackboard.Set<Vector>(BlackboardKeys::INVESTIGATION_TARGET, vec_zero);
    blackboard.Set<size_t>(BlackboardKeys::INVESTIGATING_MEMORY_INDEX, SIZE_MAX);
    blackboard.Set<bool>(BlackboardKeys::REACHED_INVESTIGATION_TARGET, false);
    blackboard.Set<int>(BlackboardKeys::SEARCH_PHASE, 0);
    blackboard.Set<float>(BlackboardKeys::SEARCH_ANGLE, 0.0f);
    blackboard.Set<bool>(BlackboardKeys::SEARCH_COMPLETE, false);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_ReturnToIdle_Execute(Blackboard &blackboard, float deltaTime)
{
    // Simple action that just succeeds
    // The behavior tree will handle transitioning to idle/patrol
    return BTNode::Status::SUCCESS;
}
