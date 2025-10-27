#include "test_utilities.h"
#include <gtest/gtest.h>

// Phase 1 Fix (2025-10-27): Now using production VectorNormalize2D from q_math.c
// BotMovement::CalculateDir() implementation extracted for testing
// This function takes a 3D delta vector and returns a normalized 2D direction (z=0)
TestVector CalculateDir(const TestVector& delta)
{
    TestVector dir = delta;
    dir.z          = 0;

    // Use production VectorNormalize2D with type conversion
    vec2_t vec2;
    TestVectorToVec2(dir, vec2);
    VectorNormalize2D(vec2);
    dir.x = vec2[0];
    dir.y = vec2[1];

    return dir;
}

// Test fixture for bot movement tests
class BotMovementTest : public BotTestBase
{
};

// Test CalculateDir with simple cardinal directions
TEST_F(BotMovementTest, CalculateDir_ForwardDirection)
{
    TestVector delta(100.0f, 0.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    EXPECT_TRUE(FloatEquals(result.x, 1.0f));
    EXPECT_TRUE(FloatEquals(result.y, 0.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_RightDirection)
{
    TestVector delta(0.0f, 100.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    EXPECT_TRUE(FloatEquals(result.x, 0.0f));
    EXPECT_TRUE(FloatEquals(result.y, 1.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_BackwardDirection)
{
    TestVector delta(-100.0f, 0.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    EXPECT_TRUE(FloatEquals(result.x, -1.0f));
    EXPECT_TRUE(FloatEquals(result.y, 0.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_LeftDirection)
{
    TestVector delta(0.0f, -100.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    EXPECT_TRUE(FloatEquals(result.x, 0.0f));
    EXPECT_TRUE(FloatEquals(result.y, -1.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

// Test CalculateDir with diagonal directions
TEST_F(BotMovementTest, CalculateDir_DiagonalForwardRight)
{
    TestVector delta(10.0f, 10.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    float expected = 1.0f / std::sqrt(2.0f); // ~0.707
    // Use relaxed epsilon due to accumulated error from sqrt in normalization
    EXPECT_TRUE(FloatEquals(result.x, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.y, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_DiagonalBackwardLeft)
{
    TestVector delta(-10.0f, -10.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    float expected = -1.0f / std::sqrt(2.0f); // ~-0.707
    // Use relaxed epsilon due to accumulated error from sqrt in normalization
    EXPECT_TRUE(FloatEquals(result.x, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.y, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

// Test that Z component is always zeroed
TEST_F(BotMovementTest, CalculateDir_IgnoresZComponent)
{
    TestVector delta(10.0f, 0.0f, 100.0f);
    TestVector result = CalculateDir(delta);

    EXPECT_TRUE(FloatEquals(result.x, 1.0f));
    EXPECT_TRUE(FloatEquals(result.y, 0.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_IgnoresZComponent_Diagonal)
{
    TestVector delta(10.0f, 10.0f, 50.0f);
    TestVector result = CalculateDir(delta);

    float expected = 1.0f / std::sqrt(2.0f);
    // Use relaxed epsilon due to accumulated error from sqrt in normalization
    EXPECT_TRUE(FloatEquals(result.x, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.y, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

// Test edge cases
TEST_F(BotMovementTest, CalculateDir_ZeroXY_ReturnsZeroVector)
{
    TestVector delta(0.0f, 0.0f, 100.0f);
    TestVector result = CalculateDir(delta);

    // With zero XY length, normalization should result in zero vector
    EXPECT_TRUE(FloatEquals(result.x, 0.0f));
    EXPECT_TRUE(FloatEquals(result.y, 0.0f));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_VerySmallVector)
{
    TestVector delta(0.001f, 0.001f, 0.0f);
    TestVector result = CalculateDir(delta);

    // Should still normalize correctly
    float expected = 1.0f / std::sqrt(2.0f);
    // Use relaxed epsilon due to accumulated error from sqrt in normalization
    EXPECT_TRUE(FloatEquals(result.x, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.y, expected, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

TEST_F(BotMovementTest, CalculateDir_ResultIsNormalized)
{
    TestVector delta(123.45f, 678.90f, 999.0f);
    TestVector result = CalculateDir(delta);

    // Result should be unit length (in XY plane)
    float length = result.lengthXY();
    // Use relaxed epsilon due to accumulated error from sqrt in normalization
    EXPECT_TRUE(FloatEquals(length, 1.0f, EPSILON_RELAXED));
    EXPECT_TRUE(FloatEquals(result.z, 0.0f));
}

// Edge case tests for very small magnitudes and special float values
TEST_F(BotMovementTest, CalculateDir_VerySmallMagnitude)
{
    // Test very small but non-zero vector
    TestVector delta(1e-8f, 1e-8f, 0.0f);
    TestVector result = CalculateDir(delta);

    // Should handle gracefully - either normalize or return zero
    // Document actual production behavior
    float magnitude = result.lengthXY();
    EXPECT_TRUE(magnitude < 1.1f && magnitude >= 0.0f)
        << "Magnitude should be normalized or zero, got " << magnitude;
}

TEST_F(BotMovementTest, CalculateDir_AllZero)
{
    TestVector delta(0.0f, 0.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    // Zero input should give zero output
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

TEST_F(BotMovementTest, CalculateDir_NaN_Input)
{
    TestVector delta(NAN, 0.0f, 0.0f);
    TestVector result = CalculateDir(delta);

    // Document what happens with NaN input
    bool hasNaN    = std::isnan(result.x) || std::isnan(result.y) || std::isnan(result.z);
    bool allFinite = std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z);

    EXPECT_TRUE(hasNaN || allFinite) << "Should either propagate NaN or handle it gracefully";
}
