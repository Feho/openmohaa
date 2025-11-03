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
// playerbot_squad.cpp: Squad coordination system for team tactics

#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_squad_range;
extern cvar_t *g_bot_squad_flank_distance;
extern cvar_t *g_bot_squad_share_info;
extern cvar_t *g_bot_squad_coordination_delay;
extern cvar_t *g_bot_squad_max_simultaneous_push;
extern cvar_t *g_bot_debug;

/*
====================
GetSquadRole

Returns the current squad role
====================
*/
BotController::SquadRole BotController::GetSquadRole(void) const
{
    return squadState.role;
}

/*
====================
HasEnemy

Returns true if the bot currently has an enemy
====================
*/
bool BotController::HasEnemy(void) const
{
    return (m_pEnemy != NULL);
}

/*
====================
GetEnemy

Returns the bot's current enemy
====================
*/
Sentient *BotController::GetEnemy(void) const
{
    return m_pEnemy;
}

/*
====================
UpdateSquadAwareness

Updates the list of nearby friendly bots that form the squad
Should be called periodically (every 500ms)
====================
*/
void BotController::UpdateSquadAwareness(void)
{
    // Only update every 500ms
    if (level.inttime < squadState.lastSquadUpdateTime + 500) {
        return;
    }

    squadState.lastSquadUpdateTime = level.inttime;

    // Clear current squad members
    squadState.squad.members.ClearObjectList();

    // Find nearby friendly bots
    const Container<BotController *>& allControllers = botManager.getControllerManager().getControllers();
    float                             squadRangeSq   = g_bot_squad_range->value * g_bot_squad_range->value;

    for (int i = 1; i <= allControllers.NumObjects(); i++) {
        BotController *bot = allControllers.ObjectAt(i);
        if (!bot || bot == this) {
            continue;
        }
        if (!bot->getControlledEntity()) {
            continue;
        }

        // Check if on same team (only in team-based gametypes)
        if (g_gametype->integer < GT_TEAM) {
            continue; // No squads in non-team games
        }
        if (bot->getControlledEntity()->GetTeam() != controlledEnt->GetTeam()) {
            continue;
        }

        // Check distance
        float distSq = (bot->getControlledEntity()->origin - controlledEnt->origin).lengthSquared();
        if (distSq <= squadRangeSq) {
            squadState.squad.members.AddObject(bot);
        }
    }

    if (g_bot_debug->integer >= 2) {
        gi.Printf(
            "[BOT] %s: Squad awareness updated - %d members nearby\n",
            controlledEnt->client->pers.netname,
            squadState.squad.members.NumObjects()
        );
    }
}

/*
====================
AssignSquadRole

Determines and assigns a squad role based on current squad composition
====================
*/
BotController::SquadRole BotController::AssignSquadRole(void)
{
    // Reassign role every 5 seconds or if no role assigned
    if (squadState.role != ROLE_NONE && level.inttime < squadState.roleAssignmentTime + 5000) {
        return squadState.role;
    }

    squadState.roleAssignmentTime = level.inttime;

    if (squadState.squad.members.NumObjects() == 0) {
        // Solo, default to aggressive behavior
        squadState.role = ROLE_AGGRESSOR;
        return squadState.role;
    }

    // Count existing roles in squad
    int aggressors = 0, flankers = 0, support = 0;

    for (int i = 1; i <= squadState.squad.members.NumObjects(); i++) {
        BotController *bot = squadState.squad.members.ObjectAt(i);
        if (!bot) {
            continue;
        }

        switch (bot->GetSquadRole()) {
        case ROLE_AGGRESSOR:
            aggressors++;
            break;
        case ROLE_FLANKER:
            flankers++;
            break;
        case ROLE_SUPPORT:
            support++;
            break;
        default:
            break;
        }
    }

    // Balance roles: 1 support, 1-2 flankers, rest aggressors
    SquadRole oldRole = squadState.role;

    if (support == 0) {
        squadState.role = ROLE_SUPPORT;
    } else if (flankers < 2) {
        squadState.role = ROLE_FLANKER;
    } else {
        squadState.role = ROLE_AGGRESSOR;
    }

    if (oldRole != squadState.role && g_bot_debug->integer >= 1) {
        const char *roleNames[] = {"NONE", "AGGRESSOR", "FLANKER", "SUPPORT", "DEFENDER"};
        gi.Printf(
            "[BOT] %s: Squad role assigned: %s (squad: %d aggressors, %d flankers, %d support)\n",
            controlledEnt->client->pers.netname,
            roleNames[squadState.role],
            aggressors,
            flankers,
            support
        );
    }

    return squadState.role;
}

