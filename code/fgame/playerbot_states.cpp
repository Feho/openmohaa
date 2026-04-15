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
// playerbot_states.cpp: Bot state machine logic
//
// This file contains all state-related functions extracted from playerbot.cpp
// to improve code organization and make state transitions easier to reason about.

#include "playerbot.h"
#include "gamecvars.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "windows.h"
#include "g_bot.h"

// Added in OPM
//  State names for debug output
static const char *botStateNames[] = {"Attack", "Curious", "Grenade", "Idle", "Weapon"};

static const char *GetStateName(int index)
{
    if (index >= 0 && index < (int)(sizeof(botStateNames) / sizeof(botStateNames[0]))) {
        return botStateNames[index];
    }
    return "Unknown";
}

/*
====================
Bot states
--------------------
____________________
--------------------
____________________
--------------------
____________________
--------------------
____________________
====================
*/

void BotController::CheckStates(void)
{
    m_StateCount = 0;

    unsigned int oldFlags = m_StateFlags;

    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        botfunc_t *func = &botfuncs[i];

        if (func->CheckCondition) {
            if ((this->*func->CheckCondition)()) {
                if (!(m_StateFlags & (1 << i))) {
                    m_StateFlags |= 1 << i;

                    // Added in OPM - Debug state transitions
                    if (g_bot_debug_state->integer) {
                        gi.Printf("BOT %s: ENTER state %s\n", controlledEnt->client->pers.netname, GetStateName(i));
                    }

                    if (func->BeginState) {
                        (this->*func->BeginState)();
                    }
                }

                if (func->ThinkState) {
                    m_StateCount++;
                    (this->*func->ThinkState)();
                }
            } else {
                if ((m_StateFlags & (1 << i))) {
                    m_StateFlags &= ~(1 << i);

                    // Added in OPM - Debug state transitions
                    if (g_bot_debug_state->integer) {
                        gi.Printf("BOT %s: EXIT state %s\n", controlledEnt->client->pers.netname, GetStateName(i));
                    }

                    if (func->EndState) {
                        (this->*func->EndState)();
                    }
                }
            }
        } else {
            if (func->ThinkState) {
                m_StateCount++;
                (this->*func->ThinkState)();
            }
        }
    }

    // Added in OPM - Debug active states (level 2)
    if (g_bot_debug_state->integer >= 2 && m_StateFlags != oldFlags) {
        char stateList[256] = {0};
        for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
            if (m_StateFlags & (1 << i)) {
                if (stateList[0]) {
                    strcat(stateList, ", ");
                }
                strcat(stateList, GetStateName(i));
            }
        }
        gi.Printf("BOT %s: Active states: [%s]\n", controlledEnt->client->pers.netname, stateList);
    }

    assert(m_StateCount);
    if (!m_StateCount) {
        gi.DPrintf("*** WARNING *** %s was stuck with no states !!!", controlledEnt->client->pers.netname);
        State_Reset();
    }
}

/*
====================
Default state


====================
*/
void BotController::State_DefaultBegin(void)
{
    movement.ClearMove();
}

void BotController::State_DefaultEnd(void) {}

void BotController::State_Reset(void)
{
    m_curious.reset();
    m_combat.reset();
    m_enemy.reset();
}

/*
====================
Idle state

Make the bot move to random directions
====================
*/
void BotController::InitState_Idle(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Idle;
    func->ThinkState     = &BotController::State_Idle;
}

bool BotController::CheckCondition_Idle(void)
{
    if (m_curious.time) {
        return false;
    }

    if (m_combat.attackTime) {
        return false;
    }

    return true;
}

