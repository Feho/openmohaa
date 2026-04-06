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
// Concrete BotState subclasses are defined here. Each class owns the
// CheckCondition/Begin/End/Think logic for one state and accesses
// BotController internals via friend access (declared in playerbot.h).

#include "playerbot.h"
#include "gamecvars.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "windows.h"
#include "g_bot.h"

/*
===========================================================================
BotStateAttack
===========================================================================
*/

// File-scope comparator used by CheckCondition to sort sentients by distance.
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

class BotStateAttack : public BotState
{
    BotController *m_controller;

public:
    BotStateAttack(BotController *controller)
        : m_controller(controller)
    {}

    const char *GetName() const override { return "Attack"; }

    bool CheckCondition() override;
    void Begin() override;
    void End() override;
    void Think() override;
};

bool BotStateAttack::CheckCondition()
{
    BotController        *c           = m_controller;
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;
    Sentient             *bestEnemy   = NULL;
    float                 bestScore   = -999999999.0f;

    bot_origin = c->controlledEnt->origin;
    sents.Sort(sentients_compare);

    // Changed in OPM
    //  Use per-bot vision distance (personality-scaled, cvar-overridable) instead
    //  of the shared world AI distance. The fog cap (farplane_distance * 0.828)
    //  is intentionally removed for bots.
    maxDistance = c->m_params.visionDistance;

    //
    // Changed in OPM
    //  Scan ALL visible enemies and score them. Scoring combines distance
    //  (closer = better) with a bonus for stationary/crouching targets,
    //  so snipers naturally pick the easiest shot when multiple enemies
    //  are visible.
    //
    for (int i = 1; i <= sents.NumObjects(); i++) {
        Sentient *sent = sents.ObjectAt(i);

        if (!c->IsValidEnemy(sent)) {
            continue;
        }

        float distSq = (sent->origin - c->controlledEnt->origin).lengthSquared();

        // Fixed in OPM
        //  Use 160° FOV (wide peripheral vision) instead of 360° so bots
        //  can't detect enemies directly behind them.
        if (c->controlledEnt->CanSee(sent, 160, maxDistance, false)) {
            // Added in OPM
            //  Target scoring: base score is inverse distance (closer = higher).
            //  Stationary or slow-moving targets get a bonus scaled by the bot's
            //  patience trait — patient bots (snipers) strongly prefer easy shots.
            float score = -distSq;

            float speed = sent->velocity.length();
            if (speed < 20) {
                // Standing still: large bonus
                score += 500000 * c->m_personality.patience;
            } else if (speed < 100) {
                // Walking/slow: moderate bonus
                score += 250000 * c->m_personality.patience;
            }

            if (score > bestScore) {
                bestScore = score;
                bestEnemy = sent;
            }
        }
    }

    // If we found a visible enemy, target them
    if (bestEnemy) {
        if (c->m_enemy.enemy != bestEnemy) {
            c->m_enemy.eyesTag = -1;
            // Reset reaction time when switching targets
            c->m_combat.lastUnseenTime = level.inttime;
        }

        c->m_enemy.enemy       = bestEnemy;
        c->m_enemy.lastPos     = c->m_enemy.enemy->origin;
        c->m_combat.attackTime = level.inttime + 500 + (int)G_Random(1000);

        // Added in OPM
        //  Update belief map with direct sighting and reset visit count
        //  so this zone becomes attractive again (enemy was found here).
        c->beliefMap.UpdateFromSighting(c->m_enemy.enemy->origin);
        c->beliefMap.ResetVisitsOnSighting(c->m_enemy.enemy->origin);

        return true;
    }

    // No visible enemy - check if we should keep hunting the last known position
    if (level.inttime > c->m_combat.attackTime) {
        if (c->m_combat.attackTime) {
            c->movement.ClearMove();
            c->m_combat.attackTime = 0;
        }

        return false;
    }

    return true;
}

// Added in OPM
//  Clear idle state and movement when entering attack mode
void BotStateAttack::Begin()
{
    BotController *c = m_controller;
    c->movement.ClearMove();
    c->m_idle.reset();

    if (g_bot_debug_state->integer && c->m_enemy.enemy) {
        const char *enemyName = "unknown";
        if (c->m_enemy.enemy->IsSubclassOfPlayer()) {
            enemyName = static_cast<Player *>(c->m_enemy.enemy.Pointer())->client->pers.netname;
        } else {
            enemyName = c->m_enemy.enemy->targetname.c_str();
        }
        float dist = (c->m_enemy.enemy->origin - c->controlledEnt->origin).length();
        gi.Printf(
            "BOT %s: Attack - targeting %s at dist=%.0f\n", c->controlledEnt->client->pers.netname, enemyName, dist
        );
    }
}