/*
====================
CountAlliesNearPosition

Counts the number of allies within a radius of a specific position
====================
*/
int BotController::CountAlliesNearPosition(Vector pos, float radius)
{
    int   count    = 0;
    float radiusSq = radius * radius;

    for (int i = 1; i <= squadState.squad.members.NumObjects(); i++) {
        BotController *bot = squadState.squad.members.ObjectAt(i);
        if (!bot || !bot->getControlledEntity()) {
            continue;
        }

        float distSq = (bot->getControlledEntity()->origin - pos).lengthSquared();
        if (distSq <= radiusSq) {
            count++;
        }
    }

    return count;
}

/*
====================
ExecuteFlankingManeuver

Calculates and moves to a flanking position relative to the enemy
====================
*/
void BotController::ExecuteFlankingManeuver(void)
{
    if (!m_pEnemy) {
        return;
    }

    // Recalculate flank position every 3 seconds or if invalid
    if (squadState.flankPositionValid && level.inttime < squadState.roleAssignmentTime + 3000) {
        // Move to existing flank position
        if (!movement.IsMoving() || movement.MoveDone()) {
            movement.MoveNear(squadState.flankPosition, BotConstants::OBSTACLE_AVOIDANCE_DISTANCE);
        }
        return;
    }

    // Calculate new flanking position
    Vector toEnemy = m_pEnemy->origin - controlledEnt->origin;
    toEnemy[2]     = 0; // Ignore vertical component
    toEnemy.normalize();

    // Create perpendicular vector for flanking (90 degrees)
    Vector flankLeft(-toEnemy[1], toEnemy[0], 0);
    Vector flankRight(toEnemy[1], -toEnemy[0], 0);

    float flankDist = g_bot_squad_flank_distance->value;

    Vector leftFlank  = m_pEnemy->origin + (flankLeft * flankDist);
    Vector rightFlank = m_pEnemy->origin + (flankRight * flankDist);

    // Choose side with fewer teammates
    int leftTeammates  = CountAlliesNearPosition(leftFlank, BotConstants::SEARCH_PATTERN_STEP);
    int rightTeammates = CountAlliesNearPosition(rightFlank, BotConstants::SEARCH_PATTERN_STEP);

    squadState.flankPosition      = (leftTeammates <= rightTeammates) ? leftFlank : rightFlank;
    squadState.flankPositionValid = true;

    // Start moving to flank position
    movement.MoveNear(squadState.flankPosition, BotConstants::OBSTACLE_AVOIDANCE_DISTANCE);

    if (g_bot_debug->integer >= 1) {
        gi.Printf(
            "[BOT] %s: Executing flanking maneuver to (%.0f, %.0f, %.0f) [%s side, %d allies nearby]\n",
            controlledEnt->client->pers.netname,
            squadState.flankPosition.x,
            squadState.flankPosition.y,
            squadState.flankPosition.z,
            (leftTeammates <= rightTeammates) ? "left" : "right",
            (leftTeammates <= rightTeammates) ? leftTeammates : rightTeammates
        );
    }
}

/*
====================
ShareEnemyInformation

Broadcasts enemy position to squad members who don't have an enemy
====================
*/
void BotController::ShareEnemyInformation(void)
{
    if (!m_pEnemy) {
        return;
    }
    if (g_bot_squad_share_info->integer == 0) {
        return;
    }

    // Share enemy info with squad members
    for (int i = 1; i <= squadState.squad.members.NumObjects(); i++) {
        BotController *bot = squadState.squad.members.ObjectAt(i);
        if (!bot) {
            continue;
        }

        if (!bot->HasEnemy()) {
            bot->ReceiveEnemyInfo(m_pEnemy, m_pEnemy->origin);

            if (g_bot_debug->integer >= 2) {
                gi.Printf(
                    "[BOT] %s: Shared enemy info with %s\n",
                    controlledEnt->client->pers.netname,
                    bot->getControlledEntity()->client->pers.netname
                );
            }
        }
    }
}

/*
====================
ReceiveEnemyInfo

Receives enemy information from a squadmate
====================
*/
void BotController::ReceiveEnemyInfo(Sentient *enemy, Vector position)
{
    if (!enemy) {
        return;
    }

    // If we don't have an enemy, investigate this one
    if (!m_pEnemy && !memoryState.enemyMemory.enemy) {
        memoryState.enemyMemory.enemy             = enemy;
        memoryState.enemyMemory.lastKnownPosition = position;
        memoryState.enemyMemory.lastSeenTime      = level.svsTime;
        memoryState.enemyMemory.confidenceLevel   = 0.5f; // Shared info is less certain

        if (g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Received enemy info at (%.0f, %.0f, %.0f) from squadmate\n",
                controlledEnt->client->pers.netname,
                position.x,
                position.y,
                position.z
            );
        }
    }
}

