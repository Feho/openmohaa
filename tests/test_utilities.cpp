#include "test_utilities.h"

// Phase 1 Fix (2025-10-27): Type conversion helpers
// Bridge between TestVector (test code) and vec_t/vec3_t (production code)

void TestVectorToVec3(const TestVector& tv, vec3_t out)
{
    out[0] = tv.x;
    out[1] = tv.y;
    out[2] = tv.z;
}

TestVector Vec3ToTestVector(const vec3_t v)
{
    return TestVector(v[0], v[1], v[2]);
}

void TestVectorToVec2(const TestVector& tv, vec2_t out)
{
    out[0] = tv.x;
    out[1] = tv.y;
}

TestVector Vec2ToTestVector(const vec2_t v)
{
    return TestVector(v[0], v[1], 0.0f);
}
