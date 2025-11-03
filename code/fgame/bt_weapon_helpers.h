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

// bt_weapon_helpers.h
// Shared weapon utility functions for behavior tree system
// Added in OPM - Phase 3 Task 3.1h

#ifndef __BT_WEAPON_HELPERS_H__
#define __BT_WEAPON_HELPERS_H__

#include "weapon.h"
#include "bot_profile.h"

/**
 * Calculates score for a weapon based on ammo, range, profile preference
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Scoring algorithm:
 *  - Invalid if no ammo: -1.0
 *  - Range suitability: 0.0 - 0.5 based on how well range matches
 *  - Profile preference: 0.0 - 0.3 based on bot's weapon preferences
 *  - Current weapon bonus: +0.2 to avoid rapid switching
 *
 * @param weapon The weapon to evaluate
 * @param currentWeapon Currently equipped weapon (for bonus)
 * @param targetDistance Distance to target in units
 * @param profile Bot profile with preferences
 * @return Score from -1.0 (invalid) to ~1.0 (best)
 */
float BT_CalculateWeaponScore(
    Weapon      *weapon,
    Weapon      *currentWeapon,
    float        targetDistance,
    BotProfile  *profile
);

#endif // __BT_WEAPON_HELPERS_H__
