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

// bt_actions_weapon.cpp
// Weapon switching action implementations
// Added in OPM - Phase 3 Task 3.1h

#include "bt_actions_weapon.h"
#include "bt_blackboard_keys.h"
#include "bt_weapon_helpers.h"
#include "bot_profile.h"
#include "g_local.h"
#include "player.h"
#include "weapon.h"

// ============================================================================
// Action_SelectBestWeapon_Execute
// ============================================================================

BTNode::Status Action_SelectBestWeapon_Execute(Blackboard &blackboard, float deltaTime)
{
    // Get required data from blackboard
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    
    if (!playerOpt || !profileOpt) {
        return BTNode::Status::FAILURE;
    }
    
    Player     *player  = *playerOpt;
    BotProfile *profile = *profileOpt;
    
    if (!player || !profile) {
        return BTNode::Status::FAILURE;
    }
    
    // Get target distance (default to preferred range if no target)
    float targetDistance = profile->GetPreferredRange();
    auto distanceOpt = blackboard.TryGet<float>(BlackboardKeys::TARGET_DISTANCE);
    if (distanceOpt) {
        targetDistance = *distanceOpt;
    }
    
    // Get current weapon
    Weapon *currentWeapon = player->GetActiveWeapon(WEAPON_MAIN);
    
    // Evaluate all weapons
    Weapon *bestWeapon = nullptr;
    float   bestScore  = -1.0f;
    
    // Added in OPM - Phase 3 Task 3.1h
    //  Iterate through player inventory to find best weapon
    for (int i = 0; i < MAX_ACTIVE_WEAPONS; i++) {
        Weapon *weapon = player->GetActiveWeapon((weaponhand_t)i);
        if (!weapon || !weapon->IsSubclassOfWeapon()) {
            continue;
        }
        
        float score = BT_CalculateWeaponScore(weapon, currentWeapon, targetDistance, profile);
        
        if (score > bestScore) {
            bestScore = score;
            bestWeapon = weapon;
        }
    }
    
    // Check if we found a valid weapon
    if (!bestWeapon || bestScore < 0.0f) {
        return BTNode::Status::FAILURE;
    }
    
    // Store selected weapon in blackboard
    blackboard.Set<Weapon *>(BlackboardKeys::SELECTED_WEAPON, bestWeapon);
    blackboard.Set<float>(BlackboardKeys::WEAPON_SWITCH_TIME, level.svsTime);
    
    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Action_SwitchWeapon_Execute
// ============================================================================

BTNode::Status Action_SwitchWeapon_Execute(Blackboard &blackboard, float deltaTime)
{
    // Get required data from blackboard
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto weaponOpt = blackboard.TryGet<Weapon *>(BlackboardKeys::SELECTED_WEAPON);
    
    if (!playerOpt || !weaponOpt) {
        return BTNode::Status::FAILURE;
    }
    
    Player *player = *playerOpt;
    Weapon *weapon = *weaponOpt;
    
    if (!player || !weapon) {
        return BTNode::Status::FAILURE;
    }
    
    // Added in OPM - Phase 3 Task 3.1h (review fix)
    //  Verify weapon is in player's inventory before attempting switch
    bool weaponInInventory = false;
    for (int i = 0; i < MAX_ACTIVE_WEAPONS; i++) {
        if (player->GetActiveWeapon((weaponhand_t)i) == weapon) {
            weaponInInventory = true;
            break;
        }
    }
    
    if (!weaponInInventory) {
        return BTNode::Status::FAILURE; // Weapon not in inventory
    }
    
    // Check if already using this weapon
    Weapon *currentWeapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (currentWeapon == weapon) {
        return BTNode::Status::SUCCESS; // Already equipped
    }
    
    // Added in OPM - Phase 3 Task 3.1h
    //  Check if weapon is in changing state (animation in progress)
    if (currentWeapon && currentWeapon->GetState() == WEAPON_CHANGING) {
        return BTNode::Status::RUNNING; // Still switching
    }
    
    // Initiate weapon switch using existing UseWeaponClass event
    int weaponClass = weapon->GetWeaponClass();
    if (weaponClass > 0) {
        Event *ev = new Event(EV_Sentient_UseWeaponClass);
        ev->AddInteger(weaponClass);
        player->ProcessEvent(ev);
        
        return BTNode::Status::RUNNING; // Switch initiated
    }
    
    return BTNode::Status::FAILURE;
}

// ============================================================================
// Helper: CalculateWeaponScore
// ============================================================================

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
static float CalculateWeaponScore(
    Weapon      *weapon,
    Weapon      *currentWeapon,
    float        targetDistance,
    BotProfile  *profile
)
{
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
