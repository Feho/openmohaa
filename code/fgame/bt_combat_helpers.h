// Added in OPM - Phase 3 Task 3.1a
// bt_combat_helpers.h: Combat-related helper functions for behavior tree actions

#ifndef __BT_COMBAT_HELPERS_H__
#define __BT_COMBAT_HELPERS_H__

#include "g_local.h"
#include "sentient.h"
#include "player.h"
#include "perception.h"

namespace BT
{
namespace Combat
{

/**
 * Validate if sentient is a valid enemy for the bot to attack
 * 
 * Checks:
 * - Not the bot itself
 * - Not hidden or FL_NOTARGET
 * - Not dead
 * - Not spectating (SOLID_NOT)
 * - Not same team
 * 
 * @param bot Bot performing the check
 * @param enemy Potential enemy to validate
 * @return true if enemy is valid and attackable
 */
bool IsValidEnemy(const Player *bot, Sentient *enemy);

/**
 * Calculate target priority score for target selection
 * Higher score = better target
 * 
 * Factors:
 * - Distance (closer is better)
 * - Target stickiness (current target gets bonus)
 * - Lock time (recent locks prevent switching)
 * 
 * @param enemy Enemy to score
 * @param currentTarget Current target (nullptr if none)
 * @param distance Distance to enemy
 * @param lockTime Time current target was locked (level.svsTime)
 * @param lockDuration How long to lock onto target (seconds)
 * @param switchThreshold Distance advantage needed to switch (units)
 * @return Priority score (higher = better target)
 */
float CalculateTargetScore(
    Sentient *enemy,
    Sentient *currentTarget,
    float     distance,
    float     lockTime,
    float     lockDuration,
    float     switchThreshold
);

/**
 * Find closest visible enemy from perception snapshot
 * 
 * @param perception Perception snapshot with visible enemies
 * @return Pointer to closest enemy info, or nullptr if none
 */
const EnemyInfo *FindClosestVisibleEnemy(const PerceptionSnapshot *perception);

// Added in OPM - Phase 3 Task 3.1g
//  Grenade system helpers

/**
 * Check if enemies are clustered together within a radius
 * 
 * @param enemies List of enemies to check
 * @param maxRadius Maximum distance from cluster center (units)
 * @return true if 2+ enemies are all within maxRadius of cluster center
 */
bool AreEnemiesClustered(const std::vector<EnemyInfo>& enemies, float maxRadius);

/**
 * Calculate geometric center of enemy cluster
 * 
 * @param enemies List of enemies
 * @return Center point (average position)
 */
Vector CalculateClusterCenter(const std::vector<EnemyInfo>& enemies);

/**
 * Check if any allies are near a position (for grenade safety)
 * 
 * @param position Position to check
 * @param safetyRadius Minimum safe distance from allies (units)
 * @param perception Perception snapshot with visible allies
 * @return true if any ally within safetyRadius of position
 */
bool HasAlliesNearPosition(Vector position, float safetyRadius, PerceptionSnapshot* perception);

// Added in OPM - Phase 3 Task 3.4
//  Utility AI system helpers

/**
 * Calculate flanking position relative to enemy
 * 
 * Creates perpendicular offset (90 degrees) from bot→enemy vector
 * Used for tactical flanking maneuvers
 * 
 * @param botPos Bot's current position
 * @param enemyPos Enemy position to flank
 * @param radius Distance from enemy for flank position (units)
 * @return Flank position (perpendicular to bot-enemy line)
 */
Vector CalculateFlankPosition(const Vector& botPos, const Vector& enemyPos, float radius);

/**
 * Test if navigation path exists to target position
 * 
 * Uses pathfinding system to validate reachability
 * Does not actually compute full path, just tests feasibility
 * 
 * @param bot Bot entity for pathfinding context
 * @param targetPos Target position to test
 * @return true if path exists and is reachable
 */
bool PathExistsTo(const Player* bot, const Vector& targetPos);

} // namespace Combat
} // namespace BT

#endif // __BT_COMBAT_HELPERS_H__
