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

// bt_conditions_combat.cpp
// Combat condition implementations
// Added in OPM - Phase 3 Task 3.1b
// Changed in OPM - Phase 3 Task 3.1f (Gemini review)
//  Refactored to stateless functions using blackboard for state

#include "bt_conditions_combat.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "g_local.h"

// ============================================================================
// Condition_IsAimedAtTarget_Check
// ============================================================================

bool Condition_IsAimedAtTarget_Check(Blackboard &blackboard)
{
    auto isAimedOpt = blackboard.TryGet<bool>(BlackboardKeys::IS_AIMED_AT_TARGET);
    
    if (isAimedOpt && *isAimedOpt) {
        return true;
    }
    
    return false;
}

// ============================================================================
// Condition_WeaponReady_Check
// ============================================================================

bool Condition_WeaponReady_Check(Blackboard &blackboard)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    
    if (!playerOpt || !profileOpt) {
        return false;
    }
    
    Player *player = *playerOpt;
    BotProfile *profile = *profileOpt;
    
    if (!player || !profile) {
        return false;
    }
    
    // Get active weapon
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return false;
    }
    
    // Check ammo
    if (player->client->ps.stats[STAT_AMMO] <= 0 && player->client->ps.stats[STAT_CLIPAMMO] <= 0) {
        return false;
    }
    
    // Added in OPM - Phase 3 Task 3.1b (Gemini review)
    //  Check if weapon is currently reloading
    if (weapon->HasAmmoInClip(FIRE_PRIMARY) == false && player->client->ps.stats[STAT_CLIPAMMO] > 0) {
        // Weapon needs reload or is reloading
        return false;
    }
    
    // Check weapon spread if fire discipline is high
    float fireDiscipline = profile->GetFireDiscipline();
    if (fireDiscipline > 0.5f) {
        float spreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);
        if (spreadFactor >= BotConstants::WEAPON_SPREAD_THRESHOLD) {
            return false; // Too inaccurate, need to wait
        }
    }
    
    return true;
}

// ============================================================================
// Condition_InMeleeRange_Check
// ============================================================================

bool Condition_InMeleeRange_Check(Blackboard &blackboard)
{
    auto targetOpt = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    
    if (!targetOpt || !playerOpt) {
        return false;
    }
    
    Sentient *target = *targetOpt;
    Player *player = *playerOpt;
    
    if (!target || !player) {
        return false;
    }
    
    // Get active weapon and check for melee capability
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return false;
    }
    
    // Check if weapon has melee secondary fire
    if (weapon->GetFireType(FIRE_SECONDARY) != FT_MELEE) {
        return false;
    }
    
    // Check distance to target
    float distanceSq = (target->origin - player->origin).lengthSquared();
    float meleeRange = weapon->GetBulletRange(FIRE_SECONDARY);
    float meleeRangeSq = meleeRange * meleeRange;
    
    if (distanceSq <= meleeRangeSq) {
        return true;
    }
    
    return false;
}
