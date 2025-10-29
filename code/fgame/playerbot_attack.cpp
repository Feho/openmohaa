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
#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_debug;
extern cvar_t *g_bot_cover_search_radius;
extern cvar_t *g_bot_cover_min_quality;
extern cvar_t *g_bot_target_switch_threshold;
extern cvar_t *g_bot_target_lock_time;

/*
====================
Attack state

Attack the enemy
====================
*/
void BotController::InitState_Attack(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Attack;
    func->EndState       = &BotController::State_EndAttack;
    func->ThinkState     = &BotController::State_Attack;
}

static Vector bot_origin;

static int sentients_compare(const void *elem1, const void *elem2)
{
    Entity *e1, *e2;
    float   delta[3];
    float   d1, d2;

    e1 = *(Entity **)elem1;
    e2 = *(Entity **)elem2;

    VectorSubtract(bot_origin, e1->origin, delta);
    d1 = VectorLengthSquared(delta);

    VectorSubtract(bot_origin, e2->origin, delta);
    d2 = VectorLengthSquared(delta);

    if (d2 <= d1) {
        return d1 > d2;
    } else {
        return -1;
    }
}

bool BotController::IsValidEnemy(Sentient *sent) const
{
    if (sent == controlledEnt) {
        return false;
    }

    if (sent->hidden() || (sent->flags & FL_NOTARGET)) {
        // Ignore hidden / non-target enemies
        return false;
    }

    if (sent->IsDead()) {
        // Ignore dead enemies
        return false;
    }

    if (sent->getSolidType() == SOLID_NOT) {
        // Ignore non-solid, like spectators
        return false;
    }

    if (sent->IsSubclassOfPlayer()) {
        Player *player = static_cast<Player *>(sent);

        if (g_gametype->integer >= GT_TEAM && player->GetTeam() == controlledEnt->GetTeam()) {
            return false;
        }
    } else {
        if (sent->m_Team == controlledEnt->m_Team) {
            return false;
        }
    }

    return true;
}

