// Tests for MemorySystem (Phase 2 Task 2A.1.5)

#include "test_utilities.h"
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

// Mock SafePtr for testing (simplified version)
template <typename T>
class MockSafePtr
{
private:
    T *ptr;

public:
    MockSafePtr() : ptr(nullptr) {}
    MockSafePtr(T *p) : ptr(p) {}

    T       *operator->() { return ptr; }
    const T *operator->() const { return ptr; }
    T       &operator*() { return *ptr; }
    const T &operator*() const { return *ptr; }

    operator bool() const { return ptr != nullptr; }
    bool operator==(const MockSafePtr &other) const { return ptr == other.ptr; }
    bool operator==(const T *other) const { return ptr == other; }

    T       *Pointer() { return ptr; }
    const T *Pointer() const { return ptr; }
};

// Mock Sentient for testing
struct MockSentient {
    int id;
    TestVector position;
    TestVector velocity;

    MockSentient(int _id, TestVector _pos, TestVector _vel)
        : id(_id)
        , position(_pos)
        , velocity(_vel)
    {
    }
};

// Mock EnemyInfo for testing (mirrors production struct)
struct MockEnemyInfo {
    MockSafePtr<MockSentient> entity;
    TestVector                position;
    TestVector                velocity;
    float                     distance;
    float                     visibilityFactor;
    float                     angleFromForward;
    bool                      isInPeripheral;

    MockEnemyInfo()
        : entity(nullptr)
        , position(TestVector(0, 0, 0))
        , velocity(TestVector(0, 0, 0))
        , distance(0.0f)
        , visibilityFactor(0.0f)
        , angleFromForward(0.0f)
        , isInPeripheral(false)
    {
    }
};

// Mock EnemyMemory for testing (mirrors production struct)
struct MockEnemyMemory {
    MockSafePtr<MockSentient> enemy;
    TestVector                lastKnownPosition;
    TestVector                lastKnownVelocity;
    TestVector                predictedPosition;
    float                     lastSeenTime;
    float                     confidenceLevel;
    int                       timesSpotted;
    bool                      investigationStarted;

    MockEnemyMemory()
        : enemy(nullptr)
        , lastKnownPosition(TestVector(0, 0, 0))
        , lastKnownVelocity(TestVector(0, 0, 0))
        , predictedPosition(TestVector(0, 0, 0))
        , lastSeenTime(0.0f)
        , confidenceLevel(0.0f)
        , timesSpotted(0)
        , investigationStarted(false)
    {
    }
};

// Mock MemorySystem for testing
class MockMemorySystem
{
public:
    std::vector<MockEnemyMemory> memories;

    // Constants from BotConstants namespace
    static constexpr float MEMORY_CONFIDENCE_DECAY_RATE = 0.1f;
    static constexpr float MEMORY_MIN_CONFIDENCE        = 0.1f;
    static constexpr float MEMORY_MAX_AGE_SECONDS       = 30.0f;

    void UpdateMemory(const MockEnemyInfo &enemyInfo, float currentTime)
    {
        if (!enemyInfo.entity) {
            return;
        }

        // Check if we already have a memory for this enemy
        bool found = false;
        for (auto &memory : memories) {
            if (memory.enemy == enemyInfo.entity) {
                // Update existing memory
                memory.lastKnownPosition = enemyInfo.position;
                memory.lastKnownVelocity = enemyInfo.velocity;
                memory.lastSeenTime      = currentTime;
                memory.confidenceLevel   = 1.0f;
                memory.timesSpotted++;
                memory.predictedPosition = enemyInfo.position;
                found                    = true;
                break;
            }
        }

        // If not found, create new memory
        if (!found) {
            MockEnemyMemory newMemory;
            newMemory.enemy               = enemyInfo.entity;
            newMemory.lastKnownPosition   = enemyInfo.position;
            newMemory.lastKnownVelocity   = enemyInfo.velocity;
            newMemory.lastSeenTime        = currentTime;
            newMemory.confidenceLevel     = 1.0f;
            newMemory.timesSpotted        = 1;
            newMemory.predictedPosition   = enemyInfo.position;
            newMemory.investigationStarted = false;

            memories.push_back(newMemory);
        }
    }

