#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>

// For testing, we include the utility curves header and implementation inline
// This avoids pulling in g_local.h dependencies
enum class CurveType {
    LINEAR,          // output = input
    EXPONENTIAL,     // output = input^exponent
    INVERSE_LINEAR,  // output = 1.0 - input
    THRESHOLD,       // output = input > threshold ? 1.0 : 0.0
    LOGISTIC         // output = 1/(1 + e^(-k*(input-0.5)))
};

// Implementation of ApplyCurve from utility_curves.cpp
float ApplyCurve(float input, CurveType type, float exponent = 2.0f, float threshold = 0.5f)
{
    // Clamp input to [0.0, 1.0] range
    float normalized = std::clamp(input, 0.0f, 1.0f);

    float output = 0.0f;

    switch (type) {
        case CurveType::LINEAR:
            // Direct passthrough: f(x) = x
            output = normalized;
            break;

        case CurveType::EXPONENTIAL:
            // Exponential curve: f(x) = x^n
            // Emphasizes high values, penalizes low values
            output = std::pow(normalized, exponent);
            break;

        case CurveType::INVERSE_LINEAR:
            // Inverted linear: f(x) = 1 - x
            // Flips the relationship
            output = 1.0f - normalized;
            break;

        case CurveType::THRESHOLD:
            // Binary threshold: f(x) = x > threshold ? 1 : 0
            // All-or-nothing decision
            output = (normalized > threshold) ? 1.0f : 0.0f;
            break;

        case CurveType::LOGISTIC:
            // S-curve (sigmoid): f(x) = 1 / (1 + e^(-k*(x-0.5)))
            // Smooth transition with adjustable steepness (k = exponent)
            {
                float k = exponent;  // Steepness parameter
                output = 1.0f / (1.0f + std::exp(-k * (normalized - 0.5f)));
            }
            break;

        default:
            output = 0.0f;
            break;
    }

    // Clamp output to [0.0, 1.0] range
    return std::clamp(output, 0.0f, 1.0f);
}

// Test LINEAR curve
TEST(UtilityCurves, LinearCurve)
{
    EXPECT_FLOAT_EQ(ApplyCurve(0.0f, CurveType::LINEAR), 0.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.5f, CurveType::LINEAR), 0.5f);
    EXPECT_FLOAT_EQ(ApplyCurve(1.0f, CurveType::LINEAR), 1.0f);

    // Verify linear relationship
    EXPECT_FLOAT_EQ(ApplyCurve(0.25f, CurveType::LINEAR), 0.25f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.75f, CurveType::LINEAR), 0.75f);
}

// Test EXPONENTIAL curve
TEST(UtilityCurves, ExponentialCurve)
{
    // With exponent = 2.0
    EXPECT_FLOAT_EQ(ApplyCurve(0.0f, CurveType::EXPONENTIAL, 2.0f), 0.0f);
    EXPECT_NEAR(ApplyCurve(0.5f, CurveType::EXPONENTIAL, 2.0f), 0.25f, 0.001f);  // 0.5^2 = 0.25
    EXPECT_FLOAT_EQ(ApplyCurve(1.0f, CurveType::EXPONENTIAL, 2.0f), 1.0f);

    // With exponent = 3.0
    EXPECT_NEAR(ApplyCurve(0.5f, CurveType::EXPONENTIAL, 3.0f), 0.125f, 0.001f); // 0.5^3 = 0.125

    // Verify it emphasizes high values
    EXPECT_GT(ApplyCurve(0.9f, CurveType::EXPONENTIAL, 2.0f), 0.8f);
}

// Test INVERSE_LINEAR curve
TEST(UtilityCurves, InverseLinearCurve)
{
    EXPECT_FLOAT_EQ(ApplyCurve(0.0f, CurveType::INVERSE_LINEAR), 1.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.5f, CurveType::INVERSE_LINEAR), 0.5f);
    EXPECT_FLOAT_EQ(ApplyCurve(1.0f, CurveType::INVERSE_LINEAR), 0.0f);

    // Verify inversion
    EXPECT_FLOAT_EQ(ApplyCurve(0.25f, CurveType::INVERSE_LINEAR), 0.75f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.75f, CurveType::INVERSE_LINEAR), 0.25f);
}

