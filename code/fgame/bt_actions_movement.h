// Added in OPM - Phase 3 Task 3.1c
// bt_actions_movement.h: Combat movement behavior tree actions

#ifndef __BT_ACTIONS_MOVEMENT_H__
#define __BT_ACTIONS_MOVEMENT_H__

#include "behavior_tree.h"

/**
 * Action: ApproachEnemy
 * Purpose: Moves the bot toward its selected target to get into weapon range
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - TARGET_DISTANCE (float): Distance to target
 * - PROFILE (BotProfile*): Bot profile with movement parameters
 * 
 * Blackboard Modifications:
 * - MOVING_TO_POSITION (Vector): Target position being moved to
 * 
 * Returns:
 * - RUNNING: Bot is moving toward target
 * - SUCCESS: Bot is within optimal weapon range
 * - FAILURE: No target or movement system failure
 */
BTNode::Status Action_ApproachEnemy(Blackboard& bb);

/**
 * Action: RetreatFromEnemy
 * Purpose: Backs away from target when too close for effective weapon use
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - TARGET_DISTANCE (float): Distance to target
 * - PROFILE (BotProfile*): Bot profile with movement parameters
 * 
 * Returns:
 * - RUNNING: Bot is retreating
 * - SUCCESS: Bot is at safe distance
 * - FAILURE: No target or movement system failure
 */
BTNode::Status Action_RetreatFromEnemy(Blackboard& bb);

/**
 * Action: MaintainDistance
 * Purpose: Strafes perpendicular to enemy to maintain optimal range and be harder to hit
 * 
 * Blackboard Requirements:
 * - BOT (BotController*): The bot controller
 * - SELECTED_TARGET (Sentient*): Current attack target
 * - PROFILE (BotProfile*): Bot profile with strafe parameters
 * 
 * Blackboard Modifications:
 * - STRAFE_DIRECTION (int): 1=right, -1=left
 * - STRAFE_TIMER (float): Time in current direction
 * 
 * Returns:
 * - SUCCESS: Always (continuous action)
 * - FAILURE: No target
 */
BTNode::Status Action_MaintainDistance(Blackboard& bb);

#endif // __BT_ACTIONS_MOVEMENT_H__