    std::vector<MockEnemyMemory> GetKnownEnemies(float currentTime) const
    {
        std::vector<MockEnemyMemory> knownEnemies;

        for (const auto &memory : memories) {
            // Skip if entity no longer exists
            if (!memory.enemy) {
                continue;
            }

            // Calculate time since last seen (clamp to 0 to handle clock skew)
            const float timeSinceLastSeen = Q_max(0.0f, currentTime - memory.lastSeenTime);

            // Skip very old memories
            if (timeSinceLastSeen > MEMORY_MAX_AGE_SECONDS) {
                continue;
            }

            // Apply confidence decay (create a copy with decayed confidence)
            const float decayAmount       = timeSinceLastSeen * MEMORY_CONFIDENCE_DECAY_RATE;
            const float decayedConfidence = Q_max(0.0f, 1.0f - decayAmount);

            // Skip memories below minimum confidence threshold
            if (decayedConfidence < MEMORY_MIN_CONFIDENCE) {
                continue;
            }

            // Create a copy with updated confidence and predicted position
            MockEnemyMemory decayedMemory = memory;
            decayedMemory.confidenceLevel = decayedConfidence;
            decayedMemory.predictedPosition = memory.lastKnownPosition + (memory.lastKnownVelocity * timeSinceLastSeen);

            knownEnemies.push_back(decayedMemory);
        }

        return knownEnemies;
    }

    void CleanupOldMemories(float currentTime, float maxAge)
    {
        // Use erase-remove idiom
        memories.erase(
            std::remove_if(
                memories.begin(),
                memories.end(),
                [currentTime, maxAge](const MockEnemyMemory &memory) {
                    // Remove if entity no longer exists
                    if (!memory.enemy) {
                        return true;
                    }

                    // Remove if too old
                    const float age = currentTime - memory.lastSeenTime;
                    if (age > maxAge) {
                        return true;
                    }

                    return false;
                }
            ),
            memories.end()
        );
    }
};

class MemorySystemTest : public ::testing::Test
{
protected:
    MockMemorySystem system;
    MockSentient     enemy1{1, TestVector(100, 0, 0), TestVector(10, 0, 0)};
    MockSentient     enemy2{2, TestVector(200, 0, 0), TestVector(5, 0, 0)};
    MockSentient     enemy3{3, TestVector(300, 0, 0), TestVector(0, 10, 0)};
};

// Test 1: UpdateMemory creates new memories for visible enemies
TEST_F(MemorySystemTest, UpdateMemory_CreatesNewMemories)
{
    // Create enemy info
    MockEnemyInfo info;
    info.entity   = &enemy1;
    info.position = enemy1.position;
    info.velocity = enemy1.velocity;

    // Update memory at t=0
    system.UpdateMemory(info, 0.0f);

    // Verify memory was created
    ASSERT_EQ(system.memories.size(), 1);
    EXPECT_EQ(system.memories[0].enemy, &enemy1);
    EXPECT_FLOAT_EQ(system.memories[0].confidenceLevel, 1.0f);
    EXPECT_EQ(system.memories[0].timesSpotted, 1);
    EXPECT_VEC3_NEAR(system.memories[0].lastKnownPosition, enemy1.position, 0.01f);
    EXPECT_VEC3_NEAR(system.memories[0].lastKnownVelocity, enemy1.velocity, 0.01f);

    // Update same enemy at t=1 (should update, not create new)
    info.position = TestVector(110, 0, 0); // Enemy moved
    system.UpdateMemory(info, 1.0f);

    // Verify still only one memory, but updated
    ASSERT_EQ(system.memories.size(), 1);
    EXPECT_EQ(system.memories[0].timesSpotted, 2);
    EXPECT_FLOAT_EQ(system.memories[0].confidenceLevel, 1.0f); // Reset to 1.0
    EXPECT_NEAR(system.memories[0].lastKnownPosition.x, 110.0f, 0.01f);

    // Add a different enemy
    MockEnemyInfo info2;
    info2.entity   = &enemy2;
    info2.position = enemy2.position;
    info2.velocity = enemy2.velocity;
    system.UpdateMemory(info2, 1.0f);

    // Verify we now have 2 memories
    EXPECT_EQ(system.memories.size(), 2);
}

