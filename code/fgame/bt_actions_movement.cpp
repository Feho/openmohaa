// Added in OPM - Phase 3 Task 3.1c
// bt_actions_movement.cpp: Combat movement behavior tree actions implementation

#include "bt_actions_movement.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "bot_profile.h"
#include "player.h"
#include "weapon.h"
#include "g_local.h"

namespace
{
// Helper: Get optimal weapon range based on profile preference
float GetOptimalWeaponRange(Weapon* weapon, const BotProfile* profile)
{
    if (!weapon || !profile) {
        return 512.0f; // Default fallback
    }

    const float maxRange = weapon->GetMaxRange();
    const float minRange = weapon->GetMinRange();
    
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Validate weapon ranges are sane
    if (maxRange <= 0.0f || maxRange <= minRange) {
        return 512.0f; // Fallback for invalid weapon ranges
    }
    
    // Use preferred_range from profile as factor (default 0.7 = 70% of max range)
    const float preferredFactor = profile->GetPreferredRange() / maxRange;
    float clampedFactor = preferredFactor;
    if (clampedFactor < 0.3f) clampedFactor = 0.3f;
    if (clampedFactor > 0.9f) clampedFactor = 0.9f;
    
    const float optimalRange = minRange + (maxRange - minRange) * clampedFactor;
    return optimalRange;
}

// Helper: Add unpredictability to movement path
Vector AddPathDeviation(const Vector& targetPos, const Vector& botPos, float deviationAmount)
{
    if (deviationAmount <= 0.0f) {
        return targetPos;
    }

    // Calculate perpendicular vector
    Vector toTarget = targetPos - botPos;
    toTarget[2] = 0; // Flatten to XY plane
    const float distance = VectorNormalize(toTarget);
    
    if (distance < 0.1f) {
        return targetPos;
    }

    // Create perpendicular vector (rotate 90 degrees in XY plane)
    Vector perpendicular(-toTarget[1], toTarget[0], 0);
    
    // Add random offset perpendicular to path
    const float randomOffset = G_CRandom() * deviationAmount * 256.0f; // Up to 256 units deviation
    
    Vector deviatedPos = targetPos + perpendicular * randomOffset;
    return deviatedPos;
}

} // anonymous namespace

//=====================================================
// Action_ApproachEnemy
//=====================================================

BTNode::Status Action_ApproachEnemy(Blackboard& bb)
{
    auto bot = bb.TryGet<BotController*>(BlackboardKeys::BOT);
    auto target = bb.TryGet<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    auto profile = bb.TryGet<BotProfile*>(BlackboardKeys::PROFILE);
    
    if (!bot || !target || !profile) {
        return BTNode::Status::FAILURE;
    }

    Player* player = (*bot)->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    Weapon* weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return BTNode::Status::FAILURE;
    }

    // Get current distance to target
    const float distance = bb.GetOrDefault<float>(BlackboardKeys::TARGET_DISTANCE, 0.0f);
    
    // Calculate optimal range for this weapon
    const float optimalRange = GetOptimalWeaponRange(weapon, *profile);
    bb.Set<float>(BlackboardKeys::OPTIMAL_RANGE, optimalRange);

    // Check if already in optimal range
    const float minRange = weapon->GetMinRange();
    if (distance >= minRange && distance <= optimalRange * 1.1f) { // 10% tolerance
        return BTNode::Status::SUCCESS;
    }

    // Need to move closer
    Vector targetPos = (*target)->origin;
    
    // Add path deviation for unpredictability (profile parameter)
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Validate deviated position is reachable before using it
    const float pathDeviation = (*profile)->GetPathDeviation();
    if (pathDeviation > 0.0f) {
        Vector deviatedPos = AddPathDeviation(targetPos, player->origin, pathDeviation);
        // Check if deviated position is reachable, fall back to original if not
        if ((*bot)->GetMovement().CanMoveTo(deviatedPos)) {
            targetPos = deviatedPos;
        }
    }

    // Store position we're moving to
    bb.Set<Vector>(BlackboardKeys::MOVING_TO_POSITION, targetPos);

    // Execute movement
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Note: MoveTo() is void, cannot check return value
    //  Movement validation done via CanMoveTo() above
    (*bot)->GetMovement().MoveTo(targetPos);

    return BTNode::Status::RUNNING;
}

