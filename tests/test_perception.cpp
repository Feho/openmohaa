// Added in OPM - Phase 2 Task 2A.1.1
// Unit tests for bot perception system structures
//
// Note: These tests verify the data structure layouts and helper methods.
// Since perception.h requires full game headers (glb_local.h, sentient.h, player.h),
// we test with mock structures that mirror the production structures.
// Full integration testing with actual game objects will be done separately.

#include "test_utilities.h"
#include <gtest/gtest.h>
#include <vector>

// Mock forward declarations (matching production)
class MockSentient {};
class MockPlayer {};

// Mock ThreatLevel enum (matching production)
enum ThreatLevel {
    THREAT_NONE,
    THREAT_LOW,
    THREAT_MEDIUM,
    THREAT_HIGH
};

// Mock EnemyInfo struct (matching production structure in perception.h)
struct MockEnemyInfo {
    MockSentient *entity;
    TestVector    position;
    TestVector    velocity;
    float         distance;
    float         visibilityFactor;
    float         angleFromForward;
    bool          isInPeripheral;

    MockEnemyInfo()
        : entity(nullptr)
        , position()
        , velocity()
        , distance(0.0f)
        , visibilityFactor(0.0f)
        , angleFromForward(0.0f)
        , isInPeripheral(false)
    {
    }

    bool IsVisible() const { return visibilityFactor > 0.1f; }
    bool IsInPeripheral() const { return isInPeripheral; }
};

// Mock AllyInfo struct (matching production structure in perception.h)
struct MockAllyInfo {
    MockPlayer *entity;
    TestVector  position;
    float       distance;
    bool        canSeeMe;

    MockAllyInfo()
        : entity(nullptr)
        , position()
        , distance(0.0f)
        , canSeeMe(false)
    {
    }
};

// Mock AudioEvent struct (matching production structure in perception.h)
struct MockAudioEvent {
    int        type;
    TestVector position;
    TestVector estimatedDirection;
    float      loudness;
    float      priority;
    float      timestamp;
    float      confidence;

    MockAudioEvent()
        : type(0)
        , position()
        , estimatedDirection()
        , loudness(0.0f)
        , priority(0.0f)
        , timestamp(0.0f)
        , confidence(0.0f)
    {
    }
};

// Mock EnemyMemory struct (matching production structure in perception.h)
struct MockEnemyMemory {
    MockSentient *enemy;
    TestVector    lastKnownPosition;
    TestVector    lastKnownVelocity;
    TestVector    predictedPosition;
    float         lastSeenTime;
    float         confidenceLevel;
    int           timesSpotted;
    bool          investigationStarted;

    MockEnemyMemory()
        : enemy(nullptr)
        , lastKnownPosition()
        , lastKnownVelocity()
        , predictedPosition()
        , lastSeenTime(0.0f)
        , confidenceLevel(0.0f)
        , timesSpotted(0)
        , investigationStarted(false)
    {
    }
};

// Mock PerceptionSnapshot struct (matching production structure in perception.h)
struct MockPerceptionSnapshot {
    std::vector<MockEnemyInfo>   visibleEnemies;
    std::vector<MockEnemyMemory> knownEnemies;
    MockEnemyInfo               *closestEnemy;
    MockEnemyInfo               *mostDangerousEnemy;
    std::vector<MockAllyInfo>    nearbyAllies;
    std::vector<MockAudioEvent>  recentSounds;
    MockAudioEvent              *loudestSound;
    ThreatLevel                  threatLevel;

    MockPerceptionSnapshot()
        : closestEnemy(nullptr)
        , mostDangerousEnemy(nullptr)
        , loudestSound(nullptr)
        , threatLevel(THREAT_NONE)
    {
    }

    bool HasVisibleEnemy() const { return !visibleEnemies.empty(); }
    bool HasKnownEnemy() const { return !knownEnemies.empty(); }
    int  GetEnemyCount() const { return visibleEnemies.size(); }
    int  GetTotalKnownEnemies() const { return visibleEnemies.size() + knownEnemies.size(); }
};

// Test fixture for perception system tests
class PerceptionTest : public BotTestBase
{
};

// ============================================================================
// EnemyInfo Tests
// ============================================================================

TEST_F(PerceptionTest, EnemyInfo_DefaultConstruction)
{
    MockEnemyInfo enemy;

    EXPECT_EQ(enemy.entity, nullptr);
    EXPECT_TRUE(FloatEquals(enemy.position.x, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.position.y, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.position.z, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.velocity.x, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.velocity.y, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.velocity.z, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.distance, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.visibilityFactor, 0.0f));
    EXPECT_TRUE(FloatEquals(enemy.angleFromForward, 0.0f));
    EXPECT_FALSE(enemy.isInPeripheral);
}

TEST_F(PerceptionTest, EnemyInfo_IsVisible_LowVisibility)
{
    MockEnemyInfo enemy;
    enemy.visibilityFactor = 0.05f; // Below threshold

    EXPECT_FALSE(enemy.IsVisible());
}

TEST_F(PerceptionTest, EnemyInfo_IsVisible_HighVisibility)
{
    MockEnemyInfo enemy;
    enemy.visibilityFactor = 0.5f; // Above threshold

    EXPECT_TRUE(enemy.IsVisible());
}

TEST_F(PerceptionTest, EnemyInfo_IsInPeripheral)
{
    MockEnemyInfo enemy;

    enemy.isInPeripheral = false;
    EXPECT_FALSE(enemy.IsInPeripheral());

    enemy.isInPeripheral = true;
    EXPECT_TRUE(enemy.IsInPeripheral());
}

