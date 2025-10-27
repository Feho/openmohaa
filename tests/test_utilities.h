#pragma once

#include <gtest/gtest.h>
#include <cmath>

// Phase 1 Fix (2025-10-27): Tests now link to actual production code
// Previously, math functions were reimplemented in this file
// This violated the principle of testing production code
// Now we link directly to q_math.c and other production files

// Forward declarations
typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];

// Constants used in tests
constexpr float EPSILON = 0.0001f;
// Relaxed epsilon for cases with accumulated floating-point error
// (e.g., diagonal normalization involves sqrt which has precision loss)
constexpr float EPSILON_RELAXED = 0.001f;
constexpr float M_PI_FLOAT = 3.14159265358979323846f;

// Utility functions for testing
inline bool FloatEquals(float a, float b, float epsilon = EPSILON)
{
    return std::fabs(a - b) < epsilon;
}

inline bool Vec3Equals(const vec3_t a, const vec3_t b, float epsilon = EPSILON)
{
    return FloatEquals(a[0], b[0], epsilon) && FloatEquals(a[1], b[1], epsilon) && FloatEquals(a[2], b[2], epsilon);
}

// Minimal Vector class stub for testing
class TestVector
{
public:
    float x, y, z;

    TestVector() : x(0), y(0), z(0) {}
    TestVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    TestVector(const vec3_t v) : x(v[0]), y(v[1]), z(v[2]) {}

    float operator[](int i) const
    {
        if (i == 0)
            return x;
        if (i == 1)
            return y;
        return z;
    }

    float& operator[](int i)
    {
        if (i == 0)
            return x;
        if (i == 1)
            return y;
        return z;
    }

    TestVector operator-(const TestVector& other) const
    {
        return TestVector(x - other.x, y - other.y, z - other.z);
    }

    TestVector operator+(const TestVector& other) const
    {
        return TestVector(x + other.x, y + other.y, z + other.z);
    }

    TestVector operator*(float scalar) const
    {
        return TestVector(x * scalar, y * scalar, z * scalar);
    }

    float lengthSquared() const { return x * x + y * y + z * z; }

    float length() const { return std::sqrt(lengthSquared()); }

    float lengthXY() const { return std::sqrt(x * x + y * y); }

    float lengthXYSquared() const { return x * x + y * y; }

    void normalize()
    {
        float len = length();
        if (len > 0.0f) {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    bool equals(const TestVector& other, float epsilon = EPSILON) const
    {
        return FloatEquals(x, other.x, epsilon) && FloatEquals(y, other.y, epsilon)
            && FloatEquals(z, other.z, epsilon);
    }
};

// Production function declarations
// These link to actual game code from q_math.c - do not reimplement!
extern "C" {
    // From code/qcommon/q_math.c
    float AngleMod(float a);
    float AngleNormalize360(float angle);
    float AngleNormalize180(float angle);
    float AngleDifference(float ang1, float ang2); // Added in OPM - now public utility
    vec_t VectorNormalize2D(vec2_t v);
    float Q_clamp_float(float value, float min, float max);
}

// Q_min and Q_max are macros in q_shared.h, redeclare for tests
#ifndef Q_min
#define Q_min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef Q_max
#define Q_max(a, b) ((a) > (b) ? (a) : (b))
#endif

// Convenience macro for comparing vectors with GoogleTest
#define EXPECT_VEC3_NEAR(v1, v2, epsilon) \
    do { \
        EXPECT_NEAR((v1).x, (v2).x, epsilon); \
        EXPECT_NEAR((v1).y, (v2).y, epsilon); \
        EXPECT_NEAR((v1).z, (v2).z, epsilon); \
    } while(0)

// Type conversion helpers between TestVector and production vec_t types
void TestVectorToVec3(const TestVector& tv, vec3_t out);
TestVector Vec3ToTestVector(const vec3_t v);
void TestVectorToVec2(const TestVector& tv, vec2_t out);
TestVector Vec2ToTestVector(const vec2_t v);

// Test fixtures for common bot test scenarios
class BotTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Common setup for bot tests
    }

    void TearDown() override
    {
        // Common cleanup
    }
};
