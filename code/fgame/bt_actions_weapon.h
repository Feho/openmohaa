/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// bt_actions_weapon.h
// Behavior tree actions for weapon switching
// Added in OPM - Phase 3 Task 3.1h

#ifndef __BT_ACTIONS_WEAPON_H__
#define __BT_ACTIONS_WEAPON_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Action_SelectBestWeapon_Execute - Selects best weapon for current situation
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Evaluates all available weapons and selects best based on range, ammo, and profile
 *
 * Reads from blackboard:
 *   - TARGET_DISTANCE (float) - Distance to current target
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile with weapon preferences
 *
 * Writes to blackboard:
 *   - SELECTED_WEAPON (Weapon*) - Best weapon selected
 *   - WEAPON_SWITCH_TIME (float) - When weapon switch was initiated
 *
 * Returns:
 *   - SUCCESS: Weapon selected
 *   - FAILURE: No suitable weapon available
 */
BTNode::Status Action_SelectBestWeapon_Execute(Blackboard &blackboard, float deltaTime);

/**
 * Action_SwitchWeapon_Execute - Switches to selected weapon
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Initiates weapon switch animation
 *
 * Reads from blackboard:
 *   - SELECTED_WEAPON (Weapon*) - Weapon to switch to
 *   - PLAYER (Player*) - Player entity
 *   - WEAPON_SWITCH_TIME (float) - When switch was initiated
 *
 * Returns:
 *   - RUNNING: Switch animation in progress
 *   - SUCCESS: Switch complete
 *   - FAILURE: Cannot switch (invalid weapon, already equipped)
 */
BTNode::Status Action_SwitchWeapon_Execute(Blackboard &blackboard, float deltaTime);

#endif // __BT_ACTIONS_WEAPON_H__
