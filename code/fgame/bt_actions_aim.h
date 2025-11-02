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
 * Action_AimAtTarget - Smoothly aims at the current target with profile-based inaccuracy
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current attack target
 *   - BOT (BotController*) - Bot controller
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - Bot profile with aim parameters
 *   - AIM_OFFSET (Vector) - Current aim offset
 *   - AIM_UPDATE_TIME (float) - Last aim offset update time
 *   - ENEMY_EYES_TAG (int) - Cached eye bone tag
 *
 * Writes to blackboard:
 *   - IS_AIMED_AT_TARGET (bool) - Whether aim is within tolerance
 *   - AIM_OFFSET (Vector) - Updated aim offset
 *   - AIM_UPDATE_TIME (float) - Updated time
 *   - ENEMY_EYES_TAG (int) - Cached tag
 *
 * Returns:
 *   - RUNNING: Aiming in progress (multi-frame)
 *   - SUCCESS: Aimed within tolerance (5 degrees)
 *   - FAILURE: No target or target invalid
 */
class Action_AimAtTarget : public BTAction
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "AimAtTarget"; }

private:
    // Changed in OPM - Phase 3 Task 3.1b (Gemini review)
    //  Moved AIM_TOLERANCE_DEGREES to profile parameter (aim_tolerance)
    static constexpr float AIM_UPDATE_INTERVAL = 0.1f;  // Update offset every 100ms
};

#endif // __BT_ACTIONS_AIM_H__
