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

// bt_actions_cover.h
// Cover system actions for behavior trees
// Added in OPM - Phase 3 Task 3.1d

#ifndef __BT_ACTIONS_COVER_H__
#define __BT_ACTIONS_COVER_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Action_FindCover_Execute - Search for suitable cover near bot position
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Port of FindBestCover() logic from playerbot_cover.cpp
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - SELECTED_TARGET (Sentient*) - Enemy to take cover from
 *
 * Writes to blackboard:
 *   - SELECTED_COVER (CoverPoint) - Best cover point found
 *   - COVER_QUALITY (float) - Quality of selected cover
 *
 * Returns:
 *   - SUCCESS: Found cover with quality >= g_bot_cover_min_quality
 *   - FAILURE: No suitable cover found
 */
BTNode::Status Action_FindCover_Execute(Blackboard &blackboard);

/**
 * Action_MoveToCover_Execute - Multi-frame action to move to selected cover
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Uses bot movement system to navigate to cover point
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - SELECTED_COVER (CoverPoint) - Cover point to move to
 *
 * Returns:
 *   - RUNNING: Still moving to cover
 *   - SUCCESS: Reached cover position
 *   - FAILURE: Cannot reach cover (path failure)
 */
BTNode::Status Action_MoveToCover_Execute(Blackboard &blackboard);

/**
 * Action_PeekFromCover_Execute - Temporarily expose from cover to fire
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Timed action that exposes bot from cover for firing
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - PROFILE (BotProfile*) - For peek timing parameters
 *   - SELECTED_COVER (CoverPoint) - Current cover position
 *
 * Writes to blackboard:
 *   - PEEK_START_TIME (float) - When peek began
 *   - PEEK_DURATION (float) - How long to peek (randomized)
 *   - PEEK_STATE (int) - 0=starting, 1=peeking, 2=complete
 *
 * Returns:
 *   - RUNNING: Currently peeking
 *   - SUCCESS: Peek duration complete
 */
BTNode::Status Action_PeekFromCover_Execute(Blackboard &blackboard);

/**
 * Action_ReturnToCover_Execute - Return to cover position after peeking
 *
 * Added in OPM - Phase 3 Task 3.1d
 *  Ensures bot returns to cover after exposing
 *
 * Reads from blackboard:
 *   - BOT (BotController*) - Bot controller
 *   - SELECTED_COVER (CoverPoint) - Cover position
 *
 * Returns:
 *   - SUCCESS: Already in cover or successfully returned
 */
BTNode::Status Action_ReturnToCover_Execute(Blackboard &blackboard);

#endif // __BT_ACTIONS_COVER_H__
