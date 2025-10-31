// Tests for PerceptionSystem integration (Phase 2 Task 2A.1.6)

#include "test_utilities.h"
#include <gtest/gtest.h>
#include <vector>
#include <deque>
#include <memory>
#include <algorithm>

// Mock Sentient for testing
class MockSentient
{
public:
    int id;
    MockSentient(int sentientId)
        : id(sentientId)
    {
    }
};

// Mock EnemyInfo (mirrors production struct)
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
        , position(TestVector(0, 0, 0))
        , velocity(TestVector(0, 0, 0))
        , distance(0.0f)
        , visibilityFactor(0.0f)
        , angleFromForward(0.0f)
        , isInPeripheral(false)
    {
    }
};

// Mock EnemyMemory (mirrors production struct)
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

// Mock AudioEvent (mirrors production struct)
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
        , position(TestVector(0, 0, 0))
        , estimatedDirection(TestVector(0, 0, 0))
        , loudness(0.0f)
        , priority(0.0f)
        , timestamp(0.0f)
        , confidence(0.0f)
    {
    }
};

// Mock PerceptionSnapshot (mirrors production struct)
struct MockPerceptionSnapshot {
    std::vector<MockEnemyInfo>   visibleEnemies;
    std::vector<MockAudioEvent>  recentSounds;
    std::vector<MockEnemyMemory> knownEnemies;

    size_t closestEnemyIndex;        // SIZE_MAX if none
    size_t mostDangerousEnemyIndex;  // SIZE_MAX if none
    size_t loudestSoundIndex;        // SIZE_MAX if none

    MockPerceptionSnapshot()
        : closestEnemyIndex(SIZE_MAX)
        , mostDangerousEnemyIndex(SIZE_MAX)
        , loudestSoundIndex(SIZE_MAX)
    {
    }

    MockEnemyInfo *GetClosestEnemy()
    {
        return closestEnemyIndex < visibleEnemies.size() ? &visibleEnemies[closestEnemyIndex] : nullptr;
    }

    MockEnemyInfo *GetMostDangerousEnemy()
    {
        return mostDangerousEnemyIndex < visibleEnemies.size() ? &visibleEnemies[mostDangerousEnemyIndex] : nullptr;
    }

    MockAudioEvent *GetLoudestSound()
    {
        return loudestSoundIndex < recentSounds.size() ? &recentSounds[loudestSoundIndex] : nullptr;
    }
};

// Mock VisionSensor
class MockVisionSensor
{
public:
    std::vector<MockEnemyInfo> UpdateVision(const TestVector &botPos, float deltaTime)
    {
        // Return pre-configured visible enemies for testing
        return visibleEnemies;
    }

    std::vector<MockEnemyInfo> visibleEnemies;
};

// Mock AudioSensor
class MockAudioSensor
{
public:
    std::deque<MockAudioEvent> eventQueue;

    void ProcessEvent(int eventType, const TestVector &position, float loudness, float timestamp)
    {
        // Calculate priority based on event type
        int priority = 0;

        switch (eventType) {
        case 0: // AI_EVENT_WEAPON_FIRE
        case 1: // AI_EVENT_WEAPON_IMPACT
        case 2: // AI_EVENT_EXPLOSION
            priority = 2;
            break;
        case 10: // AI_EVENT_FOOTSTEP
        case 11: // AI_EVENT_VOICE
            priority = 1;
            break;
        default:
            return;
        }

        MockAudioEvent event;
        event.type      = eventType;
        event.position  = position;
        event.loudness  = loudness;
        event.priority  = priority / 2.0f;
        event.timestamp = timestamp;

        eventQueue.push_back(event);

        if (eventQueue.size() > 100) {
            eventQueue.pop_front();
        }
    }

    std::vector<MockAudioEvent> GetRecentSounds(const TestVector &botPos, float currentTime, float timeWindow)
    {
        std::vector<MockAudioEvent> recentSounds;
        recentSounds.reserve(eventQueue.size());

        for (const auto &event : eventQueue) {
            if (currentTime - event.timestamp <= timeWindow) {
                MockAudioEvent modifiedEvent = event;

                // Calculate direction
                TestVector toSound  = modifiedEvent.position - botPos;
                float      distance = toSound.length();

                if (distance > 0.001f) {
                    toSound.normalize();
                    modifiedEvent.estimatedDirection = toSound;

                    const float maxAudioDistance = 2000.0f;
                    modifiedEvent.confidence     = 1.0f - std::min(distance / maxAudioDistance, 1.0f);

                    if (distance > 1.0f) {
                        const float refDist           = 100.0f;
                        const float attenuationFactor = (refDist * refDist) / (distance * distance);
                        modifiedEvent.loudness *= std::min(attenuationFactor, 1.0f);
                    }
                } else {
                    modifiedEvent.estimatedDirection = TestVector(0, 0, 0);
                    modifiedEvent.confidence         = 1.0f;
                }

                recentSounds.push_back(std::move(modifiedEvent));
            }
        }

        // Sort by priority, then loudness
        std::sort(recentSounds.begin(), recentSounds.end(), [](const MockAudioEvent &a, const MockAudioEvent &b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            return a.loudness > b.loudness;
        });

        return recentSounds;
    }
};

