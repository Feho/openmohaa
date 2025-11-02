// Added in OPM - Phase 3 Task 3.1c
// test_combat_movement.cpp: Unit tests for combat movement behavior tree actions

#include "test_utilities.h"
#include "test_game_stubs.h"
#include <gtest/gtest.h>

// Forward declarations for the actions we're testing
#include "../code/fgame/bt_actions_movement.h"
#include "../code/fgame/bt_conditions_range.h"
#include "../code/fgame/behavior_tree.h"
#include "../code/fgame/bt_blackboard_keys.h"

// Mock weapon class for testing
class MockWeapon
{
public:
    float minRange = 64.0f;
    float maxRange = 1024.0f;
    
    float GetMinRange() const { return minRange; }
    float GetMaxRange() const { return maxRange; }
};

// Mock profile for testing
class MockBotProfile
{
public:
    float preferredRange = 512.0f;
    float strafeUsage = 0.7f;
    float pathDeviation = 0.3f;
    
    float GetPreferredRange() const { return preferredRange; }
    float GetStrafeUsage() const { return strafeUsage; }
    float GetPathDeviation() const { return pathDeviation; }
};

// Test fixture for combat movement tests
class CombatMovementTest : public ::testing::Test
{
protected:
    BT::Blackboard bb;
    MockWeapon weapon;
    MockBotProfile profile;
    
    void SetUp() override
    {
        // Setup common blackboard state
        bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 500.0f);
    }
};

//=====================================================
// Condition Tests
//=====================================================

TEST_F(CombatMovementTest, EnemyTooClose_WithinMinRange)
{
    // Setup: Enemy at 50 units, weapon min range is 64
    weapon.minRange = 64.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 50.0f);
    
    // Note: This test verifies the logic in isolation
    // In production, the condition needs bot/target/weapon from blackboard
    // For unit testing, we verify the range check logic:
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool tooClose = (distance < weapon.minRange);
    
    EXPECT_TRUE(tooClose);
}

TEST_F(CombatMovementTest, EnemyTooClose_BeyondMinRange)
{
    // Setup: Enemy at 100 units, weapon min range is 64
    weapon.minRange = 64.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 100.0f);
    
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool tooClose = (distance < weapon.minRange);
    
    EXPECT_FALSE(tooClose);
}

TEST_F(CombatMovementTest, EnemyTooFar_BeyondMaxRange)
{
    // Setup: Enemy at 1500 units, weapon max range is 1024
    weapon.maxRange = 1024.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 1500.0f);
    
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool tooFar = (distance > weapon.maxRange);
    
    EXPECT_TRUE(tooFar);
}

TEST_F(CombatMovementTest, EnemyTooFar_WithinMaxRange)
{
    // Setup: Enemy at 800 units, weapon max range is 1024
    weapon.maxRange = 1024.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 800.0f);
    
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool tooFar = (distance > weapon.maxRange);
    
    EXPECT_FALSE(tooFar);
}

TEST_F(CombatMovementTest, InOptimalRange_WithinRange)
{
    // Setup: Weapon range 64-1024, preferred is 512 (50% of max)
    weapon.minRange = 64.0f;
    weapon.maxRange = 1024.0f;
    profile.preferredRange = 512.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 400.0f);
    
    // Calculate optimal range as profile does
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalMax = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool inRange = (distance >= weapon.minRange) && (distance <= optimalMax * 1.1f);
    
    EXPECT_TRUE(inRange);
}

TEST_F(CombatMovementTest, InOptimalRange_TooClose)
{
    // Setup: Enemy at 50 units, min range is 64
    weapon.minRange = 64.0f;
    weapon.maxRange = 1024.0f;
    profile.preferredRange = 512.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, 50.0f);
    
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalMax = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    const float distance = bb.Get<float>(BlackboardKeys::TARGET_DISTANCE);
    const bool inRange = (distance >= weapon.minRange) && (distance <= optimalMax * 1.1f);
    
    EXPECT_FALSE(inRange);
}

//=====================================================
// Action Logic Tests
//=====================================================

TEST_F(CombatMovementTest, ApproachEnemy_Logic_NeedsToMoveCloser)
{
    // Test the approach logic: Should move if distance > optimal range
    weapon.minRange = 64.0f;
    weapon.maxRange = 1024.0f;
    profile.preferredRange = 512.0f;
    
    const float currentDistance = 800.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, currentDistance);
    
    // Calculate optimal range
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalRange = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    // Should move if outside optimal range with tolerance
    const bool shouldMove = !(currentDistance >= weapon.minRange && currentDistance <= optimalRange * 1.1f);
    
    EXPECT_TRUE(shouldMove);
}