void BotController::State_Idle(void)
{
    if (CheckWindows()) {
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_iLastFireTime = level.inttime;
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        CheckReload();
    }

    //
    // Added in OPM
    //  Human-like idle behavior: periodic pauses to look around
    //
    if (m_idle.pausing) {
        // Currently paused - look around
        if (level.inttime >= m_idle.pauseTime) {
            // Done pausing, resume movement
            m_idle.pausing = false;
            // Sometimes start walking instead of running after a pause
            if (rand() % 4 == 0) {
                m_idle.walking  = true;
                m_idle.walkTime = level.inttime + 2000 + (int)G_Random(3000);
            }
        } else {
            // Look around periodically during pause
            if (level.inttime >= m_idle.lookTime) {
                m_idle.lookTime = level.inttime + 800 + (int)G_Random(1200);

                // Pick a random look direction
                Vector lookAngles = controlledEnt->angles;
                lookAngles.y += G_CRandom(90);
                lookAngles.x = G_CRandom(15);
                rotation.SetTargetAngles(lookAngles);
            }
            return;
        }
    } else {
        // Check if we should start a pause
        if (rand() % 400 == 0) {
            m_idle.pausing   = true;
            m_idle.pauseTime = level.inttime + 1500 + (int)G_Random(2500);
            m_idle.lookTime  = level.inttime + 500;
            movement.ClearMove();
            return;
        }
    }

    //
    // Added in OPM
    //  Occasionally walk instead of run
    //
    if (m_idle.walking && level.inttime >= m_idle.walkTime) {
        m_idle.walking = false;
    }

    // Changed in OPM
    //  Pre-aim toward highest-belief direction when not in combat and
    //  the zone is visible. Otherwise look along the path direction so
    //  the bot doesn't stare at walls.
    {
        Vector beliefPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
        if (beliefPos != vec_zero && controlledEnt->CanSee(beliefPos, 120, 2048, false)) {
            rotation.AimAt(beliefPos);
        } else {
            AimAtAimNode();
        }
    }

    // Changed in OPM
    //  Belief-driven patrol: move toward the highest-belief zone instead
    //  of wandering randomly. Falls back to death positions and random
    //  movement when no zone has significant belief.
    if (!movement.IsMoving()) {
        Vector beliefPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
        if (beliefPos != vec_zero) {
            movement.MoveTo(beliefPos);

            if (movement.MoveDone()) {
                beliefMap.ClearZone(beliefPos);
            }
        } else if (m_enemy.deathPos != vec_zero) {
            movement.MoveTo(m_enemy.deathPos);

            if (movement.MoveDone()) {
                m_enemy.deathPos = vec_zero;
            }
        } else {
            Vector randomDir(G_CRandom(16), G_CRandom(16), G_CRandom(16));
            Vector preferredDir;
            float  radius = 512 + G_Random(2048);

            preferredDir += Vector(controlledEnt->orientation[0]) * (rand() % 5 ? 1024 : -1024);
            preferredDir += Vector(controlledEnt->orientation[2]) * (rand() % 5 ? 1024 : -1024);
            movement.AvoidPath(controlledEnt->origin + randomDir, radius, preferredDir);
        }
    }
}

/*
====================
Curious state

Forward to the last event position
====================
*/
void BotController::InitState_Curious(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Curious;
    func->BeginState     = &BotController::State_BeginCurious;
    func->ThinkState     = &BotController::State_Curious;
}

// Added in OPM
//  Clear idle state and movement when entering curious mode.
//  Immediately turn toward the sound source.
void BotController::State_BeginCurious(void)
{
    movement.ClearMove();
    m_idle.reset();

    // Immediately look toward the sound source
    // Prefer the specific sound location over the general belief map area
    Vector targetPos = m_curious.targetPos;
    if (targetPos == vec_zero) {
        targetPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
    }
    if (targetPos != vec_zero) {
        rotation.AimAt(targetPos);

        if (g_bot_debug_state->integer) {
            float dist = (targetPos - controlledEnt->origin).length();
            gi.Printf(
                "BOT %s: Curious - investigating position (%.0f, %.0f, %.0f) dist=%.0f\n",
                controlledEnt->client->pers.netname,
                targetPos.x,
                targetPos.y,
                targetPos.z,
                dist
            );
        }
    }
}

