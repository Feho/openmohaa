// Added in OPM - Phase 3 Task 3.1c
// bt_conditions_range.h: Range-based combat conditions for behavior trees

#ifndef __BT_CONDITIONS_RANGE_H__
#define __BT_CONDITIONS_RANGE_H__

#include "behavior_tree.h"

/**
 * Condition: EnemyTooClose
 * Purpose: Checks if the current target is too close for effective weapon use
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - TARGET_DISTANCE (float): Distance to target
 * 
 * Returns:
 * - SUCCESS: Target is closer than weapon's minimum range
 * - FAILURE: Target is at acceptable distance or no target
 */
BTNode::Status Condition_EnemyTooClose(Blackboard& bb);

/**
 * Condition: EnemyTooFar
 * Purpose: Checks if the current target is too far for effective weapon use
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - TARGET_DISTANCE (float): Distance to target
 * 
 * Returns:
 * - SUCCESS: Target is farther than weapon's maximum range
 * - FAILURE: Target is within range or no target
 */
BTNode::Status Condition_EnemyTooFar(Blackboard& bb);

/**
 * Condition: InOptimalRange
 * Purpose: Checks if the current target is within optimal weapon range
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - TARGET_DISTANCE (float): Distance to target
 * - PROFILE (BotProfile*): Bot profile for range preferences
 * 
 * Returns:
 * - SUCCESS: Target is within optimal range (between min and preferred max)
 * - FAILURE: Target is outside optimal range or no target
 */
BTNode::Status Condition_InOptimalRange(Blackboard& bb);

#endif // __BT_CONDITIONS_RANGE_H__
