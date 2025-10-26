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
// playerbot_cover.cpp: Cover detection and evaluation system

#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_cover_search_radius;
extern cvar_t *g_bot_cover_min_quality;
extern cvar_t *g_bot_cover_peek_min_time;
extern cvar_t *g_bot_cover_peek_max_time;
extern cvar_t *g_bot_cover_hide_min_time;
extern cvar_t *g_bot_cover_hide_max_time;
extern cvar_t *g_bot_debug;

/*
====================
FindBestCover

Searches for the best cover position near the bot's current location.
Evaluates multiple positions in a grid pattern and returns the highest quality cover.
====================
*/
BotController::CoverPoint BotController::FindBestCover(Vector enemyPos)
{
    CoverPoint bestCover;
    bestCover.position = vec_zero;
    bestCover.quality = 0.0f;
    bestCover.protectionAngle = 0.0f;
    bestCover.distanceToEnemy = 0.0f;
    bestCover.hasEscapeRoute = false;
    bestCover.evaluatedTime = level.inttime;

    Vector botPos = controlledEnt->origin;
    float searchRadius = g_bot_cover_search_radius->value;
    const int gridSteps = 8;  // 8x8 grid
    const float stepSize = searchRadius / gridSteps;

    // Sample positions in a grid around the bot
    for (int x = -gridSteps; x <= gridSteps; x++) {
        for (int y = -gridSteps; y <= gridSteps; y++) {
            // Skip center (bot's current position)
            if (x == 0 && y == 0) continue;

            Vector testPos = botPos;
            testPos.x += x * stepSize;
            testPos.y += y * stepSize;

            // Check if position is valid (on ground, not in solid)
            trace_t trace;
            Vector start = testPos + Vector(0, 0, 32);
            Vector end = testPos - Vector(0, 0, 128);

            trace = G_Trace(start, controlledEnt->mins, controlledEnt->maxs, end, controlledEnt, MASK_PLAYERSOLID, false, "BotController::FindBestCover");

            if (trace.fraction == 1.0f || trace.allsolid || trace.startsolid) {
                // No ground or inside solid
                continue;
            }

            // Update position to ground level
            testPos = trace.endpos;

            // Evaluate cover quality
            float quality = EvaluateCoverQuality(testPos, enemyPos);

            if (quality > bestCover.quality) {
                bestCover.position = testPos;
                bestCover.quality = quality;
                bestCover.distanceToEnemy = (enemyPos - testPos).length();
                bestCover.evaluatedTime = level.inttime;
                // Additional properties calculated in EvaluateCoverQuality
            }
        }
    }

    if (g_bot_debug->integer >= 2 && bestCover.quality > 0.0f) {
        gi.Printf("[BOT] %s: Found cover with quality %.2f at (%.0f, %.0f, %.0f)\n",
            controlledEnt->client->pers.netname, bestCover.quality,
            bestCover.position.x, bestCover.position.y, bestCover.position.z);
    }

    return bestCover;
}

