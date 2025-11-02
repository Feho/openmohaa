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

// bt_conditions_combat.h
// Combat-related condition checks for behavior trees
// Added in OPM - Phase 3 Task 3.1b

#ifndef __BT_CONDITIONS_COMBAT_H__
#define __BT_CONDITIONS_COMBAT_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Condition_IsAimedAtTarget_Check - Check if bot is aimed at target within tolerance
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function
 *
 * Reads from blackboard:
 *   - IS_AIMED_AT_TARGET (bool) - Aim state set by Action_AimAtTarget_Execute
 *
 * Returns:
 *   - true: Aimed within tolerance (profile.aim_tolerance degrees)
 *   - false: Not aimed or no aim data available
 */
bool Condition_IsAimedAtTarget_Check(Blackboard &blackboard);

/**
 * Condition_WeaponReady_Check - Check if weapon has ammo and acceptable spread
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function
 *
 * Reads from blackboard:
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - For fire discipline settings
 *
 * Returns:
 *   - true: Weapon has ammo and spread acceptable
 *   - false: No ammo or spread too high
 */
bool Condition_WeaponReady_Check(Blackboard &blackboard);

/**
 * Condition_InMeleeRange_Check - Check if target is within melee attack range
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current target
 *   - PLAYER (Player*) - Player entity
 *
 * Returns:
 *   - true: Target within secondary weapon range
 *   - false: Target too far or no target/weapon
 */
bool Condition_InMeleeRange_Check(Blackboard &blackboard);

#endif // __BT_CONDITIONS_COMBAT_H__
