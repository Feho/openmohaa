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

// bt_actions_cover.cpp
// Implementation of cover system behavior tree actions
// Added in OPM - Phase 3 Task 3.1d

#include "g_local.h"
#include "bt_actions_cover.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "player.h"
#include "sentient.h"
#include "bot_profile.h"

extern cvar_t *g_bot_cover_search_radius;
extern cvar_t *g_bot_cover_min_quality;
extern cvar_t *g_bot_cover_peek_min_time;
extern cvar_t *g_bot_cover_peek_max_time;
extern cvar_t *g_bot_debug;

/*
====================
EvaluateCoverQualityBT

Helper function to evaluate cover quality for a position.
Ported from BotController::EvaluateCoverQuality() in playerbot_cover.cpp

Added in OPM - Phase 3 Task 3.1d
 Behavior tree version of cover quality evaluation
====================
*/
static float EvaluateCoverQualityBT(
    BotController *bot,
    Player        *player,
    const Vector  &pos,
    const Vector  &enemyPos
)
{
    float quality = 0.0f;

    // 1. Check if there's an obstruction between position and enemy
    Vector eyePos      = pos + Vector(0, 0, player->viewheight);
    Vector enemyEyePos = enemyPos + Vector(0, 0, 64); // Approximate enemy eye height

    trace_t trace = G_Trace(
        eyePos,
        vec_zero,
        vec_zero,
        enemyEyePos,
        player,
        MASK_SHOT,
        false,
        "EvaluateCoverQualityBT"
    );

    if (trace.fraction < BotConstants::TRACE_COMPLETE && trace.entityNum != ENTITYNUM_NONE) {
        // Something is blocking line of sight - this is good cover
        quality += 0.5f;

        // Check protection angle (how much of the position is protected)
        int       protectedAngles = 0;
        const int angleSteps      = 8;

        for (int i = 0; i < angleSteps; i++) {
            float  angle      = (i * 360.0f / angleSteps) * M_PI / 180.0f;
            Vector offset(cos(angle) * 32, sin(angle) * 32, 0);
            Vector checkPos = eyePos + offset;

            trace_t angleTrace = G_Trace(
                eyePos,
                vec_zero,
                vec_zero,
                checkPos,
                player,
                MASK_SHOT,
                false,
                "EvaluateCoverQualityBT"
            );

            if (angleTrace.fraction < BotConstants::TRACE_COMPLETE) {
                protectedAngles++;
            }
        }

        // More protected angles = better cover
        float protectionRatio = protectedAngles / (float)angleSteps;
        quality += protectionRatio * 0.3f;
    }

    // 2. Distance factor (prefer cover at ideal distance from enemy)
    float distToEnemy    = (enemyPos - pos).length();
    float idealDistance  = BotConstants::AWARENESS_RADIUS; // Ideal cover distance
    float distanceFactor = 1.0f - fabs(distToEnemy - idealDistance) / 1024.0f;
    if (distanceFactor < 0.0f) {
        distanceFactor = 0.0f;
    }
    quality += distanceFactor * 0.2f;

    // 3. Check for escape routes (can bot move away from this position?)
    int          escapeRoutes      = 0;
    const Vector testDirections[4] = {
        Vector(64, 0, 0),  // North
        Vector(-64, 0, 0), // South
        Vector(0, 64, 0),  // East
        Vector(0, -64, 0)  // West
    };

    for (int i = 0; i < 4; i++) {
        Vector  escapePos   = pos + testDirections[i];
        trace_t escapeTrace = G_Trace(
            pos,
            player->mins,
            player->maxs,
            escapePos,
            player,
            MASK_PLAYERSOLID,
            false,
            "EvaluateCoverQualityBT"
        );

        if (escapeTrace.fraction > 0.5f) {
            escapeRoutes++;
        }
    }

    if (escapeRoutes >= 2) {
        quality += 0.1f; // Bonus for having escape routes
    }

    // Clamp quality to 0.0-1.0
    if (quality > 1.0f) {
        quality = 1.0f;
    }
    if (quality < 0.0f) {
        quality = 0.0f;
    }

    return quality;
}

