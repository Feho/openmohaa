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
// playerbot_tactics.cpp: Tactical combat system for advanced bot behavior

#include "g_local.h"
#include "playerbot.h"

extern cvar_t *g_bot_suppression_duration;
extern cvar_t *g_bot_suppression_spread;
extern cvar_t *g_bot_retreat_health_threshold;
extern cvar_t *g_bot_retreat_distance;
extern cvar_t *g_bot_burst_range_long;
extern cvar_t *g_bot_burst_range_medium;
extern cvar_t *g_bot_ammo_low_threshold;
extern cvar_t *g_bot_debug;

/*
====================
SetFireMode

Sets the current fire mode for the bot
====================
*/
void BotController::SetFireMode(FireMode mode)
{
    if (m_fireMode != mode) {
        m_fireMode = mode;

        if (g_bot_debug->integer >= 2) {
            const char *modeName[] = {"ACCURATE", "BURST", "SUPPRESSION", "MELEE"};
            gi.Printf("[BOT] %s: Fire mode changed to %s\n", controlledEnt->client->pers.netname, modeName[mode]);
        }
    }
}

/*
====================
CountEnemiesInRadius

Counts the number of enemies within the specified radius
====================
*/
int BotController::CountEnemiesInRadius(float radius)
{
    int    count    = 0;
    float  radiusSq = radius * radius;
    Vector origin   = controlledEnt->origin;

    for (int i = 0; i < game.maxclients; i++) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->entity || !ent->client) {
            continue;
        }

        Sentient *sent = (Sentient *)ent->entity;
        if (sent == controlledEnt) {
            continue;
        }
        if (sent->deadflag) {
            continue;
        }

        // Check if enemy (not on same team)
        bool isEnemy = false;
        if (sent->IsSubclassOfPlayer()) {
            Player *player = static_cast<Player *>(sent);
            if (g_gametype->integer >= GT_TEAM) {
                isEnemy = (player->GetTeam() != controlledEnt->GetTeam());
            } else {
                isEnemy = true; // Everyone is enemy in non-team modes
            }
        } else {
            isEnemy = (sent->m_Team != controlledEnt->m_Team);
        }

        if (isEnemy) {
            float distSq = (sent->origin - origin).lengthSquared();
            if (distSq <= radiusSq) {
                count++;
            }
        }
    }

    return count;
}

/*
====================
CountAlliesInRadius

Counts the number of allies within the specified radius
====================
*/
int BotController::CountAlliesInRadius(float radius)
{
    int    count    = 0;
    float  radiusSq = radius * radius;
    Vector origin   = controlledEnt->origin;

    for (int i = 0; i < game.maxclients; i++) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->entity || !ent->client) {
            continue;
        }

        Sentient *sent = (Sentient *)ent->entity;
        if (sent == controlledEnt) {
            continue;
        }
        if (sent->deadflag) {
            continue;
        }

        // Check if ally (on same team)
        bool isAlly = false;
        if (sent->IsSubclassOfPlayer()) {
            Player *player = static_cast<Player *>(sent);
            if (g_gametype->integer >= GT_TEAM) {
                isAlly = (player->GetTeam() == controlledEnt->GetTeam());
            }
            // In non-team modes, no one is an ally
        } else {
            isAlly = (sent->m_Team == controlledEnt->m_Team);
        }

        if (isAlly) {
            float distSq = (sent->origin - origin).lengthSquared();
            if (distSq <= radiusSq) {
                count++;
            }
        }
    }

    return count;
}

/*
====================
TrackDamage

Tracks damage taken for tactical retreat decisions.
Accumulates damage in a 2-second sliding window.
Added in OPM - Phase 3 Task 3.1e
====================
*/
void BotController::TrackDamage(float damage)
{
    float currentTime = level.svsTime / 1000.0f;

    // Clear old damage outside 2-second window
    if (combatState.damageWindowStart == 0 || 
        (currentTime - combatState.damageWindowStart) > 2.0f) {
        combatState.recentDamage = 0.0f;
        combatState.damageWindowStart = level.inttime;
    }

    // Accumulate damage
    combatState.recentDamage += damage;

    if (g_bot_debug && g_bot_debug->integer >= 2) {
        gi.Printf(
            "[BOT] %s: Tracked %.1f damage (recent total: %.1f)\n",
            controlledEnt->client->pers.netname,
            damage,
            combatState.recentDamage
        );
    }
}

/*
====================
DetermineCombatProfile

Determines the current combat profile based on health, ammo, cover, and tactical situation
====================
*/
BotController::CombatProfile BotController::DetermineCombatProfile(void)
{
    if (ShouldRetreat()) {
        return RETREATING;
    }

    float health      = controlledEnt->health;
    float maxHealth   = controlledEnt->max_health;
    float healthRatio = health / maxHealth;
    // Changed in OPM
    //  Refactored to use CoverStateData struct
    bool hasGoodCover = (coverState.current.quality > 0.7f);

    if (healthRatio > 0.7f && !hasGoodCover) {
        return AGGRESSIVE;
    }
    // Changed in OPM
    //  Refactored to use CombatState struct
    if (healthRatio < 0.5f || combatState.ammoLow) {
        return DEFENSIVE;
    }
    return CAUTIOUS;
}

