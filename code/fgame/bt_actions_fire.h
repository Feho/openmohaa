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

// bt_actions_fire.h
// Behavior tree actions for weapon firing and melee
// Added in OPM - Phase 3 Task 3.1b

#ifndef __BT_ACTIONS_FIRE_H__
#define __BT_ACTIONS_FIRE_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Action_FireWeapon_Execute - Fires weapon with burst control
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function using blackboard for state
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current attack target
 *   - IS_AIMED_AT_TARGET (bool) - Whether aimed properly
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile with burst parameters
 *   - BURST_STATE (int) - 0=not firing, 1=burst, 2=pause (state)
 *   - BURST_START_TIME (float) - When burst started (state)
 *   - CONTINUOUS_FIRE_TIME (float) - Total fire time (state)
 *   - LAST_FIRE_TIME (float) - Last shot time (state)
 *
 * Writes to blackboard:
 *   - BURST_STATE (int) - Updated state
 *   - BURST_START_TIME (float) - Updated time
 *   - CONTINUOUS_FIRE_TIME (float) - Updated time
 *   - LAST_FIRE_TIME (float) - Updated time
 *
 * Returns:
 *   - RUNNING: Burst in progress
 *   - SUCCESS: Burst complete (pause started)
 *   - FAILURE: Cannot fire (no target, no ammo, out of range)
 */
BTNode::Status Action_FireWeapon_Execute(Blackboard &blackboard, float deltaTime);

// Burst state constants
namespace BurstState {
    constexpr int IDLE = 0;
    constexpr int FIRING = 1;
    constexpr int PAUSING = 2;
}

/**
 * Action_MeleeAttack_Execute - Executes melee attack (secondary fire)
 *
 * Changed in OPM - Phase 3 Task 3.1f (Gemini review)
 *  Refactored from stateful class to stateless function
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current target
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - TARGET_DISTANCE (float) - Distance to target
 *
 * Returns:
 *   - SUCCESS: Melee attack performed
 *   - FAILURE: Cannot melee (no target, no weapon, out of range)
 */
BTNode::Status Action_MeleeAttack_Execute(Blackboard &blackboard, float deltaTime);

#endif // __BT_ACTIONS_FIRE_H__
