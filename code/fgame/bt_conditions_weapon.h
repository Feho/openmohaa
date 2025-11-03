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

// bt_conditions_weapon.h
// Behavior tree conditions for weapon state checks
// Added in OPM - Phase 3 Task 3.1h

#ifndef __BT_CONDITIONS_WEAPON_H__
#define __BT_CONDITIONS_WEAPON_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Condition_CurrentWeaponEmpty_Check - Checks if current weapon has no ammo
 *
 * Added in OPM - Phase 3 Task 3.1h
 *
 * Reads from blackboard:
 *   - PLAYER (Player*) - Player entity
 *
 * Returns:
 *   - true if current weapon has no ammo (clip and reserve)
 *   - false otherwise
 */
bool Condition_CurrentWeaponEmpty_Check(Blackboard &blackboard);

/**
 * Condition_BetterWeaponAvailable_Check - Checks if another weapon scores higher
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Evaluates if switching would be beneficial
 *
 * Reads from blackboard:
 *   - TARGET_DISTANCE (float) - Distance to target
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile
 *
 * Returns:
 *   - true if another weapon scores significantly better (> 0.3 difference)
 *   - false otherwise
 */
bool Condition_BetterWeaponAvailable_Check(Blackboard &blackboard);

/**
 * Condition_WeaponSwitchReady_Check - Checks if bot can switch weapons now
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Prevents switching during reload or other animations
 *
 * Reads from blackboard:
 *   - PLAYER (Player*) - Player entity
 *
 * Returns:
 *   - true if not currently switching, reloading, or firing
 *   - false otherwise
 */
bool Condition_WeaponSwitchReady_Check(Blackboard &blackboard);

#endif // __BT_CONDITIONS_WEAPON_H__