void BotStateAttack::End()
{
    BotController *c = m_controller;

    if (g_bot_debug_state->integer) {
        const char *reason = "unknown";
        if (!c->m_enemy.enemy) {
            reason = "no enemy";
        } else if (!c->IsValidEnemy(c->m_enemy.enemy)) {
            reason = "enemy invalid (dead/hidden/team)";
        } else if (c->m_combat.attackTime && level.inttime > c->m_combat.attackTime) {
            reason = "attack timer expired";
        }
        gi.Printf("BOT %s: Attack ended - %s\n", c->controlledEnt->client->pers.netname, reason);
    }

    c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    c->m_botCmd.rightmove      = 0;
    c->m_botCmd.upmove         = 0;
    c->m_combat.strafeTime     = 0;
    c->m_combat.strafeDir      = 0;
    c->m_combat.standingStill  = false;
    c->m_combat.crouching      = false;
    c->m_combat.crouchDecided  = false;
    c->m_combat.overwatchUntil = 0;
    c->m_idle.leanDir          = 0;
    c->controlledEnt->ZoomOff();
}

void BotStateAttack::Think()
{
    BotController *c          = m_controller;
    bool           bMelee     = false;
    bool           bCanSee    = false;
    bool           bCanAttack = false;
    float          fEnemyDistanceSquared;
    Weapon        *pWeap          = c->controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    bool           bNoMove        = false;
    bool           bFiring        = false;
    bool           bInWeaponRange = false;

    // Changed in OPM
    //  When enemy is gone but we recently had one, keep looking at the last
    //  known position briefly instead of snapping away.
    //  Sniper overwatch: hold position, stay crouched and scoped, watching
    //  the kill zone for additional targets.
    if (!c->m_enemy.enemy || !c->IsValidEnemy(c->m_enemy.enemy)) {
        if (level.inttime < c->m_combat.attackStopAimTime && c->m_enemy.lastPos != vec_zero) {
            c->rotation.AimAt(c->m_enemy.lastPos);
            c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);

            // Added in OPM
            //  Sniper overwatch: stay scoped and stationary after a kill.
            //  Crouch based on crouchChance (personality-scaled) rather than always.
            if (level.inttime < c->m_combat.overwatchUntil) {
                if (!c->m_combat.crouchDecided) {
                    c->m_combat.crouchDecided = true;
                    c->m_combat.crouching     = (rand() % 100 < c->m_params.crouchChance);
                }
                if (c->m_combat.crouching) {
                    c->m_botCmd.upmove = -127;
                }
                c->movement.ClearMove();

                // Keep scoped in if weapon has zoom
                if (pWeap && pWeap->GetZoom() && !c->controlledEnt->IsZoomed()) {
                    c->m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                }
            }

            c->m_combat.attackTime = level.inttime + 200 + (int)G_Random(300);
            return;
        }

        c->m_combat.attackTime     = 0;
        c->m_combat.overwatchUntil = 0;
        return;
    }

    float fDistanceSquared = (c->m_enemy.enemy->origin - c->controlledEnt->origin).lengthSquared();

    c->m_enemy.oldPos = c->m_enemy.lastPos;

    // Changed in OPM
    //  Use 120° FOV instead of 20° so the bot recognizes enemies in peripheral
    //  vision.
    bCanSee = c->controlledEnt->CanSee(c->m_enemy.enemy, 120, c->m_params.visionDistance, false);

    if (bCanSee) {
        if (!pWeap) {
            return;
        }

        bCanAttack = true;
        if (c->m_combat.lastUnseenTime) {
            const float        reactionTime = Q_min(1000 * Q_min(1, fDistanceSquared / Square(2048)), 1000);
            const unsigned int minDelay     = c->m_params.attackReactMinDelay * 1000;
            const unsigned int randomDelay  = c->m_params.attackReactRandomDelay * 1000;
            if (level.inttime <= c->m_combat.lastUnseenTime + minDelay + G_Random(randomDelay)) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Attack - waiting for reaction delay (elapsed=%dms, min=%dms)\n",
                        c->controlledEnt->client->pers.netname,
                        level.inttime - c->m_combat.lastUnseenTime,
                        minDelay
                    );
                }
                bCanAttack = false;
            } else {
                c->m_combat.lastUnseenTime = 0;
            }
        }

        if (bCanAttack) {
            const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
            float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
            float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;
            // Removed in OPM
            //  Artificial range check removed — bots fire regardless of distance;
            //  the engine handles whether bullets reach the target.
            bInWeaponRange = (fDistanceSquared <= Square(pWeap->GetBulletRange(FIRE_PRIMARY)));

            const int maxcontinuousFireTime = fireDelay + c->m_params.attackContinuousFireMinTime * 1000
                                            + G_Random(c->m_params.attackContinuousFireRandomTime * 1000);
            const int maxBurstTime =
                fireDelay + c->m_params.attackBurstMinTime * 1000 + G_Random(c->m_params.attackBurstRandomDelay * 1000);

            //
            // check the fire movement speed if the weapon has a max fire movement
            //
            if (pWeap->GetMaxFireMovement() < 1 && pWeap->HasAmmoInClip(FIRE_PRIMARY)) {
                float length = c->controlledEnt->velocity.length();
                if ((length / sv_runspeed->value) > (pWeap->GetMaxFireMovementMult())) {
                    bNoMove = true;
                    c->movement.ClearMove();
                }
            }

            if (c->controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                && c->controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf("BOT %s: Attack - no ammo, switching weapon\n", c->controlledEnt->client->pers.netname);
                }
                c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                c->controlledEnt->ZoomOff();

                // Added in OPM
                //  Switch to next weapon when out of ammo (with cooldown to prevent spam)
                if (level.inttime > c->m_combat.lastWeaponSwitchTime + 500) {
                    c->m_combat.lastWeaponSwitchTime = level.inttime;
                    Event ev;
                    c->controlledEnt->SelectNextWeapon(&ev);
                }
            } else {
                //
                // Attacking
                //
                if (pWeap->IsSemiAuto()) {
                    bool bWeaponBusy = c->controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                                    && (c->controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                                        || c->controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2);

                    if (bWeaponBusy) {
                        if (g_bot_debug_state->integer >= 2) {
                            gi.Printf(
                                "BOT %s: Attack - waiting for weapon idle (anim=%d)\n",
                                c->controlledEnt->client->pers.netname,
                                c->controlledEnt->client->ps.iViewModelAnim
                            );
                        }
                        c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);

                        // Changed in OPM
                        //  Don't unzoom during bolt cycle for scoped weapons.
                        //  Stay scoped so we can fire immediately when idle.
                        if (!pWeap->GetZoom()) {
                            c->controlledEnt->ZoomOff();
                        }
                    } else {
                        // Changed in OPM
                        //  For zoom weapons (snipers): scope in first, wait for
                        //  aim to settle, then fire. The settle delay gives the
                        //  aim offset time to lerp toward center, making the
                        //  first shot more accurate.
                        if (pWeap->GetZoom()) {
                            if (!c->controlledEnt->IsZoomed()) {
                                // Zoom in first, don't fire yet
                                c->m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                                c->m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
                                c->m_combat.scopeInTime = 0;
                            } else {
                                // Track when scope-in completed
                                if (!c->m_combat.scopeInTime) {
                                    c->m_combat.scopeInTime = level.inttime;
                                }

                                int settleMs = (int)(c->m_params.scopeSettleDelay * 1000);
                                if (level.inttime < c->m_combat.scopeInTime + settleMs) {
                                    // Settling — hold fire, let aim converge
                                    c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                                } else {
                                    // Settled — fire
                                    bFiring = true;
                                    c->m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                                    c->m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                                }
                            }
                        } else {
                            bFiring = true;
                            c->m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                        }
                    }
                } else {
                    bFiring = true;
                    c->m_botCmd.buttons |= BUTTON_ATTACKLEFT;
                }
            }

            //
            // Burst
            //

            if (c->m_combat.lastBurstTime) {
                if (level.inttime > c->m_combat.lastBurstTime + maxBurstTime) {
                    c->m_combat.lastBurstTime      = 0;
                    c->m_combat.continuousFireTime = 0;
                } else {
                    c->m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
                }
            } else {
                if (bFiring) {
                    c->m_combat.continuousFireTime += level.intframetime;
                } else {
                    c->m_combat.continuousFireTime = 0;
                }

                if (!c->m_combat.lastBurstTime && c->m_combat.continuousFireTime > maxcontinuousFireTime) {
                    c->m_combat.lastBurstTime      = level.inttime;
                    c->m_combat.continuousFireTime = 0;
                }
            }

            c->m_iLastFireTime = level.inttime;

            if (pWeap->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
                if (c->controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                    && c->controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                    bMelee = true;
                } else if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                    bMelee = true;
                }
            }

            if (bMelee) {
                c->m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;

                if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                    c->m_botCmd.buttons ^= BUTTON_ATTACKRIGHT;
                } else {
                    c->m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                }
            }

            c->m_combat.attackTime        = level.inttime + 500 + (int)G_Random(1000);
            c->m_combat.attackStopAimTime = level.inttime + 500 + (int)G_Random(1000);
            c->m_combat.lastSeenTime      = level.inttime;
            c->m_enemy.lastPos            = c->m_enemy.enemy->origin;
        }
    } else {
        c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);

        if (level.inttime > c->m_combat.lastSeenTime + 2000) {
            c->m_combat.lastUnseenTime = level.inttime;
        }
    }

    if (bCanSee || level.inttime < c->m_combat.attackStopAimTime) {
        Vector        vTarget;
        orientation_t eyes_or;

        if (c->m_enemy.eyesTag == -1) {
            // Cache the tag
            c->m_enemy.eyesTag = gi.Tag_NumForName(c->m_enemy.enemy->edict->tiki, "eyes bone");
        }

        if (c->m_enemy.eyesTag != -1) {
            // Use the enemy's eyes bone
            c->m_enemy.enemy->GetTag(c->m_enemy.eyesTag, &eyes_or);
            vTarget = eyes_or.origin;
        } else {
            vTarget = c->m_enemy.enemy->origin;
        }

        //
        // Changed in OPM
        //  Humanized aiming: pick a new random offset target every 300-600ms,
        //  then smoothly lerp toward it. Scale offset magnitude by distance
        //  so bots are more accurate at close range.
        //
        if (level.inttime >= c->m_combat.lastAimTime + 300 + (int)G_Random(300)) {
            float halfW = (c->m_enemy.enemy->maxs.x - c->m_enemy.enemy->mins.x) * 0.5;
            float halfD = (c->m_enemy.enemy->maxs.y - c->m_enemy.enemy->mins.y) * 0.5;

            // Scale offset by distance: close (< 256) = tight, far (> 1024) = full spread
            float fDist     = sqrt(fDistanceSquared);
            float distScale = Q_clamp_float((fDist - 256) / 768, 0.15, 1.0);

            // Added in OPM
            //  Detect head-only visibility: if the enemy's torso center is blocked
            //  but the head (eyes bone) is visible, tighten the aim offset so the
            //  bot doesn't scatter shots into cover below the head.
            bool bHeadOnly = false;
            if (bCanSee && c->m_enemy.eyesTag != -1) {
                Vector torsoPos = c->m_enemy.enemy->origin;
                torsoPos.z += c->m_enemy.enemy->viewheight * 0.5f;

                bool bCanSeeTorso = G_SightTrace(
                    c->controlledEnt->centroid,
                    vec_zero,
                    vec_zero,
                    torsoPos,
                    c->controlledEnt,
                    c->m_enemy.enemy,
                    MASK_CANSEE,
                    qfalse,
                    "BotStateAttack::HeadCheck"
                );
                bHeadOnly = !bCanSeeTorso;
            }

            if (c->m_enemy.eyesTag != -1) {
                c->m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                c->m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                if (bHeadOnly) {
                    // Head-only: minimal vertical scatter around the head
                    c->m_combat.aimOffsetTarget[2] = G_CRandom(8) * distScale;
                } else {
                    c->m_combat.aimOffsetTarget[2] = -G_Random(c->m_enemy.enemy->maxs.z * 0.5) * distScale;
                }
            } else {
                c->m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                c->m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                c->m_combat.aimOffsetTarget[2] = 16 + G_Random(c->m_enemy.enemy->viewheight - 16) * distScale;
            }

            c->m_combat.lastAimTime      = level.inttime;
            c->m_combat.aimLerpStartTime = level.inttime;
        }

        // Smoothly lerp current offset toward target offset
        {
            float dt       = level.frametime * c->m_params.aimLerpSpeed;
            float lerpFrac = Q_clamp_float(dt, 0.0, 1.0);

            c->m_combat.aimOffset[0] =
                c->m_combat.aimOffset[0] + (c->m_combat.aimOffsetTarget[0] - c->m_combat.aimOffset[0]) * lerpFrac;
            c->m_combat.aimOffset[1] =
                c->m_combat.aimOffset[1] + (c->m_combat.aimOffsetTarget[1] - c->m_combat.aimOffset[1]) * lerpFrac;
            c->m_combat.aimOffset[2] =
                c->m_combat.aimOffset[2] + (c->m_combat.aimOffsetTarget[2] - c->m_combat.aimOffset[2]) * lerpFrac;
        }

        // Added in OPM
        //  Scoped accuracy: reduce aim spread when zoomed in, rewarding the
        //  bot for taking time to scope. The benefit is reduced against fast-
        //  moving targets — tracking a sprinting enemy through a scope is hard.
        float spreadMult = c->m_params.attackSpreadMult;
        if (c->controlledEnt->IsZoomed()) {
            float targetSpeed = c->m_enemy.enemy->velocity.length();
            // Full scoped benefit at speed 0, no benefit above 200 units/s
            float movePenalty = Q_clamp_float(targetSpeed / 200.0f, 0.0f, 1.0f);
            float scopeScale  = c->m_params.scopedAimScale + (1.0f - c->m_params.scopedAimScale) * movePenalty;
            spreadMult *= scopeScale;
        }

        c->rotation.AimAt(vTarget + c->m_combat.aimOffset * spreadMult);
    } else {
        c->AimAtAimNode();
    }

    if (bNoMove) {
        c->m_combat.standingStill = true;
        return;
    }

    fEnemyDistanceSquared = (c->controlledEnt->origin - c->m_enemy.lastPos).lengthSquared();

    //
    // Added in OPM
    //  Stand still to aim at long range targets (more accurate).
    //  standStillDistance is modulated by personality: patient bots (snipers)
    //  stop moving at shorter distances, aggressive bots keep pushing.
    //
    const float standStillSq       = c->m_params.standStillDistance * c->m_params.standStillDistance;
    const float longRangeThreshold = (c->m_params.standStillDistance * 2) * (c->m_params.standStillDistance * 2);

    if (bCanSee && bFiring && bInWeaponRange && fEnemyDistanceSquared > longRangeThreshold) {
        // Long range: always stop
        c->m_combat.standingStill = true;
        c->movement.ClearMove();
    } else if (bCanSee && bFiring && bInWeaponRange && fEnemyDistanceSquared > standStillSq) {
        // Mid range: stop periodically to aim, then move
        if (rand() % 100 < 30) {
            c->m_combat.standingStill = true;
            c->movement.ClearMove();
        } else {
            c->m_combat.standingStill = false;
        }
    } else {
        c->m_combat.standingStill = false;
    }

    //
    // Added in OPM
    //  Leaning during combat: periodically lean left/right when stationary
    //
    if (bCanSee && c->m_combat.standingStill) {
        if (level.inttime >= c->m_idle.leanTime) {
            c->m_idle.leanTime = level.inttime + 1500 + (int)G_Random(2000);

            // Pick lean direction: left, right, or none
            int roll = rand() % 5;
            if (roll < 2) {
                c->m_idle.leanDir = -1;
            } else if (roll < 4) {
                c->m_idle.leanDir = 1;
            } else {
                c->m_idle.leanDir = 0;
            }
        }
    } else {
        // Not standing still, don't lean
        c->m_idle.leanDir = 0;
    }

    //
    // Added in OPM
    //  Combat crouching: crouch when standing still to reduce profile
    //  and improve accuracy. Decided once when entering standing-still state.
    //
    if (c->m_combat.standingStill) {
        if (!c->m_combat.crouching && !c->m_combat.crouchDecided) {
            c->m_combat.crouchDecided = true;
            if (rand() % 100 < c->m_params.crouchChance) {
                c->m_combat.crouching = true;
            }
        }
    } else {
        c->m_combat.crouching     = false;
        c->m_combat.crouchDecided = false;
    }

    if (c->m_combat.crouching) {
        c->m_botCmd.upmove = -127;
    } else {
        c->m_botCmd.upmove = 0;
    }

    //
    // Changed in OPM
    //  Combat strafing with varied, unpredictable timing.
    //
    if (bCanSee && !bMelee) {
        // Added in OPM
        //  strafeChance controls whether the bot strafes at all during combat.
        //  Patient bots (snipers/campers) strafe less to stay accurate.
        //  In close combat (< engageDistanceMin), always strafe for survival.
        bool bShouldStrafe = (rand() % 100 < c->m_params.strafeChance)
                          || fEnemyDistanceSquared < c->m_params.engageDistanceMin * c->m_params.engageDistanceMin;

        if (bShouldStrafe && level.inttime >= c->m_combat.strafeTime) {
            int roll = rand() % 10;

            if (roll < 2) {
                // Quick tap: short hold, then switch
                c->m_combat.strafeTime = level.inttime + 150 + (int)G_Random(250);
                c->m_combat.strafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 4) {
                // Hold direction: commit to one side for a while
                c->m_combat.strafeTime = level.inttime + 600 + (int)G_Random(1200);
                c->m_combat.strafeDir  = (rand() % 2) ? 127 : -127;
            } else if (roll < 8) {
                // Pause: stop strafing, longer duration
                c->m_combat.strafeTime = level.inttime + 300 + (int)G_Random(700);
                c->m_combat.strafeDir  = 0;
            } else {
                // Double-tap: reverse current direction
                c->m_combat.strafeTime = level.inttime + 100 + (int)G_Random(200);
                c->m_combat.strafeDir  = c->m_combat.strafeDir > 0 ? -127 : 127;
            }
        } else if (!bShouldStrafe) {
            c->m_combat.strafeDir = 0;
        }

        c->m_botCmd.rightmove = c->m_combat.strafeDir;
    }

    if (c->m_combat.standingStill) {
        return;
    }

    // Changed in OPM
    //  Combat movement: when the bot can see and attack, stop and fight.
    //  engageDistanceMin: patient bots (snipers) back away when enemies get too close.
    const float engageMinSq = c->m_params.engageDistanceMin * c->m_params.engageDistanceMin;

    if (bCanSee && bCanAttack && !bMelee && fEnemyDistanceSquared < engageMinSq) {
        // Too close — back away from enemy
        Vector awayDir = c->controlledEnt->origin - c->m_enemy.enemy->origin;
        awayDir.z      = 0;
        if (awayDir.lengthSquared() > 1) {
            awayDir.normalize();
        }
        c->movement.MoveTo(c->controlledEnt->origin + awayDir * c->m_params.engageDistanceMin);
    } else if (bCanSee && bCanAttack && !bMelee && bInWeaponRange) {
        // In weapon range — stop and fight, let strafing handle lateral movement
        c->movement.ClearMove();
    } else if (!c->movement.IsMoving() || (c->m_enemy.oldPos != c->m_enemy.lastPos && !c->movement.MoveDone())) {
        // Can't see enemy or using melee — close the distance
        c->movement.MoveTo(c->m_enemy.lastPos);

        if (!bCanSee && c->movement.MoveDone()) {
            // Lost track of the enemy
            c->ClearEnemy();
            return;
        }
    }

    if (c->movement.IsMoving()) {
        c->m_combat.attackTime = level.inttime + 500 + (int)G_Random(1000);
    }
}