// ============================================================================
// AllyInfo Tests
// ============================================================================

TEST_F(PerceptionTest, AllyInfo_DefaultConstruction)
{
    MockAllyInfo ally;

    EXPECT_EQ(ally.entity, nullptr);
    EXPECT_TRUE(FloatEquals(ally.position.x, 0.0f));
    EXPECT_TRUE(FloatEquals(ally.position.y, 0.0f));
    EXPECT_TRUE(FloatEquals(ally.position.z, 0.0f));
    EXPECT_TRUE(FloatEquals(ally.distance, 0.0f));
    EXPECT_FALSE(ally.canSeeMe);
}

// ============================================================================
// AudioEvent Tests
// ============================================================================

TEST_F(PerceptionTest, AudioEvent_DefaultConstruction)
{
    MockAudioEvent audio;

    EXPECT_EQ(audio.type, 0);
    EXPECT_TRUE(FloatEquals(audio.position.x, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.position.y, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.position.z, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.estimatedDirection.x, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.estimatedDirection.y, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.estimatedDirection.z, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.loudness, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.priority, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.timestamp, 0.0f));
    EXPECT_TRUE(FloatEquals(audio.confidence, 0.0f));
}

// ============================================================================
// EnemyMemory Tests
// ============================================================================

TEST_F(PerceptionTest, EnemyMemory_DefaultConstruction)
{
    MockEnemyMemory memory;

    EXPECT_EQ(memory.enemy, nullptr);
    EXPECT_TRUE(FloatEquals(memory.lastKnownPosition.x, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastKnownPosition.y, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastKnownPosition.z, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastKnownVelocity.x, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastKnownVelocity.y, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastKnownVelocity.z, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.predictedPosition.x, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.predictedPosition.y, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.predictedPosition.z, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.lastSeenTime, 0.0f));
    EXPECT_TRUE(FloatEquals(memory.confidenceLevel, 0.0f));
    EXPECT_EQ(memory.timesSpotted, 0);
    EXPECT_FALSE(memory.investigationStarted);
}

// ============================================================================
// PerceptionSnapshot Tests
// ============================================================================

TEST_F(PerceptionTest, PerceptionSnapshot_DefaultConstruction)
{
    MockPerceptionSnapshot snapshot;

    EXPECT_TRUE(snapshot.visibleEnemies.empty());
    EXPECT_TRUE(snapshot.knownEnemies.empty());
    EXPECT_EQ(snapshot.closestEnemy, nullptr);
    EXPECT_EQ(snapshot.mostDangerousEnemy, nullptr);
    EXPECT_TRUE(snapshot.nearbyAllies.empty());
    EXPECT_TRUE(snapshot.recentSounds.empty());
    EXPECT_EQ(snapshot.loudestSound, nullptr);
    EXPECT_EQ(snapshot.threatLevel, THREAT_NONE);
}

TEST_F(PerceptionTest, PerceptionSnapshot_HasVisibleEnemy)
{
    MockPerceptionSnapshot snapshot;

    EXPECT_FALSE(snapshot.HasVisibleEnemy());

    MockEnemyInfo enemy;
    snapshot.visibleEnemies.push_back(enemy);

    EXPECT_TRUE(snapshot.HasVisibleEnemy());
}

TEST_F(PerceptionTest, PerceptionSnapshot_HasKnownEnemy)
{
    MockPerceptionSnapshot snapshot;

    EXPECT_FALSE(snapshot.HasKnownEnemy());

    MockEnemyMemory memory;
    snapshot.knownEnemies.push_back(memory);

    EXPECT_TRUE(snapshot.HasKnownEnemy());
}

TEST_F(PerceptionTest, PerceptionSnapshot_GetEnemyCount)
{
    MockPerceptionSnapshot snapshot;

    EXPECT_EQ(snapshot.GetEnemyCount(), 0);

    snapshot.visibleEnemies.push_back(MockEnemyInfo());
    EXPECT_EQ(snapshot.GetEnemyCount(), 1);

    snapshot.visibleEnemies.push_back(MockEnemyInfo());
    EXPECT_EQ(snapshot.GetEnemyCount(), 2);

    // Known enemies should not affect visible count
    snapshot.knownEnemies.push_back(MockEnemyMemory());
    EXPECT_EQ(snapshot.GetEnemyCount(), 2);
}

TEST_F(PerceptionTest, PerceptionSnapshot_GetTotalKnownEnemies)
{
    MockPerceptionSnapshot snapshot;

    EXPECT_EQ(snapshot.GetTotalKnownEnemies(), 0);

    snapshot.visibleEnemies.push_back(MockEnemyInfo());
    EXPECT_EQ(snapshot.GetTotalKnownEnemies(), 1);

    snapshot.knownEnemies.push_back(MockEnemyMemory());
    EXPECT_EQ(snapshot.GetTotalKnownEnemies(), 2);

    snapshot.visibleEnemies.push_back(MockEnemyInfo());
    snapshot.knownEnemies.push_back(MockEnemyMemory());
    EXPECT_EQ(snapshot.GetTotalKnownEnemies(), 4);
}

// ============================================================================
// ThreatLevel Tests
// ============================================================================

TEST_F(PerceptionTest, ThreatLevel_EnumValues)
{
    // Verify enum values exist and are distinct
    ThreatLevel none   = THREAT_NONE;
    ThreatLevel low    = THREAT_LOW;
    ThreatLevel medium = THREAT_MEDIUM;
    ThreatLevel high   = THREAT_HIGH;

    EXPECT_NE(none, low);
    EXPECT_NE(low, medium);
    EXPECT_NE(medium, high);
    EXPECT_NE(none, high);
}