//=====================================================
// Action_RetreatFromEnemy
//=====================================================

BTNode::Status Action_RetreatFromEnemy(Blackboard& bb)
{
    auto bot = bb.TryGet<BotController*>(BlackboardKeys::BOT);
    auto target = bb.TryGet<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    auto profile = bb.TryGet<BotProfile*>(BlackboardKeys::PROFILE);
    
    if (!bot || !target || !profile) {
        return BTNode::Status::FAILURE;
    }

    Player* player = (*bot)->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    Weapon* weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return BTNode::Status::FAILURE;
    }

    // Get current distance to target
    const float distance = bb.GetOrDefault<float>(BlackboardKeys::TARGET_DISTANCE, 0.0f);
    
    // Get weapon's minimum safe range
    const float minRange = weapon->GetMinRange();
    
    // Check if already at safe distance
    if (distance >= minRange * 1.2f) { // 20% buffer for safety
        return BTNode::Status::SUCCESS;
    }

    // Calculate retreat direction (away from target)
    Vector retreatDir = player->origin - (*target)->origin;
    retreatDir[2] = 0; // Flatten to XY plane
    VectorNormalizeFast(retreatDir);

    // Use AvoidPath to retreat
    const float safeDistance = minRange * 1.3f;
    const Vector leashVector = retreatDir * 512.0f; // Retreat up to 512 units
    
    (*bot)->GetMovement().AvoidPath((*target)->origin, safeDistance, leashVector);

    return BTNode::Status::RUNNING;
}

//=====================================================
// Action_MaintainDistance
//=====================================================

BTNode::Status Action_MaintainDistance(Blackboard& bb)
{
    auto bot = bb.TryGet<BotController*>(BlackboardKeys::BOT);
    auto target = bb.TryGet<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    auto profile = bb.TryGet<BotProfile*>(BlackboardKeys::PROFILE);
    
    if (!bot || !target || !profile) {
        return BTNode::Status::FAILURE;
    }

    Player* player = (*bot)->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    // Check strafe usage from profile (0.0 = never strafe, 1.0 = always strafe)
    const float strafeUsage = (*profile)->GetStrafeUsage();
    if (strafeUsage < 0.1f || G_Random() > strafeUsage) {
        // Don't strafe this frame
        return BTNode::Status::SUCCESS;
    }

    // Get or initialize strafe state
    const float currentTime = static_cast<float>(level.svsTime) * 0.001f; // Convert to seconds
    int strafeDirection = bb.GetOrDefault<int>(BlackboardKeys::STRAFE_DIRECTION, 0);
    float strafeTimer = bb.GetOrDefault<float>(BlackboardKeys::STRAFE_TIMER, 0.0f);

    // Change direction every 2 seconds (or initialize)
    constexpr float STRAFE_CHANGE_INTERVAL = 2.0f;
    if (strafeDirection == 0 || (currentTime - strafeTimer) >= STRAFE_CHANGE_INTERVAL) {
        // Pick new random direction
        strafeDirection = (G_Random() > 0.5f) ? 1 : -1;
        strafeTimer = currentTime;
        
        bb.Set<int>(BlackboardKeys::STRAFE_DIRECTION, strafeDirection);
        bb.Set<float>(BlackboardKeys::STRAFE_TIMER, strafeTimer);
    }

    // Calculate perpendicular strafe vector
    Vector toTarget = (*target)->origin - player->origin;
    toTarget[2] = 0; // Flatten
    VectorNormalizeFast(toTarget);

    // Perpendicular vector (90 degrees in XY plane)
    Vector strafeVector(-toTarget[1], toTarget[0], 0);
    strafeVector = strafeVector * (static_cast<float>(strafeDirection) * 256.0f); // Strafe distance

    // Calculate strafe destination
    Vector strafePos = player->origin + strafeVector;

    // Move in strafe direction with reduced speed (70% of normal)
    (*bot)->GetMovement().MoveTo(strafePos);

    return BTNode::Status::SUCCESS;
}