/*
====================
Action_FindCover_Execute

Searches for the best cover position near the bot.
Evaluates multiple positions in a grid pattern and returns the highest quality cover.

Added in OPM - Phase 3 Task 3.1d
 Ported from BotController::FindBestCover() in playerbot_cover.cpp
====================
*/
BTNode::Status Action_FindCover_Execute(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return BTNode::Status::FAILURE;
    }

    auto target = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    if (!target || !*target) {
        return BTNode::Status::FAILURE;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return BTNode::Status::FAILURE;
    }
    Player *player = *playerOpt;

    Vector enemyPos = (*target)->origin;
    Vector botPos   = player->origin;

    BotController::CoverPoint bestCover;
    bestCover.position        = vec_zero;
    bestCover.quality         = 0.0f;
    bestCover.protectionAngle = 0.0f;
    bestCover.distanceToEnemy = 0.0f;
    bestCover.hasEscapeRoute  = false;
    bestCover.evaluatedTime   = level.inttime;

    float       searchRadius = g_bot_cover_search_radius->value;
    const int   gridSteps    = 8; // 8x8 grid
    const float stepSize     = searchRadius / gridSteps;

    // Sample positions in a grid around the bot
    for (int x = -gridSteps; x <= gridSteps; x++) {
        for (int y = -gridSteps; y <= gridSteps; y++) {
            // Skip center (bot's current position)
            if (x == 0 && y == 0) {
                continue;
            }

            Vector testPos = botPos;
            testPos.x += x * stepSize;
            testPos.y += y * stepSize;

            // Check if position is valid (on ground, not in solid)
            trace_t trace;
            Vector  start = testPos + Vector(0, 0, 32);
            Vector  end   = testPos - Vector(0, 0, 128);

            trace = G_Trace(
                start,
                player->mins,
                player->maxs,
                end,
                player,
                MASK_PLAYERSOLID,
                false,
                "Action_FindCover_Execute"
            );

            if (trace.fraction == BotConstants::TRACE_COMPLETE || trace.allsolid || trace.startsolid) {
                // No ground or inside solid
                continue;
            }

            // Update position to ground level
            testPos = trace.endpos;

            // Evaluate cover quality
            float quality = EvaluateCoverQualityBT(*bot, player, testPos, enemyPos);

            if (quality > bestCover.quality) {
                bestCover.position        = testPos;
                bestCover.quality         = quality;
                bestCover.distanceToEnemy = (enemyPos - testPos).length();
                bestCover.evaluatedTime   = level.inttime;
            }
        }
    }

    // Check if we found acceptable cover
    float minQuality = g_bot_cover_min_quality->value;
    if (bestCover.quality < minQuality) {
        if (g_bot_debug->integer >= 2) {
            gi.Printf(
                "[BOT] %s: No suitable cover found (best quality: %.2f, min: %.2f)\n",
                player->client->pers.netname,
                bestCover.quality,
                minQuality
            );
        }
        return BTNode::Status::FAILURE;
    }

    // Store cover in blackboard
    blackboard.Set<BotController::CoverPoint>(BlackboardKeys::SELECTED_COVER, bestCover);
    blackboard.Set<float>(BlackboardKeys::COVER_QUALITY, bestCover.quality);

    if (g_bot_debug->integer >= 2) {
        gi.Printf(
            "[BOT] %s: Found cover with quality %.2f at (%.0f, %.0f, %.0f)\n",
            player->client->pers.netname,
            bestCover.quality,
            bestCover.position.x,
            bestCover.position.y,
            bestCover.position.z
        );
    }

    return BTNode::Status::SUCCESS;
}

/*
====================
Action_MoveToCover_Execute

Multi-frame action that moves the bot to the selected cover position.

Added in OPM - Phase 3 Task 3.1d
 Uses bot movement system to navigate to cover
====================
*/
BTNode::Status Action_MoveToCover_Execute(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return BTNode::Status::FAILURE;
    }

    auto coverPoint = blackboard.TryGet<BotController::CoverPoint>(BlackboardKeys::SELECTED_COVER);
    if (!coverPoint) {
        return BTNode::Status::FAILURE;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return BTNode::Status::FAILURE;
    }
    Player *player = *playerOpt;

    // Check if we've reached the cover position
    float distToCover = (player->origin - coverPoint->position).length();
    if (distToCover < BotConstants::ESCAPE_ROUTE_TEST_DISTANCE) {
        // Reached cover
        blackboard.Set<int>(BlackboardKeys::COVER_STATE, BotController::COVER_IN_COVER);

        if (g_bot_debug->integer >= 2) {
            gi.Printf("[BOT] %s: Reached cover position\n", player->client->pers.netname);
        }

        return BTNode::Status::SUCCESS;
    }

    // Set movement target to cover position
    blackboard.Set<int>(BlackboardKeys::COVER_STATE, BotController::COVER_MOVING_TO);
    blackboard.Set<Vector>(BlackboardKeys::MOVING_TO_POSITION, coverPoint->position);

    // The bot's movement system will handle the actual navigation
    // This action returns RUNNING to indicate ongoing movement
    return BTNode::Status::RUNNING;
}