bool BotController::CheckCondition_Curious(void)
{
    if (m_combat.attackTime) {
        if (g_bot_debug_state->integer >= 2 && m_curious.time) {
            gi.Printf(
                "BOT %s: Curious blocked - in combat (attackTime=%dms)\n",
                controlledEnt->client->pers.netname,
                m_combat.attackTime - level.inttime
            );
        }
        m_curious.time = 0;
        return false;
    }

    if (level.inttime > m_curious.time) {
        if (m_curious.time) {
            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Curious expired (curiousTime=%d, inttime=%d)\n",
                    controlledEnt->client->pers.netname,
                    m_curious.time,
                    level.inttime
                );
            }
            movement.ClearMove();
            m_curious.time = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_Curious(void)
{
    if (CheckWindows()) {
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_iLastFireTime = level.inttime;
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    }

    // Changed in OPM
    //  Turn toward the sound source if visible, otherwise look along the path.
    //  This prevents bots from staring at walls while walking, which looks unnatural.
    //  Only turn toward invisible sounds briefly at the start (handled in BeginState).
    {
        Vector targetPos = (m_curious.targetPos != vec_zero) ? m_curious.targetPos
                                                             : beliefMap.GetHighestBeliefPos(controlledEnt->origin);

        if (targetPos != vec_zero && controlledEnt->CanSee(targetPos, 120, 2048, false)) {
            // Can see the target position - aim at it
            rotation.AimAt(targetPos);
        } else if (movement.IsMoving()) {
            // Can't see target, but we're moving - look along path
            AimAtAimNode();
        } else if (targetPos != vec_zero) {
            // Not moving and can't see - still face the target direction
            rotation.AimAt(targetPos);
        }
    }

    // Changed in OPM
    //  In Curious state, prioritize investigating the sound location over
    //  wandering to attractive points. Attractive points are for idle patrol,
    //  not for investigating threats.
    //  Use MoveNear instead of MoveTo - the sound position might not be
    //  exactly on the navigation mesh, so find a path to anywhere within
    //  512 units of the target.
    {
        Vector beliefPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
        Vector targetPos = (m_curious.targetPos != vec_zero) ? m_curious.targetPos : beliefPos;

        if (targetPos != vec_zero && m_curious.lastPos != targetPos) {
            movement.MoveNear(targetPos, 512);
            m_curious.lastPos = targetPos;

            if (g_bot_debug_state->integer >= 2) {
                if (movement.IsMoving()) {
                    gi.Printf(
                        "BOT %s: Curious moving to investigate (%.0f, %.0f, %.0f)\n",
                        controlledEnt->client->pers.netname,
                        targetPos.x,
                        targetPos.y,
                        targetPos.z
                    );
                } else {
                    gi.Printf(
                        "BOT %s: Curious can't path to (%.0f, %.0f, %.0f) - will look toward it\n",
                        controlledEnt->client->pers.netname,
                        targetPos.x,
                        targetPos.y,
                        targetPos.z
                    );
                }
            }
        }
    }

    if (movement.MoveDone()) {
        float distToTarget = (m_curious.targetPos - controlledEnt->origin).length();

        // If we arrived close to the target, clear curious
        if (distToTarget < 256) {
            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Curious arrived at target (dist=%.0f)\n", controlledEnt->client->pers.netname, distToTarget
                );
            }
            beliefMap.ClearZone(controlledEnt->origin);
            m_curious.time = 0;
        } else if (!movement.IsMoving()) {
            // Can't path to target - stay alert briefly (3 seconds) then resume normal behavior
            // This gives time to spot an enemy while not freezing the bot
            if (level.inttime + 17000 > m_curious.time) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Curious can't reach target (dist=%.0f) - resuming patrol\n",
                        controlledEnt->client->pers.netname,
                        distToTarget
                    );
                }
                m_curious.time = 0;
            }
        }
    }
}

/*
====================
Attack state

Attack the enemy
====================
*/
void BotController::InitState_Attack(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Attack;
    func->BeginState     = &BotController::State_BeginAttack;
    func->EndState       = &BotController::State_EndAttack;
    func->ThinkState     = &BotController::State_Attack;
}