// Changed in OPM
//  Refactored to use SelectBestTarget() helper function
bool BotController::CheckCondition_Attack(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * BotConstants::FARPLANE_VISION_FACTOR);

    // Scan for enemies (even if we already have one, we might want to switch)
    float     bestDistanceSq = BotConstants::LARGE_DISTANCE_SQ;
    Sentient *bestEnemy      = SelectBestTarget(maxDistance, bestDistanceSq);

    // If we found a visible enemy, target it
    if (bestEnemy) {
        bool shouldSwitch = false;

        if (m_pEnemy != bestEnemy) {
            // Trying to switch to a different target - check time lock
            if (!m_pEnemy) {
                // No current target, always allow
                shouldSwitch = true;
            } else if (!IsValidEnemy(m_pEnemy)) {
                // Current target became invalid (dead, hidden, etc), always allow switch
                shouldSwitch = true;
            } else {
                // Check if enough time has passed since target lock
                float timeSinceLock = (level.svsTime - m_iTargetLockTime);
                float minLockTime   = g_bot_target_lock_time->value;

                if (timeSinceLock >= minLockTime) {
                    shouldSwitch = true;
                } else {
                    // Time lock still active, stick with current target if still visible
                    shouldSwitch = false;

                    // Keep current target if it's still in the visible list
                    for (int i = 1; i <= SentientList.NumObjects(); i++) {
                        Sentient *sent = SentientList.ObjectAt(i);
                        if (sent == m_pEnemy
                            && controlledEnt->CanSee(sent, BotConstants::DEFAULT_FOV_DEGREES, maxDistance, false)) {
                            bestEnemy      = m_pEnemy;
                            bestDistanceSq = (m_pEnemy->origin - controlledEnt->origin).lengthSquared();
                            break;
                        }
                    }
                }
            }

            if (shouldSwitch) {
                m_iEnemyEyesTag = -1;

                // Debug output for target switching
                if (m_pEnemy && g_bot_debug->integer >= 1) {
                    const char *oldName = "AI";
                    const char *newName = "AI";

                    Sentient *oldEnemy = static_cast<Sentient *>(m_pEnemy.Pointer());
                    if (oldEnemy && oldEnemy->IsSubclassOfPlayer()) {
                        oldName = static_cast<Player *>(oldEnemy)->client->pers.netname;
                    }
                    if (bestEnemy->IsSubclassOfPlayer()) {
                        newName = static_cast<Player *>(bestEnemy)->client->pers.netname;
                    }

                    gi.Printf(
                        "[BOT] %s: Switching target from %s to %s (lock time: %.1fs)\n",
                        controlledEnt->client->pers.netname,
                        oldName,
                        newName,
                        (level.svsTime - m_iTargetLockTime)
                    );
                }
            }
        } else {
            // Same target, no switch needed
            shouldSwitch = false;
        }

        // Acquire new target or refresh lock time
        if (!m_pEnemy || shouldSwitch) {
            if (!m_pEnemy) {
                m_iLastUnseenTime = level.inttime;

                // Debug output for initial target acquisition
                if (g_bot_debug->integer >= 1) {
                    const char *enemyName = "AI";
                    if (bestEnemy->IsSubclassOfPlayer()) {
                        enemyName = static_cast<Player *>(bestEnemy)->client->pers.netname;
                    }
                    gi.Printf(
                        "[BOT] %s: Acquired new target: %s at distance %.0f\n",
                        controlledEnt->client->pers.netname,
                        enemyName,
                        sqrt(bestDistanceSq)
                    );
                }
            }

            m_pEnemy          = bestEnemy;
            m_iTargetLockTime = level.svsTime;
        }

        m_vLastEnemyPos = m_pEnemy->origin;

        // Changed in OPM
        //  Refactored to use MemoryState struct
        // Update enemy memory
        memoryState.enemyMemory.enemy             = bestEnemy;
        memoryState.enemyMemory.lastKnownPosition = bestEnemy->origin;
        memoryState.enemyMemory.lastKnownVelocity = bestEnemy->velocity;
        memoryState.enemyMemory.lastSeenTime      = level.svsTime;
        memoryState.enemyMemory.confidenceLevel   = 1.0f;

        m_iAttackTime = level.inttime + BotConstants::ATTACK_REACQUIRE_DELAY;
        return true;
    }

    // No visible enemies found
    if (level.inttime > m_iAttackTime) {
        if (m_iAttackTime) {
            movement.ClearMove();
            m_iAttackTime = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_EndAttack(void)
{
    m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    controlledEnt->ZoomOff();
}

// Added in OPM
//  Validate that attack preconditions are still met
bool BotController::ValidateAttackPreconditions(void)
{
    if (!m_pEnemy || !IsValidEnemy(m_pEnemy)) {
        // Ignore dead enemies
        return false;
    }

    return true;
}

// Added in OPM
//  Scan visible enemies and select the best target based on distance and target stickiness
Sentient *BotController::SelectBestTarget(float maxDistance, float& outDistanceSq)
{
    Sentient *bestEnemy      = NULL;
    float     bestDistanceSq = 999999.0f;

    for (int i = 1; i <= SentientList.NumObjects(); i++) {
        Sentient *sent = SentientList.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        // Increased FOV from 80 to 100 degrees for better peripheral vision
        if (controlledEnt->CanSee(sent, 100, maxDistance, false)) {
            float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

            // Target stickiness: prefer current target unless new target is significantly closer
            if (!bestEnemy) {
                // No candidate yet, select this enemy
                bestEnemy      = sent;
                bestDistanceSq = distSq;
            } else {
                // Already have a candidate, only switch if new enemy is significantly closer
                float switchThreshold   = g_bot_target_switch_threshold->value;
                float switchThresholdSq = switchThreshold * switchThreshold;

                // Calculate distance advantage (positive if new enemy is closer)
                float distAdvantage = bestDistanceSq - distSq;

                // Switch if new enemy is much closer, or if we don't have a locked current target
                if (distAdvantage > switchThresholdSq || !m_pEnemy) {
                    bestEnemy      = sent;
                    bestDistanceSq = distSq;
                }
            }
        }
    }

    outDistanceSq = bestDistanceSq;
    return bestEnemy;
}

// Added in OPM
//  Calculate aim offset and aim at target or aim node
void BotController::AimAtTarget(bool canSee)
{
    if (canSee || level.inttime < m_iAttackStopAimTime) {
        Vector        vRandomOffset;
        Vector        vTarget;
        orientation_t eyes_or;

        if (m_iEnemyEyesTag == -1) {
            // Cache the tag
            m_iEnemyEyesTag = gi.Tag_NumForName(m_pEnemy->edict->tiki, "eyes bone");
        }

        if (m_iEnemyEyesTag != -1) {
            // Use the enemy's eyes bone
            m_pEnemy->GetTag(m_iEnemyEyesTag, &eyes_or);

            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), -G_Random(32));
            vTarget = eyes_or.origin;
        } else {
            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), 16 + G_Random(m_pEnemy->viewheight - 16));
            vTarget = m_pEnemy->origin;
        }

        if (level.inttime >= m_iLastAimTime + BotConstants::AIM_UPDATE_INTERVAL) {
            if (m_iEnemyEyesTag != -1) {
                m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5);
                m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5);
                m_vAimOffset[2] = -G_Random(m_pEnemy->maxs.z * 0.5);
            } else {
                m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5);
                m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5);
                m_vAimOffset[2] = 16 + G_Random(m_pEnemy->viewheight - 16);
            }
            m_iLastAimTime = level.inttime;
        }

        rotation.AimAt(vTarget + m_vAimOffset * g_bot_attack_spreadmult->value);
    } else {
        AimAtAimNode();
    }
}