/*
===========================================================================
BotStateGrenade
===========================================================================
*/

class BotStateGrenade : public BotState
{
    BotController *m_controller;

public:
    BotStateGrenade(BotController *controller)
        : m_controller(controller)
    {}

    const char *GetName() const override { return "Grenade"; }

    bool CheckCondition() override;
    void Begin() override;
    void Think() override;
};

bool BotStateGrenade::CheckCondition()
{
    BotController *c = m_controller;

    // Added in OPM
    //  Scan for nearby enemy projectiles (grenades) and flee from them
    if (c->m_grenade.grenade && c->m_grenade.grenade->IsSubclassOfProjectile()) {
        float distSq = (c->m_grenade.grenade->origin - c->controlledEnt->origin).lengthSquared();
        float radius = c->m_params.grenadeAvoidRadius;

        if (distSq < radius * radius) {
            return true;
        }
    }

    c->m_grenade.grenade = NULL;

    float      radiusSq = Square(c->m_params.grenadeAvoidRadius);
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
        if (proj->GetOwner() == c->controlledEnt) {
            continue;
        }

        // Ignore friendly projectiles in team games
        Sentient *projOwner = proj->GetOwner();
        if (projOwner && projOwner->IsSubclassOfPlayer() && g_gametype->integer >= GT_TEAM) {
            Player *p = static_cast<Player *>(projOwner);
            if (p->GetTeam() == c->controlledEnt->GetTeam()) {
                continue;
            }
        }

        float distSq = (ent->origin - c->controlledEnt->origin).lengthSquared();
        if (distSq < radiusSq) {
            c->m_grenade.grenade   = ent;
            c->m_grenade.avoidTime = level.inttime + 3000;
            return true;
        }
    }

    if (level.inttime < c->m_grenade.avoidTime) {
        return true;
    }

    return false;
}

