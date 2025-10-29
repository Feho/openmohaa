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
#include "g_local.h"
#include "playerbot.h"

/*
====================
Grenade state

Avoid any grenades
====================
*/
void BotController::InitState_Grenade(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Grenade;
    func->ThinkState     = &BotController::State_Grenade;
}

bool BotController::CheckCondition_Grenade(void)
{
    // TODO: Implement grenade detection and avoidance trigger
    //  Current grenade state is a placeholder
    //  Needs:
    //  - Detect nearby grenades (search for Projectile entities with grenade class)
    //  - Calculate danger radius and blast time
    //  - Trigger state when grenade is within threat range
    //  - Priority should override other states except Attack
    return false;
}

void BotController::State_Grenade(void)
{
    // TODO: Implement grenade avoidance behavior
    //  Current grenade state is a placeholder
    //  Needs:
    //  - Calculate safe escape vector away from grenade
    //  - Use BotMovement to move to safety
    //  - Consider jumping or taking cover
    //  - Handle multiple grenades (prioritize closest/most dangerous)
    //  - Return to previous state once safe
}