// Added in OPM
//  Clear idle state and movement when entering attack mode
void BotController::State_BeginAttack(void)
{
    movement.ClearMove();
    m_idle.reset();

    if (g_bot_debug_state->integer && m_enemy.enemy) {
        const char *enemyName = "unknown";
        if (m_enemy.enemy->IsSubclassOfPlayer()) {
            enemyName = static_cast<Player *>(m_enemy.enemy.Pointer())->client->pers.netname;
        } else {
            enemyName = m_enemy.enemy->targetname.c_str();
        }
        float dist = (m_enemy.enemy->origin - controlledEnt->origin).length();
        gi.Printf("BOT %s: Attack - targeting %s at dist=%.0f\n", controlledEnt->client->pers.netname, enemyName, dist);
    }
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

bool BotController::CheckCondition_Attack(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;
    Sentient             *bestEnemy   = NULL;
    float                 bestDistSq  = 999999999.0f;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828);

    //
    // Changed in OPM
    //  Scan ALL visible enemies and pick the closest one, rather than
    //  returning early when any enemy is found. Use 360° FOV so bots
    //  detect enemies regardless of where they're currently looking -
    //  a player would notice someone appearing in their peripheral vision.
    //
    for (int i = 1; i <= sents.NumObjects(); i++) {
        Sentient *sent = sents.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

        // Use 360° FOV - detect enemies anywhere, not just where we're looking
        if (controlledEnt->CanSee(sent, 360, maxDistance, false)) {
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestEnemy  = sent;
            }
        }
    }

    // If we found a visible enemy, target them
    if (bestEnemy) {
        if (m_enemy.enemy != bestEnemy) {
            m_enemy.eyesTag = -1;
            // Reset reaction time when switching targets
            m_combat.lastUnseenTime = level.inttime;
        }

        m_enemy.enemy       = bestEnemy;
        m_enemy.lastPos     = m_enemy.enemy->origin;
        m_combat.attackTime = level.inttime + 500 + (int)G_Random(1000);

        // Added in OPM
        //  Update belief map with direct sighting
        beliefMap.UpdateFromSighting(m_enemy.enemy->origin);

        return true;
    }

    // No visible enemy - check if we should keep hunting the last known position
    if (level.inttime > m_combat.attackTime) {
        if (m_combat.attackTime) {
            movement.ClearMove();
            m_combat.attackTime = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_EndAttack(void)
{
    if (g_bot_debug_state->integer) {
        const char *reason = "unknown";
        if (!m_enemy.enemy) {
            reason = "no enemy";
        } else if (!IsValidEnemy(m_enemy.enemy)) {
            reason = "enemy invalid (dead/hidden/team)";
        } else if (m_combat.attackTime && level.inttime > m_combat.attackTime) {
            reason = "attack timer expired";
        }
        gi.Printf("BOT %s: Attack ended - %s\n", controlledEnt->client->pers.netname, reason);
    }

    m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    m_botCmd.rightmove     = 0;
    m_botCmd.upmove        = 0;
    m_combat.strafeTime    = 0;
    m_combat.strafeDir     = 0;
    m_combat.standingStill = false;
    m_combat.crouching     = false;
    m_combat.crouchDecided = false;
    m_idle.leanDir         = 0;
    controlledEnt->ZoomOff();
}

void BotController::State_Attack(void)
{
    bool    bMelee              = false;
    bool    bCanSee             = false;
    bool    bCanAttack          = false;
    float   fMinDistance        = 128;
    float   fMinDistanceSquared = fMinDistance * fMinDistance;
    float   fEnemyDistanceSquared;
    Weapon *pWeap   = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    bool    bNoMove = false;
    bool    bFiring = false;

    // Changed in OPM
    //  When enemy is gone but we recently had one, keep looking at the last
    //  known position briefly instead of snapping away. This prevents the
    //  jarring 180-degree turn after a kill.
    // Changed in OPM
    //  When enemy is gone but we recently had one, keep looking at the last
    //  known position briefly instead of snapping away.
    if (!m_enemy.enemy || !IsValidEnemy(m_enemy.enemy)) {
        if (level.inttime < m_combat.attackStopAimTime && m_enemy.lastPos != vec_zero) {
            rotation.AimAt(m_enemy.lastPos);
            m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
            m_combat.attackTime = level.inttime + 200 + (int)G_Random(300);
            return;
        }

        m_combat.attackTime = 0;
        return;
    }
    float fDistanceSquared = (m_enemy.enemy->origin - controlledEnt->origin).lengthSquared();

    m_enemy.oldPos = m_enemy.lastPos;

    // Changed in OPM
    //  Use 120° FOV instead of 20° so the bot recognizes enemies in peripheral
    //  vision. The narrow 20° FOV caused bots to ignore enemies that appeared
    //  while they were looking elsewhere (e.g., toward a previous target).
    bCanSee = controlledEnt->CanSee(
        m_enemy.enemy, 120, Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828), false
    );

    if (bCanSee) {
        if (!pWeap) {
            return;
        }

        bCanAttack = true;
        if (m_combat.lastUnseenTime) {
            const float        reactionTime = Q_min(1000 * Q_min(1, fDistanceSquared / Square(2048)), 1000);
            const unsigned int minDelay     = g_bot_attack_react_min_delay->value * 1000;
            const unsigned int randomDelay  = g_bot_attack_react_random_delay->value * 1000;
            if (level.inttime <= m_combat.lastUnseenTime + minDelay + G_Random(randomDelay)) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Attack - waiting for reaction delay (elapsed=%dms, min=%dms)\n",
                        controlledEnt->client->pers.netname,
                        level.inttime - m_combat.lastUnseenTime,
                        minDelay
                    );
                }
                bCanAttack = false;
            } else {
                m_combat.lastUnseenTime = 0;
            }
        }

        if (bCanAttack) {
            const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
            float     fPrimaryBulletRange          = pWeap->GetBulletRange(FIRE_PRIMARY) / 1.25f;
            float     fPrimaryBulletRangeSquared   = fPrimaryBulletRange * fPrimaryBulletRange;
            float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
            float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;

            const int maxcontinuousFireTime = fireDelay + g_bot_attack_continuousfire_min_firetime->value * 1000
                                            + G_Random(g_bot_attack_continuousfire_random_firetime->value * 1000);
            const int maxBurstTime = fireDelay + g_bot_attack_burst_min_time->value * 1000
                                   + G_Random(g_bot_attack_burst_random_delay->value * 1000);

            //
            // check the fire movement speed if the weapon has a max fire movement
            //
            if (pWeap->GetMaxFireMovement() < 1 && pWeap->HasAmmoInClip(FIRE_PRIMARY)) {
                float length;

                length = controlledEnt->velocity.length();
                if ((length / sv_runspeed->value) > (pWeap->GetMaxFireMovementMult())) {
                    bNoMove = true;
                    movement.ClearMove();
                }
            }

            fMinDistance = fPrimaryBulletRange;

            if (fMinDistance > 256) {
                fMinDistance = 256;
            }

            fMinDistanceSquared = fMinDistance * fMinDistance;

            if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf("BOT %s: Attack - no ammo, switching weapon\n", controlledEnt->client->pers.netname);
                }
                m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                controlledEnt->ZoomOff();

                // Added in OPM
                //  Switch to next weapon when out of ammo (with cooldown to prevent spam)
                if (level.inttime > m_combat.lastWeaponSwitchTime + 500) {
                    m_combat.lastWeaponSwitchTime = level.inttime;
                    Event ev;
                    controlledEnt->SelectNextWeapon(&ev);
                }
            } else if (fDistanceSquared > fPrimaryBulletRangeSquared) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Attack - out of range (dist=%.0f, range=%.0f)\n",
                        controlledEnt->client->pers.netname,
                        sqrtf(fDistanceSquared),
                        fPrimaryBulletRange
                    );
                }
                m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                controlledEnt->ZoomOff();
            } else {
                //
                // Attacking
                //

                if (pWeap->IsSemiAuto()) {
                    if (controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                        && (controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                            || controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
                        if (g_bot_debug_state->integer >= 2) {
                            gi.Printf(
                                "BOT %s: Attack - waiting for weapon idle (anim=%d)\n",
                                controlledEnt->client->pers.netname,
                                controlledEnt->client->ps.iViewModelAnim
                            );
                        }
                        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                        controlledEnt->ZoomOff();
                    } else {
                        // Changed in OPM
                        //  Removed spread factor check - bots should fire while moving
                        //  like real players do. The weapon's inherent spread handles accuracy.
                        bFiring = true;
                        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                        if (pWeap->GetZoom()) {
                            if (!controlledEnt->IsZoomed()) {
                                m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                            } else {
                                m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                            }
                        }
                    }
                } else {
                    bFiring = true;
                    m_botCmd.buttons |= BUTTON_ATTACKLEFT;
                }
            }

            //
            // Burst
            //

            if (m_combat.lastBurstTime) {
                if (level.inttime > m_combat.lastBurstTime + maxBurstTime) {
                    m_combat.lastBurstTime      = 0;
                    m_combat.continuousFireTime = 0;
                } else {
                    m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
                }
            } else {
                if (bFiring) {
                    m_combat.continuousFireTime += level.intframetime;
                } else {
                    m_combat.continuousFireTime = 0;
                }

                if (!m_combat.lastBurstTime && m_combat.continuousFireTime > maxcontinuousFireTime) {
                    m_combat.lastBurstTime      = level.inttime;
                    m_combat.continuousFireTime = 0;
                }
            }

            m_iLastFireTime = level.inttime;

            if (pWeap->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
                if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                    && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                    bMelee = true;
                } else if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                    bMelee = true;
                }
            }

            if (bMelee) {
                m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;

                if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                    m_botCmd.buttons ^= BUTTON_ATTACKRIGHT;
                } else {
                    m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                }
            }

            m_combat.attackTime        = level.inttime + 500 + (int)G_Random(1000);
            m_combat.attackStopAimTime = level.inttime + 500 + (int)G_Random(1000);
            m_combat.lastSeenTime      = level.inttime;
            m_enemy.lastPos            = m_enemy.enemy->origin;
        }
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        fMinDistanceSquared = 0;

        if (level.inttime > m_combat.lastSeenTime + 2000) {
            m_combat.lastUnseenTime = level.inttime;
        }
    }

    if (bCanSee || level.inttime < m_combat.attackStopAimTime) {
        Vector        vRandomOffset;
        Vector        vTarget;
        orientation_t eyes_or;

        if (m_enemy.eyesTag == -1) {
            // Cache the tag
            m_enemy.eyesTag = gi.Tag_NumForName(m_enemy.enemy->edict->tiki, "eyes bone");
        }

        if (m_enemy.eyesTag != -1) {
            // Use the enemy's eyes bone
            m_enemy.enemy->GetTag(m_enemy.eyesTag, &eyes_or);

            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), -G_Random(32));
            vTarget = eyes_or.origin;
        } else {
            //vRandomOffset = Vector(G_CRandom(8), G_CRandom(8), 16 + G_Random(m_enemy.enemy->viewheight - 16));
            vTarget = m_enemy.enemy->origin;
        }

        //
        // Changed in OPM
        //  Humanized aiming: pick a new random offset target every 300-600ms,
        //  then smoothly lerp toward it. Scale offset magnitude by distance
        //  so bots are more accurate at close range.
        //
        if (level.inttime >= m_combat.lastAimTime + 300 + (int)G_Random(300)) {
            float halfW = (m_enemy.enemy->maxs.x - m_enemy.enemy->mins.x) * 0.5;
            float halfD = (m_enemy.enemy->maxs.y - m_enemy.enemy->mins.y) * 0.5;

            // Scale offset by distance: close (< 256) = tight, far (> 1024) = full spread
            float fDist     = sqrt(fDistanceSquared);
            float distScale = Q_clamp_float((fDist - 256) / 768, 0.15, 1.0);

            if (m_enemy.eyesTag != -1) {
                m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                m_combat.aimOffsetTarget[2] = -G_Random(m_enemy.enemy->maxs.z * 0.5) * distScale;
            } else {
                m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                m_combat.aimOffsetTarget[2] = 16 + G_Random(m_enemy.enemy->viewheight - 16) * distScale;
            }

            m_combat.lastAimTime      = level.inttime;
            m_combat.aimLerpStartTime = level.inttime;
        }

        // Smoothly lerp current offset toward target offset
        {
            float dt       = level.frametime * g_bot_aim_lerp_speed->value;
            float lerpFrac = Q_clamp_float(dt, 0.0, 1.0);

            m_combat.aimOffset[0] =
                m_combat.aimOffset[0] + (m_combat.aimOffsetTarget[0] - m_combat.aimOffset[0]) * lerpFrac;
            m_combat.aimOffset[1] =
                m_combat.aimOffset[1] + (m_combat.aimOffsetTarget[1] - m_combat.aimOffset[1]) * lerpFrac;
            m_combat.aimOffset[2] =
                m_combat.aimOffset[2] + (m_combat.aimOffsetTarget[2] - m_combat.aimOffset[2]) * lerpFrac;
        }

        rotation.AimAt(vTarget + m_combat.aimOffset * g_bot_attack_spreadmult->value);
    } else {
        AimAtAimNode();
    }

    if (bNoMove) {
        m_combat.standingStill = true;
        return;
    }

    fEnemyDistanceSquared = (controlledEnt->origin - m_enemy.lastPos).lengthSquared();

    //
    // Added in OPM
    //  Stand still to aim at long range targets (more accurate)
    //  At close range, keep moving
    //
    const float longRangeThreshold = 800 * 800;
    const float midRangeThreshold  = 400 * 400;

    if (bCanSee && bFiring && fEnemyDistanceSquared > longRangeThreshold) {
        // Long range: stop forward movement, strafing handled separately
        m_combat.standingStill = true;
        movement.ClearMove();
    } else if (bCanSee && bFiring && fEnemyDistanceSquared > midRangeThreshold) {
        // Mid range: stop periodically to aim, then move
        if (rand() % 100 < 30) {
            m_combat.standingStill = true;
            movement.ClearMove();
        } else {
            m_combat.standingStill = false;
        }
    } else {
        m_combat.standingStill = false;
    }

    //
    // Added in OPM
    //  Leaning during combat: periodically lean left/right when stationary
    //
    if (bCanSee && m_combat.standingStill) {
        if (level.inttime >= m_idle.leanTime) {
            m_idle.leanTime = level.inttime + 1500 + (int)G_Random(2000);

            // Pick lean direction: left, right, or none
            int roll = rand() % 5;
            if (roll < 2) {
                m_idle.leanDir = -1;
            } else if (roll < 4) {
                m_idle.leanDir = 1;
            } else {
                m_idle.leanDir = 0;
            }
        }
    } else {
        // Not standing still, don't lean
        m_idle.leanDir = 0;
    }

    //
    // Added in OPM
    //  Combat crouching: crouch when standing still to reduce profile
    //  and improve accuracy. Decided once when entering standing-still state.
    //
    if (m_combat.standingStill) {
        if (!m_combat.crouching && !m_combat.crouchDecided) {
            m_combat.crouchDecided = true;
            if (rand() % 100 < g_bot_crouch_chance->integer) {
                m_combat.crouching = true;
            }
        }
    } else {
        m_combat.crouching     = false;
        m_combat.crouchDecided = false;
    }

    if (m_combat.crouching) {
        m_botCmd.upmove = -127;
    } else {
        m_botCmd.upmove = 0;
    }

    //
    // Changed in OPM
    //  Combat strafing: ADAD spam when actively firing and visible to enemy.
    //  Placed before the standing-still return so bots strafe at all ranges,
    //  even when holding position.
    //
    // Changed in OPM
    //  Combat strafing with varied, unpredictable timing.
    //  Mix of quick direction changes, longer holds, and brief pauses
    //  so the pattern is never consistent.
    if (bCanSee && !bMelee) {
        if (level.inttime >= m_combat.strafeTime) {
            int roll = rand() % 10;

            if (roll < 2) {
                // Quick tap: short hold, then switch
                m_combat.strafeTime = level.inttime + 150 + (int)G_Random(250);
                m_combat.strafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 4) {
                // Hold direction: commit to one side for a while
                m_combat.strafeTime = level.inttime + 600 + (int)G_Random(1200);
                m_combat.strafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 8) {
                // Pause: stop strafing, longer duration
                m_combat.strafeTime = level.inttime + 300 + (int)G_Random(700);
                m_combat.strafeDir  = 0;
            } else {
                // Double-tap: reverse current direction
                m_combat.strafeTime = level.inttime + 100 + (int)G_Random(200);
                m_combat.strafeDir  = m_combat.strafeDir > 0 ? -127 : 127;
            }
        }

        m_botCmd.rightmove = m_combat.strafeDir;
    }

    if (m_combat.standingStill) {
        return;
    }

    // Changed in OPM
    //  Combat movement: when the bot can see and attack, stop and fight.
    //  Don't continue walking toward navigation goals - that makes bots
    //  walk right up to enemies instead of engaging from a distance.
    //  Only advance when the enemy is not visible or when using melee.
    if (bCanSee && bCanAttack && !bMelee) {
        // Can see and shoot — stop and fight, let strafing handle lateral movement
        movement.ClearMove();
    } else if ((!movement.IsMoving()) || (m_enemy.oldPos != m_enemy.lastPos && !movement.MoveDone())) {
        // Can't see enemy or using melee — close the distance
        movement.MoveTo(m_enemy.lastPos);

        if (!bCanSee && movement.MoveDone()) {
            // Lost track of the enemy
            ClearEnemy();
            return;
        }
    }

    if (movement.IsMoving()) {
        m_combat.attackTime = level.inttime + 500 + (int)G_Random(1000);
    }
}