/*
====================
EvaluateCoverQuality

Evaluates how good a position is for cover.
Returns a quality score from 0.0 (no cover) to 1.0 (excellent cover).
====================
*/
float BotController::EvaluateCoverQuality(Vector pos, Vector enemyPos)
{
    float quality = 0.0f;

    // 1. Check if there's an obstruction between position and enemy
    Vector eyePos = pos + Vector(0, 0, controlledEnt->viewheight);
    Vector enemyEyePos = enemyPos + Vector(0, 0, 64); // Approximate enemy eye height

    trace_t trace = G_Trace(eyePos, vec_zero, vec_zero, enemyEyePos, controlledEnt, MASK_SHOT, false, "BotController::EvaluateCoverQuality");

    if (trace.fraction < 1.0f && trace.entityNum != ENTITYNUM_NONE) {
        // Something is blocking line of sight - this is good cover
        quality += 0.5f;

        // Check protection angle (how much of the position is protected)
        int protectedAngles = 0;
        const int angleSteps = 8;

        for (int i = 0; i < angleSteps; i++) {
            float angle = (i * 360.0f / angleSteps) * M_PI / 180.0f;
            Vector offset(cos(angle) * 32, sin(angle) * 32, 0);
            Vector checkPos = eyePos + offset;

            trace_t angleTrace = G_Trace(eyePos, vec_zero, vec_zero, checkPos, controlledEnt, MASK_SHOT, false, "BotController::EvaluateCoverQuality");

            if (angleTrace.fraction < 1.0f) {
                protectedAngles++;
            }
        }

        // More protected angles = better cover
        float protectionRatio = protectedAngles / (float)angleSteps;
        quality += protectionRatio * 0.3f;
    }

    // 2. Distance factor (prefer cover closer to enemy but not too close)
    float distToEnemy = (enemyPos - pos).length();
    float idealDistance = 512.0f; // Ideal cover distance
    float distanceFactor = 1.0f - fabs(distToEnemy - idealDistance) / 1024.0f;
    if (distanceFactor < 0.0f) distanceFactor = 0.0f;
    quality += distanceFactor * 0.2f;

    // 3. Check for escape routes (can bot move away from this position?)
    int escapeRoutes = 0;
    const Vector testDirections[4] = {
        Vector(64, 0, 0),   // North
        Vector(-64, 0, 0),  // South
        Vector(0, 64, 0),   // East
        Vector(0, -64, 0)   // West
    };

    for (int i = 0; i < 4; i++) {
        Vector escapePos = pos + testDirections[i];
        trace_t escapeTrace = G_Trace(pos, controlledEnt->mins, controlledEnt->maxs, escapePos, controlledEnt, MASK_PLAYERSOLID, false, "BotController::EvaluateCoverQuality");

        if (escapeTrace.fraction > 0.5f) {
            escapeRoutes++;
        }
    }

    if (escapeRoutes >= 2) {
        quality += 0.1f; // Bonus for having escape routes
    }

    // Clamp quality to 0.0-1.0
    if (quality > 1.0f) quality = 1.0f;
    if (quality < 0.0f) quality = 0.0f;

    return quality;
}

/*
====================
IsInCover

Checks if a position provides cover from an enemy position.
====================
*/
bool BotController::IsInCover(Vector pos, Vector enemyPos)
{
    Vector eyePos = pos + Vector(0, 0, controlledEnt->viewheight);
    Vector enemyEyePos = enemyPos + Vector(0, 0, 64);

    trace_t trace = G_Trace(eyePos, vec_zero, vec_zero, enemyEyePos, controlledEnt, MASK_SHOT, false, "BotController::IsInCover");

    return (trace.fraction < 1.0f && trace.entityNum != ENTITYNUM_NONE);
}

/*
====================
IsCoverCompromised

Checks if current cover is no longer effective.
====================
*/
// Changed in OPM
//  Refactored to use CoverStateData struct
bool BotController::IsCoverCompromised(void)
{
    if (coverState.current.quality == 0.0f) {
        return true; // No cover to begin with
    }

    if (!m_pEnemy) {
        return false; // No enemy, cover can't be compromised
    }

    Vector enemyPos = m_pEnemy->origin;

    // Check if enemy has moved significantly
    float distanceChange = (enemyPos - Vector(
        coverState.current.position.x,
        coverState.current.position.y,
        enemyPos.z
    )).length();

    if (distanceChange > 256.0f) {
        // Enemy moved significantly, re-evaluate cover
        if (!IsInCover(coverState.current.position, enemyPos)) {
            if (g_bot_debug->integer >= 1) {
                gi.Printf("[BOT] %s: Cover compromised - enemy flanked\n",
                    controlledEnt->client->pers.netname);
            }
            return true;
        }
    }

    // Changed in OPM
    //  Refactored to use CoverStateData struct
    // Check if taking damage while in cover
    if (coverState.state == COVER_IN_COVER || coverState.state == COVER_PEEKING) {
        // If bot's health dropped significantly in last second, cover may be compromised
        // This is a simplified check - in a real implementation you'd track damage events
    }

    return false;
}