// Added in OPM
//  Clear idle state and movement when fleeing from grenade
void BotStateGrenade::Begin()
{
    BotController *c = m_controller;
    c->movement.ClearMove();
    c->m_idle.reset();
}

void BotStateGrenade::Think()
{
    BotController *c = m_controller;

    // Added in OPM
    //  Flee away from the grenade
    if (!c->m_grenade.grenade) {
        return;
    }

    Vector grenadePos = c->m_grenade.grenade->origin;
    Vector fleeDir    = c->controlledEnt->origin - grenadePos;
    VectorNormalizeFast(fleeDir);

    c->movement.AvoidPath(grenadePos, c->m_params.grenadeAvoidRadius, fleeDir * 512);
}

/*
===========================================================================
BotStateIdle
===========================================================================
*/

class BotStateIdle : public BotState
{
    BotController *m_controller;

public:
    BotStateIdle(BotController *controller)
        : m_controller(controller)
    {}

    const char *GetName() const override { return "Idle"; }

    bool CheckCondition() override;
    void Think() override;
};

bool BotStateIdle::CheckCondition()
{
    BotController *c = m_controller;

    if (c->m_combat.attackTime) {
        return false;
    }

    return true;
}

void BotStateIdle::Think()
{
    BotController *c = m_controller;

    if (c->CheckWindows()) {
        c->m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        c->m_iLastFireTime = level.inttime;
    } else {
        c->m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        c->CheckReload();
    }

    //
    // Added in OPM
    //  Human-like idle behavior: periodic pauses to look around
    //
    if (c->m_idle.pausing) {
        // Currently paused - look around
        if (level.inttime >= c->m_idle.pauseTime) {
            // Done pausing, resume movement
            c->m_idle.pausing = false;
            // Changed in OPM
            //  Walk frequency and duration are now personality-driven via
            //  idleWalkChance/idleWalkMinTime/idleWalkRandomTime (stealth trait).
            if (rand() % c->m_params.idleWalkChance == 0) {
                c->m_idle.walking  = true;
                c->m_idle.walkTime = level.inttime + (int)(c->m_params.idleWalkMinTime * 1000)
                                   + (int)G_Random(c->m_params.idleWalkRandomTime * 1000);
            }
        } else {
            // Look around periodically during pause
            if (level.inttime >= c->m_idle.lookTime) {
                c->m_idle.lookTime = level.inttime + 800 + (int)G_Random(1200);

                // Pick a random look direction
                Vector lookAngles = c->controlledEnt->angles;
                lookAngles.y += G_CRandom(90);
                lookAngles.x = G_CRandom(15);
                c->rotation.SetTargetAngles(lookAngles);
            }
            return;
        }
    } else {
        // Changed in OPM
        //  Pause frequency and duration are now personality-driven via
        //  idlePauseChance/idlePauseMinTime/idlePauseRandomTime (patience trait).
        // Changed in OPM
        //  Idle pause gate no longer consults attractive nodes — belief spikes
        //  are handled by the m_bBeliefSpiked path below, and the unified
        //  belief map is the single source of truth for "busy area".
        // Fixed in OPM
        //  Guard pause with !IsMoving(): before this commit MoveToBestAttractivePoint
        //  acted as the gate (returning true = "issued a move, skip pause"). Without
        //  that call the bot was pausing mid-patrol on every unlucky rand() roll.
        if (!c->movement.IsMoving() && rand() % c->m_params.idlePauseChance == 0) {
            c->m_idle.pausing   = true;
            c->m_idle.pauseTime = level.inttime + (int)(c->m_params.idlePauseMinTime * 1000)
                                + (int)G_Random(c->m_params.idlePauseRandomTime * 1000);
            c->m_idle.lookTime = level.inttime + 500;
            c->movement.ClearMove();
            return;
        }
    }

    //
    // Added in OPM
    //  Occasionally walk instead of run
    //
    if (c->m_idle.walking && level.inttime >= c->m_idle.walkTime) {
        c->m_idle.walking = false;
    }

    // Added in OPM
    //  Belief spike: a sound event passed the gate in NoticeEvent.
    //  Interrupt whatever we're doing (pause or current move) and
    //  immediately re-evaluate the patrol target so the bot reacts
    //  without waiting for the current destination to be reached.
    if (c->m_bBeliefSpiked) {
        c->m_bBeliefSpiked = false;
        c->m_idle.pausing  = false;
        c->movement.ClearMove();
    }

    // Changed in OPM
    //  Pre-aim toward highest-belief direction when not in combat.
    {
        Vector beliefPos = c->beliefMap.GetHighestBeliefPos(c->controlledEnt->origin);
        if (beliefPos != vec_zero && c->controlledEnt->CanSee(beliefPos, 120, 2048, false)) {
            c->rotation.AimAt(beliefPos);
        } else {
            c->AimAtAimNode();
        }
    }

    // Changed in OPM
    //  Belief-driven patrol: move toward the highest-belief zone. The belief
    //  map is now the single source of truth — attractive nodes are injected
    //  into it via UpdateFromAttractiveNodes.
    if (!c->movement.IsMoving()) {
        Vector beliefPos = c->beliefMap.GetHighestBeliefPos(c->controlledEnt->origin);
        if (beliefPos != vec_zero) {
            if (g_bot_debug_state->integer >= 2) {
                gi.Printf(
                    "BOT %s: Patrol - moving to belief zone at (%.0f, %.0f), blocked=%d\n",
                    c->controlledEnt->client->pers.netname,
                    beliefPos.x,
                    beliefPos.y,
                    c->beliefMap.IsPathBlocked(beliefPos) ? 1 : 0
                );
            }
            c->movement.MoveTo(beliefPos);

            if (c->movement.MoveDone()) {
                c->beliefMap.ClearZone(beliefPos);
                c->beliefMap.MarkVisited(beliefPos);
            }
        } else if (c->m_enemy.deathPos != vec_zero) {
            c->movement.MoveTo(c->m_enemy.deathPos);

            if (c->movement.MoveDone()) {
                c->m_enemy.deathPos = vec_zero;
            }
        } else {
            // Fixed in OPM
            //  Replaced AvoidPath with MoveTo + retry cooldown.
            //  AvoidPath called NewMove() (m_bPathing = true) before checking
            //  whether pathfinding succeeded, so when FindPathAway found no nodes
            //  it left m_bPathing=true with a random 256-unit fallback goal.
            //  MoveThink then unconditionally called ClearMove() (no path nodes)
            //  but still applied forwardmove toward that goal for the frame —
            //  causing the bot to twitch/run-in-place every frame.
            //  MoveTo sets m_bPathing=false when no path is found, so MoveThink
            //  exits early with forwardmove=0 and there is no visible artifact.
            //  The cooldown prevents hammering the pathfinder on every frame when
            //  the random destination is persistently unreachable.
            if (level.inttime >= c->m_idle.exploreRetryTime) {
                Vector randomDir(G_CRandom(1.0f), G_CRandom(1.0f), 0);
                VectorNormalize2D(randomDir);
                c->movement.MoveTo(c->controlledEnt->origin + randomDir * (512 + G_Random(1024)));

                if (!c->movement.IsMoving()) {
                    // Path not found — back off and try a new direction later
                    c->m_idle.exploreRetryTime = level.inttime + 500;
                }
            }
        }
    }
}

