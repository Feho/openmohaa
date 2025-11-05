// bt_actions_idle.h
// Behavior tree actions for idle behaviors (curious, patrol, wander, idle)
// Added in OPM - Phase 3 Task 3.3

#ifndef __BT_ACTIONS_IDLE_H__
#define __BT_ACTIONS_IDLE_H__

#include "behavior_tree.h"

// Forward declarations
class Blackboard;

// ============================================================================
// Curious Behavior Actions
// ============================================================================

/**
 * Sets the target for curious investigation.
 * Finds curious sounds (footsteps, ambient, doors) and sets investigation target.
 * @return SUCCESS if curious sound found, FAILURE otherwise
 */
BTNode::Status Action_SetCuriousTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Moves the bot to the curious location.
 * @return SUCCESS when reached, RUNNING while moving, FAILURE on timeout/unreachable
 */
BTNode::Status Action_MoveToCuriousLocation_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Performs a quick look-around at the curious location.
 * Looks left and right briefly (shorter than full investigation).
 * @return SUCCESS when complete, RUNNING while looking, FAILURE on error
 */
BTNode::Status Action_QuickLookAround_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Clears the curious investigation state.
 * @return Always SUCCESS
 */
BTNode::Status Action_ClearCuriousState_Execute(Blackboard &blackboard, float deltaTime);

// ============================================================================
// Patrol System Actions
// ============================================================================

/**
 * Moves to the next waypoint in the patrol route.
 * @return SUCCESS when reached, RUNNING while moving, FAILURE if no route
 */
BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Pauses briefly at the current patrol waypoint.
 * @return SUCCESS when pause complete, RUNNING during pause
 */
BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Advances to the next waypoint in the patrol route.
 * Handles loop and bounce modes.
 * @return Always SUCCESS
 */
BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Continues patrol movement (placeholder for status reporting).
 * @return Always SUCCESS
 */
BTNode::Status Action_ContinuePatrol_Execute(Blackboard &blackboard, float deltaTime);

// ============================================================================
// Wander System Actions
// ============================================================================

/**
 * Picks a random nearby position to wander to.
 * @return SUCCESS if valid target found, FAILURE otherwise
 */
BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Moves to the wander target position.
 * @return SUCCESS when reached, RUNNING while moving, FAILURE on timeout
 */
BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Pauses after reaching the wander target.
 * @return SUCCESS when pause complete, RUNNING during pause
 */
BTNode::Status Action_PauseAfterWander_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Clears the wander state.
 * @return Always SUCCESS
 */
BTNode::Status Action_ClearWanderTarget_Execute(Blackboard &blackboard, float deltaTime);

// ============================================================================
// Attractive Node & Idle Actions
// ============================================================================

/**
 * Moves to an attractive map node (sniper spot, cover, etc.).
 * @return SUCCESS when reached, RUNNING while moving, FAILURE if no node
 */
BTNode::Status Action_MoveToAttractiveNode_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Uses the attractive node (look in direction, snipe, etc.).
 * @return SUCCESS when done using node, RUNNING while using
 */
BTNode::Status Action_UseAttractiveNode_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Bot stands in place (stops all movement).
 * @return Always SUCCESS
 */
BTNode::Status Action_StandInPlace_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Occasionally looks around while idle.
 * @return Always SUCCESS
 */
BTNode::Status Action_OccasionalLookAround_Execute(Blackboard &blackboard, float deltaTime);

// ============================================================================
// Registration Function
// ============================================================================

/**
 * Registers all idle actions with the behavior tree action registry.
 * Call this during game initialization.
 */
void RegisterIdleActions();

#endif // __BT_ACTIONS_IDLE_H__
