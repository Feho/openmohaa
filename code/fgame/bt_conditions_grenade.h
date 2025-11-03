// bt_conditions_grenade.h
// Grenade-related conditions for behavior trees
// Added in OPM - Phase 3 Task 3.1g: Grenade System

#ifndef __BT_CONDITIONS_GRENADE_H__
#define __BT_CONDITIONS_GRENADE_H__

#include "behavior_tree.h"

/**
 * Check if bot has grenades in inventory.
 *
 * Blackboard inputs:
 * - PLAYER (Player*) - The player entity
 *
 * @param bb Blackboard
 * @return true if bot has grenades
 */
bool Condition_HasGrenades(Blackboard &bb);

/**
 * Check if visible enemies are clustered together.
 * Requires at least 2 enemies within GRENADE_CLUSTER_RADIUS.
 *
 * Blackboard inputs:
 * - PERCEPTION (PerceptionSnapshot*) - Current perception
 *
 * @param bb Blackboard
 * @return true if 2+ enemies clustered
 */
bool Condition_EnemiesClustered(Blackboard &bb);

/**
 * Master check for grenade throw conditions.
 * Combines all requirements:
 * - Has grenades
 * - Enemies clustered (2+ within radius)
 * - No allies in blast zone
 * - Cooldown expired
 * - Profile grenade frequency check
 *
 * Blackboard inputs:
 * - PLAYER (Player*)
 * - PERCEPTION (PerceptionSnapshot*)
 * - PROFILE (BotProfile*)
 * - LAST_GRENADE_TIME (float) - When last grenade thrown
 *
 * @param bb Blackboard
 * @return true if all conditions met for grenade throw
 */
bool Condition_ShouldThrowGrenade(Blackboard &bb);

#endif // __BT_CONDITIONS_GRENADE_H__