// Test 2: Confidence decay for enemies not seen
TEST_F(MemorySystemTest, GetKnownEnemies_ConfidenceDecay)
{
    // Create and store memory at t=0
    MockEnemyInfo info;
    info.entity   = &enemy1;
    info.position = TestVector(100, 0, 0);
    info.velocity = TestVector(0, 0, 0);
    system.UpdateMemory(info, 0.0f);

    // Query at t=0 - should have full confidence
    std::vector<MockEnemyMemory> known = system.GetKnownEnemies(0.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_FLOAT_EQ(known[0].confidenceLevel, 1.0f);

    // Query at t=5 seconds - confidence should have decayed
    // Decay: 5 * 0.1 = 0.5, so confidence = 1.0 - 0.5 = 0.5
    known = system.GetKnownEnemies(5.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].confidenceLevel, 0.5f, 0.01f);

    // Query at t=8 seconds - confidence should be 0.2 (well above threshold)
    // Decay: 8 * 0.1 = 0.8, so confidence = 1.0 - 0.8 = 0.2
    known = system.GetKnownEnemies(8.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].confidenceLevel, 0.2f, 0.01f);

    // Query at t=10 seconds - confidence at or below threshold (0.0), should be filtered out
    // Decay: 10 * 0.1 = 1.0, so confidence = 1.0 - 1.0 = 0.0 < 0.1 threshold
    known = system.GetKnownEnemies(10.0f);
    EXPECT_EQ(known.size(), 0); // Filtered out due to low confidence
}

// Test 3: Position prediction based on velocity
TEST_F(MemorySystemTest, GetKnownEnemies_PositionPrediction)
{
    // Create memory with moving enemy at t=0
    MockEnemyInfo info;
    info.entity   = &enemy1;
    info.position = TestVector(100, 0, 0);
    info.velocity = TestVector(10, 5, 0); // Moving at 10 units/sec in x, 5 in y
    system.UpdateMemory(info, 0.0f);

    // Query at t=2 seconds - should predict position
    // Predicted: (100, 0, 0) + (10, 5, 0) * 2 = (120, 10, 0)
    std::vector<MockEnemyMemory> known = system.GetKnownEnemies(2.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].predictedPosition.x, 120.0f, 0.01f);
    EXPECT_NEAR(known[0].predictedPosition.y, 10.0f, 0.01f);
    EXPECT_NEAR(known[0].predictedPosition.z, 0.0f, 0.01f);

    // Query at t=5 seconds
    // Predicted: (100, 0, 0) + (10, 5, 0) * 5 = (150, 25, 0)
    known = system.GetKnownEnemies(5.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].predictedPosition.x, 150.0f, 0.01f);
    EXPECT_NEAR(known[0].predictedPosition.y, 25.0f, 0.01f);
    EXPECT_NEAR(known[0].predictedPosition.z, 0.0f, 0.01f);

    // Verify confidence also decayed appropriately
    // At t=5, decay = 5 * 0.1 = 0.5, confidence = 0.5
    EXPECT_NEAR(known[0].confidenceLevel, 0.5f, 0.01f);
}

