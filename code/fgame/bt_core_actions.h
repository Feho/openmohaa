// Added in OPM - Phase 2B Task 2B.2 Review Fixes
// bt_core_actions.h: Declaration for core behavior tree actions and conditions registration

#ifndef __BT_CORE_ACTIONS_H__
#define __BT_CORE_ACTIONS_H__

/**
 * Register all core behavior tree actions and conditions.
 * This function must be called during game initialization before any behavior
 * trees are loaded or executed.
 *
 * Registered Actions:
 * - AimAtEnemy: Aim at the closest visible enemy
 * - ShootEnemy: Fire weapon at closest enemy if in range and has ammo
 * - Idle: Stand still and do nothing
 * - Retreat: Move away from closest enemy
 * - MoveToSound: Move toward the loudest recent sound
 * - PatrolWaypoints: Follow patrol waypoints (TODO: not yet implemented)
 *
 * Registered Conditions:
 * - HasVisibleEnemy: Check if bot can see any enemies
 * - HasKnownEnemy: Check if bot knows about any enemies (visible or recent)
 * - LowHealth: Check if bot health is below 25%
 * - HasAmmo: Check if bot has ammunition for active weapon
 * - HeardRecentSound: Check if bot heard any sounds recently
 *
 * All registered functions require these blackboard keys:
 * - "bot" (BotController*): The bot controller
 * - "perception" (PerceptionSnapshot*): Current perception data
 * - "player" (Player*): The player entity (some actions only)
 */
void RegisterCoreBTActions();

#endif // __BT_CORE_ACTIONS_H__
