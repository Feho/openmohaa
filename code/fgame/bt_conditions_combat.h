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

// bt_conditions_combat.h
// Combat-related condition checks for behavior trees
// Added in OPM - Phase 3 Task 3.1b

#ifndef __BT_CONDITIONS_COMBAT_H__
#define __BT_CONDITIONS_COMBAT_H__

#include "behavior_tree.h"
#include "playerbot.h"

/**
 * Condition_IsAimedAtTarget - Check if bot is aimed at target within tolerance
 *
 * Reads from blackboard:
 *   - IS_AIMED_AT_TARGET (bool) - Aim state set by Action_AimAtTarget
 *
 * Returns:
 *   - SUCCESS: Aimed within tolerance (5 degrees)
 *   - FAILURE: Not aimed or no aim data available
 */
class Condition_IsAimedAtTarget : public BTCondition
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "IsAimedAtTarget"; }
};

/**
 * Condition_WeaponReady - Check if weapon has ammo and acceptable spread
 *
 * Reads from blackboard:
 *   - PLAYER (Player*) - Player entity
 *   - PROFILE (BotProfile*) - For fire discipline settings
 *
 * Returns:
 *   - SUCCESS: Weapon has ammo and spread acceptable
 *   - FAILURE: No ammo or spread too high
 */
class Condition_WeaponReady : public BTCondition
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "WeaponReady"; }
};

/**
 * Condition_InMeleeRange - Check if target is within melee attack range
 *
 * Reads from blackboard:
 *   - SELECTED_TARGET (Sentient*) - Current target
 *   - PLAYER (Player*) - Player entity
 *
 * Returns:
 *   - SUCCESS: Target within secondary weapon range
 *   - FAILURE: Target too far or no target/weapon
 */
class Condition_InMeleeRange : public BTCondition
{
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override;
    void   Reset() override;
    const char *GetName() const override { return "InMeleeRange"; }
};

#endif // __BT_CONDITIONS_COMBAT_H__