/*
====================
Action_PeekFromCover_Execute

Timed action that exposes the bot from cover to fire at enemies.

Added in OPM - Phase 3 Task 3.1d
 Implements cover peek timing system
====================
*/
BTNode::Status Action_PeekFromCover_Execute(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return BTNode::Status::FAILURE;
    }

    auto profile = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    if (!profile || !*profile) {
        return BTNode::Status::FAILURE;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return BTNode::Status::FAILURE;
    }
    Player *player = *playerOpt;

    // Get or initialize peek state
    auto peekState = blackboard.TryGet<int>(BlackboardKeys::PEEK_STATE);
    int  state     = peekState ? *peekState : 0;

    if (state == 0) {
        // Starting peek - use profile parameters with CVar fallback
        // Added in OPM - Phase 3 Task 3.1.06 (Gemini review)
        float peekMin     = (*profile)->GetPeekDurationMin() * 1000.0f;
        float peekMax     = (*profile)->GetPeekDurationMax() * 1000.0f;
        
        // CVar override if set (for backward compatibility)
        if (g_bot_cover_peek_min_time->value > 0.0f) {
            peekMin = g_bot_cover_peek_min_time->value * 1000.0f;
        }
        if (g_bot_cover_peek_max_time->value > 0.0f) {
            peekMax = g_bot_cover_peek_max_time->value * 1000.0f;
        }
        
        float peekDuration = G_Random(peekMax - peekMin) + peekMin;

        blackboard.Set<float>(BlackboardKeys::PEEK_START_TIME, (float)level.inttime);
        blackboard.Set<float>(BlackboardKeys::PEEK_DURATION, peekDuration);
        blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 1); // Peeking state
        blackboard.Set<int>(BlackboardKeys::COVER_STATE, BotController::COVER_PEEKING);

        if (g_bot_debug->integer >= 2) {
            gi.Printf(
                "[BOT] %s: Peeking from cover (%.1fs)\n",
                player->client->pers.netname,
                peekDuration / 1000.0f
            );
        }

        return BTNode::Status::RUNNING;
    } else if (state == 1) {
        // Currently peeking - check if duration elapsed
        auto startTime = blackboard.TryGet<float>(BlackboardKeys::PEEK_START_TIME);
        auto duration  = blackboard.TryGet<float>(BlackboardKeys::PEEK_DURATION);

        if (!startTime || !duration) {
            // Missing data, reset
            blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 0);
            return BTNode::Status::FAILURE;
        }

        if (level.inttime >= *startTime + (int)*duration) {
            // Peek duration complete
            blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 2); // Complete
            return BTNode::Status::SUCCESS;
        }

        return BTNode::Status::RUNNING;
    }

    // State 2 or invalid - peek complete
    return BTNode::Status::SUCCESS;
}

/*
====================
Action_ReturnToCover_Execute

Returns the bot to cover position after peeking.

Added in OPM - Phase 3 Task 3.1d
 Ensures bot returns to safe cover position
====================
*/
BTNode::Status Action_ReturnToCover_Execute(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return BTNode::Status::FAILURE;
    }

    auto coverPoint = blackboard.TryGet<BotController::CoverPoint>(BlackboardKeys::SELECTED_COVER);
    if (!coverPoint) {
        return BTNode::Status::FAILURE;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return BTNode::Status::FAILURE;
    }
    Player *player = *playerOpt;

    // Check if already in cover
    float distToCover = (player->origin - coverPoint->position).length();
    if (distToCover < BotConstants::ESCAPE_ROUTE_TEST_DISTANCE) {
        // Already in cover
        blackboard.Set<int>(BlackboardKeys::COVER_STATE, BotController::COVER_IN_COVER);
        blackboard.Set<int>(BlackboardKeys::PEEK_STATE, 0); // Reset peek state

        if (g_bot_debug->integer >= 2) {
            gi.Printf("[BOT] %s: Returned to cover\n", player->client->pers.netname);
        }

        return BTNode::Status::SUCCESS;
    }

    // Set movement target back to cover position
    blackboard.Set<Vector>(BlackboardKeys::MOVING_TO_POSITION, coverPoint->position);

    return BTNode::Status::SUCCESS;
}
