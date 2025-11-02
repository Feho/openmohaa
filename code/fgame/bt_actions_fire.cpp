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

// bt_actions_fire.cpp
// Weapon firing and melee implementation
// Added in OPM - Phase 3 Task 3.1b

#include "bt_actions_fire.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "g_local.h"

// ============================================================================
// Action_FireWeapon
// ============================================================================

void Action_FireWeapon::Reset()
{
    lastStatus = Status::FAILURE;
    // Note: Burst state persists across Reset() calls to maintain firing cadence.
    // Burst state is explicitly cleared in blackboard when target is lost or
    // action fails, ensuring clean transitions.
}

BTNode::Status Action_FireWeapon::Execute(Blackboard &blackboard, float deltaTime)
{
    // Get required data from blackboard
    auto targetOpt = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    auto isAimedOpt = blackboard.TryGet<bool>(BlackboardKeys::IS_AIMED_AT_TARGET);

    if (!targetOpt || !botOpt || !playerOpt || !profileOpt) {
        return Status::FAILURE;
    }

    Sentient    *target  = *targetOpt;
    BotController *bot   = *botOpt;
    Player      *player  = *playerOpt;
    BotProfile  *profile = *profileOpt;

    if (!target || !bot || !player || !profile) {
        // Added in OPM - Phase 3 Task 3.1b (Gemini review)
        //  Clear burst state when target is lost for clean state machine transitions
        blackboard.Set<int>(BlackboardKeys::BURST_STATE, 0); // BURST_IDLE
        blackboard.Set<float>(BlackboardKeys::CONTINUOUS_FIRE_TIME, 0.0f);
        return Status::FAILURE;
    }

    // Get weapon
    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return Status::FAILURE;
    }

    // Check ammo
    if (player->client->ps.stats[STAT_AMMO] <= 0 && player->client->ps.stats[STAT_CLIPAMMO] <= 0) {
        usercmd_t &botCmd = bot->GetBotCmd();
        botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        player->ZoomOff();
        // Added in OPM - Phase 3 Task 3.1b (Gemini review)
        //  Clear burst state when out of ammo (allows reload or weapon switch to take over)
        blackboard.Set<int>(BlackboardKeys::BURST_STATE, 0); // BURST_IDLE
        blackboard.Set<float>(BlackboardKeys::CONTINUOUS_FIRE_TIME, 0.0f);
        return Status::FAILURE;
    }

    // Check range
    float distanceSq = (target->origin - player->origin).lengthSquared();
    float primaryRange = weapon->GetBulletRange(FIRE_PRIMARY) / BotConstants::ATTACK_RANGE_DIVISOR;
    float primaryRangeSq = primaryRange * primaryRange;

    if (distanceSq > primaryRangeSq) {
        usercmd_t &botCmd = bot->GetBotCmd();
        botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        player->ZoomOff();
        return Status::FAILURE;
    }

    // Added in OPM - Phase 3 Task 3.1b (Gemini review)
    //  Final line-of-sight check to prevent firing at walls when target just moved to cover
    trace_t trace = G_Trace(
        player->origin + Vector(0, 0, player->viewheight),
        vec_zero,
        vec_zero,
        target->centroid,
        player,
        MASK_SHOT,
        qfalse,
        "FireWeapon_LOS"
    );
    
    if (trace.fraction < 1.0f && trace.entityNum != target->entnum) {
        // Something is blocking line of sight
        usercmd_t &botCmd = bot->GetBotCmd();
        botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        return Status::FAILURE;
    }

    // Check max fire movement - stop if weapon requires accuracy
    if (weapon->GetMaxFireMovement() < 1 && weapon->HasAmmoInClip(FIRE_PRIMARY)) {
        float length = player->velocity.length();
        if ((length / sv_runspeed->value) > weapon->GetMaxFireMovementMult()) {
            BotMovement &movement = bot->GetMovement();
            movement.ClearMove();
        }
    }

    // Get burst state
    int burstState = BURST_IDLE;
    auto burstStateOpt = blackboard.TryGet<int>(BlackboardKeys::BURST_STATE);
    if (burstStateOpt) {
        burstState = *burstStateOpt;
    }

    float burstStartTime = 0.0f;
    auto burstStartOpt = blackboard.TryGet<float>(BlackboardKeys::BURST_START_TIME);
    if (burstStartOpt) {
        burstStartTime = *burstStartOpt;
    }

    float continuousFireTime = 0.0f;
    auto continuousFireOpt = blackboard.TryGet<float>(BlackboardKeys::CONTINUOUS_FIRE_TIME);
    if (continuousFireOpt) {
        continuousFireTime = *continuousFireOpt;
    }

    // Get profile burst parameters
    float burstLengthMin = profile->GetBurstLengthMin();
    float burstLengthMax = profile->GetBurstLengthMax();
    float burstDelayMin = profile->GetBurstDelayMin();
    float burstDelayMax = profile->GetBurstDelayMax();
    float fireDiscipline = profile->GetFireDiscipline();

    // Calculate burst duration and delay based on profile
    float burstDuration = burstLengthMin + G_Random(burstLengthMax - burstLengthMin);
    float burstDelay = burstDelayMin + G_Random(burstDelayMax - burstDelayMin);

    usercmd_t &botCmd = bot->GetBotCmd();
    float currentTime = level.svsTime;

    // Handle burst states
    if (burstState == BURST_PAUSING) {
        // In pause between bursts
        if (currentTime - burstStartTime >= burstDelay) {
            // Pause complete, return to idle
            burstState = BURST_IDLE;
            continuousFireTime = 0.0f;
            blackboard.Set<int>(BlackboardKeys::BURST_STATE, burstState);
            blackboard.Set<float>(BlackboardKeys::CONTINUOUS_FIRE_TIME, continuousFireTime);
        } else {
            // Still pausing, don't fire
            botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            return Status::RUNNING;
        }
    }

    // Check if we're aimed properly (required for semi-auto)
    bool isAimed = isAimedOpt && *isAimedOpt;

    // Handle semi-auto vs full-auto firing
    if (weapon->IsSemiAuto()) {
        // Semi-auto: tap fire only when idle and aimed
        if (player->client->ps.iViewModelAnim != VM_ANIM_IDLE
            && (player->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                || player->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
            // Not idle, don't fire
            botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            player->ZoomOff();
        } else {
            // Check spread factor
            float spreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);
            if (spreadFactor < BotConstants::WEAPON_SPREAD_THRESHOLD && isAimed) {
                // Good spread and aimed, toggle fire button for tap
                botCmd.buttons ^= BUTTON_ATTACKLEFT;
                
                // Handle zoom for scoped weapons
                if (weapon->GetZoom()) {
                    if (!player->IsZoomed()) {
                        botCmd.buttons |= BUTTON_ATTACKRIGHT;
                    } else {
                        botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                    }
                }
                
                blackboard.Set<float>(BlackboardKeys::LAST_FIRE_TIME, currentTime);
                return Status::SUCCESS;
            } else {
                // Spread too high or not aimed, wait
                BotMovement &movement = bot->GetMovement();
                movement.ClearMove();
                botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            }
        }
    } else {
        // Full-auto: burst fire control
        float spreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);
        
        // High fire discipline means we require low spread
        if (fireDiscipline > 0.5f && spreadFactor >= BotConstants::WEAPON_SPREAD_THRESHOLD) {
            // Need low spread but don't have it - stop moving and don't fire
            BotMovement &movement = bot->GetMovement();
            movement.ClearMove();
            botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            return Status::RUNNING;
        }

        if (burstState == BURST_IDLE) {
            // Start new burst
            burstState = BURST_FIRING;
            burstStartTime = currentTime;
            continuousFireTime = 0.0f;
            blackboard.Set<int>(BlackboardKeys::BURST_STATE, burstState);
            blackboard.Set<float>(BlackboardKeys::BURST_START_TIME, burstStartTime);
        }

        if (burstState == BURST_FIRING) {
            // Continue burst
            continuousFireTime += deltaTime;
            blackboard.Set<float>(BlackboardKeys::CONTINUOUS_FIRE_TIME, continuousFireTime);
            
            if (continuousFireTime >= burstDuration) {
                // Burst complete, start pause
                burstState = BURST_PAUSING;
                burstStartTime = currentTime;
                blackboard.Set<int>(BlackboardKeys::BURST_STATE, burstState);
                blackboard.Set<float>(BlackboardKeys::BURST_START_TIME, burstStartTime);
                botCmd.buttons &= ~BUTTON_ATTACKLEFT;
                return Status::SUCCESS; // Burst complete
            } else {
                // Continue firing
                botCmd.buttons |= BUTTON_ATTACKLEFT;
                blackboard.Set<float>(BlackboardKeys::LAST_FIRE_TIME, currentTime);
                return Status::RUNNING;
            }
        }
    }

    return Status::RUNNING;
}

// ============================================================================
// Action_MeleeAttack
// ============================================================================

void Action_MeleeAttack::Reset()
{
    lastStatus = Status::FAILURE;
}

BTNode::Status Action_MeleeAttack::Execute(Blackboard &blackboard, float deltaTime)
{
    // Get required data from blackboard
    auto targetOpt = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!targetOpt || !botOpt || !playerOpt) {
        return Status::FAILURE;
    }

    Sentient    *target  = *targetOpt;
    BotController *bot   = *botOpt;
    Player      *player  = *playerOpt;

    if (!target || !bot || !player) {
        return Status::FAILURE;
    }

    // Get weapon
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

    usercmd_t &botCmd = bot->GetBotCmd();

    // Clear primary fire button
    botCmd.buttons &= ~BUTTON_ATTACKLEFT;

    if (distanceSq <= meleeRangeSq) {
        // In range, toggle melee attack (secondary fire)
        botCmd.buttons ^= BUTTON_ATTACKRIGHT;
        return Status::SUCCESS;
    } else {
        // Out of range, clear melee button
        botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
        return Status::FAILURE;
    }
}