// Added in OPM
//  Handle melee attack logic
// Changed in OPM
//  Now accepts weapon pointer to avoid redundant GetActiveWeapon() call
void BotController::HandleMeleeAttack(
    bool canSee, float distanceSq, float secondaryRangeSq, Weapon *weapon, bool& outMelee
)
{
    if (!weapon) {
        return;
    }

    if (weapon->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
        if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0 && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
            outMelee = true;
        } else if (distanceSq <= secondaryRangeSq) {
            outMelee = true;
        }
    }

    if (outMelee) {
        m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;

        if (distanceSq <= secondaryRangeSq) {
            m_botCmd.buttons ^= BUTTON_ATTACKRIGHT;
        } else {
            m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
        }
    }
}

// Added in OPM
//  Handle burst fire control timing
void BotController::HandleBurstControl(bool firing, int fireDelay, int maxContinuousFireTime, int maxBurstTime)
{
    if (m_iLastBurstTime) {
        if (level.inttime > m_iLastBurstTime + maxBurstTime) {
            m_iLastBurstTime      = 0;
            m_iContinuousFireTime = 0;
        } else {
            m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
        }
    } else {
        if (firing) {
            m_iContinuousFireTime += level.intframetime;
        } else {
            m_iContinuousFireTime = 0;
        }

        if (!m_iLastBurstTime && m_iContinuousFireTime > maxContinuousFireTime) {
            m_iLastBurstTime      = level.inttime;
            m_iContinuousFireTime = 0;
        }
    }
}

// Added in OPM
//  Handle weapon firing logic including semi-auto, full-auto, and zoom
// Changed in OPM
//  Accepts weapon pointer to avoid redundant GetActiveWeapon() call
void BotController::HandleWeaponFiring(
    bool    canSee,
    float   distanceSq,
    float   primaryRangeSq,
    float   secondaryRangeSq,
    Weapon *weapon,
    bool&   outNoMove,
    bool&   outFiring,
    bool&   outMelee
)
{
    if (!weapon) {
        return;
    }

    float fSpreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);

    //
    // check the fire movement speed if the weapon has a max fire movement
    //
    if (weapon->GetMaxFireMovement() < 1 && weapon->HasAmmoInClip(FIRE_PRIMARY)) {
        float length;

        length = controlledEnt->velocity.length();
        if ((length / sv_runspeed->value) > (weapon->GetMaxFireMovementMult())) {
            outNoMove = true;
            movement.ClearMove();
        }
    }

    if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0 && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        controlledEnt->ZoomOff();
    } else if (distanceSq > primaryRangeSq) {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        controlledEnt->ZoomOff();
    } else {
        //
        // Attacking
        //

        if (weapon->IsSemiAuto()) {
            if (controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                && (controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                    || controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
                m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                controlledEnt->ZoomOff();
            } else if (fSpreadFactor < BotConstants::WEAPON_SPREAD_THRESHOLD) {
                outFiring = true;
                m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                if (weapon->GetZoom()) {
                    if (!controlledEnt->IsZoomed()) {
                        m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                    } else {
                        m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                    }
                }
            } else {
                outNoMove = true;
                movement.ClearMove();
            }
        } else {
            // Changed in OPM
            //  Refactored to use CombatState struct
            // Full-auto: check if low spread required (accurate fire mode)
            if (combatState.requireLowSpread && fSpreadFactor >= BotConstants::WEAPON_SPREAD_THRESHOLD) {
                // Need low spread but don't have it - stop moving
                outNoMove = true;
                movement.ClearMove();
                m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            } else {
                outFiring = true;
                m_botCmd.buttons |= BUTTON_ATTACKLEFT;
            }
        }
    }
}