/*
===========================================================================
BotController — state management
===========================================================================
*/

// Added in OPM
//  Create one concrete BotState instance per slot.
//  Slot order matches the original botfuncs[] order so m_StateFlags bits
//  are preserved (Attack=0, Curious=1, Grenade=2, Idle=3, Weapon=4).
void BotController::InitStates()
{
    m_states[0] = new BotStateAttack(this);
    m_states[1] = nullptr; // Removed in OPM — curious state replaced by belief-map-driven patrol
    m_states[2] = new BotStateGrenade(this);
    m_states[3] = new BotStateIdle(this);
    m_states[4] = nullptr; // Weapon state — disabled, slot reserved
}

void BotController::CheckStates(void)
{
    m_StateCount = 0;

    unsigned int oldFlags = m_StateFlags;

    for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
        BotState *state = m_states[i];

        if (!state) {
            continue;
        }

        if (state->CheckCondition()) {
            if (!(m_StateFlags & (1 << i))) {
                m_StateFlags |= 1 << i;

                if (g_bot_debug_state->integer) {
                    gi.Printf("BOT %s: ENTER state %s\n", controlledEnt->client->pers.netname, state->GetName());
                }

                state->Begin();
            }

            m_StateCount++;
            state->Think();
        } else {
            if (m_StateFlags & (1 << i)) {
                m_StateFlags &= ~(1 << i);

                if (g_bot_debug_state->integer) {
                    gi.Printf("BOT %s: EXIT state %s\n", controlledEnt->client->pers.netname, state->GetName());
                }

                state->End();
            }
        }
    }

    // Added in OPM - Debug active states (level 2)
    if (g_bot_debug_state->integer >= 2 && m_StateFlags != oldFlags) {
        char stateList[256] = {0};
        for (int i = 0; i < MAX_BOT_FUNCTIONS; i++) {
            if ((m_StateFlags & (1 << i)) && m_states[i]) {
                if (stateList[0]) {
                    strcat(stateList, ", ");
                }
                strcat(stateList, m_states[i]->GetName());
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

void BotController::State_Reset(void)
{
    m_combat.reset();
    m_enemy.reset();
    m_bBeliefSpiked        = false;
    m_iBeliefSpikeCooldown = 0;
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
