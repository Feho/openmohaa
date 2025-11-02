// Added in OPM - Phase 3 Task 3.1c
// bt_conditions_range.cpp: Range-based combat conditions implementation

#include "bt_conditions_range.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "bot_profile.h"
#include "player.h"
#include "weapon.h"

//=====================================================
// Condition_EnemyTooClose
//=====================================================

BTNode::Status Condition_EnemyTooClose(Blackboard& bb)
{
    auto bot = bb.TryGet<BotController*>(BlackboardKeys::BOT);
    auto target = bb.TryGet<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    
    if (!bot || !target) {
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
    
    // Get weapon's minimum effective range
    const float minRange = weapon->GetMinRange();
    const float maxRange = weapon->GetMaxRange();
    
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Validate weapon ranges are sane
    if (maxRange <= 0.0f || maxRange <= minRange) {
        return BTNode::Status::FAILURE; // Invalid weapon ranges
    }

    // Target is too close if within minimum range
    return (distance < minRange) ? BTNode::Status::SUCCESS : BTNode::Status::FAILURE;
}

//=====================================================
// Condition_EnemyTooFar
//=====================================================

BTNode::Status Condition_EnemyTooFar(Blackboard& bb)
{
    auto bot = bb.TryGet<BotController*>(BlackboardKeys::BOT);
    auto target = bb.TryGet<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    
    if (!bot || !target) {
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
    
    // Get weapon's maximum effective range
    const float maxRange = weapon->GetMaxRange();
    const float minRange = weapon->GetMinRange();
    
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Validate weapon ranges are sane
    if (maxRange <= 0.0f || maxRange <= minRange) {
        return BTNode::Status::FAILURE; // Invalid weapon ranges
    }

    // Target is too far if beyond maximum range
    return (distance > maxRange) ? BTNode::Status::SUCCESS : BTNode::Status::FAILURE;
}

//=====================================================
// Condition_InOptimalRange
//=====================================================

BTNode::Status Condition_InOptimalRange(Blackboard& bb)
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
    
    // Get weapon ranges
    const float minRange = weapon->GetMinRange();
    const float maxRange = weapon->GetMaxRange();
    
    // Added in OPM - Phase 3 Task 3.1c (Gemini review)
    //  Validate weapon ranges are sane
    if (maxRange <= 0.0f || maxRange <= minRange) {
        return BTNode::Status::FAILURE; // Invalid weapon ranges
    }
    
    // Calculate optimal range based on profile preference
    // Use preferred_range as factor (e.g., 512 / 2048 = 0.25 means prefer 25% into range)
    const float preferredFactor = (*profile)->GetPreferredRange() / maxRange;
    float clampedFactor = preferredFactor;
    if (clampedFactor < 0.3f) clampedFactor = 0.3f;
    if (clampedFactor > 0.9f) clampedFactor = 0.9f;
    
    const float optimalMax = minRange + (maxRange - minRange) * clampedFactor;

    // In optimal range if between min and preferred max (with 10% tolerance on max)
    const bool inRange = (distance >= minRange) && (distance <= optimalMax * 1.1f);

    return inRange ? BTNode::Status::SUCCESS : BTNode::Status::FAILURE;
}
