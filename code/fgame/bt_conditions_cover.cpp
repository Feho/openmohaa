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

// bt_conditions_cover.cpp
// Implementation of cover system condition checks
// Added in OPM - Phase 3 Task 3.1d

#include "g_local.h"
#include "bt_conditions_cover.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "player.h"
#include "sentient.h"
#include "bot_profile.h"

extern cvar_t *g_bot_cover_min_quality;

/*
====================
Condition_HasCoverAvailable_Check

Checks if suitable cover has been found and stored in blackboard.

Added in OPM - Phase 3 Task 3.1d
 Verifies cover quality meets minimum threshold
====================
*/
bool Condition_HasCoverAvailable_Check(Blackboard &blackboard)
{
    auto coverPoint = blackboard.TryGet<BotController::CoverPoint>(BlackboardKeys::SELECTED_COVER);
    if (!coverPoint) {
        return false;
    }

    auto coverQuality = blackboard.TryGet<float>(BlackboardKeys::COVER_QUALITY);
    if (!coverQuality) {
        return false;
    }

    float minQuality = g_bot_cover_min_quality->value;
    return (*coverQuality >= minQuality);
}

/*
====================
Condition_IsInCover_Check

Checks if the bot is currently at the cover position.

Added in OPM - Phase 3 Task 3.1d
 Uses proximity distance to determine if bot is "in cover"
====================
*/
bool Condition_IsInCover_Check(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return false;
    }

    auto coverPoint = blackboard.TryGet<BotController::CoverPoint>(BlackboardKeys::SELECTED_COVER);
    if (!coverPoint) {
        return false;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return false;
    }
    Player *player = *playerOpt;

    // Check if bot is within cover proximity distance
    float distToCover = (player->origin - coverPoint->position).length();
    return (distToCover < BotConstants::ESCAPE_ROUTE_TEST_DISTANCE);
}

/*
====================
Condition_ShouldUseCover_Check

Determines if the bot should use cover based on tactical situation.
Cover is disabled at close range to allow aggressive engagement.

Added in OPM - Phase 3 Task 3.1d
 Implements tactical rule: no cover at close range (< 384 units)
====================
*/
bool Condition_ShouldUseCover_Check(Blackboard &blackboard)
{
    auto bot = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot || !*bot) {
        return false;
    }

    auto target = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    if (!target || !*target) {
        // No target, no reason to take cover
        return false;
    }

    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt || !*playerOpt) {
        return false;
    }
    Player *player = *playerOpt;

    // Calculate distance to target
    float distToTarget = (player->origin - (*target)->origin).length();

    // Disable cover at close range (close combat threshold)
    if (distToTarget < BotConstants::CLOSE_RANGE_THRESHOLD) {
        return false;
    }

    // Check profile for cover usage preference
    // Added in OPM - Phase 3 Task 3.1.06 (Gemini review)
    auto profile = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);
    if (profile && *profile) {
        // Profile determines if bot should use cover (0.0 = never, 1.0 = always)
        float coverUsage = (*profile)->GetCoverUsage();
        // Random check: bot uses cover based on profile preference
        if (G_Random() > coverUsage) {
            return false; // Bot decides not to use cover this time
        }
    }

    return true;
}
