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

#include "bt_conditions_combat.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "g_local.h"

// ============================================================================
// Condition_IsAimedAtTarget
// ============================================================================

void Condition_IsAimedAtTarget::Reset()
{
    lastStatus = Status::FAILURE;
}

BTNode::Status Condition_IsAimedAtTarget::Execute(Blackboard &blackboard, float deltaTime)
{
    auto isAimedOpt = blackboard.TryGet<bool>(BlackboardKeys::IS_AIMED_AT_TARGET);
    
    if (isAimedOpt && *isAimedOpt) {
        return Status::SUCCESS;
    }
    
    return Status::FAILURE;
}

// ============================================================================
// Condition_WeaponReady
// ============================================================================

void Condition_WeaponReady::Reset()
{
    lastStatus = Status::FAILURE;
}

BTNode::Status Condition_WeaponReady::Execute(Blackboard &blackboard, float deltaTime)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    
    if (!playerOpt || !profileOpt) {
        return Status::FAILURE;
    }
    
    Player *player = *playerOpt;
    BotProfile *profile = *profileOpt;
    
    if (!player || !profile) {
        return Status::FAILURE;
    }
    
    // Get active weapon
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return Status::FAILURE;
    }
    
    // Check ammo
    if (player->client->ps.stats[STAT_AMMO] <= 0 && player->client->ps.stats[STAT_CLIPAMMO] <= 0) {
        return Status::FAILURE;
    }
    
    // Added in OPM - Phase 3 Task 3.1b (Gemini review)
    //  Check if weapon is currently reloading
    if (weapon->HasAmmoInClip(FIRE_PRIMARY) == false && player->client->ps.stats[STAT_CLIPAMMO] > 0) {
        // Weapon needs reload or is reloading
        return Status::FAILURE;
    }
    
    // Check weapon spread if fire discipline is high
    float fireDiscipline = profile->GetFireDiscipline();
    if (fireDiscipline > 0.5f) {
        float spreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);
        if (spreadFactor >= BotConstants::WEAPON_SPREAD_THRESHOLD) {
            return Status::FAILURE; // Too inaccurate, need to wait
        }
    }
    
    return Status::SUCCESS;
}

// ============================================================================
// Condition_InMeleeRange
// ============================================================================

void Condition_InMeleeRange::Reset()
{
    lastStatus = Status::FAILURE;
}

BTNode::Status Condition_InMeleeRange::Execute(Blackboard &blackboard, float deltaTime)
{
    auto targetOpt = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    
    if (!targetOpt || !playerOpt) {
        return Status::FAILURE;
    }
    
    Sentient *target = *targetOpt;
    Player *player = *playerOpt;
    
    if (!target || !player) {
        return Status::FAILURE;
    }
    
    // Get active weapon and check for melee capability
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return Status::FAILURE;
    }
    
    // Check if weapon has melee secondary fire
    if (weapon->GetFireType(FIRE_SECONDARY) != FT_MELEE) {
        return Status::FAILURE;
    }
    
    // Check distance to target
    float distanceSq = (target->origin - player->origin).lengthSquared();
    float meleeRange = weapon->GetBulletRange(FIRE_SECONDARY);
    float meleeRangeSq = meleeRange * meleeRange;
    
    if (distanceSq <= meleeRangeSq) {
        return Status::SUCCESS;
    }
    
    return Status::FAILURE;
}