/*
====================
ShouldRetreat

Determines if the bot should retreat from combat
Added in OPM - Phase 3 Task 3.1e
====================
*/
bool BotController::ShouldRetreat(void)
{
    // Get retreat threshold from profile, with fallback to cvar
    float retreatThreshold = BotConstants::HEALTH_RETREAT_THRESHOLD;
    float damageThreshold = BotConstants::DAMAGE_RETREAT_THRESHOLD;
    
    if (profile) {
        retreatThreshold = profile->GetRetreatThreshold();
        // damageThreshold could be a profile parameter in the future
    } else if (g_bot_retreat_health_threshold) {
        retreatThreshold = g_bot_retreat_health_threshold->value / 100.0f;
    }

    // Health check
    float health = controlledEnt->health;
    float maxHealth = controlledEnt->max_health;
    float healthRatio = health / maxHealth;

    if (healthRatio < retreatThreshold) {
        if (g_bot_debug && g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Should retreat - low health (%.1f%% < %.1f%%)\n",
                controlledEnt->client->pers.netname,
                healthRatio * 100.0f,
                retreatThreshold * 100.0f
            );
        }
        return true;
    }

    // Recent damage check (>30 damage in 2 seconds)
    float currentTime = level.svsTime / 1000.0f;
    float damageWindow = (level.inttime - combatState.damageWindowStart) / 1000.0f;
    if (combatState.recentDamage > damageThreshold && damageWindow < 2.0f) {
        if (g_bot_debug && g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Should retreat - heavy damage (%.1f > %.1f in 2s)\n",
                controlledEnt->client->pers.netname,
                combatState.recentDamage,
                damageThreshold
            );
        }
        return true;
    }

    // Outnumbered check (3+ enemies)
    int nearbyEnemies = CountEnemiesInRadius(BotConstants::AWARENESS_RADIUS);
    if (nearbyEnemies >= 3) {
        if (g_bot_debug && g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Should retreat - outnumbered (%d enemies)\n",
                controlledEnt->client->pers.netname,
                nearbyEnemies
            );
        }
        return true;
    }

    return false;
}

/*
====================
ExecuteRetreat

Executes tactical retreat behavior
====================
*/
void BotController::ExecuteRetreat(void)
{
    if (!m_pEnemy) {
        return;
    }

    // Find path away from enemy
    Vector retreatDir = controlledEnt->origin - m_pEnemy->origin;
    retreatDir.normalize();

    movement.AvoidPath(m_pEnemy->origin, g_bot_retreat_distance->value, retreatDir);

    // Use suppression fire while retreating
    SetFireMode(FIRE_SUPPRESSION);

    if (g_bot_debug->integer >= 1) {
        gi.Printf(
            "[BOT] %s: Executing tactical retreat (health: %.0f%%, enemies: %d, allies: %d)\n",
            controlledEnt->client->pers.netname,
            (controlledEnt->health / controlledEnt->max_health) * 100.0f,
            CountEnemiesInRadius(BotConstants::AWARENESS_RADIUS),
            CountAlliesInRadius(BotConstants::AWARENESS_RADIUS)
        );
    }
}

/*
====================
UpdateSuppressionFire

Fires at the last known enemy position with random spread for area denial
====================
*/
void BotController::UpdateSuppressionFire(void)
{
    // Changed in OPM
    //  Refactored to use MemoryState struct
    if (!memoryState.enemyMemory.enemy) {
        return;
    }

    // Changed in OPM
    //  Refactored to use MemoryState struct
    // Fire at last known position even without LOS
    Vector targetPos = memoryState.enemyMemory.lastKnownPosition;

    // Add random spread for area denial
    float spread = g_bot_suppression_spread->value;
    targetPos.x += G_Random(spread * 2) - spread;
    targetPos.y += G_Random(spread * 2) - spread;

    rotation.AimAt(targetPos);

    // Hold fire button for suppression duration
    if (level.inttime < m_iSuppressionEndTime) {
        m_botCmd.buttons |= BUTTON_ATTACKLEFT;

        if (g_bot_debug->integer >= 2) {
            gi.Printf(
                "[BOT] %s: Suppressing area at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname,
                targetPos.x,
                targetPos.y,
                targetPos.z
            );
        }
    }
}

