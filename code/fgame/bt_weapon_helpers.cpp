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

// bt_weapon_helpers.cpp
// Shared weapon utility function implementations
// Added in OPM - Phase 3 Task 3.1h

#include "bt_weapon_helpers.h"
#include "g_local.h"

float BT_CalculateWeaponScore(
    Weapon      *weapon,
    Weapon      *currentWeapon,
    float        targetDistance,
    BotProfile  *profile
)
{
    if (!weapon || !profile) {
        return -1.0f;
    }
    
    float score = 0.0f;
    
    // 1. Ammo check - eliminate if empty
    if (!weapon->HasAmmo(FIRE_PRIMARY)) {
        return -1.0f; // Invalid weapon
    }
    
    // 2. Range suitability (0.0 - 0.5)
    float minRange = weapon->GetMinRange();
    float maxRange = weapon->GetMaxRange();
    
    if (targetDistance < minRange || targetDistance > maxRange) {
        score += 0.0f; // Out of range
    } else {
        // In range: score based on optimal distance
        float optimalRange = (minRange + maxRange) / 2.0f;
        float distanceFromOptimal = fabs(targetDistance - optimalRange);
        float rangeScore = 1.0f - (distanceFromOptimal / maxRange);
        score += rangeScore * 0.5f;
    }
    
    // 3. Profile preference (0.0 - 0.3)
    float preference = profile->GetWeaponPreference(weapon->GetWeaponClass());
    score += preference * 0.3f;
    
    // 4. Current weapon bonus (0.2 to avoid rapid switching)
    if (weapon == currentWeapon) {
        score += 0.2f;
    }
    
    return score;
}
