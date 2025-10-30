// Added in OPM - Phase 2 Task 2A.1.2
// Unit tests for VisionSensor functionality
//
// Tests verify vision sensor calculations including:
// - Distance-based visibility attenuation
// - Field of view (FOV) cone checks
// - Peripheral vs central vision detection
// - Visibility factor calculations

#include "test_utilities.h"
#include <gtest/gtest.h>
#include <cmath>

// Constants matching BotConstants from playerbot.h
namespace TestBotConstants
{
    constexpr float DEFAULT_FOV_DEGREES    = 100.0f;
    constexpr float NARROW_FOV_DEGREES     = 20.0f;
    constexpr float FARPLANE_VISION_FACTOR = 0.828f;
    constexpr float VISIBILITY_THRESHOLD   = 0.1f;
    constexpr float EPSILON                = 0.0001f;
} // namespace TestBotConstants

// Test fixture for vision sensor tests
class VisionSensorTest : public BotTestBase
{
protected:
    // Helper to calculate visibility factor (mirrors production code)
    float CalculateVisibilityFactor(float distance, float maxDistance, bool isPeripheral) const
    {
        float factor = 1.0f;

        if (maxDistance > TestBotConstants::EPSILON) {
            // Linear attenuation: 1.0 at close range, 0.0 at max distance
            factor = 1.0f - (distance / maxDistance);
            factor = Q_max(0.0f, factor); // Clamp to [0, 1]
        }

        // Reduce visibility for peripheral vision
        if (isPeripheral) {
            factor *= 0.4f; // Peripheral vision has 40% of central vision clarity
        }

        return factor;
    }

    // Helper to check if point is within FOV cone (mirrors production code)
    bool CheckFOV(
        const TestVector &botPos,
        const TestVector &botForward,
        const TestVector &targetPos,
        float             fovDegrees,
        float            &angleFromForward
    ) const
    {
        // Calculate direction to target
        TestVector toTarget = targetPos - botPos;
        toTarget.normalize();

        // Calculate dot product to get angle
        const float dotProduct = toTarget.x * botForward.x + toTarget.y * botForward.y + toTarget.z * botForward.z;

        // Clamp dot product to valid range for acos
        const float clampedDot = Q_clamp_float(dotProduct, -1.0f, 1.0f);

        // Calculate angle in degrees
        angleFromForward = static_cast<float>(acos(clampedDot)) * 180.0f / M_PI_FLOAT;

        // Check if within FOV cone (half-angle)
        const float halfFOV = fovDegrees * 0.5f;
        return angleFromForward <= halfFOV;
    }
};

// ============================================================================
// Test 1: Distance Check - Visibility attenuation based on distance
// ============================================================================

TEST_F(VisionSensorTest, DistanceCheck_LinearAttenuation)
{
    const float maxDistance = 1000.0f;

    // Test at various distances
    struct TestCase {
        float distance;
        float expectedFactor;
        const char *description;
    };

    TestCase testCases[] = {
        {0.0f, 1.0f, "Zero distance (point blank)"},
        {250.0f, 0.75f, "25% of max distance"},
        {500.0f, 0.5f, "50% of max distance"},
        {750.0f, 0.25f, "75% of max distance"},
        {1000.0f, 0.0f, "Max distance"},
        {1500.0f, 0.0f, "Beyond max distance (clamped to 0)"},
    };

    for (const auto &test : testCases) {
        float factor = CalculateVisibilityFactor(test.distance, maxDistance, false);
        EXPECT_NEAR(factor, test.expectedFactor, EPSILON) << test.description;
    }
}

TEST_F(VisionSensorTest, DistanceCheck_ZeroMaxDistance)
{
    // When max distance is 0 or negative, factor should remain 1.0
    float factor = CalculateVisibilityFactor(100.0f, 0.0f, false);
    EXPECT_NEAR(factor, 1.0f, EPSILON);

    factor = CalculateVisibilityFactor(100.0f, -100.0f, false);
    EXPECT_NEAR(factor, 1.0f, EPSILON);
}

// ============================================================================
// Test 2: FOV Check - Target within FOV cone
// ============================================================================

