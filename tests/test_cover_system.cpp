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

// test_cover_system.cpp
// Unit tests for cover system blackboard integration
// Added in OPM - Phase 3 Task 3.1d

// Note: These tests verify blackboard key storage and retrieval for the cover system.
// Full integration tests require the complete game environment.

#define BEHAVIOR_TREE_TESTING
#include <gtest/gtest.h>
#include "../fgame/behavior_tree.h"
#include "../fgame/bt_blackboard_keys.h"

// Mock CoverPoint structure for testing blackboard storage
struct MockCoverPoint {
    float position[3];
    float quality;
    float distanceToEnemy;
};

class CoverSystemTest : public ::testing::Test
{
protected:
    Blackboard blackboard;
};

// Test 1: Blackboard storage and retrieval of cover point
TEST_F(CoverSystemTest, Blackboard_SelectedCover_Storage)
{
    MockCoverPoint cover;
    cover.position[0] = 128.0f;
    cover.position[1] = 64.0f;
    cover.position[2] = 32.0f;
    cover.quality = 0.8f;
    cover.distanceToEnemy = 256.0f;

    blackboard.Set<MockCoverPoint>(BlackboardKeys::SELECTED_COVER, cover);

    auto retrieved = blackboard.TryGet<MockCoverPoint>(BlackboardKeys::SELECTED_COVER);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(retrieved->quality, 0.8f);
    EXPECT_FLOAT_EQ(retrieved->position[0], 128.0f);
    EXPECT_FLOAT_EQ(retrieved->distanceToEnemy, 256.0f);
}

// Test 2: Cover quality storage
TEST_F(CoverSystemTest, Blackboard_CoverQuality_Storage)
{
    float quality = 0.75f;
    blackboard.Set<float>(BlackboardKeys::COVER_QUALITY, quality);

    auto retrieved = blackboard.TryGet<float>(BlackboardKeys::COVER_QUALITY);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(*retrieved, 0.75f);
}

// Test 3: Cover state storage
TEST_F(CoverSystemTest, Blackboard_CoverState_Storage)
{
    int coverState = 2; // COVER_IN_COVER
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, coverState);

    auto retrieved = blackboard.TryGet<int>(BlackboardKeys::COVER_STATE);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(*retrieved, 2);
}

// Test 4: Peek timing data storage
TEST_F(CoverSystemTest, Blackboard_PeekTiming_Storage)
{
    float startTime = 1000.0f;
    float duration = 1500.0f;
    int peekState = 1;

    blackboard.Set<float>(BlackboardKeys::PEEK_START_TIME, startTime);
    blackboard.Set<float>(BlackboardKeys::PEEK_DURATION, duration);
    blackboard.Set<int>(BlackboardKeys::PEEK_STATE, peekState);

    auto retrievedStart = blackboard.TryGet<float>(BlackboardKeys::PEEK_START_TIME);
    auto retrievedDuration = blackboard.TryGet<float>(BlackboardKeys::PEEK_DURATION);
    auto retrievedState = blackboard.TryGet<int>(BlackboardKeys::PEEK_STATE);

    ASSERT_TRUE(retrievedStart.has_value());
    ASSERT_TRUE(retrievedDuration.has_value());
    ASSERT_TRUE(retrievedState.has_value());

    EXPECT_FLOAT_EQ(*retrievedStart, 1000.0f);
    EXPECT_FLOAT_EQ(*retrievedDuration, 1500.0f);
    EXPECT_EQ(*retrievedState, 1);
}

