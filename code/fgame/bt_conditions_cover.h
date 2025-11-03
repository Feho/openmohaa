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

// bt_conditions_cover.h
// Cover system condition checks for behavior trees
// Added in OPM - Phase 3 Task 3.1d

#ifndef __BT_CONDITIONS_COVER_H__
#define __BT_CONDITIONS_COVER_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Condition_HasCoverAvailable_Check - Check if suitable cover has been found
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Verifies that a cover point exists with acceptable quality
 *
 * Reads from blackboard:
 *   - SELECTED_COVER (CoverPoint) - Cover point found by FindCover action
 *   - COVER_QUALITY (float) - Quality of selected cover
 *
 * Returns:
 *   - true: Cover exists with quality >= g_bot_cover_min_quality
 *   - false: No cover or quality too low
 */
bool Condition_HasCoverAvailable_Check(Blackboard &blackboard);

/**
 * Condition_IsInCover_Check - Check if bot is currently at cover position
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Verifies bot is within cover proximity distance
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - SELECTED_COVER (CoverPoint) - Cover position
 *
 * Returns:
 *   - true: Bot is within 64 units of cover position
 *   - false: Bot is not at cover
 */
bool Condition_IsInCover_Check(Blackboard &blackboard);

/**
 * Condition_ShouldUseCover_Check - Check if bot should use cover based on tactical situation
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Cover is disabled at close range (< 384 units)
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - SELECTED_TARGET (Sentient*) - Current target
 *   - PROFILE (BotProfile*) - For cover usage preference
 *
 * Returns:
 *   - false: Enemy too close (< 384 units), cover disabled
 *   - true: Normal range, cover should be used
 */
bool Condition_ShouldUseCover_Check(Blackboard &blackboard);

#endif // __BT_CONDITIONS_COVER_H__
