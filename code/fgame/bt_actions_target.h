// Added in OPM - Phase 3 Task 3.1a
// bt_actions_target.h: Target selection and tracking actions for behavior trees

#ifndef __BT_ACTIONS_TARGET_H__
#define __BT_ACTIONS_TARGET_H__

/**
 * Register target selection actions and conditions
 * 
 * Actions:
 * - SelectTarget: Scan visible enemies and select best target
 * 
 * Conditions:
 * - HasValidTarget: Check if current target is valid and attackable
 * - TargetVisible: Check if current target is visible
 * 
 * Required blackboard keys:
 * - "bot" (BotController*): The bot controller
 * - "perception" (PerceptionSnapshot*): Current perception data
 * - "player" (Player*): The player entity
 * - "profile" (BotProfile*): Bot profile with target selection parameters
 * 
 * Blackboard keys set by SelectTarget:
 * - "selectedTarget" (Sentient*): Selected target
 * - "targetDistance" (float): Distance to target
 * - "targetLockTime" (float): When target was locked
 * - "targetSwitched" (bool): Whether we switched targets
 * - "previousTarget" (Sentient*): Previous target for comparison
 */
void RegisterTargetActions();

#endif // __BT_ACTIONS_TARGET_H__