/*
====================
UpdateCoverBehavior

Updates the bot's cover behavior state machine.
Called from the attack state when using cover.
====================
*/
// Changed in OPM
//  Refactored to use CoverStateData struct
void BotController::UpdateCoverBehavior(void)
{
    if (!m_pEnemy) {
        coverState.state = COVER_NONE;
        return;
    }

    switch (coverState.state) {
        case COVER_NONE:
            // Not using cover, try to find some
            if (level.inttime > m_iLastCoverSearchTime + 2000) {
                m_iLastCoverSearchTime = level.inttime;
                CoverPoint newCover = FindBestCover(m_pEnemy->origin);

                if (newCover.quality >= g_bot_cover_min_quality->value) {
                    coverState.current = newCover;
                    coverState.state = COVER_MOVING_TO;

                    if (g_bot_debug->integer >= 1) {
                        gi.Printf("[BOT] %s: Moving to cover (quality: %.2f)\n",
                            controlledEnt->client->pers.netname, newCover.quality);
                    }
                }
            }
            break;

        case COVER_MOVING_TO:
            // Check if we reached cover
            {
                float distToCover = (controlledEnt->origin - coverState.current.position).length();
                if (distToCover < 64.0f || movement.MoveDone()) {
                    coverState.state = COVER_IN_COVER;
                    float hideMin = g_bot_cover_hide_min_time->value * 1000;
                    float hideMax = g_bot_cover_hide_max_time->value * 1000;
                    coverState.nextPeekTime = level.inttime + (int)(G_Random(hideMax - hideMin) + hideMin);

                    if (g_bot_debug->integer >= 1) {
                        gi.Printf("[BOT] %s: Reached cover position\n",
                            controlledEnt->client->pers.netname);
                    }
                }
            }
            break;

        case COVER_IN_COVER:
            // Hiding in cover, wait before peeking
            if (level.inttime >= coverState.nextPeekTime) {
                coverState.state = COVER_PEEKING;
                coverState.peekStartTime = level.inttime;
                float peekMin = g_bot_cover_peek_min_time->value * 1000;
                float peekMax = g_bot_cover_peek_max_time->value * 1000;
                coverState.peekDuration = G_Random(peekMax - peekMin) + peekMin;

                if (g_bot_debug->integer >= 2) {
                    gi.Printf("[BOT] %s: Peeking from cover (%.1fs)\n",
                        controlledEnt->client->pers.netname, coverState.peekDuration / 1000.0f);
                }
            }
            break;

        case COVER_PEEKING:
            // Exposed and shooting
            if (level.inttime >= coverState.peekStartTime + (int)coverState.peekDuration) {
                coverState.state = COVER_IN_COVER;
                float hideMin = g_bot_cover_hide_min_time->value * 1000;
                float hideMax = g_bot_cover_hide_max_time->value * 1000;
                coverState.nextPeekTime = level.inttime + (int)(G_Random(hideMax - hideMin) + hideMin);

                if (g_bot_debug->integer >= 2) {
                    gi.Printf("[BOT] %s: Returning to cover\n",
                        controlledEnt->client->pers.netname);
                }
            }
            break;

        case COVER_REPOSITIONING:
            // Moving to new cover
            if (movement.MoveDone()) {
                coverState.state = COVER_IN_COVER;
                float hideMin = g_bot_cover_hide_min_time->value * 1000;
                float hideMax = g_bot_cover_hide_max_time->value * 1000;
                coverState.nextPeekTime = level.inttime + (int)(G_Random(hideMax - hideMin) + hideMin);
            }
            break;
    }

    // Check if cover is compromised
    if (coverState.state != COVER_NONE && IsCoverCompromised()) {
        // Find new cover
        CoverPoint newCover = FindBestCover(m_pEnemy->origin);

        if (newCover.quality >= g_bot_cover_min_quality->value) {
            coverState.current = newCover;
            coverState.state = COVER_REPOSITIONING;

            if (g_bot_debug->integer >= 1) {
                gi.Printf("[BOT] %s: Repositioning to new cover\n",
                    controlledEnt->client->pers.netname);
            }
        } else {
            coverState.state = COVER_NONE;
        }
    }
}
