// investigation_helpers.cpp
// Helper functions for investigation behavior
// Added in OPM - Phase 3 Task 3.2

#include "investigation_helpers.h"
#include "playerbot.h"

Vector FindNearbyReachablePosition(const Vector &origin, const Vector &target, float searchRadius)
{
    const int numAttempts = 8;

    for (int i = 0; i < numAttempts; i++) {
        float angle = (i * 360.0f) / numAttempts;
        
        // Use engine AngleVectors for proper angle handling
        vec3_t angles = {0, angle, 0};
        vec3_t forward, right, up;
        AngleVectors(angles, forward, right, up);
        
        Vector offset = Vector(forward) * searchRadius;
        Vector testPos = target + offset;

        if (IsPositionReachable(origin, testPos)) {
            return testPos;
        }
    }

    return vec_zero; // None found
}

bool IsPositionReachable(const Vector &origin, const Vector &target)
{
    // Simple trace check - if we can see it and there's no solid blocking, consider it reachable
    trace_t trace = G_Trace(origin, vec_zero, vec_zero, target, nullptr, MASK_SOLID, qfalse, "IsPositionReachable");

    // If trace reaches target (or very close), it's reachable
    float distanceToTarget = (target - origin).length();
    float distanceReached  = (trace.endpos - origin).length();

    // Consider reachable if we got within 32 units
    return (distanceToTarget - distanceReached) < 32.0f;
}
