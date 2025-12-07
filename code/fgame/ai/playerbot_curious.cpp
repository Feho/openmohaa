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
#include "../g_local.h"
#include "playerbot.h"

/*
====================
Curious state

Forward to the last event position
====================
*/
void BotController::InitState_Curious(botfunc_t *func)
{
    func->CheckCondition = &BotController::CheckCondition_Curious;
    func->ThinkState     = &BotController::State_Curious;
}

bool BotController::CheckCondition_Curious(void)
{
    if (m_iAttackTime) {
        m_iCuriousTime = 0;
        return false;
    }

    if (level.inttime > m_iCuriousTime) {
        if (m_iCuriousTime) {
            movement.ClearMove();
            m_iCuriousTime = 0;
        }

        return false;
    }

    return true;
}

void BotController::State_Curious(void)
{
    if (CheckWindows()) {
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        m_iLastFireTime = level.inttime;
    } else {
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
    }

    AimAtAimNode();

    if (!movement.MoveToBestAttractivePoint(3) && (!movement.IsMoving() || m_vLastCuriousPos != m_vNewCuriousPos)) {
        movement.MoveTo(m_vNewCuriousPos);
        m_vLastCuriousPos = m_vNewCuriousPos;
    }

    if (movement.MoveDone()) {
        m_iCuriousTime = 0;
    }
}
