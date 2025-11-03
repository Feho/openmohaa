// bt_actions_tactical.h
// Tactical combat actions for behavior trees
// Added in OPM - Phase 3 Task 3.1e: Tactical Combat & Retreat System

#ifndef __BT_ACTIONS_TACTICAL_H__
#define __BT_ACTIONS_TACTICAL_H__

#include "behavior_tree.h"

/**
 * Tactical retreat: Find safe position away from enemies and retreat.
 * Multi-frame action that returns RUNNING while retreating.
 *
 * Blackboard inputs:
 * - BOT (BotController*)
 * - SELECTED_TARGET (Sentient*) - Enemy to retreat from
 *
 * Blackboard outputs:
 * - RETREAT_POSITION (Vector) - Safe position to retreat to
 *
 * @param bb Blackboard
 * @param deltaTime Frame delta time
 * @return RUNNING (retreating), SUCCESS (reached safety), FAILURE (no safe position)
 */
BTNode::Status Action_TacticalRetreat(Blackboard &bb, float deltaTime);

/**
 * Safe reload: Reload weapon, preferably behind cover.
 * Multi-frame action that returns RUNNING while reloading.
 *
 * Blackboard inputs:
 * - BOT (BotController*)
 * - COVER_STATE (int) - Current cover state
 *
 * Blackboard outputs:
 * - RELOAD_START_TIME (float) - When reload started
 *
 * @param bb Blackboard
 * @param deltaTime Frame delta time
 * @return RUNNING (reloading), SUCCESS (complete), FAILURE (no weapon/ammo)
 */
BTNode::Status Action_SafeReload(Blackboard &bb, float deltaTime);

/**
 * Suppression fire: Fire at last-known enemy position to suppress.
 * Fires for 2-3 seconds even if enemy not visible.
 *
 * Blackboard inputs:
 * - BOT (BotController*)
 * - LAST_KNOWN_ENEMY_POS (Vector) - Last known enemy position
 *
 * Blackboard outputs:
 * - SUPPRESS_START_TIME (float) - When suppression started
 *
 * @param bb Blackboard
 * @param deltaTime Frame delta time
 * @return RUNNING (suppressing), SUCCESS (duration complete), FAILURE (no target)
 */
BTNode::Status Action_SuppressFire(Blackboard &bb, float deltaTime);

#endif // __BT_ACTIONS_TACTICAL_H__
