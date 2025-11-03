// bt_actions_investigation.h
// Behavior tree actions for investigation system
// Added in OPM - Phase 3 Task 3.2

#ifndef __BT_ACTIONS_INVESTIGATION_H__
#define __BT_ACTIONS_INVESTIGATION_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Action_SetInvestigationTarget - Sets the target position for investigation from enemy memory
 *
 * Reads from blackboard:
 *   - PERCEPTION (PerceptionSnapshot*) - Perception snapshot with enemy memories
 *
 * Writes to blackboard:
 *   - INVESTIGATION_TARGET (Vector) - Target position to investigate
 *   - INVESTIGATION_START_TIME (float) - When investigation started (level.svsTime)
 *   - INVESTIGATION_RADIUS (float) - Search radius around target
 *   - INVESTIGATING_MEMORY_INDEX (size_t) - Index of memory being investigated (safe)
 *
 * Returns:
 *   - SUCCESS: Target set successfully
 *   - FAILURE: No high-confidence memory to investigate
 */
BTNode::Status Action_SetInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_MoveToInvestigationTarget - Moves bot toward investigation target
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - INVESTIGATION_TARGET (Vector) - Target position
 *
 * Writes to blackboard:
 *   - REACHED_INVESTIGATION_TARGET (bool) - Whether target was reached
 *
 * Returns:
 *   - RUNNING: Moving toward target (multi-frame)
 *   - SUCCESS: Reached target (within 128 units)
 *   - FAILURE: Cannot reach target (no valid path)
 */
BTNode::Status Action_MoveToInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_SearchArea - Performs systematic search of area around target
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - INVESTIGATION_TARGET (Vector) - Center position for search
 *   - INVESTIGATION_RADIUS (float) - Search radius
 *
 * Writes to blackboard:
 *   - SEARCH_PHASE (int) - Current search phase (0=look around, 1=spiral, 2=complete)
 *   - SEARCH_ANGLE (float) - Current angle in search pattern
 *   - SEARCH_TIMER (float) - Timer for look-around phase
 *   - SEARCH_COMPLETE (bool) - Whether search is complete
 *
 * Returns:
 *   - RUNNING: Searching in progress (multi-frame)
 *   - SUCCESS: Search complete
 *   - FAILURE: Unable to search
 */
BTNode::Status Action_SearchArea_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_SetSoundInvestigationTarget - Sets target to investigate sound
 *
 * Reads from blackboard:
 *   - PERCEPTION (PerceptionSnapshot*) - Perception snapshot with sound events
 *
 * Writes to blackboard:
 *   - INVESTIGATION_TARGET (Vector) - Sound origin position
 *   - INVESTIGATION_START_TIME (float) - When investigation started
 *   - INVESTIGATING_SOUND (bool) - Whether investigating a sound
 *
 * Returns:
 *   - SUCCESS: Sound target set successfully
 *   - FAILURE: No interesting sound to investigate
 */
BTNode::Status Action_SetSoundInvestigationTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_MoveToSoundLocation - Moves to sound origin
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - INVESTIGATION_TARGET (Vector) - Sound location
 *
 * Writes to blackboard:
 *   - REACHED_SOUND_LOCATION (bool) - Whether reached sound location
 *
 * Returns:
 *   - RUNNING: Moving toward sound (multi-frame)
 *   - SUCCESS: Reached sound location (within 128 units)
 *   - FAILURE: Cannot reach location
 */
BTNode::Status Action_MoveToSoundLocation_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_LookAround - Looks in multiple directions from current position
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *
 * Writes to blackboard:
 *   - LOOK_TIMER (float) - Time spent looking in current direction
 *   - LOOK_COUNT (int) - Number of directions looked in
 *   - LOOK_COMPLETE (bool) - Whether look-around is complete
 *
 * Returns:
 *   - RUNNING: Looking around in progress (multi-frame)
 *   - SUCCESS: Looked in all directions (4 total)
 *   - FAILURE: Unable to look around
 */
BTNode::Status Action_LookAround_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_MarkSoundInvestigated - Marks sound as investigated to avoid re-investigating
 *
 * Writes to blackboard:
 *   - INVESTIGATING_SOUND (bool) - Cleared
 *   - REACHED_SOUND_LOCATION (bool) - Cleared
 *   - LOOK_COMPLETE (bool) - Cleared
 *
 * Returns:
 *   - SUCCESS: Always succeeds
 */
BTNode::Status Action_MarkSoundInvestigated_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_AbandonInvestigation - Gives up investigation and cleans up state
 *
 * Reads from blackboard:
 *   - INVESTIGATING_MEMORY_INDEX (size_t) - Index of memory being investigated (optional)
 *   - PERCEPTION (PerceptionSnapshot*) - For safe memory lookup
 *
 * Writes to blackboard:
 *   - Clears all investigation-related state
 *
 * Returns:
 *   - SUCCESS: Always succeeds
 */
BTNode::Status Action_AbandonInvestigation_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_ReturnToIdle - Returns bot to idle state after investigation
 *
 * Returns:
 *   - SUCCESS: Always succeeds
 */
BTNode::Status Action_ReturnToIdle_Execute(Blackboard &blackboard, float deltaTime);

#endif // __BT_ACTIONS_INVESTIGATION_H__