TEST_F(VisionSensorTest, FOVCheck_WithinFOV)
{
    const TestVector botPos(0.0f, 0.0f, 0.0f);
    const TestVector botForward(1.0f, 0.0f, 0.0f); // Looking along +X axis
    const float      fov = 90.0f;                  // 90-degree FOV (45 degrees each side)

    struct TestCase {
        TestVector  targetPos;
        bool        expectedInFOV;
        const char *description;
    };

    TestCase testCases[] = {
        {TestVector(100.0f, 0.0f, 0.0f), true, "Directly ahead"},
        {TestVector(100.0f, 40.0f, 0.0f), true, "Slightly to the right (within 45 deg)"},
        {TestVector(100.0f, -40.0f, 0.0f), true, "Slightly to the left (within 45 deg)"},
        {TestVector(70.7f, 70.7f, 0.0f), true, "Exactly 45 degrees right (edge of FOV)"},
        {TestVector(70.7f, -70.7f, 0.0f), true, "Exactly 45 degrees left (edge of FOV)"},
    };

    for (const auto &test : testCases) {
        float angleFromForward = 0.0f;
        bool  inFOV            = CheckFOV(botPos, botForward, test.targetPos, fov, angleFromForward);
        EXPECT_EQ(inFOV, test.expectedInFOV) << test.description << " (angle: " << angleFromForward << ")";
    }
}

// ============================================================================
// Test 3: FOV Check - Target outside FOV cone
// ============================================================================

TEST_F(VisionSensorTest, FOVCheck_OutsideFOV)
{
    const TestVector botPos(0.0f, 0.0f, 0.0f);
    const TestVector botForward(1.0f, 0.0f, 0.0f); // Looking along +X axis
    const float      fov = 90.0f;                  // 90-degree FOV (45 degrees each side)

    struct TestCase {
        TestVector  targetPos;
        bool        expectedInFOV;
        const char *description;
    };

    TestCase testCases[] = {
        {TestVector(-100.0f, 0.0f, 0.0f), false, "Directly behind"},
        {TestVector(0.0f, 100.0f, 0.0f), false, "90 degrees to the right (outside FOV)"},
        {TestVector(0.0f, -100.0f, 0.0f), false, "90 degrees to the left (outside FOV)"},
        {TestVector(40.0f, 100.0f, 0.0f), false, "More than 45 degrees right"},
        {TestVector(-50.0f, 50.0f, 0.0f), false, "Behind and to the right"},
    };

    for (const auto &test : testCases) {
        float angleFromForward = 0.0f;
        bool  inFOV            = CheckFOV(botPos, botForward, test.targetPos, fov, angleFromForward);
        EXPECT_EQ(inFOV, test.expectedInFOV) << test.description << " (angle: " << angleFromForward << ")";
    }
}

TEST_F(VisionSensorTest, FOVCheck_NarrowFOV)
{
    const TestVector botPos(0.0f, 0.0f, 0.0f);
    const TestVector botForward(1.0f, 0.0f, 0.0f);
    const float      narrowFOV = TestBotConstants::NARROW_FOV_DEGREES; // 20 degrees

    // Target at 8 degrees should be visible (well within 10-degree half-angle)
    TestVector targetInside(100.0f, 14.1f, 0.0f); // atan(14.1/100) ≈ 8 degrees
    float      angle1 = 0.0f;
    bool       inFOV1 = CheckFOV(botPos, botForward, targetInside, narrowFOV, angle1);
    EXPECT_TRUE(inFOV1) << "Target at ~8 degrees should be in 20-degree FOV (angle: " << angle1 << ")";

    // Target at 15 degrees should NOT be visible (outside 10-degree half-angle)
    TestVector targetOutside(96.6f, 25.9f, 0.0f); // atan(25.9/96.6) ≈ 15 degrees
    float      angle2 = 0.0f;
    bool       inFOV2 = CheckFOV(botPos, botForward, targetOutside, narrowFOV, angle2);
    EXPECT_FALSE(inFOV2) << "Target at ~15 degrees should NOT be in 20-degree FOV (angle: " << angle2 << ")";
}

// ============================================================================
// Test 4: Peripheral Vision - Peripheral vs Central detection
// ============================================================================

