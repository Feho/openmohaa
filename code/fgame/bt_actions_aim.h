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

// bt_actions_aim.h
// Behavior tree actions for aiming system
// Added in OPM - Phase 3 Task 3.1b

#ifndef __BT_ACTIONS_AIM_H__
#define __BT_ACTIONS_AIM_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Action_AimAtTarget_Execute - Smoothly aims at the current target with profile-based inaccuracy
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function using blackboard for state
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current attack target
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile with aim parameters
 *   - AIM_OFFSET (Vector) - Current aim offset (state)
 *   - AIM_UPDATE_TIME (float) - Last aim offset update time (state)
 *   - ENEMY_EYES_TAG (int) - Cached eye bone tag (state)
 *
 * Writes to blackboard:
 *   - IS_AIMED_AT_TARGET (bool) - Whether aim is within tolerance
 *   - AIM_OFFSET (Vector) - Updated aim offset (state)
 *   - AIM_UPDATE_TIME (float) - Updated time (state)
 *   - ENEMY_EYES_TAG (int) - Cached tag (state)
 *
 * Returns:
 *   - RUNNING: Aiming in progress (multi-frame)
 *   - SUCCESS: Aimed within tolerance (profile.aim_tolerance degrees)
 *   - FAILURE: No target or target invalid
 */
BTNode::Status Action_AimAtTarget_Execute(Blackboard &blackboard, float deltaTime);

#endif // __BT_ACTIONS_AIM_H__