/*
====================
Grenade state

Avoid any grenades
====================
*/
void BotController::InitState_Grenade(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Grenade;
    func->BeginState     = &BotController::State_BeginGrenade;
    func->ThinkState     = &BotController::State_Grenade;
}

// Added in OPM
//  Clear idle state and movement when fleeing from grenade
void BotController::State_BeginGrenade(void)
{
    movement.ClearMove();
    m_idle.reset();
}

bool BotController::CheckCondition_Grenade(void)
{
    // Added in OPM
    //  Scan for nearby enemy projectiles (grenades) and flee from them
    if (m_grenade.grenade && m_grenade.grenade->IsSubclassOfProjectile()) {
        float distSq = (m_grenade.grenade->origin - controlledEnt->origin).lengthSquared();
        float radius = g_bot_grenade_avoid_radius->value;

        if (distSq < radius * radius) {
            return true;
        }
    }

    m_grenade.grenade = NULL;

    float      radiusSq = Square(g_bot_grenade_avoid_radius->value);
    gentity_t *edict;
    int        i;

    for (i = game.maxclients, edict = &g_entities[i]; i < globals.num_entities; i++, edict++) {
        if (!edict->inuse || !edict->entity) {
            continue;
        }

        Entity *ent = edict->entity;
        if (!ent->IsSubclassOfProjectile()) {
            continue;
        }

        Projectile *proj = static_cast<Projectile *>(ent);

        // Ignore own projectiles
        if (proj->GetOwner() == controlledEnt) {
            continue;
        }

        // Ignore friendly projectiles in team games
        Sentient *projOwner = proj->GetOwner();
        if (projOwner && projOwner->IsSubclassOfPlayer() && g_gametype->integer >= GT_TEAM) {
            Player *p = static_cast<Player *>(projOwner);
            if (p->GetTeam() == controlledEnt->GetTeam()) {
                continue;
            }
        }

        float distSq = (ent->origin - controlledEnt->origin).lengthSquared();
        if (distSq < radiusSq) {
            m_grenade.grenade   = ent;
            m_grenade.avoidTime = level.inttime + 3000;
            return true;
        }
    }

    if (level.inttime < m_grenade.avoidTime) {
        return true;
    }

    return false;
}