TEST_F(VisionSensorTest, PeripheralVision_ReducedVisibility)
{
    const float maxDistance = 1000.0f;
    const float distance    = 500.0f; // 50% of max distance

    // Central vision at 50% distance should have 0.5 visibility factor
    float centralFactor = CalculateVisibilityFactor(distance, maxDistance, false);
    EXPECT_NEAR(centralFactor, 0.5f, EPSILON) << "Central vision at 50% distance";

    // Peripheral vision should have 40% of central vision clarity
    float peripheralFactor = CalculateVisibilityFactor(distance, maxDistance, true);
    EXPECT_NEAR(peripheralFactor, 0.2f, EPSILON) << "Peripheral vision at 50% distance (0.5 * 0.4 = 0.2)";

    // Verify peripheral is exactly 40% of central
    EXPECT_NEAR(peripheralFactor, centralFactor * 0.4f, EPSILON) << "Peripheral should be 40% of central";
}

TEST_F(VisionSensorTest, PeripheralVision_CentralVsPeripheralComparison)
{
    const float maxDistance = 1000.0f;

    struct TestCase {
        float       distance;
        float       expectedCentral;
        float       expectedPeripheral;
        const char *description;
    };

    TestCase testCases[] = {
        {0.0f, 1.0f, 0.4f, "Point blank"},
        {250.0f, 0.75f, 0.3f, "25% distance"},
        {500.0f, 0.5f, 0.2f, "50% distance"},
        {750.0f, 0.25f, 0.1f, "75% distance"},
        {1000.0f, 0.0f, 0.0f, "Max distance"},
    };

    for (const auto &test : testCases) {
        float centralFactor = CalculateVisibilityFactor(test.distance, maxDistance, false);
        EXPECT_NEAR(centralFactor, test.expectedCentral, EPSILON)
            << test.description << " - central vision";

        float peripheralFactor = CalculateVisibilityFactor(test.distance, maxDistance, true);
        EXPECT_NEAR(peripheralFactor, test.expectedPeripheral, EPSILON)
            << test.description << " - peripheral vision";
    }
}

// ============================================================================
// Test 5: Visibility Factor - Overall visibility calculation
// ============================================================================

TEST_F(VisionSensorTest, VisibilityFactor_ThresholdCheck)
{
    const float maxDistance = 1000.0f;
    const float threshold   = TestBotConstants::VISIBILITY_THRESHOLD; // 0.1

    // Test distances that produce factors above and below threshold
    struct TestCase {
        float       distance;
        bool        isPeripheral;
        bool        expectedVisible; // Factor > 0.1
        const char *description;
    };

    TestCase testCases[] = {
        {0.0f, false, true, "Point blank central (factor = 1.0)"},
        {0.0f, true, true, "Point blank peripheral (factor = 0.4)"},
        {500.0f, false, true, "50% distance central (factor = 0.5)"},
        {500.0f, true, true, "50% distance peripheral (factor = 0.2)"},
        {850.0f, false, true, "85% distance central (factor = 0.15, above threshold)"},
        {700.0f, true, true, "70% distance peripheral (factor = 0.12, above threshold)"},
        {950.0f, false, false, "95% distance central (factor = 0.05, below threshold)"},
        {800.0f, true, false, "80% distance peripheral (factor = 0.08, below threshold)"},
    };

    for (const auto &test : testCases) {
        float factor    = CalculateVisibilityFactor(test.distance, maxDistance, test.isPeripheral);
        bool  isVisible = factor > threshold;
        EXPECT_EQ(isVisible, test.expectedVisible) << test.description << " (factor: " << factor << ")";
    }
}

TEST_F(VisionSensorTest, VisibilityFactor_Clamping)
{
    const float maxDistance = 1000.0f;

    // Test that visibility factor is properly clamped to [0, 1] range
    float factor = CalculateVisibilityFactor(0.0f, maxDistance, false);
    EXPECT_GE(factor, 0.0f) << "Factor should not be negative";
    EXPECT_LE(factor, 1.0f) << "Factor should not exceed 1.0";

    factor = CalculateVisibilityFactor(2000.0f, maxDistance, false);
    EXPECT_GE(factor, 0.0f) << "Factor at excessive distance should not be negative";
    EXPECT_LE(factor, 1.0f) << "Factor at excessive distance should not exceed 1.0";

    // For peripheral vision, factor is 40% of central, so max is 0.4
    factor = CalculateVisibilityFactor(0.0f, maxDistance, true);
    EXPECT_GE(factor, 0.0f) << "Peripheral factor should not be negative";
    EXPECT_LE(factor, 0.4f) << "Peripheral factor should not exceed 0.4 (40% of 1.0)";
}
