// investigation_helpers.h
// Helper functions for investigation behavior
// Added in OPM - Phase 3 Task 3.2

#ifndef __INVESTIGATION_HELPERS_H__
#define __INVESTIGATION_HELPERS_H__

#include "g_local.h"

/**
 * FindNearbyReachablePosition - Finds a reachable position near a target
 *
 * Tests positions in a circle around the target position to find one that
 * is reachable from the origin. Tests 8 positions at 45-degree intervals.
 *
 * @param origin - Starting position
 * @param target - Desired target position (may be unreachable)
 * @param searchRadius - Radius to search around target (default 128 units)
 * @return Reachable position near target, or vec_zero if none found
 */
Vector FindNearbyReachablePosition(const Vector &origin, const Vector &target, float searchRadius = 128.0f);

/**
 * IsPositionReachable - Checks if a position is reachable from origin
 *
 * Uses pathfinding to determine if there's a valid path.
 *
 * @param origin - Starting position
 * @param target - Target position to check
 * @return true if target is reachable, false otherwise
 */
bool IsPositionReachable(const Vector &origin, const Vector &target);

#endif // __INVESTIGATION_HELPERS_H__
