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
// playerbot_rotation.cpp: Manages bot rotation

#include "g_local.h"
#include "playerbot.h"
#include "gamecvars.h"

BotRotation::BotRotation()
{
    m_vAngDelta      = vec_zero;
    m_vAngSpeed      = vec_zero;
    m_vTargetAng     = vec_zero;
    m_vCurrentAng    = vec_zero;
    m_vPrevTargetAng = vec_zero;
    m_bOvershootPhase = false;
    m_fSettleFrac     = 1.0;
    m_fOvershootYaw   = 0;
    m_fOvershootPitch = 0;
}

void BotRotation::SetControlledEntity(Player *newEntity)
{
    controlledEntity = newEntity;
}

float AngleDifference(float ang1, float ang2)
{
    float diff;

    diff = ang1 - ang2;
    if (ang1 > ang2) {
        if (diff > 180.0) {
            diff -= 360.0;
        }
    } else {
        if (diff < -180.0) {
            diff += 360.0;
        }
    }
    return diff;
}

// Changed in OPM
//  Two-phase aim model: fast flick with overshoot, then dampened settle.
//  When the target changes significantly (>15 degrees), the bot flicks
//  past the target by a random amount, then smoothly settles back.
//  Small tracking adjustments use smooth interpolation with micro-noise.
void BotRotation::TurnThink(usercmd_t& botcmd, usereyes_t& eyeinfo)
{
    float maxChange = Q_max(360, g_bot_turn_speed->integer);

    if (m_vTargetAng[PITCH] > 180) {
        m_vTargetAng[PITCH] -= 360;
    }

    //
    // Detect large target change (new flick)
    //
    float yawDelta   = fabs(AngleDifference(m_vTargetAng[YAW], m_vPrevTargetAng[YAW]));
    float pitchDelta = fabs(AngleDifference(m_vTargetAng[PITCH], m_vPrevTargetAng[PITCH]));

    if (yawDelta > 15 || pitchDelta > 10) {
        // New target acquired - start overshoot phase
        float overshootScale = g_bot_aim_overshoot->value;

        m_bOvershootPhase = true;
        m_fSettleFrac     = 0;

        // Overshoot proportional to angle change, with randomness
        m_fOvershootYaw   = AngleDifference(m_vPrevTargetAng[YAW], m_vTargetAng[YAW]) * overshootScale * (0.7 + G_Random(0.6));
        m_fOvershootPitch = AngleDifference(m_vPrevTargetAng[PITCH], m_vTargetAng[PITCH]) * overshootScale * (0.5 + G_Random(0.5));
    }

    m_vPrevTargetAng = m_vTargetAng;

    //
    // Calculate effective target (with overshoot applied)
    //
    Vector effectiveTarget = m_vTargetAng;

    if (m_bOvershootPhase) {
        float settleSpeed = g_bot_aim_settle_speed->value;
        m_fSettleFrac += level.frametime * settleSpeed;

        if (m_fSettleFrac >= 1.0) {
            m_fSettleFrac     = 1.0;
            m_bOvershootPhase = false;
        }

        // Overshoot decays as settle progresses
        float overshootMult = 1.0 - m_fSettleFrac;
        overshootMult *= overshootMult; // Quadratic decay for snappy settle

        effectiveTarget[YAW]   += m_fOvershootYaw * overshootMult;
        effectiveTarget[PITCH] += m_fOvershootPitch * overshootMult;
    }

    //
    // Add micro-noise to simulate hand tremor
    //
    float noiseScale = g_bot_aim_noise->value;
    effectiveTarget[YAW]   += G_CRandom(noiseScale);
    effectiveTarget[PITCH] += G_CRandom(noiseScale * 0.5);

    //
    // Smooth interpolation toward effective target
    //
    for (int i = 0; i < 2; i++) {
        m_vCurrentAng[i]  = AngleMod(m_vCurrentAng[i]);
        effectiveTarget[i] = AngleMod(effectiveTarget[i]);

        float diff      = AngleDifference(m_vCurrentAng[i], effectiveTarget[i]);
        float deltaDiff = fabs(diff);

        float maxChangeDelta = maxChange * level.frametime;
        if (maxChangeDelta > deltaDiff) {
            maxChangeDelta = deltaDiff;
        }

        // Acceleration: faster rotation for larger differences
        float changeSpeed = g_bot_turn_speed->integer;
        if (deltaDiff >= 20) {
            m_vAngSpeed[i] = Q_min(1.0, m_vAngSpeed[i] + changeSpeed * level.frametime);
            maxChangeDelta *= m_vAngSpeed[i];
        } else {
            m_vAngSpeed[i] = Q_max(0.0, m_vAngSpeed[i] - changeSpeed * level.frametime);
        }

        float speed      = diff * level.frametime * 10;
        m_vAngDelta[i]   = Q_clamp_float(speed, -maxChangeDelta, maxChangeDelta);
        m_vCurrentAng[i] = AngleMod(m_vCurrentAng[i] - m_vAngDelta[i]);
    }

    if (m_vCurrentAng[PITCH] > 180) {
        m_vCurrentAng[PITCH] -= 360;
    }

    eyeinfo.angles[0] = m_vCurrentAng[0];
    eyeinfo.angles[1] = m_vCurrentAng[1];
    botcmd.angles[0]  = ANGLE2SHORT(m_vCurrentAng[0]) - controlledEntity->client->ps.delta_angles[0];
    botcmd.angles[1]  = ANGLE2SHORT(m_vCurrentAng[1]) - controlledEntity->client->ps.delta_angles[1];
    botcmd.angles[2]  = ANGLE2SHORT(m_vCurrentAng[2]) - controlledEntity->client->ps.delta_angles[2];
}

/*
====================
GetTargetAngles

Return the target angle
====================
*/
const Vector& BotRotation::GetTargetAngles() const
{
    return m_vTargetAng;
}

/*
====================
SetTargetAngles

Set the bot's angle
====================
*/
void BotRotation::SetTargetAngles(Vector vAngles)
{
    m_vTargetAng = vAngles;
}

/*
====================
AimAt

Make the bot face to the specified direction
====================
*/
void BotRotation::AimAt(Vector vPos)
{
    Vector vDelta = vPos - controlledEntity->EyePosition();
    Vector vTarget;

    VectorNormalize(vDelta);
    vectoangles(vDelta, vTarget);

    SetTargetAngles(vTarget);
}
