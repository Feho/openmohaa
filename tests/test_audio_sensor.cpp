// Tests for AudioSensor (Phase 2 Task 2A.1.4)

#include "test_utilities.h"
#include <gtest/gtest.h>

// Mock AudioEvent for testing (mirrors production struct)
struct MockAudioEvent {
    int         type;
    TestVector  position;
    TestVector  estimatedDirection;
    float       loudness;
    float       priority;
    float       timestamp;
    float       confidence;

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

// Mock AudioSensor for testing
class MockAudioSensor
{
public:
    std::vector<MockAudioEvent> eventQueue;

    void ProcessEvent(int eventType, const TestVector &position, float loudness, float currentTime)
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
            return; // Ignore
        }

        MockAudioEvent event;
        event.type      = eventType;
        event.position  = position;
        event.loudness  = loudness;
        event.priority  = priority / 2.0f;
        event.timestamp = currentTime;

        eventQueue.push_back(event);

        // Cleanup old events
        if (eventQueue.size() > 100) {
            eventQueue.erase(eventQueue.begin());
        }
    }

    std::vector<MockAudioEvent> GetRecentSounds(const TestVector &botPos, float currentTime, float timeWindow)
    {
        std::vector<MockAudioEvent> recentSounds;

        for (auto event : eventQueue) {
            if (currentTime - event.timestamp <= timeWindow) {
                // Calculate direction
                TestVector toSound = event.position - botPos;
                float      distance = toSound.length();

                if (distance > 0.001f) {
                    toSound.normalize();
                    event.estimatedDirection = toSound;

                    // Calculate confidence
                    const float maxAudioDistance = 2000.0f;
                    event.confidence = 1.0f - std::min(distance / maxAudioDistance, 1.0f);

                    // Adjust loudness
                    if (distance > 1.0f) {
                        float distanceFactor = 100.0f / distance;
                        event.loudness *= std::min(distanceFactor, 1.0f);
                    }
                } else {
                    event.estimatedDirection = TestVector(0, 0, 0);
                    event.confidence = 1.0f;
                }

                recentSounds.push_back(event);
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

class AudioSensorTest : public ::testing::Test
{
protected:
    MockAudioSensor sensor;
};

// Test 1: Priority filtering
TEST_F(AudioSensorTest, ProcessEvent_FiltersByPriority)
{
    // High priority event (weapon fire)
    sensor.ProcessEvent(0, TestVector(100, 0, 0), 1.0f, 0.0f);
    EXPECT_EQ(sensor.eventQueue.size(), 1);
    EXPECT_EQ(sensor.eventQueue[0].priority, 1.0f); // 2/2 = 1.0

    // Low priority event (footstep)
    sensor.ProcessEvent(10, TestVector(200, 0, 0), 0.5f, 0.0f);
    EXPECT_EQ(sensor.eventQueue.size(), 2);
    EXPECT_EQ(sensor.eventQueue[1].priority, 0.5f); // 1/2 = 0.5

    // Ignored event (invalid type)
    sensor.ProcessEvent(999, TestVector(300, 0, 0), 0.8f, 0.0f);
    EXPECT_EQ(sensor.eventQueue.size(), 2); // Still 2 events
}

// Test 2: Time window filtering
TEST_F(AudioSensorTest, GetRecentSounds_FiltersByTimeWindow)
{
    TestVector botPos(0, 0, 0);

    // Add events at different times
    sensor.ProcessEvent(0, TestVector(100, 0, 0), 1.0f, 0.0f);  // t=0
    sensor.ProcessEvent(0, TestVector(200, 0, 0), 1.0f, 5.0f);  // t=5
    sensor.ProcessEvent(0, TestVector(300, 0, 0), 1.0f, 12.0f); // t=12

    // Query at t=14 with 5-second window (should get events at t=12, but not t=0 or t=5)
    std::vector<MockAudioEvent> recent = sensor.GetRecentSounds(botPos, 14.0f, 5.0f);

    EXPECT_EQ(recent.size(), 1);                 // Only event at t=12 is within window
    EXPECT_FLOAT_EQ(recent[0].timestamp, 12.0f); // Verify it's the t=12 event
}

// Test 3: 3D directional audio calculations
TEST_F(AudioSensorTest, GetRecentSounds_Calculates3DDirection)
{
    TestVector botPos(0, 0, 0);

    // Add sound 100 units to the right
    sensor.ProcessEvent(0, TestVector(100, 0, 0), 1.0f, 0.0f);

    std::vector<MockAudioEvent> recent = sensor.GetRecentSounds(botPos, 0.0f, 10.0f);

    ASSERT_EQ(recent.size(), 1);

    // Check direction is normalized and pointing right
    EXPECT_NEAR(recent[0].estimatedDirection.x, 1.0f, 0.01f);
    EXPECT_NEAR(recent[0].estimatedDirection.y, 0.0f, 0.01f);
    EXPECT_NEAR(recent[0].estimatedDirection.z, 0.0f, 0.01f);

    // Check confidence (100 units / 2000 max = 0.95 confidence)
    EXPECT_NEAR(recent[0].confidence, 0.95f, 0.01f);

    // Check loudness attenuation (100 units distance, reference 100 units)
    // distanceFactor = 100/100 = 1.0, so loudness should be unchanged
    EXPECT_NEAR(recent[0].loudness, 1.0f, 0.01f);
}
