// bt_conditions_investigation.h
// Behavior tree conditions for investigation system
// Added in OPM - Phase 3 Task 3.2

#ifndef __BT_CONDITIONS_INVESTIGATION_H__
#define __BT_CONDITIONS_INVESTIGATION_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Condition_HasHighConfidenceMemory - Checks if bot has high-confidence enemy memory
 *
 * Reads from blackboard:
 *   - PERCEPTION (PerceptionSnapshot*) - Perception snapshot
 *
 * Returns:
 *   - true: At least one enemy memory with confidence > 0.5
 *   - false: No high-confidence memories
 */
bool Condition_HasHighConfidenceMemory_Check(Blackboard &blackboard);

/**
 * Condition_HasInterestingSound - Checks if bot heard an interesting sound
 *
 * Reads from blackboard:
 *   - PERCEPTION (PerceptionSnapshot*) - Perception snapshot
 *
 * Returns:
 *   - true: Loudest sound is weapon fire, footstep, or grenade with priority > 0.5
 *   - false: No interesting sound
 */
bool Condition_HasInterestingSound_Check(Blackboard &blackboard);

/**
 * Condition_ReachedInvestigationTarget - Checks if bot reached investigation target
 *
 * Reads from blackboard:
 *   - REACHED_INVESTIGATION_TARGET (bool) - Whether target was reached
 *
 * Returns:
 *   - true: Bot reached investigation target
 *   - false: Target not yet reached
 */
bool Condition_ReachedInvestigationTarget_Check(Blackboard &blackboard);

/**
 * Condition_InvestigationTimedOut - Checks if investigation exceeded time limit
 *
 * Reads from blackboard:
 *   - INVESTIGATION_START_TIME (float) - When investigation started
 *
 * Returns:
 *   - true: Investigation has been running for > 10 seconds
 *   - false: Investigation still within time limit
 */
bool Condition_InvestigationTimedOut_Check(Blackboard &blackboard);

/**
 * Condition_ReachedSoundLocation - Checks if bot reached sound location
 *
 * Reads from blackboard:
 *   - REACHED_SOUND_LOCATION (bool) - Whether sound location was reached
 *
 * Returns:
 *   - true: Bot reached sound location
 *   - false: Location not yet reached
 */
bool Condition_ReachedSoundLocation_Check(Blackboard &blackboard);

/**
 * Condition_HasVisibleEnemy - Checks if bot can see an enemy
 *
 * Reads from blackboard:
 *   - PERCEPTION (PerceptionSnapshot*) - Perception snapshot
 *
 * Returns:
 *   - true: At least one visible enemy
 *   - false: No visible enemies
 */
bool Condition_HasVisibleEnemy_Check(Blackboard &blackboard);

#endif // __BT_CONDITIONS_INVESTIGATION_H__