// Test 5: Complete cover workflow simulation
TEST_F(CoverSystemTest, Blackboard_CoverWorkflow_Simulation)
{
    // Step 1: Find and store cover
    MockCoverPoint cover;
    cover.position[0] = 256.0f;
    cover.position[1] = 128.0f;
    cover.position[2] = 0.0f;
    cover.quality = 0.85f;
    cover.distanceToEnemy = 512.0f;
    
    blackboard.Set<MockCoverPoint>(BlackboardKeys::SELECTED_COVER, cover);
    blackboard.Set<float>(BlackboardKeys::COVER_QUALITY, cover.quality);
    
    auto retrievedCover = blackboard.TryGet<MockCoverPoint>(BlackboardKeys::SELECTED_COVER);
    ASSERT_TRUE(retrievedCover.has_value());
    EXPECT_FLOAT_EQ(retrievedCover->quality, 0.85f);
    
    // Step 2: Move to cover
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 1); // COVER_MOVING_TO
    auto stateMoving = blackboard.TryGet<int>(BlackboardKeys::COVER_STATE);
    EXPECT_EQ(*stateMoving, 1);
    
    // Step 3: Arrive at cover
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 2); // COVER_IN_COVER
    auto stateInCover = blackboard.TryGet<int>(BlackboardKeys::COVER_STATE);
    EXPECT_EQ(*stateInCover, 2);
    
    // Step 4: Peek from cover
    blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 1);
    blackboard.Set<float>(BlackboardKeys::PEEK_START_TIME, 1000.0f);
    blackboard.Set<float>(BlackboardKeys::PEEK_DURATION, 1500.0f);
    
    auto peekStart = blackboard.TryGet<float>(BlackboardKeys::PEEK_START_TIME);
    EXPECT_FLOAT_EQ(*peekStart, 1000.0f);
    
    // Step 5: Return to cover
    blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 0);
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 2);
    
    auto finalState = blackboard.TryGet<int>(BlackboardKeys::COVER_STATE);
    EXPECT_EQ(*finalState, 2);
}

// Test 6: Multiple cover keys coexist
TEST_F(CoverSystemTest, Blackboard_MultipleKeys_Coexistence)
{
    MockCoverPoint cover;
    cover.quality = 0.9f;
    
    blackboard.Set<MockCoverPoint>(BlackboardKeys::SELECTED_COVER, cover);
    blackboard.Set<float>(BlackboardKeys::COVER_QUALITY, 0.9f);
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 2);
    blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 1);
    blackboard.Set<float>(BlackboardKeys::PEEK_START_TIME, 2000.0f);
    blackboard.Set<float>(BlackboardKeys::PEEK_DURATION, 1200.0f);
    
    // Verify all keys are retrievable
    EXPECT_TRUE(blackboard.TryGet<MockCoverPoint>(BlackboardKeys::SELECTED_COVER).has_value());
    EXPECT_TRUE(blackboard.TryGet<float>(BlackboardKeys::COVER_QUALITY).has_value());
    EXPECT_TRUE(blackboard.TryGet<int>(BlackboardKeys::COVER_STATE).has_value());
    EXPECT_TRUE(blackboard.TryGet<int>(BlackboardKeys::PEEK_STATE).has_value());
    EXPECT_TRUE(blackboard.TryGet<float>(BlackboardKeys::PEEK_START_TIME).has_value());
    EXPECT_TRUE(blackboard.TryGet<float>(BlackboardKeys::PEEK_DURATION).has_value());
}

// Test 7: Blackboard key constants are unique
TEST_F(CoverSystemTest, Blackboard_KeyConstants_Uniqueness)
{
    const char* keys[] = {
        BlackboardKeys::SELECTED_COVER,
        BlackboardKeys::COVER_QUALITY,
        BlackboardKeys::COVER_STATE,
        BlackboardKeys::PEEK_STATE,
        BlackboardKeys::PEEK_START_TIME,
        BlackboardKeys::PEEK_DURATION
    };
    
    const int numKeys = sizeof(keys) / sizeof(keys[0]);
    
    for (int i = 0; i < numKeys; i++) {
        for (int j = i + 1; j < numKeys; j++) {
            EXPECT_STRNE(keys[i], keys[j]);
        }
    }
}

// Test 8: Cover state transitions
TEST_F(CoverSystemTest, Blackboard_CoverState_Transitions)
{
    // None -> Moving
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 0);
    EXPECT_EQ(*blackboard.TryGet<int>(BlackboardKeys::COVER_STATE), 0);
    
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 1);
    EXPECT_EQ(*blackboard.TryGet<int>(BlackboardKeys::COVER_STATE), 1);
    
    // Moving -> InCover
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 2);
    EXPECT_EQ(*blackboard.TryGet<int>(BlackboardKeys::COVER_STATE), 2);
    
    // InCover -> Peeking
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, 3);
    EXPECT_EQ(*blackboard.TryGet<int>(BlackboardKeys::COVER_STATE), 3);
}
