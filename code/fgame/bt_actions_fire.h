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
 * Action_FireWeapon - Fires weapon with burst control
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current attack target
 *   - IS_AIMED_AT_TARGET (bool) - Whether aimed properly
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile with burst parameters
 *   - BURST_STATE (int) - 0=not firing, 1=burst, 2=pause
 *   - BURST_START_TIME (float) - When burst started
 *   - CONTINUOUS_FIRE_TIME (float) - Total fire time
 *   - LAST_FIRE_TIME (float) - Last shot time
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
class Action_FireWeapon : public BTAction
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "FireWeapon"; }

private:
    enum BurstState {
        BURST_IDLE = 0,
        BURST_FIRING = 1,
        BURST_PAUSING = 2
    };
};

/**
 * Action_MeleeAttack - Executes melee attack (secondary fire)
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
class Action_MeleeAttack : public BTAction
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "MeleeAttack"; }
};

#endif // __BT_ACTIONS_FIRE_H__
