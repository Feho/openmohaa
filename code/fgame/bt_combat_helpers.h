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

} // namespace Combat
} // namespace BT

#endif // __BT_COMBAT_HELPERS_H__