// Added in OPM
//  Execute all firing logic including weapon firing, burst control, and melee
// Changed in OPM
//  Now returns fMinDistance based on weapon range instead of accepting it as parameter
float BotController::ExecuteFiring(bool canSee, float distanceSq, bool& outNoMove, bool& outFiring, bool& outMelee)
{
    static constexpr float DEFAULT_MIN_ATTACK_DISTANCE = BotConstants::OBSTACLE_AVOIDANCE_DISTANCE;
    static constexpr float MAX_MIN_ATTACK_DISTANCE     = BotConstants::SEARCH_PATTERN_STEP;
    static constexpr float ATTACK_RANGE_DIVISOR        = 1.25f; // Safety factor for primary weapon range

    if (!canSee) {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);

        if (level.inttime > m_iLastSeenTime + BotConstants::TARGET_UNSEEN_THRESHOLD) {
            if (!m_iLastUnseenTime && g_bot_debug->integer >= 2) {
                gi.Printf(
                    "[BOT] %s: Lost sight of enemy (%.1fs ago)\n",
                    controlledEnt->client->pers.netname,
                    (level.inttime - m_iLastSeenTime) / 1000.0f
                );
            }
            m_iLastUnseenTime = level.inttime;
        }
        return DEFAULT_MIN_ATTACK_DISTANCE;
    }

    Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return DEFAULT_MIN_ATTACK_DISTANCE;
    }

    bool bCanAttack = true;
    if (m_iLastUnseenTime) {
        const float        reactionTime = Q_min(1000 * Q_min(1, distanceSq / Square(2048)), 1000);
        const unsigned int minDelay     = g_bot_attack_react_min_delay->value * 1000;
        const unsigned int randomDelay  = g_bot_attack_react_random_delay->value * 1000;
        if (level.inttime <= m_iLastUnseenTime + minDelay + G_Random(randomDelay)) {
            bCanAttack = false;
        } else {
            m_iLastUnseenTime = 0;
        }
    }

    if (!bCanAttack) {
        return DEFAULT_MIN_ATTACK_DISTANCE;
    }

    const int fireDelay                    = weapon->FireDelay(FIRE_PRIMARY) * 1000;
    float     fPrimaryBulletRange          = weapon->GetBulletRange(FIRE_PRIMARY) / ATTACK_RANGE_DIVISOR;
    float     fPrimaryBulletRangeSquared   = fPrimaryBulletRange * fPrimaryBulletRange;
    float     fSecondaryBulletRange        = weapon->GetBulletRange(FIRE_SECONDARY);
    float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;

    // Changed in OPM
    //  Refactored to use CombatState struct
    // Use tactical combat burst timing (calculated based on range and combat profile)
    const int maxcontinuousFireTime = fireDelay + (int)(combatState.burstDuration * 1000);
    const int maxBurstTime          = fireDelay + (int)(combatState.burstDelay * 1000);

    HandleWeaponFiring(
        canSee,
        distanceSq,
        fPrimaryBulletRangeSquared,
        fSecondaryBulletRangeSquared,
        weapon,
        outNoMove,
        outFiring,
        outMelee
    );

    //
    // Burst
    //
    HandleBurstControl(outFiring, fireDelay, maxcontinuousFireTime, maxBurstTime);

    m_iLastFireTime = level.inttime;

    HandleMeleeAttack(canSee, distanceSq, fSecondaryBulletRangeSquared, weapon, outMelee);

    m_iAttackTime        = level.inttime + BotConstants::SECONDS_TO_MS;
    m_iAttackStopAimTime = level.inttime + BotConstants::ATTACK_STOP_AIM_DURATION;
    m_iLastSeenTime      = level.inttime;
    m_vLastEnemyPos      = m_pEnemy->origin;

    // Changed in OPM
    //  Refactored to use MemoryState struct
    // Update enemy memory continuously while visible
    memoryState.enemyMemory.enemy             = m_pEnemy;
    memoryState.enemyMemory.lastKnownPosition = m_pEnemy->origin;
    memoryState.enemyMemory.lastKnownVelocity = m_pEnemy->velocity;
    memoryState.enemyMemory.lastSeenTime      = level.svsTime;
    memoryState.enemyMemory.confidenceLevel   = 1.0f;

    // Calculate minimum attack distance based on weapon range (matching original behavior)
    float fMinDistance = fPrimaryBulletRange;
    if (fMinDistance > MAX_MIN_ATTACK_DISTANCE) {
        fMinDistance = MAX_MIN_ATTACK_DISTANCE;
    }

    return fMinDistance;
}