// Test 7: CleanupOldMemories removes stale memories beyond maxAge
TEST_F(MemorySystemTest, CleanupOldMemories_RemovesStaleData)
{
    // Create three enemies at different times
    MockEnemyInfo info1, info2, info3;
    info1.entity   = &enemy1;
    info1.position = enemy1.position;
    info1.velocity = enemy1.velocity;

    info2.entity   = &enemy2;
    info2.position = enemy2.position;
    info2.velocity = enemy2.velocity;

    info3.entity   = &enemy3;
    info3.position = enemy3.position;
    info3.velocity = enemy3.velocity;

    // Spot enemies at t=0, t=10, t=20
    system.UpdateMemory(info1, 0.0f);
    system.UpdateMemory(info2, 10.0f);
    system.UpdateMemory(info3, 20.0f);

    // Verify all three memories exist
    ASSERT_EQ(system.memories.size(), 3);

    // Cleanup at t=35 with maxAge=30
    // enemy1 age=35, enemy2 age=25, enemy3 age=15
    // Only enemy1 should be removed (35 > 30)
    system.CleanupOldMemories(35.0f, 30.0f);

    // Verify only two memories remain
    ASSERT_EQ(system.memories.size(), 2);
    EXPECT_EQ(system.memories[0].enemy, &enemy2); // enemy2 survives (age=25)
    EXPECT_EQ(system.memories[1].enemy, &enemy3); // enemy3 survives (age=15)

    // Cleanup at t=50 with maxAge=30
    // enemy2 age=40, enemy3 age=30
    // enemy2 should be removed (40 > 30), enemy3 exactly at boundary (30 not > 30)
    system.CleanupOldMemories(50.0f, 30.0f);

    // Verify only one memory remains
    ASSERT_EQ(system.memories.size(), 1);
    EXPECT_EQ(system.memories[0].enemy, &enemy3); // enemy3 survives (age=30, not greater)

    // Cleanup at t=51 with maxAge=30
    // enemy3 age=31 (31 > 30)
    system.CleanupOldMemories(51.0f, 30.0f);

    // Verify all memories cleared
    EXPECT_EQ(system.memories.size(), 0);
}

// Test 8: GetKnownEnemies skips null entities (simulates entity destruction)
TEST_F(MemorySystemTest, GetKnownEnemies_SkipsNullEntities)
{
    // Create memory for enemy1
    MockEnemyInfo info;
    info.entity   = &enemy1;
    info.position = enemy1.position;
    info.velocity = enemy1.velocity;
    system.UpdateMemory(info, 0.0f);

    // Verify memory exists and is returned
    std::vector<MockEnemyMemory> known = system.GetKnownEnemies(0.5f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_EQ(known[0].enemy, &enemy1);

    // Simulate entity destruction by setting enemy pointer to null
    system.memories[0].enemy = nullptr;

    // GetKnownEnemies should now return empty vector (filters out null)
    known = system.GetKnownEnemies(1.0f);
    EXPECT_EQ(known.size(), 0);

    // Verify the memory still exists in storage (not removed yet)
    EXPECT_EQ(system.memories.size(), 1);

    // CleanupOldMemories should remove the null entity
    system.CleanupOldMemories(2.0f, 30.0f);
    EXPECT_EQ(system.memories.size(), 0);
}

// Test 9: UpdateMemory resets confidence to 1.0 when enemy is re-spotted
TEST_F(MemorySystemTest, UpdateMemory_ResetConfidenceOnResight)
{
    // Create memory for enemy1 at t=0
    MockEnemyInfo info;
    info.entity   = &enemy1;
    info.position = enemy1.position;
    info.velocity = enemy1.velocity;
    system.UpdateMemory(info, 0.0f);

    // Query at t=5 - confidence should be decayed
    // decay = 5 * 0.1 = 0.5, confidence = 1.0 - 0.5 = 0.5
    std::vector<MockEnemyMemory> known = system.GetKnownEnemies(5.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].confidenceLevel, 0.5f, 0.01f);

    // Re-spot enemy at t=5 (UpdateMemory should reset confidence to 1.0)
    info.position = TestVector(110, 0, 0); // Enemy moved slightly
    system.UpdateMemory(info, 5.0f);

    // Query immediately at t=5.1 - confidence should be back to ~1.0, not decayed
    // If confidence was properly reset at t=5, then at t=5.1:
    // timeSinceLastSeen = 0.1, decay = 0.1 * 0.1 = 0.01, confidence = 0.99
    known = system.GetKnownEnemies(5.1f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].confidenceLevel, 0.99f, 0.01f);

    // Verify the stored confidence in memory is 1.0 (before decay calculation)
    EXPECT_FLOAT_EQ(system.memories[0].confidenceLevel, 1.0f);
    EXPECT_FLOAT_EQ(system.memories[0].lastSeenTime, 5.0f);

    // Query much later at t=10 - should decay from the re-sight time (t=5)
    // timeSinceLastSeen = 5, decay = 5 * 0.1 = 0.5, confidence = 0.5
    known = system.GetKnownEnemies(10.0f);
    ASSERT_EQ(known.size(), 1);
    EXPECT_NEAR(known[0].confidenceLevel, 0.5f, 0.01f);
}