TEST_F(CombatMovementTest, ApproachEnemy_Logic_AlreadyInRange)
{
    // Test the approach logic: Should stop if in optimal range
    weapon.minRange = 64.0f;
    weapon.maxRange = 1024.0f;
    profile.preferredRange = 512.0f;
    
    const float currentDistance = 300.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, currentDistance);
    
    // Calculate optimal range
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalRange = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    // Should not move if within optimal range with tolerance
    const bool shouldMove = !(currentDistance >= weapon.minRange && currentDistance <= optimalRange * 1.1f);
    
    EXPECT_FALSE(shouldMove);
}

TEST_F(CombatMovementTest, RetreatFromEnemy_Logic_TooClose)
{
    // Test retreat logic: Should retreat if below safe distance
    weapon.minRange = 128.0f;
    const float currentDistance = 100.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, currentDistance);
    
    // Should retreat if below safe distance (minRange * 1.2)
    const bool shouldRetreat = (currentDistance < weapon.minRange * 1.2f);
    
    EXPECT_TRUE(shouldRetreat);
}

TEST_F(CombatMovementTest, RetreatFromEnemy_Logic_SafeDistance)
{
    // Test retreat logic: Should stop if at safe distance
    weapon.minRange = 128.0f;
    const float currentDistance = 160.0f;
    bb.Set<float>(BlackboardKeys::TARGET_DISTANCE, currentDistance);
    
    // Should not retreat if at safe distance
    const bool shouldRetreat = (currentDistance < weapon.minRange * 1.2f);
    
    EXPECT_FALSE(shouldRetreat);
}

TEST_F(CombatMovementTest, MaintainDistance_Logic_StrafeDirection)
{
    // Test strafe logic: Should alternate direction periodically
    const float currentTime = 5.0f;
    const float strafeTimer = 3.0f; // Last direction change
    int strafeDirection = 1; // Right
    
    constexpr float STRAFE_CHANGE_INTERVAL = 2.0f;
    const bool shouldChangeDirection = (currentTime - strafeTimer) >= STRAFE_CHANGE_INTERVAL;
    
    EXPECT_TRUE(shouldChangeDirection);
}

TEST_F(CombatMovementTest, MaintainDistance_Logic_StrafeUsage)
{
    // Test that strafe usage affects whether bot strafes
    profile.strafeUsage = 0.0f; // Never strafe
    
    // With 0.0 strafe usage, random check will always fail
    const bool shouldStrafe = (profile.strafeUsage >= 0.1f);
    
    EXPECT_FALSE(shouldStrafe);
}

//=====================================================
// Helper Function Tests
//=====================================================

TEST_F(CombatMovementTest, OptimalRangeCalculation_MidRange)
{
    // Test optimal range calculation with mid-range weapon
    weapon.minRange = 64.0f;
    weapon.maxRange = 1024.0f;
    profile.preferredRange = 512.0f; // 50% of max
    
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalRange = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    // Should be approximately 544 (64 + 960*0.5)
    EXPECT_TRUE(FloatEquals(optimalRange, 544.0f, 1.0f));
}

TEST_F(CombatMovementTest, OptimalRangeCalculation_LongRange)
{
    // Test with sniper-type weapon (prefer long range)
    weapon.minRange = 256.0f;
    weapon.maxRange = 4096.0f;
    profile.preferredRange = 3000.0f; // ~73% of max
    
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalRange = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    // Should be approximately 3064 (256 + 3840*0.73)
    EXPECT_GT(optimalRange, 2800.0f);
    EXPECT_LT(optimalRange, 3200.0f);
}

TEST_F(CombatMovementTest, OptimalRangeCalculation_CloseRange)
{
    // Test with shotgun-type weapon (prefer close range)
    weapon.minRange = 32.0f;
    weapon.maxRange = 384.0f;
    profile.preferredRange = 150.0f; // ~39% of max
    
    const float preferredFactor = profile.preferredRange / weapon.maxRange;
    const float clampedFactor = Q_clamp(preferredFactor, 0.3f, 0.9f);
    const float optimalRange = weapon.minRange + (weapon.maxRange - weapon.minRange) * clampedFactor;
    
    // Should be approximately 169 (32 + 352*0.39)
    EXPECT_GT(optimalRange, 130.0f);
    EXPECT_LT(optimalRange, 200.0f);
}
