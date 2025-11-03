// bt_actions_grenade.h
// Grenade throwing actions for behavior trees
// Added in OPM - Phase 3 Task 3.1g: Grenade System

#ifndef __BT_ACTIONS_GRENADE_H__
#define __BT_ACTIONS_GRENADE_H__

#include "behavior_tree.h"

/**
 * Calculate optimal grenade target position.
 * Uses cluster center of visible enemies.
 *
 * Blackboard inputs:
 * - PERCEPTION (PerceptionSnapshot*) - Current perception with visible enemies
 *
 * Blackboard outputs:
 * - GRENADE_TARGET_POSITION (Vector) - Calculated target position
 *
 * @param bb Blackboard
 * @param deltaTime Frame delta time
 * @return SUCCESS if target calculated, FAILURE if no enemies
 */
BTNode::Status Action_CalculateGrenadeTarget(Blackboard &bb, float deltaTime);

/**
 * Throw grenade at calculated target position.
 * Updates LAST_GRENADE_TIME on successful throw.
 *
 * Blackboard inputs:
 * - PLAYER (Player*) - The player entity
 * - GRENADE_TARGET_POSITION (Vector) - Target position for throw
 *
 * Blackboard outputs:
 * - LAST_GRENADE_TIME (float) - When grenade was thrown (level.svsTime)
 *
 * @param bb Blackboard
 * @param deltaTime Frame delta time
 * @return SUCCESS when grenade thrown, FAILURE if can't throw
 */
BTNode::Status Action_ThrowGrenade(Blackboard &bb, float deltaTime);

#endif // __BT_ACTIONS_GRENADE_H__