// Mock MemorySystem
class MockMemorySystem
{
public:
    std::vector<MockEnemyMemory> memories;

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
            newMemory.enemy              = enemyInfo.entity;
            newMemory.lastKnownPosition  = enemyInfo.position;
            newMemory.lastKnownVelocity  = enemyInfo.velocity;
            newMemory.lastSeenTime       = currentTime;
            newMemory.confidenceLevel    = 1.0f;
            newMemory.timesSpotted       = 1;
            newMemory.predictedPosition  = enemyInfo.position;
            newMemory.investigationStarted = false;

            memories.push_back(newMemory);
        }
    }

    std::vector<MockEnemyMemory> GetKnownEnemies(float currentTime) const
    {
        std::vector<MockEnemyMemory> knownEnemies;

        for (const auto &memory : memories) {
            if (!memory.enemy) {
                continue;
            }

            const float timeSinceLastSeen = std::max(0.0f, currentTime - memory.lastSeenTime);
            const float maxAge            = 30.0f; // MEMORY_MAX_AGE_SECONDS

            if (timeSinceLastSeen > maxAge) {
                continue;
            }

            // Apply confidence decay
            const float decayRate         = 0.03333f; // MEMORY_CONFIDENCE_DECAY_RATE
            const float decayAmount       = timeSinceLastSeen * decayRate;
            const float decayedConfidence = std::max(0.0f, 1.0f - decayAmount);

            const float minConfidence = 0.1f; // MEMORY_MIN_CONFIDENCE
            if (decayedConfidence < minConfidence) {
                continue;
            }

            MockEnemyMemory decayedMemory       = memory;
            decayedMemory.confidenceLevel       = decayedConfidence;
            decayedMemory.predictedPosition = memory.lastKnownPosition + (memory.lastKnownVelocity * timeSinceLastSeen);

            knownEnemies.push_back(decayedMemory);
        }

        return knownEnemies;
    }

    void CleanupOldMemories(float currentTime, float maxAge)
    {
        memories.erase(
            std::remove_if(
                memories.begin(),
                memories.end(),
                [currentTime, maxAge](const MockEnemyMemory &memory) {
                    if (!memory.enemy) {
                        return true;
                    }
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

// Mock PerceptionSystem that integrates all sensors
class MockPerceptionSystem
{
public:
    MockPerceptionSystem()
        : visionSensor(std::make_unique<MockVisionSensor>())
        , audioSensor(std::make_unique<MockAudioSensor>())
        , memory(std::make_unique<MockMemorySystem>())
    {
    }

    MockPerceptionSnapshot Update(const TestVector &botPos, float deltaTime, float currentTime)
    {
        MockPerceptionSnapshot snapshot;

        // Step 1: Update vision - get currently visible enemies
        snapshot.visibleEnemies = visionSensor->UpdateVision(botPos, deltaTime);

        // Step 2: Update memory with visible enemies
        for (const auto &enemyInfo : snapshot.visibleEnemies) {
            memory->UpdateMemory(enemyInfo, currentTime);
        }

        // Step 3: Get known enemies from memory (includes decay and prediction)
        snapshot.knownEnemies = memory->GetKnownEnemies(currentTime);

        // Step 4: Get recent audio events (5 second window per plan)
        snapshot.recentSounds = audioSensor->GetRecentSounds(botPos, currentTime, 5.0f);

        // Step 5: Calculate closest enemy index
        snapshot.closestEnemyIndex = SIZE_MAX;
        if (!snapshot.visibleEnemies.empty()) {
            float minDistance = FLT_MAX;
            for (size_t i = 0; i < snapshot.visibleEnemies.size(); i++) {
                if (snapshot.visibleEnemies[i].distance < minDistance) {
                    minDistance                = snapshot.visibleEnemies[i].distance;
                    snapshot.closestEnemyIndex = i;
                }
            }
        }

        // Step 6: Calculate most dangerous enemy index
        snapshot.mostDangerousEnemyIndex = SIZE_MAX;
        if (!snapshot.visibleEnemies.empty()) {
            // For now, use closest enemy as most dangerous
            snapshot.mostDangerousEnemyIndex = snapshot.closestEnemyIndex;
        }

        // Step 7: Calculate loudest sound index
        snapshot.loudestSoundIndex = SIZE_MAX;
        if (!snapshot.recentSounds.empty()) {
            float maxLoudness = 0.0f;
            for (size_t i = 0; i < snapshot.recentSounds.size(); i++) {
                if (snapshot.recentSounds[i].loudness > maxLoudness) {
                    maxLoudness                = snapshot.recentSounds[i].loudness;
                    snapshot.loudestSoundIndex = i;
                }
            }
        }

        // Step 8: Cleanup old memories
        const float maxAge = 30.0f; // MEMORY_MAX_AGE_SECONDS
        memory->CleanupOldMemories(currentTime, maxAge);

        return snapshot;
    }

    std::unique_ptr<MockVisionSensor> visionSensor;
    std::unique_ptr<MockAudioSensor>  audioSensor;
    std::unique_ptr<MockMemorySystem> memory;
};

// Test fixture
class PerceptionSystemTest : public ::testing::Test
{
protected:
    MockPerceptionSystem system;
};

// ============================================================================
// Test 1: Update() integrates all sensors correctly
// ============================================================================
TEST_F(PerceptionSystemTest, Update_IntegratesAllSensors)
{
    const TestVector botPos(0, 0, 0);
    const float      currentTime = 10.0f;

    // Create mock enemies
    MockSentient enemy1(1);
    MockSentient enemy2(2);
    MockSentient enemy3(3);

    // Configure vision sensor with 3 visible enemies
    MockEnemyInfo visibleEnemy1;
    visibleEnemy1.entity   = &enemy1;
    visibleEnemy1.position = TestVector(100, 0, 0);
    visibleEnemy1.distance = 100.0f;

    MockEnemyInfo visibleEnemy2;
    visibleEnemy2.entity   = &enemy2;
    visibleEnemy2.position = TestVector(50, 0, 0);
    visibleEnemy2.distance = 50.0f;

    MockEnemyInfo visibleEnemy3;
    visibleEnemy3.entity   = &enemy3;
    visibleEnemy3.position = TestVector(200, 0, 0);
    visibleEnemy3.distance = 200.0f;

    system.visionSensor->visibleEnemies.push_back(visibleEnemy1);
    system.visionSensor->visibleEnemies.push_back(visibleEnemy2);
    system.visionSensor->visibleEnemies.push_back(visibleEnemy3);

    // Process audio events
    system.audioSensor->ProcessEvent(0, TestVector(300, 0, 0), 0.8f, currentTime - 1.0f); // Weapon fire
    system.audioSensor->ProcessEvent(10, TestVector(150, 0, 0), 0.5f, currentTime - 2.0f); // Footstep
    system.audioSensor->ProcessEvent(2, TestVector(400, 0, 0), 1.0f, currentTime - 6.0f); // Old explosion (6s ago)

    // Update perception system
    MockPerceptionSnapshot snapshot = system.Update(botPos, 0.1f, currentTime);

    // Verify vision sensor integration
    EXPECT_EQ(snapshot.visibleEnemies.size(), 3);

    // Verify memory system integration (all visible enemies should be in memory)
    EXPECT_EQ(snapshot.knownEnemies.size(), 3);
    for (const auto &memory : snapshot.knownEnemies) {
        EXPECT_TRUE(FloatEquals(memory.confidenceLevel, 1.0f)); // Just seen = full confidence
        EXPECT_EQ(memory.timesSpotted, 1);
    }

    // Verify audio sensor integration (5 second window filters out 6s old event)
    EXPECT_EQ(snapshot.recentSounds.size(), 2); // Only weapon fire and footstep
}

// ============================================================================
// Test 2: Update() calculates closest enemy correctly
// ============================================================================
TEST_F(PerceptionSystemTest, Update_CalculatesClosestEnemy)
{
    const TestVector botPos(0, 0, 0);
    const float      currentTime = 10.0f;

    // Create mock enemies at different distances
    MockSentient enemy1(1);
    MockSentient enemy2(2);
    MockSentient enemy3(3);

    MockEnemyInfo farEnemy;
    farEnemy.entity   = &enemy1;
    farEnemy.position = TestVector(200, 0, 0);
    farEnemy.distance = 200.0f;

    MockEnemyInfo closestEnemy;
    closestEnemy.entity   = &enemy2;
    closestEnemy.position = TestVector(50, 0, 0);
    closestEnemy.distance = 50.0f;

    MockEnemyInfo midEnemy;
    midEnemy.entity   = &enemy3;
    midEnemy.position = TestVector(100, 0, 0);
    midEnemy.distance = 100.0f;

    system.visionSensor->visibleEnemies.push_back(farEnemy);
    system.visionSensor->visibleEnemies.push_back(closestEnemy);
    system.visionSensor->visibleEnemies.push_back(midEnemy);

    // Update perception system
    MockPerceptionSnapshot snapshot = system.Update(botPos, 0.1f, currentTime);

    // Verify closest enemy is calculated correctly
    EXPECT_NE(snapshot.closestEnemyIndex, SIZE_MAX);
    EXPECT_EQ(snapshot.closestEnemyIndex, 1); // Index 1 is the 50-unit enemy

    MockEnemyInfo *closest = snapshot.GetClosestEnemy();
    ASSERT_NE(closest, nullptr);
    EXPECT_TRUE(FloatEquals(closest->distance, 50.0f));
    EXPECT_EQ(closest->entity, &enemy2);

    // Most dangerous should be same as closest for now
    EXPECT_EQ(snapshot.mostDangerousEnemyIndex, snapshot.closestEnemyIndex);
}

// ============================================================================
// Test 3: Update() identifies loudest sound correctly
// ============================================================================
TEST_F(PerceptionSystemTest, Update_IdentifiesLoudestSound)
{
    const TestVector botPos(0, 0, 0);
    const float      currentTime = 10.0f;

    // Process audio events with different loudness values
    // Note: Loudness is affected by inverse square law distance attenuation

    // Event 1: Weapon fire at 100 units, loudness 0.5
    // After attenuation: 0.5 * (100^2 / 100^2) = 0.5 (LOUDEST)
    system.audioSensor->ProcessEvent(0, TestVector(100, 0, 0), 0.5f, currentTime - 1.0f);

    // Event 2: Explosion at 150 units, loudness 0.8
    // After attenuation: 0.8 * (100^2 / 150^2) = 0.8 * 0.444 = 0.355
    system.audioSensor->ProcessEvent(2, TestVector(150, 0, 0), 0.8f, currentTime - 2.0f);

    // Event 3: Footstep at 200 units, loudness 0.3
    // After attenuation: 0.3 * (100^2 / 200^2) = 0.3 * 0.25 = 0.075
    system.audioSensor->ProcessEvent(10, TestVector(200, 0, 0), 0.3f, currentTime - 0.5f);

    // Update perception system
    MockPerceptionSnapshot snapshot = system.Update(botPos, 0.1f, currentTime);

    // Verify we have all 3 sounds
    EXPECT_EQ(snapshot.recentSounds.size(), 3);

    // Verify loudest sound is identified
    EXPECT_NE(snapshot.loudestSoundIndex, SIZE_MAX);

    MockAudioEvent *loudest = snapshot.GetLoudestSound();
    ASSERT_NE(loudest, nullptr);

    // After distance attenuation, event 1 (weapon fire at 100 units) is loudest
    EXPECT_EQ(loudest->type, 0); // Weapon fire type
    EXPECT_TRUE(FloatEquals(loudest->loudness, 0.5f, 0.01f));
}

// ============================================================================
// Edge Case Tests
// ============================================================================

// Test: Empty visible enemies returns SIZE_MAX for closestEnemyIndex
TEST_F(PerceptionSystemTest, Update_EmptyEnemies_ReturnsNoClosest)
{
    const TestVector botPos(0, 0, 0);
    const float      currentTime = 10.0f;

    // No visible enemies
    MockPerceptionSnapshot snapshot = system.Update(botPos, 0.1f, currentTime);

    EXPECT_EQ(snapshot.visibleEnemies.size(), 0);
    EXPECT_EQ(snapshot.closestEnemyIndex, SIZE_MAX);
    EXPECT_EQ(snapshot.mostDangerousEnemyIndex, SIZE_MAX);
    EXPECT_EQ(snapshot.GetClosestEnemy(), nullptr);
    EXPECT_EQ(snapshot.GetMostDangerousEnemy(), nullptr);
}

// Test: No audio events returns SIZE_MAX for loudestSoundIndex
TEST_F(PerceptionSystemTest, Update_NoSounds_ReturnsNoLoudest)
{
    const TestVector botPos(0, 0, 0);
    const float      currentTime = 10.0f;

    // No audio events
    MockPerceptionSnapshot snapshot = system.Update(botPos, 0.1f, currentTime);

    EXPECT_EQ(snapshot.recentSounds.size(), 0);
    EXPECT_EQ(snapshot.loudestSoundIndex, SIZE_MAX);
    EXPECT_EQ(snapshot.GetLoudestSound(), nullptr);
}
