// idle_helpers.h
// Helper functions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#ifndef __IDLE_HELPERS_H__
#define __IDLE_HELPERS_H__

#include "g_local.h"

class PathNode;

// Note: IsPositionReachable and FindNearbyReachablePosition are implemented
// in investigation_helpers.cpp and will be reused here

// Attractive node helpers
PathNode *FindNearbyAttractiveNode(const Vector &origin, float searchRadius);

#endif // __IDLE_HELPERS_H__