// Added in OPM
//  Handle movement towards/away from enemy during attack
void BotController::UpdateAttackMovement(bool noMove, bool melee, bool canSee, float minDistanceSq)
{
    float fEnemyDistanceSquared = (controlledEnt->origin - m_vLastEnemyPos).lengthSquared();

    if ((!movement.MoveToBestAttractivePoint(5) && !movement.IsMoving())
        || (m_vOldEnemyPos != m_vLastEnemyPos && !movement.MoveDone()) || fEnemyDistanceSquared < minDistanceSq) {
        if (!melee || !canSee) {
            if (fEnemyDistanceSquared < minDistanceSq) {
                Vector vDir = controlledEnt->origin - m_vLastEnemyPos;
                VectorNormalizeFast(vDir);

                float fMinDistance = sqrt(minDistanceSq);
                movement.AvoidPath(m_vLastEnemyPos, fMinDistance, Vector(controlledEnt->orientation[1]) * 512);
            } else {
                movement.MoveTo(m_vLastEnemyPos);
            }

            if (!canSee && movement.MoveDone()) {
                // Lost track of the enemy
                ClearEnemy();
                return;
            }
        } else {
            movement.MoveTo(m_vLastEnemyPos);
        }
    }

    if (movement.IsMoving()) {
        m_iAttackTime = level.inttime + BotConstants::ATTACK_REACQUIRE_DELAY;
    }
}

// Changed in OPM
//  Refactored to use extracted helper functions for improved readability
void BotController::State_Attack(void)
{
    // Validate preconditions
    if (!ValidateAttackPreconditions()) {
        m_iAttackTime = 0;
        return;
    }

    bool  bMelee           = false;
    bool  bCanSee          = false;
    bool  bNoMove          = false;
    bool  bFiring          = false;
    float fDistanceSquared = (m_pEnemy->origin - controlledEnt->origin).lengthSquared();
    m_vOldEnemyPos         = m_vLastEnemyPos;

    bCanSee = controlledEnt->CanSee(
        m_pEnemy,
        20,
        Q_min(world->m_fAIVisionDistance, world->farplane_distance * BotConstants::FARPLANE_VISION_FACTOR),
        false
    );

    // Execute firing logic and get calculated minimum distance based on weapon range
    float fMinDistance        = ExecuteFiring(bCanSee, fDistanceSquared, bNoMove, bFiring, bMelee);
    float fMinDistanceSquared = fMinDistance * fMinDistance;

    // Aim at target
    AimAtTarget(bCanSee);

    // Update cover behavior
    UpdateCoverBehavior();

    // Update tactical combat system
    UpdateTacticalCombat();

    // Update squad coordination
    CoordinateAttack();

    // Disable cover behavior at close range - prioritize direct combat
    const float closeRangeThreshold        = 384.0f;
    const float closeRangeThresholdSquared = closeRangeThreshold * closeRangeThreshold;
    const bool  isCloseRange               = fDistanceSquared < closeRangeThresholdSquared;

    // Changed in OPM
    //  Refactored to use CoverStateData struct
    // Handle cover-based movement and firing (disabled at close range)
    if (!isCloseRange) {
        if (coverState.state == COVER_IN_COVER) {
            // In cover, not shooting
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            movement.ClearMove();
            return;
        } else if (coverState.state == COVER_MOVING_TO || coverState.state == COVER_REPOSITIONING) {
            // Moving to cover, stop shooting
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);

            if (!movement.IsMoving() || movement.MoveDone()) {
                movement.MoveTo(coverState.current.position);
            }
            return;
        }
    }
    // COVER_PEEKING, COVER_NONE, or close range: continue with normal combat behavior

    if (bNoMove) {
        return;
    }

    // Update attack movement
    UpdateAttackMovement(bNoMove, bMelee, bCanSee, fMinDistanceSquared);
}