// Test THRESHOLD curve
TEST(UtilityCurves, ThresholdCurve)
{
    // Default threshold = 0.5
    EXPECT_FLOAT_EQ(ApplyCurve(0.0f, CurveType::THRESHOLD, 2.0f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.4f, CurveType::THRESHOLD, 2.0f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.5f, CurveType::THRESHOLD, 2.0f, 0.5f), 0.0f);  // Exactly on threshold = false
    EXPECT_FLOAT_EQ(ApplyCurve(0.51f, CurveType::THRESHOLD, 2.0f, 0.5f), 1.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(1.0f, CurveType::THRESHOLD, 2.0f, 0.5f), 1.0f);

    // Custom threshold = 0.75
    EXPECT_FLOAT_EQ(ApplyCurve(0.7f, CurveType::THRESHOLD, 2.0f, 0.75f), 0.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(0.8f, CurveType::THRESHOLD, 2.0f, 0.75f), 1.0f);
}

// Test LOGISTIC curve (S-curve)
TEST(UtilityCurves, LogisticCurve)
{
    // S-curve should pass through 0.5 at input = 0.5
    EXPECT_NEAR(ApplyCurve(0.5f, CurveType::LOGISTIC, 5.0f), 0.5f, 0.01f);

    // S-curve should be bounded [0, 1]
    EXPECT_GT(ApplyCurve(0.0f, CurveType::LOGISTIC, 5.0f), 0.0f);
    EXPECT_LT(ApplyCurve(0.0f, CurveType::LOGISTIC, 5.0f), 0.1f);
    EXPECT_GT(ApplyCurve(1.0f, CurveType::LOGISTIC, 5.0f), 0.9f);
    EXPECT_LT(ApplyCurve(1.0f, CurveType::LOGISTIC, 5.0f), 1.0f);

    // S-curve should be monotonic increasing
    EXPECT_LT(ApplyCurve(0.3f, CurveType::LOGISTIC, 5.0f),
              ApplyCurve(0.5f, CurveType::LOGISTIC, 5.0f));
    EXPECT_LT(ApplyCurve(0.5f, CurveType::LOGISTIC, 5.0f),
              ApplyCurve(0.7f, CurveType::LOGISTIC, 5.0f));
}

// Test clamping behavior
TEST(UtilityCurves, ClampingBehavior)
{
    // Input clamping: values < 0 should be treated as 0
    EXPECT_FLOAT_EQ(ApplyCurve(-0.5f, CurveType::LINEAR), 0.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(-1.0f, CurveType::EXPONENTIAL, 2.0f), 0.0f);

    // Input clamping: values > 1 should be treated as 1
    EXPECT_FLOAT_EQ(ApplyCurve(1.5f, CurveType::LINEAR), 1.0f);
    EXPECT_FLOAT_EQ(ApplyCurve(2.0f, CurveType::EXPONENTIAL, 2.0f), 1.0f);

    // Output should always be in [0, 1]
    for (float input = -1.0f; input <= 2.0f; input += 0.1f) {
        float output = ApplyCurve(input, CurveType::LINEAR);
        EXPECT_GE(output, 0.0f);
        EXPECT_LE(output, 1.0f);
    }
}

// Test edge cases
TEST(UtilityCurves, EdgeCases)
{
    // Zero exponent should not crash (though mathematically undefined)
    EXPECT_NO_THROW(ApplyCurve(0.5f, CurveType::EXPONENTIAL, 0.0f));

    // Very large exponent
    float result = ApplyCurve(0.9f, CurveType::EXPONENTIAL, 10.0f);
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);

    // Very large steepness for logistic
    result = ApplyCurve(0.5f, CurveType::LOGISTIC, 100.0f);
    EXPECT_NEAR(result, 0.5f, 0.1f);
}
