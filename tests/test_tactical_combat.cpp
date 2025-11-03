/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// test_tactical_combat.cpp
// Unit tests for tactical combat and retreat system
// Added in OPM - Phase 3 Task 3.1e

// Note: These tests verify blackboard key storage and retrieval for tactical combat.
// Full integration tests require the complete game environment.

#define BEHAVIOR_TREE_TESTING
#include <gtest/gtest.h>
#include "../code/fgame/behavior_tree.h"
#include "../code/fgame/bt_blackboard_keys.h"

class TacticalCombatTest : public ::testing::Test
{
protected:
    Blackboard blackboard;
};

// Test 1: Recent damage tracking
TEST_F(TacticalCombatTest, RecentDamage_Tracking)
{
    float recentDamage = 25.0f;
    float lastDamageTime = 1000.0f;

    blackboard.Set<float>(BlackboardKeys::RECENT_DAMAGE, recentDamage);
    blackboard.Set<float>(BlackboardKeys::LAST_DAMAGE_TIME, lastDamageTime);

    auto retrievedDamage = blackboard.TryGet<float>(BlackboardKeys::RECENT_DAMAGE);
    auto retrievedTime = blackboard.TryGet<float>(BlackboardKeys::LAST_DAMAGE_TIME);

    ASSERT_TRUE(retrievedDamage.has_value());
    ASSERT_TRUE(retrievedTime.has_value());
    EXPECT_FLOAT_EQ(*retrievedDamage, 25.0f);
    EXPECT_FLOAT_EQ(*retrievedTime, 1000.0f);
}

// Test 2: Combat profile storage
TEST_F(TacticalCombatTest, CombatProfile_Storage)
{
    int combatProfile = 2; // DEFENSIVE
    blackboard.Set<int>(BlackboardKeys::COMBAT_PROFILE, combatProfile);

    auto retrieved = blackboard.TryGet<int>(BlackboardKeys::COMBAT_PROFILE);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, 2);
}

// Test 3: Retreat position storage
TEST_F(TacticalCombatTest, RetreatPosition_Storage)
{
    struct Vector {
        float x, y, z;
    };
    
    Vector retreatPos = {128.0f, 256.0f, 32.0f};
    blackboard.Set<Vector>(BlackboardKeys::RETREAT_POSITION, retreatPos);

    auto retrieved = blackboard.TryGet<Vector>(BlackboardKeys::RETREAT_POSITION);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(retrieved->x, 128.0f);
    EXPECT_FLOAT_EQ(retrieved->y, 256.0f);
    EXPECT_FLOAT_EQ(retrieved->z, 32.0f);
}

// Test 4: Suppression fire timing
TEST_F(TacticalCombatTest, SuppressionFire_Timing)
{
    float suppressStartTime = 2000.0f;
    blackboard.Set<float>(BlackboardKeys::SUPPRESS_START_TIME, suppressStartTime);

    auto retrieved = blackboard.TryGet<float>(BlackboardKeys::SUPPRESS_START_TIME);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(*retrieved, 2000.0f);
}

// Test 5: Reload timing
TEST_F(TacticalCombatTest, Reload_Timing)
{
    float reloadStartTime = 1500.0f;
    blackboard.Set<float>(BlackboardKeys::RELOAD_START_TIME, reloadStartTime);

    auto retrieved = blackboard.TryGet<float>(BlackboardKeys::RELOAD_START_TIME);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(*retrieved, 1500.0f);
}

// Test 6: Last known enemy position
TEST_F(TacticalCombatTest, LastKnownEnemyPos_Storage)
{
    struct Vector {
        float x, y, z;
    };
    
    Vector lastKnownPos = {512.0f, 128.0f, 64.0f};
    blackboard.Set<Vector>(BlackboardKeys::LAST_KNOWN_ENEMY_POS, lastKnownPos);

    auto retrieved = blackboard.TryGet<Vector>(BlackboardKeys::LAST_KNOWN_ENEMY_POS);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(retrieved->x, 512.0f);
    EXPECT_FLOAT_EQ(retrieved->y, 128.0f);
    EXPECT_FLOAT_EQ(retrieved->z, 64.0f);
}

// Test 7: Enemy count tracking
TEST_F(TacticalCombatTest, EnemyCount_Storage)
{
    int enemyCount = 3;
    blackboard.Set<int>(BlackboardKeys::ENEMY_COUNT, enemyCount);

    auto retrieved = blackboard.TryGet<int>(BlackboardKeys::ENEMY_COUNT);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, 3);
}

// Test 8: Multiple tactical data storage
TEST_F(TacticalCombatTest, MultipleData_Storage)
{
    float recentDamage = 35.0f;
    float lastDamageTime = 1200.0f;
    int combatProfile = 3; // RETREATING
    int enemyCount = 4;

    blackboard.Set<float>(BlackboardKeys::RECENT_DAMAGE, recentDamage);
    blackboard.Set<float>(BlackboardKeys::LAST_DAMAGE_TIME, lastDamageTime);
    blackboard.Set<int>(BlackboardKeys::COMBAT_PROFILE, combatProfile);
    blackboard.Set<int>(BlackboardKeys::ENEMY_COUNT, enemyCount);

    EXPECT_FLOAT_EQ(blackboard.Get<float>(BlackboardKeys::RECENT_DAMAGE), 35.0f);
    EXPECT_FLOAT_EQ(blackboard.Get<float>(BlackboardKeys::LAST_DAMAGE_TIME), 1200.0f);
    EXPECT_EQ(blackboard.Get<int>(BlackboardKeys::COMBAT_PROFILE), 3);
    EXPECT_EQ(blackboard.Get<int>(BlackboardKeys::ENEMY_COUNT), 4);
}

// Test 9: Blackboard key removal
TEST_F(TacticalCombatTest, Blackboard_KeyRemoval)
{
    float reloadStartTime = 1000.0f;
    blackboard.Set<float>(BlackboardKeys::RELOAD_START_TIME, reloadStartTime);

    ASSERT_TRUE(blackboard.Has(BlackboardKeys::RELOAD_START_TIME));

    blackboard.Remove(BlackboardKeys::RELOAD_START_TIME);

    EXPECT_FALSE(blackboard.Has(BlackboardKeys::RELOAD_START_TIME));
}

// Test 10: Blackboard GetOrDefault
TEST_F(TacticalCombatTest, Blackboard_GetOrDefault)
{
    float defaultDamage = 0.0f;
    float retrievedDamage = blackboard.GetOrDefault<float>(BlackboardKeys::RECENT_DAMAGE, defaultDamage);

    EXPECT_FLOAT_EQ(retrievedDamage, 0.0f);

    blackboard.Set<float>(BlackboardKeys::RECENT_DAMAGE, 15.0f);
    retrievedDamage = blackboard.GetOrDefault<float>(BlackboardKeys::RECENT_DAMAGE, defaultDamage);

    EXPECT_FLOAT_EQ(retrievedDamage, 15.0f);
}
