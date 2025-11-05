// bt_conditions_idle.h
// Behavior tree conditions for idle behaviors
// Added in OPM - Phase 3 Task 3.3

#ifndef __BT_CONDITIONS_IDLE_H__
#define __BT_CONDITIONS_IDLE_H__

#include "behavior_tree.h"

// Forward declarations
class Blackboard;

// ============================================================================
// Curious Behavior Conditions
// ============================================================================

/**
 * Checks if there is a curious sound event (footsteps, ambient, etc.).
 * Filters out weapon fire and urgent sounds (those are for investigation).
 * @return true if curious sound detected, false otherwise
 */
bool Condition_HasCuriousSound(Blackboard &blackboard);

/**
 * Checks if the bot has reached the curious location.
 * @return true if reached, false otherwise
 */
bool Condition_ReachedCuriousLocation(Blackboard &blackboard);

// ============================================================================
// Patrol System Conditions
// ============================================================================

/**
 * Checks if the bot has a patrol route assigned.
 * @return true if patrol route exists and has waypoints, false otherwise
 */
bool Condition_HasPatrolRoute(Blackboard &blackboard);

/**
 * Checks if the bot has reached the current patrol waypoint.
 * @return true if reached, false otherwise
 */
bool Condition_ReachedPatrolWaypoint(Blackboard &blackboard);

// ============================================================================
// Wander System Conditions
// ============================================================================

/**
 * Checks if the bot should wander.
 * Returns true if no patrol route exists and enough time has passed since last wander.
 * @return true if should wander, false otherwise
 */
bool Condition_ShouldWander(Blackboard &blackboard);

/**
 * Checks if the bot has reached the wander target.
 * @return true if reached, false otherwise
 */
bool Condition_ReachedWanderTarget(Blackboard &blackboard);

// ============================================================================
// Attractive Node Conditions
// ============================================================================

/**
 * Checks if there is a nearby attractive node (sniper spot, cover, etc.).
 * @return true if attractive node found, false otherwise
 */
bool Condition_HasNearbyAttractiveNode(Blackboard &blackboard);

/**
 * Checks if the bot has reached the attractive node.
 * @return true if reached, false otherwise
 */
bool Condition_ReachedAttractiveNode(Blackboard &blackboard);

// ============================================================================
// Registration Function
// ============================================================================

/**
 * Registers all idle conditions with the behavior tree action registry.
 * Call this during game initialization.
 */
void RegisterIdleConditions();

#endif // __BT_CONDITIONS_IDLE_H__