void BotController::State_Grenade(void)
{
    // Added in OPM
    //  Flee away from the grenade
    if (!m_grenade.grenade) {
        return;
    }

    Vector grenadePos = m_grenade.grenade->origin;
    Vector fleeDir    = controlledEnt->origin - grenadePos;
    VectorNormalizeFast(fleeDir);

    movement.AvoidPath(grenadePos, g_bot_grenade_avoid_radius->value, fleeDir * 512);
}

/*
====================
Weapon state

Change weapon when necessary
====================
*/
void BotController::InitState_Weapon(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Weapon;
    func->BeginState     = &BotController::State_BeginWeapon;
}

bool BotController::CheckCondition_Weapon(void)
{
    return controlledEnt->GetActiveWeapon(WEAPON_MAIN)
        != controlledEnt->BestWeapon(NULL, false, WEAPON_CLASS_THROWABLE);
}

void BotController::State_BeginWeapon(void)
{
    Weapon *weap = controlledEnt->BestWeapon(NULL, false, WEAPON_CLASS_THROWABLE);

    if (weap == NULL) {
        SendCommand("safeholster 1");
        return;
    }

    SendCommand(va("use \"%s\"", weap->model.c_str()));
}

/*
====================
CheckWindows

Check if there is a window in front of the bot
Returns true if a window is blocking
====================
*/
bool BotController::CheckWindows(void)
{
    trace_t trace;
    Vector  start, end;
    Vector  dir;

    controlledEnt->angles.AngleVectorsLeft(&dir);
    start = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    end   = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight) + dir * 64;

    trace = G_Trace(start, vec_zero, vec_zero, end, controlledEnt, MASK_PLAYERSOLID, false, "BotController::CheckUse");

    if (trace.fraction != 1 && trace.ent) {
        if (trace.ent->entity->isSubclassOf(WindowObject)) {
            return true;
        }
    }

    return false;
}
