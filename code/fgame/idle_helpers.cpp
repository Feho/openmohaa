// idle_helpers.cpp
// Helper functions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#include "g_local.h"
#include "idle_helpers.h"
#include "navigate.h"
#include "playerbot.h"

// Note: IsPositionReachable and FindNearbyReachablePosition are already implemented
// in investigation_helpers.cpp and will be reused. We only need to add FindNearbyAttractiveNode.

/**
 * FindNearbyAttractiveNode - Finds an attractive tactical node near the origin
 *
 * Searches for nodes with AI_SNIPER, AI_COVER, AI_CORNER_LEFT, or AI_CORNER_RIGHT flags.
 * Scores nodes based on distance (closer is better) with bonus for sniper positions.
 * Returns the best scoring node within searchRadius, or nullptr if none found.
 *
 * @param origin - Starting position to search from
 * @param searchRadius - Maximum distance to search (default 1024 units)
 * @return Best attractive node found, or nullptr if none available
 */
PathNode *FindNearbyAttractiveNode(const Vector& origin, float searchRadius)
{
    PathNode *bestNode  = nullptr;
    float     bestScore = 0.0f;

    // Iterate through all path nodes in the map
    for (int i = 1; i <= PathSearch::nodecount; i++) {
        PathNode *node = PathSearch::pathnodes[i];
        if (!node) {
            continue;
        }

        // Check if node has attractive flags
        const bool isSniper = (node->nodeflags & AI_SNIPER) != 0;
        const bool isCover  = (node->nodeflags & AI_COVER) != 0;
        const bool isCorner = (node->nodeflags & (AI_CORNER_LEFT | AI_CORNER_RIGHT)) != 0;

        if (!isSniper && !isCover && !isCorner) {
            continue; // Not an attractive node
        }

        // Calculate distance to node
        float distance = (node->origin - origin).length();
        if (distance >= searchRadius) {
            continue; // Too far away
        }

        // Score based on distance (closer is better)
        // Score ranges from 0.0 (at searchRadius) to 1.0 (at origin)
        float score = 1.0f - (distance / searchRadius);

        // Bonus for sniper nodes (1.5x multiplier)
        if (isSniper) {
            score *= 1.5f;
        }

        // Track best scoring node
        if (score > bestScore) {
            bestScore = score;
            bestNode  = node;
        }
    }

    return bestNode;
}