/*
====================
CheckStaggeredEngagement

Prevents all squad members from rushing simultaneously
Maintains tactical spacing and staged attacks
====================
*/
void BotController::CheckStaggeredEngagement(void)
{
    if (!m_pEnemy) {
        return;
    }
    if (squadState.role != ROLE_AGGRESSOR) {
        return;
    }

    // Count how many allies are actively pushing the enemy
    int aggressiveAllies = 0;

    for (int i = 1; i <= squadState.squad.members.NumObjects(); i++) {
        BotController *bot = squadState.squad.members.ObjectAt(i);
        if (!bot || !bot->getControlledEntity()) {
            continue;
        }

        if (bot->m_combatProfile == AGGRESSIVE && bot->GetEnemy()) {
            float distToEnemy = (bot->GetEnemy()->origin - bot->getControlledEntity()->origin).length();
            if (distToEnemy < BotConstants::AWARENESS_RADIUS) {
                aggressiveAllies++;
            }
        }
    }

    // If max simultaneous pushers reached, hang back
    int maxPush = g_bot_squad_max_simultaneous_push->integer;
    if (aggressiveAllies >= maxPush && m_combatProfile == AGGRESSIVE) {
        m_combatProfile = CAUTIOUS;

        if (g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Staggering engagement - %d allies already pushing (max: %d)\n",
                controlledEnt->client->pers.netname,
                aggressiveAllies,
                maxPush
            );
        }
    }
}

/*
====================
CoordinateAttack

Main squad coordination function - coordinates attacks based on squad roles
====================
*/
void BotController::CoordinateAttack(void)
{
    if (!m_pEnemy) {
        return;
    }

    // Update squad awareness
    UpdateSquadAwareness();

    // Assign/update role
    AssignSquadRole();

    // Share enemy information
    ShareEnemyInformation();

    // Check staggered engagement
    CheckStaggeredEngagement();

    // Execute role-specific behavior
    switch (squadState.role) {
    case ROLE_AGGRESSOR:
        {
            // Wait for flankers to get in position before pushing hard
            bool flankersReady = true;
            bool hasFlankers   = false;

            for (int i = 1; i <= squadState.squad.members.NumObjects(); i++) {
                BotController *bot = squadState.squad.members.ObjectAt(i);
                if (!bot) {
                    continue;
                }

                if (bot->GetSquadRole() == ROLE_FLANKER) {
                    hasFlankers = true;
                    if (bot->movement.IsMoving() && !bot->movement.MoveDone()) {
                        flankersReady = false;
                        break;
                    }
                }
            }

            if (hasFlankers) {
                if (flankersReady) {
                    // All flankers in position, push aggressively
                    if (m_combatProfile != AGGRESSIVE && m_combatProfile != RETREATING) {
                        m_combatProfile = AGGRESSIVE;

                        if (g_bot_debug->integer >= 1) {
                            gi.Printf(
                                "[BOT] %s: Flankers ready - pushing aggressively\n", controlledEnt->client->pers.netname
                            );
                        }
                    }
                } else {
                    // Flankers moving, provide suppression
                    SetFireMode(FIRE_SUPPRESSION);
                    if (level.inttime > m_iSuppressionEndTime) {
                        m_iSuppressionEndTime = level.inttime + (int)(g_bot_squad_coordination_delay->value * 1000);
                    }

                    if (g_bot_debug->integer >= 2) {
                        gi.Printf("[BOT] %s: Suppressing while flankers move\n", controlledEnt->client->pers.netname);
                    }
                }
            }
            break;
        }

    case ROLE_FLANKER:
        {
            // Execute flanking maneuver
            // ExecuteFlankingManeuver(); // Disabled for testing
            break;
        }

    case ROLE_SUPPORT:
        {
            // Hold position, provide covering fire
            if (coverState.current.quality > 0.0f) {
                // Use cover and suppression fire
                SetFireMode(FIRE_SUPPRESSION);
                if (level.inttime > m_iSuppressionEndTime) {
                    m_iSuppressionEndTime = level.inttime + (int)(g_bot_squad_coordination_delay->value * 1000);
                }
            } else {
                // No cover, use defensive profile
                if (m_combatProfile != DEFENSIVE && m_combatProfile != RETREATING) {
                    m_combatProfile = DEFENSIVE;
                }
            }

            if (g_bot_debug->integer >= 2) {
                gi.Printf("[BOT] %s: Providing support fire\n", controlledEnt->client->pers.netname);
            }
            break;
        }

    case ROLE_DEFENDER:
        {
            // Hold position, defensive tactics
            if (m_combatProfile != DEFENSIVE && m_combatProfile != RETREATING) {
                m_combatProfile = DEFENSIVE;
            }
            break;
        }

    default:
        break;
    }
}