/*
====================
CalculateBurstTiming

Calculates burst duration and delay based on distance to enemy
====================
*/
void BotController::CalculateBurstTiming(void)
{
    if (!m_pEnemy) {
        return;
    }

    float distToEnemy = (m_pEnemy->origin - controlledEnt->origin).length();

    // Changed in OPM
    //  Refactored to use CombatState struct
    if (distToEnemy > g_bot_burst_range_long->value) {
        // Long range: short, accurate bursts
        combatState.burstDuration    = G_Random(0.3f) + 0.3f; // 0.3-0.6s
        combatState.burstDelay       = G_Random(0.4f) + 0.4f; // 0.4-0.8s
        combatState.requireLowSpread = true;

        if (g_bot_debug->integer >= 2) {
            gi.Printf("[BOT] %s: Long range burst (%.1f units)\n", controlledEnt->client->pers.netname, distToEnemy);
        }
    } else if (distToEnemy > g_bot_burst_range_medium->value) {
        // Medium range: moderate bursts
        combatState.burstDuration    = G_Random(0.5f) + 0.5f; // 0.5-1.0s
        combatState.burstDelay       = G_Random(0.3f) + 0.2f; // 0.2-0.5s
        combatState.requireLowSpread = false;

        if (g_bot_debug->integer >= 2) {
            gi.Printf("[BOT] %s: Medium range burst (%.1f units)\n", controlledEnt->client->pers.netname, distToEnemy);
        }
    } else {
        // Close range: sustained fire
        combatState.burstDuration    = G_Random(1.0f) + 1.0f; // 1.0-2.0s
        combatState.burstDelay       = G_Random(0.2f) + 0.1f; // 0.1-0.3s
        combatState.requireLowSpread = false;

        if (g_bot_debug->integer >= 2) {
            gi.Printf("[BOT] %s: Close range burst (%.1f units)\n", controlledEnt->client->pers.netname, distToEnemy);
        }
    }
}

/*
====================
CheckAmmoConservation

Checks ammo levels and adjusts fire mode for conservation
====================
*/
// Changed in OPM
//  Refactored to use CombatState struct
void BotController::CheckAmmoConservation(void)
{
    Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        combatState.ammoLow = false;
        return;
    }

    // Get current ammo from player stats (reserve + clip)
    int ammo      = controlledEnt->client->ps.stats[STAT_AMMO];
    int clipAmmo  = controlledEnt->client->ps.stats[STAT_CLIPAMMO];
    int totalAmmo = ammo + clipAmmo;

    // Simple check: if total ammo is low (<=10 rounds), enable conservation
    if (totalAmmo <= 10 && totalAmmo > 0) {
        // Low ammo: single shots only
        combatState.ammoLow = true;
        SetFireMode(FIRE_ACCURATE);
        combatState.burstDuration    = 0.1f;
        combatState.requireLowSpread = true;

        if (g_bot_debug->integer >= 1) {
            gi.Printf(
                "[BOT] %s: Low ammo conservation mode (%d rounds remaining)\n",
                controlledEnt->client->pers.netname,
                totalAmmo
            );
        }
    } else if (totalAmmo <= 30 && totalAmmo > 10) {
        // Medium ammo: controlled bursts
        combatState.ammoLow = false;
        SetFireMode(FIRE_BURST);
    } else {
        combatState.ammoLow = false;
    }
}

/*
====================
UpdateTacticalCombat

Main update function for the tactical combat system
Called from the attack state
====================
*/
void BotController::UpdateTacticalCombat(void)
{
    // Check ammo conservation
    CheckAmmoConservation();

    // Determine combat profile
    CombatProfile oldProfile = m_combatProfile;
    m_combatProfile          = DetermineCombatProfile();

    if (oldProfile != m_combatProfile && g_bot_debug->integer >= 1) {
        const char *profileNames[] = {"AGGRESSIVE", "CAUTIOUS", "DEFENSIVE", "RETREATING"};
        gi.Printf(
            "[BOT] %s: Combat profile changed to %s\n",
            controlledEnt->client->pers.netname,
            profileNames[m_combatProfile]
        );
    }

    // Handle retreat
    if (m_combatProfile == RETREATING) {
        ExecuteRetreat();
        return;
    }

    // Calculate burst timing based on range
    if (m_pEnemy) {
        CalculateBurstTiming();
    }

    // Changed in OPM
    //  Refactored to use CombatState and MemoryState structs
    // Apply fire mode based on profile
    switch (m_combatProfile) {
    case AGGRESSIVE:
        if (!combatState.ammoLow) {
            SetFireMode(FIRE_BURST);
        }
        break;
    case CAUTIOUS:
        SetFireMode(FIRE_BURST);
        break;
    case DEFENSIVE:
        // Use suppression if we have enemy memory but no current LOS
        if (!m_pEnemy && memoryState.enemyMemory.enemy) {
            SetFireMode(FIRE_SUPPRESSION);
            if (level.inttime > m_iSuppressionEndTime) {
                m_iSuppressionEndTime = level.inttime + (int)(g_bot_suppression_duration->value * 1000);
            }
            UpdateSuppressionFire();
        } else {
            SetFireMode(FIRE_ACCURATE);
        }
        break;
    case RETREATING:
        // Already handled above
        break;
    }
}
