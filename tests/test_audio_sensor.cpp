// Tests for AudioSensor (Phase 2 Task 2A.1.4)

#include "test_utilities.h"
#include <gtest/gtest.h>
#include <deque>
#include <algorithm>

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
    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
    //  Changed from std::vector to std::deque to match production code
    std::deque<MockAudioEvent> eventQueue;

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

        // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
        //  Use pop_front() for O(1) removal (now using deque)
        if (eventQueue.size() > 100) {
            eventQueue.pop_front();
        }
    }

    std::vector<MockAudioEvent> GetRecentSounds(const TestVector &botPos, float currentTime, float timeWindow)
    {
        std::vector<MockAudioEvent> recentSounds;
        recentSounds.reserve(eventQueue.size());

        // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
        //  Use const reference to avoid copying
        for (const auto& event : eventQueue) {
            if (currentTime - event.timestamp <= timeWindow) {
                MockAudioEvent modifiedEvent = event;

                // Calculate direction
                TestVector toSound = modifiedEvent.position - botPos;
                float      distance = toSound.length();

                if (distance > 0.001f) {
                    toSound.normalize();
                    modifiedEvent.estimatedDirection = toSound;

                    // Calculate confidence
                    const float maxAudioDistance = 2000.0f;
                    modifiedEvent.confidence = 1.0f - std::min(distance / maxAudioDistance, 1.0f);

                    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
                    //  Use true inverse square law to match production code
                    if (distance > 1.0f) {
                        const float refDist = 100.0f;
                        const float attenuationFactor = (refDist * refDist) / (distance * distance);
                        modifiedEvent.loudness *= std::min(attenuationFactor, 1.0f);
                    }
                } else {
                    modifiedEvent.estimatedDirection = TestVector(0, 0, 0);
                    modifiedEvent.confidence = 1.0f;
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

// Added in OPM - Phase 2 Task 2A.1.4 Code Review
//  Edge case tests to verify queue overflow, zero distance, and inverse square attenuation

// Test 4: Queue overflow behavior
TEST_F(AudioSensorTest, ProcessEvent_HandlesQueueOverflow)
{
    // Fill queue to exactly max capacity
    for (int i = 0; i < 100; i++) {
        sensor.ProcessEvent(0, TestVector(i * 10.0f, 0, 0), 1.0f, static_cast<float>(i));
    }
    EXPECT_EQ(sensor.eventQueue.size(), 100);

    // Add one more - should pop oldest
    sensor.ProcessEvent(0, TestVector(1000, 0, 0), 1.0f, 100.0f);
    EXPECT_EQ(sensor.eventQueue.size(), 100);

    // Verify oldest was removed (timestamp should now be 1.0, not 0.0)
    EXPECT_GT(sensor.eventQueue.front().timestamp, 0.5f);
}

// Test 5: Zero distance sound
TEST_F(AudioSensorTest, GetRecentSounds_HandlesZeroDistance)
{
    TestVector botPos(100, 100, 100);

    // Sound at exact bot position
    sensor.ProcessEvent(0, botPos, 1.0f, 0.0f);

    std::vector<MockAudioEvent> recent = sensor.GetRecentSounds(botPos, 0.0f, 10.0f);

    ASSERT_EQ(recent.size(), 1);
    EXPECT_FLOAT_EQ(recent[0].confidence, 1.0f);
    EXPECT_NEAR(recent[0].estimatedDirection.length(), 0.0f, 0.01f);
}

// Test 6: Loudness attenuation inverse square
TEST_F(AudioSensorTest, GetRecentSounds_InverseSquareAttenuation)
{
    TestVector botPos(0, 0, 0);

    // Sound at 200 units distance
    sensor.ProcessEvent(0, TestVector(200, 0, 0), 1.0f, 0.0f);

    std::vector<MockAudioEvent> recent = sensor.GetRecentSounds(botPos, 0.0f, 10.0f);

    ASSERT_EQ(recent.size(), 1);

    // Inverse square: (100*100) / (200*200) = 10000/40000 = 0.25
    EXPECT_NEAR(recent[0].loudness, 0.25f, 0.01f);
}
