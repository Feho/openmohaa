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

// bt_conditions_weapon.cpp
// Weapon state condition implementations
// Added in OPM - Phase 3 Task 3.1h

#include "bt_conditions_weapon.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "g_local.h"
#include "player.h"
#include "weapon.h"
#include "playerbot.h"

// Forward declare helper (same as in bt_actions_weapon.cpp)
static float CalculateWeaponScore(
    Weapon      *weapon,
    Weapon      *currentWeapon,
    float        targetDistance,
    BotProfile  *profile
);

// ============================================================================
// Condition_CurrentWeaponEmpty_Check
// ============================================================================

bool Condition_CurrentWeaponEmpty_Check(Blackboard &blackboard)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    
    if (!playerOpt) {
        return false;
    }
    
    Player *player = *playerOpt;
    if (!player) {
        return false;
    }
    
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return true; // No weapon = empty
    }
    
    // Added in OPM - Phase 3 Task 3.1h
    //  Check both clip and reserve ammo
    return !weapon->HasAmmo(FIRE_PRIMARY);
}

// ============================================================================
// Condition_BetterWeaponAvailable_Check
// ============================================================================

bool Condition_BetterWeaponAvailable_Check(Blackboard &blackboard)
{
    // Get required data from blackboard
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    
    if (!playerOpt || !profileOpt) {
        return false;
    }
    
    Player     *player  = *playerOpt;
    BotProfile *profile = *profileOpt;
    
    if (!player || !profile) {
        return false;
    }
    
    // Get target distance (default to preferred range if no target)
    float targetDistance = profile->GetPreferredRange();
    auto distanceOpt = blackboard.TryGet<float>(BlackboardKeys::TARGET_DISTANCE);
    if (distanceOpt) {
        targetDistance = *distanceOpt;
    }
    
    // Get current weapon and its score
    Weapon *currentWeapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!currentWeapon) {
        return true; // No weapon = need better weapon
    }
    
    float currentScore = CalculateWeaponScore(currentWeapon, currentWeapon, targetDistance, profile);
    
    // Check all weapons for better option
    float bestScore = currentScore;
    
    // Added in OPM - Phase 3 Task 3.1h
    //  Iterate through inventory to find if better weapon exists
    for (int i = 0; i < MAX_ACTIVE_WEAPONS; i++) {
        Weapon *weapon = player->GetActiveWeapon((weaponhand_t)i);
        if (!weapon || weapon == currentWeapon || !weapon->IsSubclassOfWeapon()) {
            continue;
        }
        
        float score = CalculateWeaponScore(weapon, currentWeapon, targetDistance, profile);
        
        if (score > bestScore) {
            bestScore = score;
        }
    }
    
    // Return true if significantly better weapon available
    return (bestScore - currentScore) > BotConstants::WEAPON_SWITCH_SCORE_THRESHOLD;
}

// ============================================================================
// Condition_WeaponSwitchReady_Check
// ============================================================================

bool Condition_WeaponSwitchReady_Check(Blackboard &blackboard)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    
    if (!playerOpt) {
        return false;
    }
    
    Player *player = *playerOpt;
    if (!player) {
        return false;
    }
    
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return true; // No weapon = can switch
    }
    
    // Added in OPM - Phase 3 Task 3.1h
    //  Check weapon state to prevent switching during animations
    weaponstate_t state = weapon->GetState();
    
    return (state != WEAPON_CHANGING 
            && state != WEAPON_RELOADING 
            && state != WEAPON_FIRING);
}

// ============================================================================
// Helper: CalculateWeaponScore (duplicate from bt_actions_weapon.cpp)
// ============================================================================

/**
 * Calculates score for a weapon based on ammo, range, profile preference
 *
 * Added in OPM - Phase 3 Task 3.1h
 *  Scoring algorithm (same as in bt_actions_weapon.cpp):
 *  - Invalid if no ammo: -1.0
 *  - Range suitability: 0.0 - 0.5
 *  - Profile preference: 0.0 - 0.3
 *  - Current weapon bonus: +0.2
 */
static float CalculateWeaponScore(
    Weapon      *weapon,
    Weapon      *currentWeapon,
    float        targetDistance,
    BotProfile  *profile
)
{
    float score = 0.0f;
    
    // 1. Ammo check
    if (!weapon->HasAmmo(FIRE_PRIMARY)) {
        return -1.0f;
    }
    
    // 2. Range suitability (0.0 - 0.5)
    float minRange = weapon->GetMinRange();
    float maxRange = weapon->GetMaxRange();
    
    if (targetDistance < minRange || targetDistance > maxRange) {
        score += 0.0f;
    } else {
        float optimalRange = (minRange + maxRange) / 2.0f;
        float distanceFromOptimal = fabs(targetDistance - optimalRange);
        float rangeScore = 1.0f - (distanceFromOptimal / maxRange);
        score += rangeScore * 0.5f;
    }
    
    // 3. Profile preference (0.0 - 0.3)
    float preference = profile->GetWeaponPreference(weapon->GetWeaponClass());
    score += preference * 0.3f;
    
    // 4. Current weapon bonus
    if (weapon == currentWeapon) {
        score += 0.2f;
    }
    
    return score;
}
