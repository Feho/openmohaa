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
#include "../g_local.h"
#include "playerbot.h"

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

bool BotController::CheckCondition_Attack(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    for (int i = 1; i <= sents.NumObjects(); i++) {
        Sentient *sent = sents.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828);

        if (controlledEnt->CanSee(sent, 80, maxDistance, false)) {
            if (m_pEnemy != sent) {
                m_iEnemyEyesTag = -1;
            }

            if (!m_pEnemy) {
                m_iLastUnseenTime = level.inttime;
            }

            m_pEnemy        = sent;
            m_vLastEnemyPos = m_pEnemy->origin;
        }

        if (m_pEnemy) {
            m_iAttackTime = level.inttime + 1000;
            return true;
        }
    }

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

    if (!m_pEnemy || !IsValidEnemy(m_pEnemy)) {
        // Ignore dead enemies
        m_iAttackTime = 0;
        return;
    }
    float fDistanceSquared = (m_pEnemy->origin - controlledEnt->origin).lengthSquared();

    m_vOldEnemyPos = m_vLastEnemyPos;

    bCanSee =
        controlledEnt->CanSee(m_pEnemy, 20, Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828), false);

    if (bCanSee) {
        if (!pWeap) {
            return;
        }

        bCanAttack = true;
        if (m_iLastUnseenTime) {
            const float reactionTime = Q_min(1000 * Q_min(1, fDistanceSquared / Square(2048)), 1000);
            const unsigned int minDelay = g_bot_attack_react_min_delay->value * 1000;
            const unsigned int randomDelay = g_bot_attack_react_random_delay->value * 1000;
            if (level.inttime <= m_iLastUnseenTime + minDelay + G_Random(randomDelay)) {
                bCanAttack = false;
            } else {
                m_iLastUnseenTime = 0;
            }
        }

        if (bCanAttack) {
            const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
            float     fPrimaryBulletRange          = pWeap->GetBulletRange(FIRE_PRIMARY) / 1.25f;
            float     fPrimaryBulletRangeSquared   = fPrimaryBulletRange * fPrimaryBulletRange;
            float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
            float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;
            float     fSpreadFactor                = pWeap->GetSpreadFactor(FIRE_PRIMARY);

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
                m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                controlledEnt->ZoomOff();
            } else if (fDistanceSquared > fPrimaryBulletRangeSquared) {
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
                        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                        controlledEnt->ZoomOff();
                    } else if (fSpreadFactor < 0.25) {
                        bFiring = true;
                        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
                        if (pWeap->GetZoom()) {
                            if (!controlledEnt->IsZoomed()) {
                                m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                            } else {
                                m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
                            }
                        }
                    } else {
                        bNoMove = true;
                        movement.ClearMove();
                    }
                } else {
                    bFiring = true;
                    m_botCmd.buttons |= BUTTON_ATTACKLEFT;
                }
            }

            //
            // Burst
            //

            if (m_iLastBurstTime) {
                if (level.inttime > m_iLastBurstTime + maxBurstTime) {
                    m_iLastBurstTime      = 0;
                    m_iContinuousFireTime = 0;
                } else {
                    m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
                }
            } else {
                if (bFiring) {
                    m_iContinuousFireTime += level.intframetime;
                } else {
                    m_iContinuousFireTime = 0;
                }

                if (!m_iLastBurstTime && m_iContinuousFireTime > maxcontinuousFireTime) {
                    m_iLastBurstTime      = level.inttime;
                    m_iContinuousFireTime = 0;
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

            m_iAttackTime        = level.inttime + 1000;
            m_iAttackStopAimTime = level.inttime + 3000;
            m_iLastSeenTime      = level.inttime;
            m_vLastEnemyPos      = m_pEnemy->origin;
        }
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        fMinDistanceSquared = 0;

        if (level.inttime > m_iLastSeenTime + 2000) {
            m_iLastUnseenTime = level.inttime;
        }
    }

    if (bCanSee || level.inttime < m_iAttackStopAimTime) {
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

        if (level.inttime >= m_iLastAimTime + 100) {
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

    if (bNoMove) {
        return;
    }

    fEnemyDistanceSquared = (controlledEnt->origin - m_vLastEnemyPos).lengthSquared();

    if ((!movement.MoveToBestAttractivePoint(5) && !movement.IsMoving())
        || (m_vOldEnemyPos != m_vLastEnemyPos && !movement.MoveDone()) || fEnemyDistanceSquared < fMinDistanceSquared) {
        if (!bMelee || !bCanSee) {
            if (fEnemyDistanceSquared < fMinDistanceSquared) {
                Vector vDir = controlledEnt->origin - m_vLastEnemyPos;
                VectorNormalizeFast(vDir);

                movement.AvoidPath(m_vLastEnemyPos, fMinDistance, Vector(controlledEnt->orientation[1]) * 512);
            } else {
                movement.MoveTo(m_vLastEnemyPos);
            }

            if (!bCanSee && movement.MoveDone()) {
                // Lost track of the enemy
                ClearEnemy();
                return;
            }
        } else {
            movement.MoveTo(m_vLastEnemyPos);
        }
    }

    if (movement.IsMoving()) {
        m_iAttackTime = level.inttime + 1000;
    }
}
