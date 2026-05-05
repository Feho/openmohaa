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
// playerbot.cpp: Multiplayer bot system.
//
// FIXME: Refactor code and use OOP-based state system

#include "g_local.h"
#include "actor.h"
#include "playerbot.h"
#include "playerbot_profile.h"
#include "consoleevent.h"
#include "debuglines.h"
#include "dm_manager.h"
#include "playerstart.h"
#include "scriptexception.h"
#include "vehicleturret.h"
#include "weaputils.h"
#include "g_bot.h"
#include "gamecvars.h"
#include "windows.h"
#include <float.h>

// We assume that we have limited access to the server-side
// and that most logic come from the playerstate_s structure

CLASS_DECLARATION(Listener, BotController, NULL) {
    {NULL, NULL}
};

static void BotSetMoveClearReason(BotMoveClearReason& current, BotMoveClearReason requested)
{
    if (current == BotMoveClearReason::None) {
        current = requested;
    }
}

BotController::BotController()
{
    if (LoadingSavegame) {
        return;
    }

    m_botCmd.serverTime = 0;
    m_botCmd.msec       = 0;
    m_botCmd.buttons    = 0;
    m_botCmd.angles[0]  = ANGLE2SHORT(0);
    m_botCmd.angles[1]  = ANGLE2SHORT(0);
    m_botCmd.angles[2]  = ANGLE2SHORT(0);

    m_botCmd.forwardmove = 0;
    m_botCmd.rightmove   = 0;
    m_botCmd.upmove      = 0;

    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;
    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = DEFAULT_VIEWHEIGHT;

    // Reset grouped state
    m_combat.reset();
    m_enemy.reset();
    m_curious.reset();
    m_grenade.reset();
    m_overwatch.reset();
    m_idle.reset();
    m_reaction.reset();
    m_scriptControl.reset();

    m_iNextTauntTime    = 0;
    m_iLastFireTime     = 0;
    m_iLastPosDebugTime = 0;
    m_randomSeed        = 1;

    m_bFirstSpawn    = true;
    m_engagementMode = BotEngagementMode::None;
    m_tacticalMode   = BotTacticalMode::None;
    m_hazardMode     = BotHazardMode::None;
}

BotController::~BotController()
{
    if (controlledEnt) {
        botManager.GetTacticalMemory().ReleaseOccupant(controlledEnt->entnum);
        controlledEnt->delegate_gotKill.Remove(delegateHandle_gotKill);
        controlledEnt->delegate_killed.Remove(delegateHandle_killed);
        controlledEnt->delegate_stufftext.Remove(delegateHandle_stufftext);
        controlledEnt->delegate_spawned.Remove(delegateHandle_spawned);
    }
}

BotMovement& BotController::GetMovement()
{
    return movement;
}

BotBeliefMap& BotController::GetBeliefMap()
{
    return beliefMap;
}

float BotController::BotRandom(void)
{
    return Q_random(&m_randomSeed);
}

float BotController::BotRandom(float n)
{
    return BotRandom() * n;
}

float BotController::BotCRandom(void)
{
    return Q_crandom(&m_randomSeed);
}

int BotController::BotRandomInt(int upperExclusive)
{
    if (upperExclusive <= 0) {
        return 0;
    }

    int value = (int)BotRandom((float)upperExclusive);
    return Q_min(value, upperExclusive - 1);
}

bool BotController::BotRandomOneIn(int n)
{
    return n > 0 && BotRandomInt(n) == 0;
}

bool BotController::BotRandomPercent(float percent)
{
    return percent > 0.0f && BotRandom(100.0f) < percent;
}

// Added in OPM
//  Draw debug visualization of belief zones. Each zone is drawn as a
//  colored circle at its centroid: green (low) -> yellow -> red (high).
//  The current patrol target zone gets a cyan pyramid marker.
//  A cyan arrow is drawn from the bot to its target zone.
//  Only drawn for the first bot to avoid visual clutter.
//  Periodic console output summarizes belief state.
void BotController::DrawDebugBeliefs()
{
    if (!beliefMap.IsInitialized()) {
        return;
    }

    // Only draw for the first bot to avoid clutter
    const Container<BotController *>& controllers = botManager.getControllerManager().getControllers();
    if (controllers.NumObjects() > 0 && controllers.ObjectAt(1) != this) {
        return;
    }

    const Container<BeliefZone>& zones    = beliefMap.GetZones();
    Vector                       botPos   = controlledEnt->origin;
    int                          bestZone = beliefMap.GetBestZone(botPos);

    int activeCount = 0;

    for (int i = 1; i <= zones.NumObjects(); i++) {
        const BeliefZone& zone = zones.ObjectAt(i);
        if (zone.belief < 0.01f) {
            continue;
        }

        activeCount++;

        float r, g, b;
        if (zone.belief < 0.5f) {
            // Green to yellow
            r = zone.belief * 2.0f;
            g = 1.0f;
            b = 0.0f;
        } else {
            // Yellow to red
            r = 1.0f;
            g = 1.0f - (zone.belief - 0.5f) * 2.0f;
            b = 0.0f;
        }

        float  radius = 32.0f + zone.belief * 64.0f;
        Vector pos    = zone.centroid;
        G_DebugCircle((float *)pos, radius, r, g, b, zone.belief, qtrue);

        // Mark the current patrol target with a cyan pyramid
        if ((i - 1) == bestZone) {
            Vector pyramidPos = zone.centroid;
            pyramidPos.z += 96.0f;
            G_DebugPyramid(pyramidPos, 48.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // Draw arrow from bot to target zone
    if (bestZone >= 0 && bestZone < zones.NumObjects()) {
        const BeliefZone& target = zones.ObjectAt(bestZone + 1);
        Vector            dir    = target.centroid - botPos;
        float             len    = dir.length();
        if (len > 1.0f) {
            VectorNormalize(dir);
            G_DebugArrow(botPos, dir, len, 0.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // Periodic console summary (every 2 seconds)
    static int lastPrintTime = 0;
    if (level.inttime - lastPrintTime >= 2000) {
        lastPrintTime = level.inttime;

        gi.Printf(
            "--- Belief Map [%s]: %d/%d active zones ---\n",
            controlledEnt->client->pers.netname,
            activeCount,
            zones.NumObjects()
        );

        if (bestZone >= 0 && bestZone < zones.NumObjects()) {
            const BeliefZone& target = zones.ObjectAt(bestZone + 1);
            gi.Printf(
                "  Target zone %d: belief=%.2f pos=(%.0f, %.0f, %.0f)\n",
                bestZone,
                target.belief,
                target.centroid.x,
                target.centroid.y,
                target.centroid.z
            );
        } else {
            gi.Printf("  No target zone (all beliefs below threshold)\n");
        }
    }
}

void BotController::Init(void) {}

void BotController::GetUsercmd(usercmd_t *ucmd)
{
    *ucmd = m_botCmd;
}

void BotController::GetEyeInfo(usereyes_t *eyeinfo)
{
    *eyeinfo = m_botEyes;
}

const char *BotController::GetEngagementModeName(BotEngagementMode mode) const
{
    switch (mode) {
    case BotEngagementMode::Attack:
        return "Attack";
    case BotEngagementMode::Curious:
        return "Curious";
    default:
        return "None";
    }
}

const char *BotController::GetTacticalModeName(BotTacticalMode mode) const
{
    switch (mode) {
    case BotTacticalMode::Idle:
        return "Idle";
    case BotTacticalMode::Overwatch:
        return "Overwatch";
    default:
        return "None";
    }
}

const char *BotController::GetHazardModeName(BotHazardMode mode) const
{
    switch (mode) {
    case BotHazardMode::Grenade:
        return "Grenade";
    default:
        return "None";
    }
}

const char *BotController::GetMoveClearReasonName(BotMoveClearReason reason) const
{
    switch (reason) {
    case BotMoveClearReason::ModeTransition:
        return "ModeTransition";
    case BotMoveClearReason::AttackExpired:
        return "AttackExpired";
    case BotMoveClearReason::CuriousExpired:
        return "CuriousExpired";
    case BotMoveClearReason::CombatStop:
        return "CombatStop";
    case BotMoveClearReason::OverwatchAnchor:
        return "OverwatchAnchor";
    case BotMoveClearReason::IdlePause:
        return "IdlePause";
    case BotMoveClearReason::Reaction:
        return "Reaction";
    case BotMoveClearReason::ScriptHold:
        return "ScriptHold";
    case BotMoveClearReason::ScriptMoveComplete:
        return "ScriptMoveComplete";
    case BotMoveClearReason::None:
    default:
        return "None";
    }
}

void BotController::ApplyButtonAction(int buttonMask, BotButtonAction action)
{
    switch (action) {
    case BotButtonAction::Clear:
        m_botCmd.buttons &= ~buttonMask;
        break;
    case BotButtonAction::Hold:
        m_botCmd.buttons |= buttonMask;
        break;
    case BotButtonAction::Toggle:
        m_botCmd.buttons ^= buttonMask;
        break;
    case BotButtonAction::Leave:
    default:
        break;
    }
}

BotMoveClearReason BotController::RefreshPerceptionState(void)
{
    if (m_reaction.lookUntil && level.inttime >= m_reaction.lookUntil) {
        m_reaction.reset();
    }

    BotMoveClearReason moveClearReason = RefreshAttackState();
    BotSetMoveClearReason(moveClearReason, RefreshCuriousState());
    RefreshGrenadeState();
    RefreshOverwatchState();

    return moveClearReason;
}

BotPerceptionSnapshot BotController::BuildPerceptionSnapshot(void) const
{
    BotPerceptionSnapshot snapshot = {};
    const float            grenadeRadiusSq = Square(g_bot_grenade_avoid_radius->value);

    snapshot.attackActive = (m_combat.attackTime > level.inttime);
    snapshot.curiousActive =
        !snapshot.attackActive && m_curious.time > level.inttime;
    snapshot.grenadeActive =
        (m_grenade.grenade && m_grenade.grenade->IsSubclassOfProjectile()
         && (m_grenade.grenade->origin - controlledEnt->origin).lengthSquared() < grenadeRadiusSq)
        || (m_grenade.avoidTime > level.inttime);
    snapshot.overwatchActive = (m_overwatch.dwellUntil > level.inttime);
    snapshot.idleActive      = !snapshot.curiousActive && !snapshot.attackActive && !snapshot.overwatchActive;
    snapshot.moving            = movement.IsMoving();
    snapshot.anchorActive      = snapshot.overwatchActive;
    snapshot.anchorDistSq      = FLT_MAX;
    snapshot.enemyAnchorDistSq = FLT_MAX;

    if (snapshot.anchorActive) {
        Vector flatOffset     = controlledEnt->origin - m_overwatch.standPos;
        flatOffset.z          = 0;
        snapshot.anchorDistSq = flatOffset.lengthSquared();

        if (m_enemy.enemy && IsValidEnemy(m_enemy.enemy)) {
            Vector enemyOffset         = m_enemy.enemy->origin - m_overwatch.standPos;
            enemyOffset.z              = 0;
            snapshot.enemyAnchorDistSq = enemyOffset.lengthSquared();
        } else if (m_enemy.lastPos != vec_zero) {
            Vector enemyOffset         = m_enemy.lastPos - m_overwatch.standPos;
            enemyOffset.z              = 0;
            snapshot.enemyAnchorDistSq = enemyOffset.lengthSquared();
        }
    }

    return snapshot;
}

void BotController::ClearOverwatchAnchor(const char *reason, bool startCooldown)
{
    if (g_bot_debug_state->integer && m_overwatch.dwellUntil) {
        gi.Printf("BOT %s: Overwatch anchor cleared (%s)\n", controlledEnt->client->pers.netname, reason);
    }

    if (startCooldown) {
        m_overwatch.cooldownUntil = level.inttime + 10000 + (int)BotRandom(20000.0f);
    }

    botManager.GetTacticalMemory().ReleaseOccupant(controlledEnt ? controlledEnt->entnum : -1);

    m_overwatch.windowPos      = vec_zero;
    m_overwatch.standPos       = vec_zero;
    m_overwatch.lookDir        = vec_zero;
    m_overwatch.anchorPos      = vec_zero;
    m_overwatch.dwellUntil     = 0;
    m_overwatch.scanTime       = 0;
    m_overwatch.displacedSince = 0;
    m_overwatch.committedSince = 0;
    m_overwatch.pathFailCount  = 0;
    m_overwatch.spotIndex      = -1;
}

BotMoveClearReason BotController::UpdateModeTransitions(const BotPerceptionSnapshot& snapshot)
{
    BotMoveClearReason moveClearReason = BotMoveClearReason::None;
    BotEngagementMode nextEngagement = BotEngagementMode::None;
    if (snapshot.attackActive) {
        nextEngagement = BotEngagementMode::Attack;
    } else if (snapshot.curiousActive) {
        nextEngagement = BotEngagementMode::Curious;
    }

    BotTacticalMode nextTactical = BotTacticalMode::None;
    if (snapshot.overwatchActive) {
        nextTactical = BotTacticalMode::Overwatch;
    } else if (snapshot.idleActive) {
        nextTactical = BotTacticalMode::Idle;
    }

    BotHazardMode nextHazard = snapshot.grenadeActive ? BotHazardMode::Grenade : BotHazardMode::None;

    if (m_engagementMode != nextEngagement) {
        if (g_bot_debug_state->integer) {
            gi.Printf(
                "BOT %s: engagement %s -> %s\n",
                controlledEnt->client->pers.netname,
                GetEngagementModeName(m_engagementMode),
                GetEngagementModeName(nextEngagement)
            );
        }

        if (nextEngagement == BotEngagementMode::Attack || nextEngagement == BotEngagementMode::Curious) {
            m_idle.reset();
            moveClearReason = BotMoveClearReason::ModeTransition;
        }

        if (nextEngagement == BotEngagementMode::Curious) {
            // Treat each curious entry as a fresh investigation so repeated
            // stimuli from the same spot still reissue movement.
            m_curious.lastPos     = vec_zero;
            m_curious.losProbePos = vec_zero;
        }

        if (m_engagementMode == BotEngagementMode::Attack && nextEngagement != BotEngagementMode::Attack) {
            m_combat.strafeTime    = 0;
            m_combat.strafeDir     = 0;
            m_combat.standingStill = false;
            m_combat.crouching     = false;
            m_combat.crouchDecided = false;
            m_idle.leanDir         = 0;
            controlledEnt->ZoomOff();
        }

        m_engagementMode = nextEngagement;
    }

    if (m_tacticalMode != nextTactical) {
        if (g_bot_debug_state->integer) {
            gi.Printf(
                "BOT %s: tactical %s -> %s\n",
                controlledEnt->client->pers.netname,
                GetTacticalModeName(m_tacticalMode),
                GetTacticalModeName(nextTactical)
            );
        }

        if (nextTactical == BotTacticalMode::Overwatch) {
            m_idle.reset();
            if (g_bot_debug_state->integer) {
                gi.Printf(
                    "BOT %s: Overwatch anchor at (%.0f, %.0f, %.0f)\n",
                    controlledEnt->client->pers.netname,
                    m_overwatch.standPos.x,
                    m_overwatch.standPos.y,
                    m_overwatch.standPos.z
                );
            }
        }

        m_tacticalMode = nextTactical;
    }

    if (m_hazardMode != nextHazard) {
        if (g_bot_debug_state->integer) {
            gi.Printf(
                "BOT %s: hazard %s -> %s\n",
                controlledEnt->client->pers.netname,
                GetHazardModeName(m_hazardMode),
                GetHazardModeName(nextHazard)
            );
        }

        if (nextHazard == BotHazardMode::Grenade) {
            m_idle.reset();
        }

        m_hazardMode = nextHazard;
    }

    return moveClearReason;
}

static Vector bot_origin;

static int sentients_compare(const void *elem1, const void *elem2)
{
    Entity *e1, *e2;
    float   delta[3];
    float   d1, d2;

    e1 = *(Entity **)elem1;
    e2 = *(Entity **)elem2;

    VectorSubtract(bot_origin, e1->origin, delta);
    d1 = VectorLengthSquared(delta);

    VectorSubtract(bot_origin, e2->origin, delta);
    d2 = VectorLengthSquared(delta);

    if (d2 <= d1) {
        return d1 > d2;
    } else {
        return -1;
    }
}

bool BotController::IsValidEnemy(Sentient *sent) const
{
    if (sent == controlledEnt) {
        return false;
    }

    if (sent->hidden() || (sent->flags & FL_NOTARGET)) {
        return false;
    }

    if (sent->IsDead()) {
        return false;
    }

    if (sent->getSolidType() == SOLID_NOT) {
        return false;
    }

    if (sent->IsSubclassOfPlayer()) {
        Player *player = static_cast<Player *>(sent);

        if (g_gametype->integer >= GT_TEAM && player->GetTeam() == controlledEnt->GetTeam()) {
            return false;
        }
    } else if (sent->m_Team == controlledEnt->m_Team) {
        return false;
    }

    return true;
}

float BotController::GetVisionDistance(void) const
{
    return Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828f) * m_profile.visionDistanceMult;
}

float BotController::GetHorizontalViewOffset(const Vector& pos) const
{
    Vector delta   = pos - controlledEnt->centroid;
    Vector forward = controlledEnt->orientation[0];

    delta.z   = 0.0f;
    forward.z = 0.0f;

    const float deltaLenSq   = delta.lengthSquared();
    const float forwardLenSq = forward.lengthSquared();

    if (deltaLenSq <= Square(1.0f) || forwardLenSq <= Square(0.001f)) {
        return 0.0f;
    }

    const float dot = DotProduct(delta, forward) / sqrtf(deltaLenSq * forwardLenSq);
    return RAD2DEG(acosf(Q_clamp_float(dot, -1.0f, 1.0f)));
}

float BotController::GetPassiveSpotRate(
    Sentient *sent,
    float     distSq,
    float    *traceFov,
    bool     *immediate,
    bool     *forceLook
) const
{
    const float viewOffset = GetHorizontalViewOffset(sent->centroid);

    if (traceFov) {
        *traceFov = m_profile.spotPeripheralFov;
    }
    if (immediate) {
        *immediate = false;
    }
    if (forceLook) {
        *forceLook = false;
    }

    if (viewOffset <= m_profile.spotImmediateFov * 0.5f) {
        if (immediate) {
            *immediate = true;
        }
        return m_profile.spotAwarenessThreshold;
    }

    if (viewOffset <= m_profile.spotLikelyFov * 0.5f) {
        return m_profile.spotLikelyRate;
    }

    if (viewOffset <= m_profile.spotPeripheralFov * 0.5f) {
        return m_profile.spotPeripheralRate;
    }

    if (distSq <= Square(m_profile.spotCloseFlankerRange)) {
        if (traceFov) {
            *traceFov = 360.0f;
        }
        if (forceLook) {
            *forceLook = true;
        }
        return m_profile.spotCloseFlankerRate;
    }

    return 0.0f;
}

void BotController::ResetPassiveSpotAwareness(void)
{
    m_combat.spotAwarenessEnemy = NULL;
    m_combat.spotAwareness      = 0.0f;
}

void BotController::DecayPassiveSpotAwareness(void)
{
    if (!m_combat.spotAwarenessEnemy) {
        m_combat.spotAwareness = 0.0f;
        return;
    }

    m_combat.spotAwareness -= level.frametime * 0.75f;
    if (m_combat.spotAwareness <= 0.0f) {
        ResetPassiveSpotAwareness();
    }
}

bool BotController::AdvancePassiveSpotAwareness(Sentient *sent, float rate)
{
    if (!sent || rate <= 0.0f) {
        DecayPassiveSpotAwareness();
        return false;
    }

    if (m_combat.spotAwarenessEnemy != sent) {
        m_combat.spotAwarenessEnemy = sent;
        m_combat.spotAwareness      = 0.0f;
    }

    m_combat.spotAwareness += rate * level.frametime;
    return m_combat.spotAwareness >= m_profile.spotAwarenessThreshold;
}

void BotController::StartCombatReactionDelay(void)
{
    const int minDelay    = Q_max(0, (int)(m_profile.reactionMinDelay * 1000.0f));
    const int randomDelay = Q_max(0, (int)(m_profile.reactionRandomDelay * 1000.0f));

    m_combat.lastUnseenTime    = level.inttime;
    m_combat.reactionReadyTime = level.inttime + minDelay;

    if (randomDelay > 0) {
        m_combat.reactionReadyTime += (int)BotRandom((float)randomDelay);
    }
}

BotMoveClearReason BotController::RefreshAttackState(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0.0f;
    Sentient             *bestEnemy     = NULL;
    Sentient             *spotCandidate = NULL;
    bool                  bestEnemyForceLook     = false;
    bool                  spotCandidateForceLook = false;
    float                 spotCandidateRate      = 0.0f;
    float                 spotCandidateDistSq    = FLT_MAX;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);

    maxDistance = GetVisionDistance();

    if (m_enemy.enemy && m_combat.attackTime > level.inttime && IsValidEnemy(m_enemy.enemy)
        && controlledEnt->CanSee(m_enemy.enemy, m_profile.spotPeripheralFov, maxDistance, false)) {
        bestEnemy = m_enemy.enemy;
    }

    for (int i = 1; i <= sents.NumObjects(); i++) {
        if (bestEnemy) {
            break;
        }

        Sentient *sent = sents.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

        float traceFov  = 0.0f;
        bool  immediate = false;
        bool  forceLook = false;
        float spotRate  = GetPassiveSpotRate(sent, distSq, &traceFov, &immediate, &forceLook);

        if (spotRate <= 0.0f || !controlledEnt->CanSee(sent, traceFov, maxDistance, false)) {
            continue;
        }

        if (immediate) {
            bestEnemy          = sent;
            bestEnemyForceLook = forceLook;
            break;
        }

        if (!spotCandidate || spotRate > spotCandidateRate
            || (spotRate == spotCandidateRate && distSq < spotCandidateDistSq)) {
            spotCandidate          = sent;
            spotCandidateForceLook = forceLook;
            spotCandidateRate      = spotRate;
            spotCandidateDistSq    = distSq;
        }
    }

    if (!bestEnemy && spotCandidate) {
        if (AdvancePassiveSpotAwareness(spotCandidate, spotCandidateRate)) {
            bestEnemy          = spotCandidate;
            bestEnemyForceLook = spotCandidateForceLook;
        }
    } else if (!bestEnemy) {
        DecayPassiveSpotAwareness();
    }

    if (bestEnemy) {
        if (m_enemy.enemy != bestEnemy) {
            ResetPassiveSpotAwareness();
            m_enemy.eyesTag = -1;
            StartCombatReactionDelay();
        }

        m_enemy.enemy       = bestEnemy;
        m_enemy.lastPos     = bestEnemy->origin;
        m_combat.attackTime = level.inttime + 500 + (int)BotRandom(1000.0f);
        if (bestEnemyForceLook) {
            m_combat.lastSeenTime      = level.inttime;
            m_combat.attackStopAimTime = level.inttime + 1250;
        }
        beliefMap.UpdateFromSighting(bestEnemy->origin);
        return BotMoveClearReason::None;
    }

    if (m_combat.attackTime && level.inttime > m_combat.attackTime) {
        m_combat.attackTime = 0;
        return BotMoveClearReason::AttackExpired;
    }

    return BotMoveClearReason::None;
}

BotMoveClearReason BotController::RefreshCuriousState(void)
{
    if (m_combat.attackTime > level.inttime) {
        if (g_bot_debug_state->integer >= 2 && m_curious.time) {
            gi.Printf(
                "BOT %s: Curious blocked - in combat (attackTime=%dms)\n",
                controlledEnt->client->pers.netname,
                m_combat.attackTime - level.inttime
            );
        }
        m_curious.time        = 0;
        m_curious.lastPos     = vec_zero;
        m_curious.losProbePos = vec_zero;
        return BotMoveClearReason::None;
    }

    const bool committedHold =
        m_overwatch.committedSince != 0 && m_overwatch.displacedSince == 0
        && level.inttime >= m_overwatch.committedSince + g_bot_tactical_commit_ms->integer;

    if (committedHold && m_curious.time > level.inttime) {
        const bool strongType = (m_curious.stimulusType == AI_EVENT_WEAPON_FIRE
                                 || m_curious.stimulusType == AI_EVENT_EXPLOSION);
        const bool closeRange = (m_curious.stimulusDistanceSq < Square(g_bot_tactical_break_dist->value));

        if (!strongType && !closeRange) {
            if (m_reaction.lookUntil < level.inttime) {
                m_reaction.lookPos   = m_curious.targetPos;
                m_reaction.lookUntil = level.inttime + 1500;
            }
            m_curious.time        = 0;
            m_curious.lastPos     = vec_zero;
            m_curious.losProbePos = vec_zero;
            return BotMoveClearReason::None;
        }

        ClearOverwatchAnchor("strong curious stimulus", true);
    }

    if (m_curious.time && level.inttime > m_curious.time) {
        if (g_bot_debug_state->integer >= 2) {
            gi.Printf(
                "BOT %s: Curious expired (curiousTime=%d, inttime=%d)\n",
                controlledEnt->client->pers.netname,
                m_curious.time,
                level.inttime
            );
        }
        m_curious.time        = 0;
        m_curious.lastPos     = vec_zero;
        m_curious.losProbePos = vec_zero;
        return BotMoveClearReason::CuriousExpired;
    }

    return BotMoveClearReason::None;
}

void BotController::RefreshGrenadeState(void)
{
    if (m_grenade.grenade && m_grenade.grenade->IsSubclassOfProjectile()) {
        float distSq = (m_grenade.grenade->origin - controlledEnt->origin).lengthSquared();
        float radius = g_bot_grenade_avoid_radius->value;

        if (distSq < radius * radius) {
            return;
        }
    }

    m_grenade.grenade = NULL;

    float      radiusSq = Square(g_bot_grenade_avoid_radius->value);
    gentity_t *edict;
    int        i;

    for (i = game.maxclients, edict = &g_entities[i]; i < globals.num_entities; i++, edict++) {
        if (!edict->inuse || !edict->entity) {
            continue;
        }

        Entity *ent = edict->entity;
        if (!ent->IsSubclassOfProjectile()) {
            continue;
        }

        Projectile *proj = static_cast<Projectile *>(ent);
        if (proj->GetOwner() == controlledEnt) {
            continue;
        }

        Sentient *projOwner = proj->GetOwner();
        if (projOwner && projOwner->IsSubclassOfPlayer() && g_gametype->integer >= GT_TEAM) {
            Player *p = static_cast<Player *>(projOwner);
            if (p->GetTeam() == controlledEnt->GetTeam()) {
                continue;
            }
        }

        float distSq = (ent->origin - controlledEnt->origin).lengthSquared();
        if (distSq < radiusSq) {
            m_grenade.grenade   = ent;
            m_grenade.avoidTime = level.inttime + 3000;
            return;
        }
    }
}

void BotController::RefreshOverwatchState(void)
{
    if (m_overwatch.cooldownUntil > level.inttime) {
        return;
    }

    if (m_overwatch.dwellUntil > level.inttime) {
        return;
    }

    if (m_overwatch.dwellUntil) {
        m_overwatch.cooldownUntil = level.inttime + 10000 + (int)BotRandom(20000.0f);
        m_overwatch.dwellUntil    = 0;
        return;
    }

    if (m_combat.attackTime > level.inttime || m_curious.time > level.inttime || m_grenade.avoidTime > level.inttime) {
        return;
    }

    {
        int team = (int)controlledEnt->GetTeam();

        PathSearchParameter params;
        params.entity     = controlledEnt;
        params.fallHeight = 128.0f;

        int idx = botManager.GetTacticalMemory().QueryBestSpot(team, controlledEnt->origin, 2048.0f, controlledEnt, params);
        if (idx >= 0) {
            const TacticalSpot& spot = botManager.GetTacticalMemory().GetSpot(idx);
            m_overwatch.standPos       = spot.standPos;
            m_overwatch.lookDir        = spot.lookDir;
            m_overwatch.anchorPos      = spot.standPos + spot.lookDir * 256.0f;
            m_overwatch.windowPos      = m_overwatch.anchorPos;
            m_overwatch.dwellUntil     = level.inttime + 5000 + (int)BotRandom(5000.0f);
            m_overwatch.scanTime       = 0;
            m_overwatch.displacedSince = 0;
            m_overwatch.committedSince = 0;
            m_overwatch.pathFailCount  = 0;
            m_overwatch.spotIndex      = idx;
            botManager.GetTacticalMemory().SetOccupant(idx, controlledEnt->entnum);
            return;
        }
    }

    static const float kWindowDetectDistance = 384.0f;
    static const float kWindowYawOffsets[]   = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f};

    Vector  eyePos = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    trace_t trace;
    bool    foundWindow = false;

    for (unsigned int i = 0; i < ARRAY_LEN(kWindowYawOffsets); ++i) {
        Vector testAngles = controlledEnt->angles;
        testAngles.y += kWindowYawOffsets[i];

        Vector dir;
        AngleVectorsLeft(testAngles, dir, NULL, NULL);

        Vector end = eyePos + dir * kWindowDetectDistance;
        trace      = G_Trace(
            eyePos, vec_zero, vec_zero, end, controlledEnt, MASK_PLAYERSOLID, false, "BotOverwatchCondition"
        );

        if (trace.fraction < 1.0f && trace.ent && trace.ent->entity->isSubclassOf(WindowObject)) {
            foundWindow = true;
            break;
        }
    }

    if (!foundWindow) {
        return;
    }

    Vector windowCentroid = trace.ent->entity->centroid;
    Vector windowNormal   = trace.plane.normal;
    windowNormal.z        = 0;

    if (windowNormal.lengthSquared() < Square(0.01f)) {
        windowNormal = controlledEnt->origin - windowCentroid;
        windowNormal.z = 0;
    }

    if (windowNormal.lengthSquared() < Square(0.01f)) {
        return;
    }

    VectorNormalizeFast(windowNormal);

    Vector moveDir = windowCentroid - controlledEnt->origin;
    moveDir.z      = 0;

    if (moveDir.lengthSquared() < Square(1.0f)) {
        moveDir = -windowNormal;
    } else {
        VectorNormalizeFast(moveDir);
    }

    static const float kWindowStandOffsets[] = {28.0f, 36.0f, 44.0f};
    Vector             standPos              = vec_zero;
    bool               foundStandPos         = false;

    for (unsigned int i = 0; i < ARRAY_LEN(kWindowStandOffsets); ++i) {
        Vector candidate = trace.endpos + windowNormal * kWindowStandOffsets[i];
        candidate.z      = controlledEnt->origin.z;

        trace_t standTrace = G_Trace(
            controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight),
            vec_zero,
            vec_zero,
            candidate + Vector(0, 0, controlledEnt->viewheight),
            controlledEnt,
            MASK_PLAYERSOLID,
            false,
            "BotOverwatchStand"
        );

        if (standTrace.fraction >= 1.0f) {
            standPos      = candidate;
            foundStandPos = true;
            break;
        }
    }

    if (!foundStandPos) {
        return;
    }

    Vector lookDir = windowCentroid - standPos;
    lookDir.z      = 0;

    if (lookDir.lengthSquared() < Square(1.0f)) {
        lookDir = moveDir;
    }

    VectorNormalizeFast(lookDir);

    Vector anchorPos = standPos + lookDir * 256.0f;
    anchorPos.z      = standPos.z + controlledEnt->viewheight;

    m_overwatch.windowPos      = windowCentroid;
    m_overwatch.standPos       = standPos;
    m_overwatch.lookDir        = lookDir;
    m_overwatch.anchorPos      = anchorPos;
    m_overwatch.dwellUntil     = level.inttime + 3000 + (int)BotRandom(4000.0f);
    m_overwatch.scanTime       = 0;
    m_overwatch.displacedSince = 0;
    m_overwatch.committedSince = 0;
    m_overwatch.pathFailCount  = 0;
    m_overwatch.spotIndex      = -1;
}

Vector BotController::ProbeLOSPosition(const Vector& targetPos)
{
    if (targetPos == vec_zero) {
        return vec_zero;
    }

    static const float kAngles[] = {30.f, -30.f, 60.f, -60.f, 90.f, -90.f};
    static const float kRadii[]  = {256.f, 384.f, 512.f};

    Vector forward = targetPos - controlledEnt->origin;
    forward.z      = 0;
    if (forward.lengthSquared() < Square(1.0f)) {
        return vec_zero;
    }
    VectorNormalizeFast(forward);
    Vector right(-forward.y, forward.x, 0);

    Vector eyeOffset(0, 0, controlledEnt->viewheight);
    Vector targetEye = targetPos + Vector(0, 0, (float)controlledEnt->viewheight);

    // Two-pass: all sight traces first (cheap), CanMoveTo only on those that pass (expensive).
    Vector sightPassed[18];
    int    numSightPassed = 0;

    for (int ri = 0; ri < 3; ri++) {
        for (int ai = 0; ai < 6; ai++) {
            float  rad       = DEG2RAD(kAngles[ai]);
            Vector candidate = controlledEnt->origin + (forward * cosf(rad) + right * sinf(rad)) * kRadii[ri];
            candidate.z      = controlledEnt->origin.z;

            if (G_SightTrace(
                    candidate + eyeOffset,
                    vec_zero,
                    vec_zero,
                    targetEye,
                    controlledEnt,
                    (Entity *)NULL,
                    MASK_CANSEE,
                    false,
                    "ProbeLOSPosition"
                )) {
                sightPassed[numSightPassed++] = candidate;
            }
        }
    }

    for (int i = 0; i < numSightPassed; i++) {
        if (movement.CanMoveTo(sightPassed[i])) {
            return sightPassed[i];
        }
    }
    return vec_zero;
}

bool BotController::CheckWindows(Vector *outWindowPos, Vector *outLookDir)
{
    trace_t trace;
    Vector  start, end;
    Vector  dir;

    controlledEnt->angles.AngleVectorsLeft(&dir);
    start = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    end   = start + dir * 128.0f;

    trace = G_Trace(start, vec_zero, vec_zero, end, controlledEnt, MASK_PLAYERSOLID, false, "BotController::CheckWindows");

    if (trace.fraction != 1 && trace.ent && trace.ent->entity->isSubclassOf(WindowObject)) {
        if (outWindowPos) {
            *outWindowPos = trace.ent->entity->centroid;
        }
        if (outLookDir) {
            Vector d = trace.ent->entity->centroid - start;
            VectorNormalizeFast(d);
            *outLookDir = d;
        }
        return true;
    }

    return false;
}

BotCombatIntent BotController::AdvanceCombatStateAndBuildIntent(const BotPerceptionSnapshot& snapshot)
{
    BotCombatIntent intent;
    intent.reset();

    if (snapshot.attackActive) {
        bool    bMelee     = false;
        bool    bCanSee    = false;
        bool    bCanAttack = false;
        float   fEnemyDistanceSquared;
        float   fDistanceSquared = 0.0f;
        Weapon *pWeap            = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
        bool    bNoMove          = false;
        bool    bFiring          = false;

        intent.mode = BotEngagementMode::Attack;

        if (!m_enemy.enemy || !IsValidEnemy(m_enemy.enemy)) {
            if (level.inttime < m_combat.attackStopAimTime && m_enemy.lastPos != vec_zero) {
                intent.aimType      = BotAimDirective::AimAtPoint;
                intent.aimTarget    = m_enemy.lastPos;
                intent.attackLeft   = BotButtonAction::Clear;
                intent.attackRight  = BotButtonAction::Clear;
                m_combat.attackTime = level.inttime + 200 + (int)BotRandom(300.0f);
                return intent;
            }

            m_combat.attackTime = 0;
            intent.mode         = BotEngagementMode::None;
            return intent;
        }

        fDistanceSquared = (m_enemy.enemy->origin - controlledEnt->origin).lengthSquared();
        m_enemy.oldPos   = m_enemy.lastPos;

        float visionDist = GetVisionDistance();
        bCanSee = controlledEnt->CanSee(m_enemy.enemy, m_profile.spotPeripheralFov, visionDist, false);

        if (bCanSee) {
            if (!pWeap) {
                return intent;
            }

            intent.visibleEnemy = true;
            bCanAttack = true;
            if (m_combat.lastUnseenTime) {
                if (m_combat.reactionReadyTime <= m_combat.lastUnseenTime) {
                    StartCombatReactionDelay();
                }

                if (level.inttime < m_combat.reactionReadyTime) {
                    if (g_bot_debug_state->integer >= 2) {
                        gi.Printf(
                            "BOT %s: Attack - waiting for reaction delay (elapsed=%dms, remaining=%dms)\n",
                            controlledEnt->client->pers.netname,
                            level.inttime - m_combat.lastUnseenTime,
                            m_combat.reactionReadyTime - level.inttime
                        );
                    }
                    bCanAttack = false;
                } else {
                    m_combat.lastUnseenTime    = 0;
                    m_combat.reactionReadyTime = 0;
                }
            }

            if (bCanAttack) {
                const int fireDelay                    = pWeap->FireDelay(FIRE_PRIMARY) * 1000;
                float     fSecondaryBulletRange        = pWeap->GetBulletRange(FIRE_SECONDARY);
                float     fSecondaryBulletRangeSquared = fSecondaryBulletRange * fSecondaryBulletRange;
                const int maxcontinuousFireTime = fireDelay + m_profile.continuousFireMinTime * 1000
                                                + BotRandom(m_profile.continuousFireRandomTime * 1000.0f);
                const int maxBurstTime =
                    fireDelay + m_profile.burstMinTime * 1000 + BotRandom(m_profile.burstRandomDelay * 1000.0f);

                if (pWeap->GetMaxFireMovement() < 1 && pWeap->HasAmmoInClip(FIRE_PRIMARY)) {
                    float length = controlledEnt->velocity.length();
                    if ((length / sv_runspeed->value) > pWeap->GetMaxFireMovementMult()) {
                        bNoMove                = true;
                        intent.moveClearReason = BotMoveClearReason::CombatStop;
                    }
                }

                if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                    && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                    if (g_bot_debug_state->integer >= 2) {
                        gi.Printf("BOT %s: Attack - no ammo, switching weapon\n", controlledEnt->client->pers.netname);
                    }
                    intent.attackLeft  = BotButtonAction::Clear;
                    intent.attackRight = BotButtonAction::Clear;
                    controlledEnt->ZoomOff();

                    if (level.inttime > m_combat.lastWeaponSwitchTime + 500) {
                        m_combat.lastWeaponSwitchTime = level.inttime;
                        Event ev;
                        controlledEnt->SelectNextWeapon(&ev);
                    }
                } else {
                    // Fire at visible targets regardless of the weapon's configured range.
                    // The user-facing request is to keep engaging once the bot has line of sight.
                    if (pWeap->IsSemiAuto()) {
                        if (controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                            && (controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                                || controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
                            if (g_bot_debug_state->integer >= 2) {
                                gi.Printf(
                                    "BOT %s: Attack - waiting for weapon idle (anim=%d)\n",
                                    controlledEnt->client->pers.netname,
                                    controlledEnt->client->ps.iViewModelAnim
                                );
                            }
                            intent.attackLeft  = BotButtonAction::Clear;
                            intent.attackRight = BotButtonAction::Clear;
                            controlledEnt->ZoomOff();
                        } else {
                            bFiring           = true;
                            intent.attackLeft = BotButtonAction::Toggle;
                            if (pWeap->GetZoom()) {
                                intent.attackRight =
                                    controlledEnt->IsZoomed() ? BotButtonAction::Clear : BotButtonAction::Hold;
                            }
                        }
                    } else {
                        bFiring           = true;
                        intent.attackLeft = BotButtonAction::Hold;
                    }
                }

                if (m_combat.lastBurstTime) {
                    if (level.inttime > m_combat.lastBurstTime + maxBurstTime) {
                        m_combat.lastBurstTime      = 0;
                        m_combat.continuousFireTime = 0;
                    } else {
                        intent.attackLeft = BotButtonAction::Clear;
                    }
                } else {
                    if (bFiring) {
                        m_combat.continuousFireTime += level.intframetime;
                    } else {
                        m_combat.continuousFireTime = 0;
                    }

                    if (!m_combat.lastBurstTime && m_combat.continuousFireTime > maxcontinuousFireTime) {
                        m_combat.lastBurstTime      = level.inttime;
                        m_combat.continuousFireTime = 0;
                    }
                }

                if (pWeap->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
                    if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0
                        && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
                        bMelee = true;
                    } else if (fDistanceSquared <= fSecondaryBulletRangeSquared) {
                        bMelee = true;
                    }
                }

                if (bMelee) {
                    intent.attackLeft  = BotButtonAction::Clear;
                    intent.attackRight = (fDistanceSquared <= fSecondaryBulletRangeSquared) ? BotButtonAction::Toggle
                                                                                            : BotButtonAction::Clear;
                }

                if (intent.attackLeft == BotButtonAction::Toggle || intent.attackLeft == BotButtonAction::Hold
                    || intent.attackRight == BotButtonAction::Toggle || intent.attackRight == BotButtonAction::Hold) {
                    intent.updatedLastFireTime = true;
                }

                m_combat.attackTime        = level.inttime + 500 + (int)BotRandom(1000.0f);
                m_combat.attackStopAimTime = level.inttime + 500 + (int)BotRandom(1000.0f);
                m_combat.lastSeenTime      = level.inttime;
                m_enemy.lastPos            = m_enemy.enemy->origin;
            }
        } else {
            intent.attackLeft  = BotButtonAction::Clear;
            intent.attackRight = BotButtonAction::Clear;

            if (level.inttime > m_combat.lastSeenTime + 2000) {
                StartCombatReactionDelay();
            }
        }

        if (bCanSee || level.inttime < m_combat.attackStopAimTime) {
            Vector        vTarget;
            orientation_t eyes_or;

            if (m_enemy.eyesTag == -1) {
                m_enemy.eyesTag = gi.Tag_NumForName(m_enemy.enemy->edict->tiki, "eyes bone");
            }

            if (m_enemy.eyesTag != -1) {
                m_enemy.enemy->GetTag(m_enemy.eyesTag, &eyes_or);
                vTarget = eyes_or.origin;
            } else {
                vTarget = m_enemy.enemy->origin;
            }

            if (level.inttime >= m_combat.lastAimTime + 300 + (int)BotRandom(300.0f)) {
                float halfW     = (m_enemy.enemy->maxs.x - m_enemy.enemy->mins.x) * 0.5f;
                float halfD     = (m_enemy.enemy->maxs.y - m_enemy.enemy->mins.y) * 0.5f;
                float fDist     = sqrtf(fDistanceSquared);
                float distScale = Q_clamp_float((fDist - 256) / 768, 0.15, 1.0);

                if (m_enemy.eyesTag != -1) {
                    m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                    m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                    m_combat.aimOffsetTarget[2] = -BotRandom(m_enemy.enemy->maxs.z * 0.5f) * distScale;
                } else {
                    m_combat.aimOffsetTarget[0] = G_CRandom(halfW) * distScale;
                    m_combat.aimOffsetTarget[1] = G_CRandom(halfD) * distScale;
                    m_combat.aimOffsetTarget[2] = 16 + BotRandom(m_enemy.enemy->viewheight - 16) * distScale;
                }

                m_combat.lastAimTime      = level.inttime;
                m_combat.aimLerpStartTime = level.inttime;
            }

            float dt       = level.frametime * m_profile.aimLerpSpeed;
            float lerpFrac = Q_clamp_float(dt, 0.0, 1.0);

            m_combat.aimOffset[0] += (m_combat.aimOffsetTarget[0] - m_combat.aimOffset[0]) * lerpFrac;
            m_combat.aimOffset[1] += (m_combat.aimOffsetTarget[1] - m_combat.aimOffset[1]) * lerpFrac;
            m_combat.aimOffset[2] += (m_combat.aimOffsetTarget[2] - m_combat.aimOffset[2]) * lerpFrac;

            intent.aimType   = BotAimDirective::AimAtPoint;
            intent.aimTarget = vTarget + m_combat.aimOffset * m_profile.aimSpreadMult;
        } else if (m_combat.losRecoverPos != vec_zero && m_enemy.lastPos != vec_zero) {
            intent.aimType   = BotAimDirective::AimAtPoint;
            intent.aimTarget = m_enemy.lastPos;
        } else {
            intent.aimType = BotAimDirective::AimAlongPath;
        }

        if (bNoMove) {
            m_combat.standingStill = true;
            intent.run             = false;
            return intent;
        }

        fEnemyDistanceSquared = (controlledEnt->origin - m_enemy.lastPos).lengthSquared();

        const float longRangeThreshold = 800 * 800;
        const float midRangeThreshold  = 400 * 400;

        if (bCanSee && bCanAttack && fEnemyDistanceSquared > longRangeThreshold) {
            m_combat.standingStill = true;
            intent.moveClearReason = BotMoveClearReason::CombatStop;
        } else if (bCanSee && bCanAttack && fEnemyDistanceSquared > midRangeThreshold) {
            if (BotRandomPercent(30.0f)) {
                m_combat.standingStill = true;
                intent.moveClearReason = BotMoveClearReason::CombatStop;
            } else {
                m_combat.standingStill = false;
            }
        } else {
            m_combat.standingStill = false;
        }

        if (bCanSee && m_combat.standingStill) {
            if (level.inttime >= m_idle.leanTime) {
                m_idle.leanTime = level.inttime + 1500 + (int)BotRandom(2000.0f);
                int roll        = BotRandomInt(5);
                if (roll < 2) {
                    m_idle.leanDir = -1;
                } else if (roll < 4) {
                    m_idle.leanDir = 1;
                } else {
                    m_idle.leanDir = 0;
                }
            }
        } else {
            m_idle.leanDir = 0;
        }

        if (m_combat.standingStill) {
            if (!m_combat.crouching && !m_combat.crouchDecided) {
                m_combat.crouchDecided = true;
                if (BotRandomPercent(m_profile.crouchChance)) {
                    m_combat.crouching = true;
                }
            }
        } else {
            m_combat.crouching     = false;
            m_combat.crouchDecided = false;
        }

        intent.upmove  = m_combat.crouching ? -127 : 0;
        intent.leanDir = m_idle.leanDir;
        intent.run     = !m_combat.standingStill;

        if (bCanSee && !bMelee) {
            if (m_combat.standingStill && m_profile.longRangeStrafeChance <= 0.0f) {
                m_combat.strafeDir  = 0;
                m_combat.strafeTime = level.inttime + 250;
            } else {
                if (level.inttime >= m_combat.strafeTime) {
                    const bool allowRangeStrafe =
                        !m_combat.standingStill || BotRandomPercent(m_profile.longRangeStrafeChance);

                    if (!allowRangeStrafe) {
                        m_combat.strafeTime = level.inttime + 300 + (int)BotRandom(700.0f);
                        m_combat.strafeDir  = 0;
                    } else {
                        int roll = BotRandomInt(10);

                        if (roll < 2) {
                            m_combat.strafeTime = level.inttime + 150 + (int)BotRandom(250.0f);
                            m_combat.strafeDir  = BotRandomInt(2) ? 127 : -127;
                        } else if (roll < 4) {
                            m_combat.strafeTime = level.inttime + 600 + (int)BotRandom(1200.0f);
                            m_combat.strafeDir  = BotRandomInt(2) ? 127 : -127;
                        } else if (roll < 8) {
                            m_combat.strafeTime = level.inttime + 300 + (int)BotRandom(700.0f);
                            m_combat.strafeDir  = 0;
                        } else {
                            m_combat.strafeTime = level.inttime + 100 + (int)BotRandom(200.0f);
                            m_combat.strafeDir  = m_combat.strafeDir > 0 ? -127 : 127;
                        }
                    }
                }
            }

            intent.rightmove = m_combat.strafeDir;
        }

        if (m_combat.standingStill) {
            return intent;
        }

        if (bCanSee && bCanAttack && !bMelee) {
            intent.moveClearReason  = BotMoveClearReason::CombatStop;
            m_combat.losRecoverPos  = vec_zero;
            m_combat.losRecoverTime = 0;
        } else if (bMelee || !bCanSee) {
            Vector moveTarget = m_enemy.lastPos;

            if (!bCanSee && !bMelee) {
                if (m_combat.losRecoverTime == 0
                    || (m_combat.losRecoverPos == vec_zero && level.inttime >= m_combat.losRecoverTime + 500)) {
                    m_combat.losRecoverPos  = ProbeLOSPosition(m_enemy.lastPos);
                    m_combat.losRecoverTime = level.inttime;

                    if (g_bot_debug_state->integer >= 2) {
                        if (m_combat.losRecoverPos != vec_zero) {
                            gi.Printf(
                                "BOT %s: Combat LOS probe -> (%.0f, %.0f, %.0f)\n",
                                controlledEnt->client->pers.netname,
                                m_combat.losRecoverPos.x,
                                m_combat.losRecoverPos.y,
                                m_combat.losRecoverPos.z
                            );
                        } else {
                            gi.Printf("BOT %s: Combat LOS probe - no pos found\n", controlledEnt->client->pers.netname);
                        }
                    }
                }
                if (m_combat.losRecoverPos != vec_zero) {
                    moveTarget = m_combat.losRecoverPos;
                }
            }

            intent.moveType    = BotMoveRequestType::MoveTo;
            intent.stuckPolicy = BotStuckPolicy::TrackAndRecover;
            intent.moveTarget  = moveTarget;

            if (!bCanSee && movement.MoveDone()) {
                ClearEnemy();
                return intent;
            }
        }

        if (movement.IsMoving() || intent.moveType == BotMoveRequestType::MoveTo) {
            m_combat.attackTime = level.inttime + 500 + (int)BotRandom(1000.0f);
        }

        return intent;
    }

    if (snapshot.curiousActive) {
        intent.mode = BotEngagementMode::Curious;

        Vector targetPos = (m_curious.targetPos != vec_zero) ? m_curious.targetPos
                                                             : beliefMap.GetHighestBeliefPos(controlledEnt->origin);

        if (CheckWindows()) {
            intent.attackLeft          = BotButtonAction::Toggle;
            intent.updatedLastFireTime = true;
        } else {
            intent.attackLeft  = BotButtonAction::Clear;
            intent.attackRight = BotButtonAction::Clear;
        }

        if (targetPos != vec_zero && controlledEnt->CanSee(targetPos, m_profile.spotPeripheralFov, 2048, false)) {
            intent.aimType   = BotAimDirective::AimAtPoint;
            intent.aimTarget = targetPos;
        } else if (movement.IsMoving()) {
            intent.aimType = BotAimDirective::AimAlongPath;
        } else if (targetPos != vec_zero) {
            intent.aimType   = BotAimDirective::AimAtPoint;
            intent.aimTarget = targetPos;
        }

        if (targetPos != vec_zero && m_curious.lastPos != targetPos) {
            m_curious.lastPos = targetPos;

            Vector losPos         = ProbeLOSPosition(targetPos);
            m_curious.losProbePos = losPos;

            if (losPos != vec_zero) {
                intent.moveType   = BotMoveRequestType::MoveTo;
                intent.moveTarget = losPos;
            } else {
                intent.moveType   = BotMoveRequestType::MoveNear;
                intent.moveTarget = targetPos;
                intent.radius     = 512.0f;
            }

            if (g_bot_debug_state->integer >= 2) {
                if (losPos != vec_zero) {
                    gi.Printf(
                        "BOT %s: Curious LOS probe -> (%.0f, %.0f, %.0f)\n",
                        controlledEnt->client->pers.netname,
                        losPos.x,
                        losPos.y,
                        losPos.z
                    );
                } else {
                    gi.Printf(
                        "BOT %s: Curious investigating (%.0f, %.0f, %.0f) [no LOS pos]\n",
                        controlledEnt->client->pers.netname,
                        targetPos.x,
                        targetPos.y,
                        targetPos.z
                    );
                }
            }
        }

        if (m_curious.scanUntil) {
            // Scan phase: arrived at target, now holding and looking around before clearing.
            if (level.inttime >= m_curious.scanUntil) {
                beliefMap.ClearZone(m_curious.targetPos != vec_zero ? m_curious.targetPos : controlledEnt->origin);
                m_curious.time        = 0;
                m_curious.lastPos     = vec_zero;
                m_curious.losProbePos = vec_zero;
                m_curious.scanUntil   = 0;
            } else {
                // Hold position; look at a random nearby point each scan tick.
                if (m_reaction.lookUntil < level.inttime) {
                    Vector scanDir(G_CRandom() * 512.0f, G_CRandom() * 512.0f, G_CRandom() * 64.0f);
                    m_reaction.lookPos   = controlledEnt->origin + scanDir;
                    m_reaction.lookUntil = level.inttime + 600 + (int)BotRandom(600.0f);
                }
                intent.aimType   = BotAimDirective::AimAtPoint;
                intent.aimTarget = m_reaction.lookPos;
            }
            return intent;
        }

        if (movement.MoveDone()) {
            const Vector& arrivalRef =
                (m_curious.losProbePos != vec_zero) ? m_curious.losProbePos : m_curious.targetPos;
            float distToTarget = (arrivalRef - controlledEnt->origin).length();
            if (distToTarget < 256) {
                // Start scan pause instead of immediately clearing.
                m_curious.scanUntil = level.inttime + 1500 + (int)BotRandom(500.0f);
                if (g_bot_debug_state->integer >= 2) {
                    gi.Printf(
                        "BOT %s: Curious arrived - scanning for %.1fs\n",
                        controlledEnt->client->pers.netname,
                        (m_curious.scanUntil - level.inttime) / 1000.0f
                    );
                }
            } else if (!movement.IsMoving() && level.inttime + 17000 > m_curious.time) {
                m_curious.time = 0;
            }
        }

        return intent;
    }

    return intent;
}

BotHazardIntent BotController::BuildHazardIntent(const BotPerceptionSnapshot& snapshot)
{
    BotHazardIntent intent;
    intent.reset();

    if (!snapshot.grenadeActive || !m_grenade.grenade) {
        return intent;
    }

    intent.mode         = BotHazardMode::Grenade;
    intent.moveType     = BotMoveRequestType::AvoidPath;
    intent.stuckPolicy  = BotStuckPolicy::Ignore;
    intent.moveTarget   = m_grenade.grenade->origin;
    intent.preferredDir = controlledEnt->origin - m_grenade.grenade->origin;
    VectorNormalizeFast(intent.preferredDir);
    intent.preferredDir *= 512.0f;
    intent.radius = g_bot_grenade_avoid_radius->value;
    return intent;
}

BotTacticalIntent BotController::AdvanceTacticalStateAndBuildIntent(const BotPerceptionSnapshot& snapshot)
{
    BotTacticalIntent intent;
    intent.reset();

    if (snapshot.anchorActive && snapshot.overwatchActive) {
        if (movement.IsPositionBanned(m_overwatch.standPos)) {
            ClearOverwatchAnchor("stand pos banned", true);
            intent.reset();
            return intent;
        }

        intent.mode         = BotTacticalMode::Overwatch;
        intent.stuckPolicy  = BotStuckPolicy::Ignore;
        intent.anchorActive = true;

        const float holdRadiusSq    = Square(16.0f);
        const float combatLeashSq   = Square(1024.0f);
        const int   returnTimeoutMs = 4000;
        const int   maxPathFailures = 2;

        if (snapshot.grenadeActive) {
            m_overwatch.displacedSince = 0;
            m_overwatch.pathFailCount  = 0;
            return intent;
        } else {
            if (snapshot.anchorDistSq > holdRadiusSq) {
                intent.anchorReturning = true;

                if (!movement.CanMoveTo(m_overwatch.standPos)) {
                    m_overwatch.pathFailCount++;
                    if (m_overwatch.pathFailCount >= maxPathFailures) {
                        ClearOverwatchAnchor("path failure", true);
                        intent.reset();
                        return intent;
                    }
                } else {
                    m_overwatch.pathFailCount = 0;
                }

                if (!m_overwatch.displacedSince) {
                    m_overwatch.displacedSince = level.inttime;
                } else if (level.inttime >= m_overwatch.displacedSince + returnTimeoutMs) {
                    ClearOverwatchAnchor("return timeout", true);
                    intent.reset();
                    return intent;
                }

                if (snapshot.attackActive && snapshot.enemyAnchorDistSq > combatLeashSq) {
                    ClearOverwatchAnchor("combat leash", true);
                    intent.reset();
                    return intent;
                }

                intent.moveType   = BotMoveRequestType::MoveTo;
                intent.moveTarget = m_overwatch.standPos;
            } else {
                intent.moveClearReason     = BotMoveClearReason::OverwatchAnchor;
                m_overwatch.displacedSince = 0;
                m_overwatch.pathFailCount  = 0;

                if (m_overwatch.committedSince == 0) {
                    m_overwatch.committedSince = level.inttime;
                    botManager.GetTacticalMemory().TryRecordSpot(
                        m_overwatch.standPos, m_overwatch.lookDir, (int)controlledEnt->GetTeam(), controlledEnt
                    );
                }

                // At the anchor with a visible enemy: lock position so combat strafing
                // doesn't push the bot off the window.
                if (snapshot.attackActive && m_enemy.enemy) {
                    float visionDist = GetVisionDistance();
                    if (controlledEnt->CanSee(m_enemy.enemy, m_profile.spotPeripheralFov, visionDist, false)) {
                        intent.lockPosition = true;
                    }
                }
            }
        }

        if (!snapshot.attackActive && level.inttime >= m_overwatch.scanTime) {
            m_overwatch.scanTime = level.inttime + 800 + (int)BotRandom(700.0f);

            Vector baseDir = m_overwatch.lookDir;
            if (m_overwatch.anchorPos != vec_zero) {
                baseDir = m_overwatch.anchorPos - m_overwatch.standPos;
            }
            if (baseDir.lengthSquared() < Square(1.0f)) {
                baseDir = m_overwatch.lookDir;
            }
            VectorNormalizeFast(baseDir);

            Vector lookAngles;
            vectoangles(baseDir, lookAngles);
            lookAngles.y += G_CRandom(20.0f);
            lookAngles.x += G_CRandom(5.0f);

            Vector perturbedDir;
            AngleVectors(lookAngles, perturbedDir, NULL, NULL);

            Vector aimOrigin = m_overwatch.standPos + Vector(0, 0, controlledEnt->viewheight);
            intent.aimType   = BotAimDirective::AimAtPoint;
            intent.aimTarget = aimOrigin + perturbedDir * 1024.0f;
        }

        if (!snapshot.attackActive) {
            intent.attackLeft  = BotButtonAction::Clear;
            intent.attackRight = BotButtonAction::Clear;
            intent.reload      = true;
        }
        return intent;
    }

    if (!snapshot.idleActive) {
        return intent;
    }

    intent.mode = BotTacticalMode::Idle;

    if (CheckWindows()) {
        intent.attackLeft          = BotButtonAction::Toggle;
        intent.updatedLastFireTime = true;
    } else {
        intent.attackLeft  = BotButtonAction::Clear;
        intent.attackRight = BotButtonAction::Clear;
        intent.reload      = true;
    }

    if (m_idle.pausing) {
        if (level.inttime >= m_idle.pauseTime) {
            m_idle.pausing = false;
            if (BotRandomOneIn(4)) {
                m_idle.walking  = true;
                m_idle.walkTime = level.inttime + 2000 + (int)BotRandom(3000.0f);
            }
        } else {
            intent.moveClearReason = BotMoveClearReason::IdlePause;
            intent.run       = false;

            if (level.inttime >= m_idle.lookTime) {
                m_idle.lookTime = level.inttime + 800 + (int)BotRandom(1200.0f);

                Vector beliefPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
                if (beliefPos != vec_zero) {
                    intent.aimType   = BotAimDirective::AimAtPoint;
                    intent.aimTarget = beliefPos;
                } else {
                    Vector lookAngles = controlledEnt->angles;
                    lookAngles.y += G_CRandom(90);
                    lookAngles.x     = G_CRandom(15);
                    intent.aimType   = BotAimDirective::SetAngles;
                    intent.aimAngles = lookAngles;
                }
            }

            return intent;
        }
    } else if (BotRandomOneIn(400)) {
        m_idle.pausing   = true;
        m_idle.pauseTime = level.inttime + 1500 + (int)BotRandom(2500.0f);
        m_idle.lookTime  = level.inttime + 500;
        intent.moveClearReason = BotMoveClearReason::IdlePause;
        intent.run       = false;
        return intent;
    }

    if (m_idle.walking && level.inttime >= m_idle.walkTime) {
        m_idle.walking = false;
    }

    intent.run = !m_idle.walking;

    Vector beliefPos = beliefMap.GetHighestBeliefPos(controlledEnt->origin);
    if (beliefPos != vec_zero && controlledEnt->CanSee(beliefPos, m_profile.spotPeripheralFov, 2048, false)) {
        intent.aimType   = BotAimDirective::AimAtPoint;
        intent.aimTarget = beliefPos;
    } else {
        intent.aimType = BotAimDirective::AimAlongPath;

        if (movement.IsMoving()) {
            if (m_idle.scanTarget != vec_zero) {
                if (level.inttime >= m_idle.scanUntil) {
                    m_idle.scanTarget   = vec_zero;
                    m_idle.scanNextTime = level.inttime + 2000 + (int)BotRandom(3000.0f);
                } else {
                    intent.aimType   = BotAimDirective::AimAtPoint;
                    intent.aimTarget = m_idle.scanTarget;
                }
            } else if (level.inttime >= m_idle.scanNextTime) {
                Vector eyePos    = controlledEnt->EyePosition();
                Vector tryAngles = rotation.GetTargetAngles();
                tryAngles.y += G_CRandom(60.0f);
                tryAngles.x = G_CRandom(15.0f);

                Vector forward;
                AngleVectors(tryAngles, forward, NULL, NULL);

                trace_t tr = G_Trace(
                    eyePos,
                    vec_zero,
                    vec_zero,
                    eyePos + forward * 4096.0f,
                    controlledEnt,
                    MASK_SOLID,
                    false,
                    "BotPatrolScan"
                );

                if (tr.fraction > 0 && (tr.endpos - eyePos).lengthSquared() >= Square(256)) {
                    m_idle.scanTarget = tr.endpos;
                    m_idle.scanUntil  = level.inttime + 500 + (int)BotRandom(1500.0f);
                } else {
                    m_idle.scanNextTime = level.inttime + 500;
                }
            }
        }
    }

    if (movement.WasGivenUp()) {
        // The movement layer gave up after repeated blocks on the previous
        // target. Discard it so the next intent picks somewhere different.
        if (ai_debugpath->integer) {
            gi.Printf(
                "BOT[%d] WasGivenUp — clearing beliefPos=%s deathPos=%s\n",
                controlledEnt->entnum,
                beliefPos != vec_zero ? "yes" : "no",
                m_enemy.deathPos != vec_zero ? "yes" : "no"
            );
        }
        if (beliefPos != vec_zero) {
            beliefMap.ClearZone(beliefPos);
        }
        m_enemy.deathPos = vec_zero;
    }

    if (!movement.IsMoving()) {
        if (beliefPos != vec_zero) {
            intent.moveType   = BotMoveRequestType::MoveNear;
            intent.moveTarget = beliefPos;
            intent.radius     = 512.0f;

            if (movement.MoveDone() && (beliefPos - controlledEnt->origin).lengthSquared() <= Square(256)) {
                beliefMap.ClearZone(beliefPos);
            }
        } else if (m_enemy.deathPos != vec_zero) {
            intent.moveType   = BotMoveRequestType::MoveTo;
            intent.moveTarget = m_enemy.deathPos;

            if (movement.MoveDone() && (m_enemy.deathPos - controlledEnt->origin).lengthSquared() <= Square(256)) {
                m_enemy.deathPos = vec_zero;
            }
        } else {
            Vector randomDir(G_CRandom(16), G_CRandom(16), G_CRandom(16));
            Vector preferredDir;
            preferredDir += Vector(controlledEnt->orientation[0]) * (BotRandomInt(5) ? 1024 : -1024);
            preferredDir += Vector(controlledEnt->orientation[2]) * (BotRandomInt(5) ? 1024 : -1024);

            intent.moveType     = BotMoveRequestType::AvoidPath;
            intent.moveTarget   = controlledEnt->origin + randomDir;
            intent.preferredDir = preferredDir;
            intent.radius       = 512 + BotRandom(2048.0f);
        }
    }

    return intent;
}

BotResolvedCommand BotController::ResolveIntents(
    const BotCombatIntent& combat,
    const BotHazardIntent& hazard,
    const BotTacticalIntent& tactical,
    BotMoveClearReason perceptionClearReason,
    BotMoveClearReason transitionClearReason
)
{
    BotResolvedCommand resolved;
    resolved.reset();

    resolved.engagementMode = combat.mode;
    resolved.tacticalMode   = tactical.mode;
    resolved.hazardMode     = hazard.mode;
    resolved.attackLeft     = BotButtonAction::Clear;
    resolved.attackRight    = BotButtonAction::Clear;
    resolved.run            = !(m_idle.pausing || m_combat.standingStill || m_idle.walking);
    resolved.leanDir        = m_idle.leanDir;
    BotSetMoveClearReason(resolved.moveClearReason, perceptionClearReason);
    BotSetMoveClearReason(resolved.moveClearReason, transitionClearReason);

    if (hazard.mode != BotHazardMode::None) {
        resolved.hazardMode   = hazard.mode;
        resolved.moveType     = hazard.moveType;
        resolved.stuckPolicy  = hazard.stuckPolicy;
        resolved.moveTarget   = hazard.moveTarget;
        resolved.preferredDir = hazard.preferredDir;
        resolved.radius       = hazard.radius;
        BotSetMoveClearReason(resolved.moveClearReason, hazard.moveClearReason);
    }

    if (tactical.mode != BotTacticalMode::None) {
        resolved.tacticalMode        = tactical.mode;
        resolved.reload              = tactical.reload;
        resolved.run                 = tactical.run && resolved.run;
        resolved.updatedLastFireTime = tactical.updatedLastFireTime;
        resolved.attackLeft          = tactical.attackLeft;
        resolved.attackRight         = tactical.attackRight;

        if (resolved.moveType == BotMoveRequestType::None && tactical.moveType != BotMoveRequestType::None) {
            resolved.moveType     = tactical.moveType;
            resolved.stuckPolicy  = tactical.stuckPolicy;
            resolved.moveTarget   = tactical.moveTarget;
            resolved.preferredDir = tactical.preferredDir;
            resolved.radius       = tactical.radius;
        }
        BotSetMoveClearReason(resolved.moveClearReason, tactical.moveClearReason);
        if (tactical.aimType != BotAimDirective::None) {
            resolved.aimType   = tactical.aimType;
            resolved.aimTarget = tactical.aimTarget;
            resolved.aimAngles = tactical.aimAngles;
        }
    }

    if (combat.mode != BotEngagementMode::None) {
        resolved.engagementMode      = combat.mode;
        resolved.attackLeft          = combat.attackLeft;
        resolved.attackRight         = combat.attackRight;
        resolved.rightmove           = tactical.lockPosition ? 0 : combat.rightmove;
        resolved.upmove              = combat.upmove;
        resolved.leanDir             = combat.leanDir;
        resolved.run                 = combat.run && resolved.run;
        resolved.visibleEnemy        = combat.visibleEnemy;
        resolved.updatedLastFireTime = resolved.updatedLastFireTime || combat.updatedLastFireTime;

        if (combat.moveType != BotMoveRequestType::None && !tactical.lockPosition) {
            resolved.moveType     = combat.moveType;
            resolved.stuckPolicy  = combat.stuckPolicy;
            resolved.moveTarget   = combat.moveTarget;
            resolved.preferredDir = combat.preferredDir;
            resolved.radius       = combat.radius;
        }
        BotSetMoveClearReason(resolved.moveClearReason, combat.moveClearReason);
        if (combat.aimType != BotAimDirective::None) {
            resolved.aimType   = combat.aimType;
            resolved.aimTarget = combat.aimTarget;
            resolved.aimAngles = combat.aimAngles;
        }
    }

    if (resolved.aimType == BotAimDirective::None && m_reaction.lookUntil > level.inttime
        && m_reaction.lookPos != vec_zero) {
        resolved.aimType   = BotAimDirective::AimAtPoint;
        resolved.aimTarget = m_reaction.lookPos;
        BotSetMoveClearReason(resolved.moveClearReason, m_reaction.moveClearReason);
    }

    return resolved;
}

void BotController::ApplyScriptControl(BotResolvedCommand& command)
{
    if (m_scriptControl.holdPosition) {
        // Script hold is authoritative and must override lower-priority move clears.
        command.moveClearReason = BotMoveClearReason::ScriptHold;
        command.moveType        = BotMoveRequestType::None;
        command.rightmove       = 0;
        command.run             = false;
    } else if (m_scriptControl.moveType != BotScriptMoveType::None) {
        if (m_scriptControl.moveStarted && (movement.ReachedMoveGoal() || movement.CompletedMove())) {
            m_scriptControl.moveType    = BotScriptMoveType::None;
            m_scriptControl.moveStarted = false;
            command.moveClearReason     = BotMoveClearReason::ScriptMoveComplete;
            command.moveType            = BotMoveRequestType::None;
            command.rightmove           = 0;
        } else {
            command.moveClearReason = BotMoveClearReason::None;
            command.moveType        = BotMoveRequestType::None;
            command.rightmove       = 0;

            if (m_scriptControl.moveStarted && !movement.IsMoving()) {
                // The path vanished without reaching the goal, so treat the scripted move as failed instead of
                // retrying a path search every frame.
                m_scriptControl.moveType    = BotScriptMoveType::None;
                m_scriptControl.moveStarted = false;
            } else if (!m_scriptControl.moveStarted) {
                switch (m_scriptControl.moveType) {
                case BotScriptMoveType::MoveTo:
                    command.moveType   = BotMoveRequestType::MoveTo;
                    command.stuckPolicy = BotStuckPolicy::Ignore;
                    command.moveTarget = m_scriptControl.moveTarget;
                    break;
                case BotScriptMoveType::MoveNear:
                    command.moveType   = BotMoveRequestType::MoveNear;
                    command.stuckPolicy = BotStuckPolicy::Ignore;
                    command.moveTarget = m_scriptControl.moveTarget;
                    command.radius     = m_scriptControl.moveRadius;
                    break;
                case BotScriptMoveType::None:
                default:
                    break;
                }

                m_scriptControl.moveStarted = command.moveType != BotMoveRequestType::None;
            }
        }
    }

    if (m_scriptControl.hasLookTarget) {
        command.aimType   = BotAimDirective::AimAtPoint;
        command.aimTarget = m_scriptControl.lookTarget;
    } else if (m_scriptControl.hasWatchTarget && !command.visibleEnemy) {
        command.aimType   = BotAimDirective::AimAtPoint;
        command.aimTarget = m_scriptControl.watchTarget;
    }

    switch (m_scriptControl.posture) {
    case BotScriptPosture::Stand:
    case BotScriptPosture::Prone:
        command.upmove = 0;
        break;
    case BotScriptPosture::Crouch:
        command.upmove = -127;
        break;
    case BotScriptPosture::None:
    default:
        break;
    }

    if (m_scriptControl.primaryFire) {
        command.attackLeft = BotButtonAction::Hold;
    }
    if (m_scriptControl.secondaryFire) {
        command.attackRight = BotButtonAction::Hold;
    }
    if (m_scriptControl.useButton) {
        command.useButton = BotButtonAction::Hold;
    }
    if (m_scriptControl.reloadRequested) {
        command.reload                    = true;
        m_scriptControl.reloadRequested = false;
    }
}

void BotController::DebugResolvedCommand(const BotResolvedCommand& command) const
{
    float anchorDist = -1.0f;
    if (m_overwatch.dwellUntil > level.inttime) {
        Vector flatOffset = controlledEnt->origin - m_overwatch.standPos;
        flatOffset.z      = 0;
        anchorDist        = sqrtf(flatOffset.lengthSquared());
    }

    gi.Printf(
        "BOT %s: resolved engagement=%s tactical=%s hazard=%s move=%d aim=%d rm=%d um=%d clear=%s anchor=%d "
        "dist=%.0f returning=%d\n",
        controlledEnt->client->pers.netname,
        GetEngagementModeName(command.engagementMode),
        GetTacticalModeName(command.tacticalMode),
        GetHazardModeName(command.hazardMode),
        (int)command.moveType,
        (int)command.aimType,
        command.rightmove,
        command.upmove,
        GetMoveClearReasonName(command.moveClearReason),
        (m_overwatch.dwellUntil > level.inttime) ? 1 : 0,
        anchorDist,
        (m_overwatch.displacedSince != 0) ? 1 : 0
    );
}

void BotController::ExecuteResolvedCommand(const BotResolvedCommand& command)
{
    m_botCmd.forwardmove = 0;
    m_botCmd.rightmove   = command.rightmove;
    m_botCmd.upmove      = command.upmove;

    // Keep movement side effects centralized here; decision code should express intent.
    if (command.moveClearReason != BotMoveClearReason::None) {
        movement.ClearMove();
    }

    switch (command.moveType) {
    case BotMoveRequestType::Clear:
        movement.ClearMove();
        break;
    case BotMoveRequestType::MoveTo:
        movement.MoveTo(command.moveTarget, command.stuckPolicy);
        break;
    case BotMoveRequestType::MoveNear:
        movement.MoveNear(command.moveTarget, command.radius, command.stuckPolicy);
        break;
    case BotMoveRequestType::AvoidPath:
        movement.AvoidPath(command.moveTarget, command.radius, command.preferredDir, command.stuckPolicy);
        break;
    case BotMoveRequestType::None:
    default:
        break;
    }

    switch (command.aimType) {
    case BotAimDirective::AimAtPoint:
        rotation.AimAt(command.aimTarget);
        break;
    case BotAimDirective::AimAlongPath:
        AimAtAimNode();
        break;
    case BotAimDirective::SetAngles:
        rotation.SetTargetAngles(command.aimAngles);
        break;
    case BotAimDirective::None:
    default:
        break;
    }

    ApplyButtonAction(BUTTON_ATTACKLEFT, command.attackLeft);
    ApplyButtonAction(BUTTON_ATTACKRIGHT, command.attackRight);
    ApplyButtonAction(BUTTON_USE, command.useButton);

    if (command.updatedLastFireTime) {
        m_iLastFireTime = level.inttime;
    }

    m_botCmd.buttons &= ~(BUTTON_LEAN_LEFT | BUTTON_LEAN_RIGHT);
    if (command.leanDir < 0) {
        m_botCmd.buttons |= BUTTON_LEAN_LEFT;
    } else if (command.leanDir > 0) {
        m_botCmd.buttons |= BUTTON_LEAN_RIGHT;
    }

    if (command.run) {
        m_botCmd.buttons |= BUTTON_RUN;
    } else {
        m_botCmd.buttons &= ~BUTTON_RUN;
    }

    if (command.reload) {
        CheckReload();
    }
}

void BotController::UpdateBotStates(void)
{
    m_botCmd.serverTime = level.svsTime;

    if (g_bot_manualmove->integer) {
        m_botCmd.buttons     = 0;
        m_botCmd.forwardmove = m_botCmd.rightmove = m_botCmd.upmove = 0;
        return;
    }

    ApplyProfilePrimaryWeapon();

    if (controlledEnt->GetTeam() == TEAM_NONE || controlledEnt->GetTeam() == TEAM_SPECTATOR) {
        float time;

        // Add some delay to avoid telefragging
        time = controlledEnt->entnum / 20.0;

        if (controlledEnt->EventPending(EV_Player_AutoJoinDMTeam)) {
            return;
        }

        //
        // Team
        //
        controlledEnt->PostEvent(EV_Player_AutoJoinDMTeam, time);
        return;
    }

    if (controlledEnt->IsDead() || controlledEnt->IsSpectator()) {
        // The bot should respawn
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        return;
    }

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = controlledEnt->viewheight;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    // Added in OPM
    //  Per-frame belief map maintenance
    beliefMap.Decay(level.frametime);
    beliefMap.ClearZonesVisibleFrom(controlledEnt, m_profile.spotPeripheralFov);

    BotMoveClearReason perceptionClearReason = RefreshPerceptionState();
    BotPerceptionSnapshot snapshot = BuildPerceptionSnapshot();
    BotMoveClearReason transitionClearReason = UpdateModeTransitions(snapshot);
    BotCombatIntent    combat   = AdvanceCombatStateAndBuildIntent(snapshot);
    BotHazardIntent    hazard   = BuildHazardIntent(snapshot);
    BotTacticalIntent  tactical = AdvanceTacticalStateAndBuildIntent(snapshot);
    BotResolvedCommand command =
        ResolveIntents(combat, hazard, tactical, perceptionClearReason, transitionClearReason);
    ApplyScriptControl(command);
    ExecuteResolvedCommand(command);

    movement.MoveThink(m_botCmd);
    rotation.TurnThink(m_botCmd, m_botEyes);
    if (!ScriptControlsUse()) {
        CheckUse();
    }

    CheckValidWeapon();

    if (g_bot_debug_state->integer >= 2) {
        DebugResolvedCommand(command);
    }

    if (g_bot_debug_state->integer && level.inttime >= m_iLastPosDebugTime + 2000) {
        m_iLastPosDebugTime = level.inttime;

        const Vector& pos = controlledEnt->origin;
        gi.Printf(
            "BOT %s: pos=(%.0f, %.0f, %.0f) moving=%d\n",
            controlledEnt->client->pers.netname,
            pos.x,
            pos.y,
            pos.z,
            movement.IsMoving() ? 1 : 0
        );
    }

    // Added in OPM
    //  Debug visualization of belief zones
    if (g_bot_debug_beliefs->integer) {
        DrawDebugBeliefs();
    }
}

void BotController::ApplyProfilePrimaryWeapon(bool force)
{
    const char *weaponPref =
        (m_profile.preferredWeapon.length() > 0 && Q_stricmp(m_profile.preferredWeapon.c_str(), "auto") != 0)
            ? m_profile.preferredWeapon.c_str()
            : "auto";

    if (!force && controlledEnt->client->pers.dm_primary[0]) {
        return;
    }

    controlledEnt->client->pers.dm_primary[0] = 0;

    Event *event = new Event(EV_Player_PrimaryDMWeapon);
    event->AddString(weaponPref);
    controlledEnt->ProcessEvent(event);

    if (!controlledEnt->client->pers.dm_primary[0] && Q_stricmp(weaponPref, "auto") != 0) {
        Event *fallback = new Event(EV_Player_PrimaryDMWeapon);
        fallback->AddString("auto");
        controlledEnt->ProcessEvent(fallback);
    }
}

void BotController::CheckUse(void)
{
    Vector  dir;
    Vector  start;
    Vector  end;
    trace_t trace;
    bool    useWhenBlocked;

    if (controlledEnt->GetLadder()) {
        return;
    }

    useWhenBlocked = movement.ShouldUseWhenBlocked();

    controlledEnt->angles.AngleVectorsLeft(&dir);

    start = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight);
    end   = controlledEnt->origin + Vector(0, 0, controlledEnt->viewheight) + dir * 64;

    trace = G_Trace(
        start, vec_zero, vec_zero, end, controlledEnt, MASK_USABLE | MASK_LADDER, false, "BotController::CheckUse"
    );

    if (!trace.ent || trace.ent->entity == world) {
        if (useWhenBlocked) {
            m_botCmd.buttons ^= BUTTON_USE;
        } else {
            m_botCmd.buttons &= ~BUTTON_USE;
        }
        return;
    }

    if (trace.ent->entity->IsSubclassOfDoor()) {
        Door *door = static_cast<Door *>(trace.ent->entity);
        if (door->isOpen()) {
            // Don't use an open door
            m_botCmd.buttons &= ~BUTTON_USE;
            return;
        }
    } else if (!trace.ent->entity->isSubclassOf(FuncLadder)) {
        if (useWhenBlocked) {
            m_botCmd.buttons ^= BUTTON_USE;
        } else {
            m_botCmd.buttons &= ~BUTTON_USE;
        }
        return;
    }

    //
    // Toggle the use button
    //
    m_botCmd.buttons ^= BUTTON_USE;

#if 0
    Vector  forward;
    Vector  start, end;

    AngleVectors(controlledEnt->GetViewAngles(), forward, NULL, NULL);

    start = (controlledEnt->m_vViewPos - forward * 12.0f);
    end   = (controlledEnt->m_vViewPos + forward * 128.0f);

    trace = G_Trace(start, vec_zero, vec_zero, end, controlledEnt, MASK_LADDER, qfalse, "checkladder");
    if (trace.ent->entity && trace.ent->entity->isSubclassOf(FuncLadder)) {
        return;
    }

    m_botCmd.buttons ^= BUTTON_USE;
#endif
}

void BotController::CheckValidWeapon()
{
    Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        // If holstered, use the best weapon available
        UseWeaponWithAmmo();
    } else if (!weapon->HasAmmo(FIRE_PRIMARY) && !controlledEnt->GetNewActiveWeapon()) {
        // In case the current weapon has no ammo, use the best available weapon
        UseWeaponWithAmmo();
    }
}

void BotController::SendCommand(const char *text)
{
    char        *buffer;
    char        *data;
    size_t       len;
    ConsoleEvent ev;

    len = strlen(text) + 1;

    buffer = (char *)gi.Malloc(len);
    data   = buffer;
    Q_strncpyz(data, text, len);

    const char *com_token = COM_Parse(&data);

    if (!com_token) {
        return;
    }

    controlledEnt->m_lastcommand = com_token;

    if (!Event::GetEvent(com_token)) {
        return;
    }

    ev = ConsoleEvent(com_token);

    if (!(ev.GetEventFlags(ev.eventnum) & EV_CONSOLE)) {
        gi.Free(buffer);
        return;
    }

    ev.SetConsoleEdict(controlledEnt->edict);

    while (1) {
        com_token = COM_Parse(&data);

        if (!com_token || !*com_token) {
            break;
        }

        ev.AddString(com_token);
    }

    gi.Free(buffer);

    try {
        controlledEnt->ProcessEvent(ev);
    } catch (ScriptException& exc) {
        gi.DPrintf("*** Bot Command Exception *** %s\n", exc.string.c_str());
    }
}

static void BotApplyModHeight(Player *player, const char *height)
{
    if (!player) {
        return;
    }

    Event event("modheight");
    event.AddString(height);
    player->ProcessEvent(event);
}

void BotController::ScriptHoldPosition(bool enabled)
{
    m_scriptControl.holdPosition = enabled;
    if (enabled) {
        movement.ClearMove();
        m_botCmd.forwardmove = 0;
        m_botCmd.rightmove   = 0;
        m_botCmd.upmove      = 0;
        m_botCmd.buttons &= ~BUTTON_RUN;
    }
}

void BotController::ScriptStop(void)
{
    m_scriptControl.moveType    = BotScriptMoveType::None;
    m_scriptControl.moveStarted = false;
    movement.ClearMove();
    m_botCmd.forwardmove = 0;
    m_botCmd.rightmove   = 0;
    m_botCmd.upmove      = 0;
}

void BotController::ScriptSetPosture(BotScriptPosture posture, bool enabled)
{
    if (!enabled) {
        if (m_scriptControl.posture == posture) {
            m_scriptControl.posture = BotScriptPosture::None;
            if (posture == BotScriptPosture::Crouch || posture == BotScriptPosture::Prone) {
                BotApplyModHeight(controlledEnt, "stand");
            }
        }
        return;
    }

    if (posture == BotScriptPosture::Prone && g_protocol >= protocol_e::PROTOCOL_MOHTA_MIN) {
        gi.DPrintf("bot_prone is not supported for this protocol\n");
        return;
    }

    m_scriptControl.posture = posture;

    switch (posture) {
    case BotScriptPosture::Stand:
        BotApplyModHeight(controlledEnt, "stand");
        break;
    case BotScriptPosture::Crouch:
        BotApplyModHeight(controlledEnt, "duck");
        break;
    case BotScriptPosture::Prone:
        BotApplyModHeight(controlledEnt, "prone");
        break;
    case BotScriptPosture::None:
    default:
        break;
    }
}

void BotController::ScriptMoveTo(const Vector& target)
{
    m_scriptControl.holdPosition = false;
    m_scriptControl.moveType     = BotScriptMoveType::MoveTo;
    m_scriptControl.moveTarget   = target;
    m_scriptControl.moveRadius   = 0.0f;
    m_scriptControl.moveStarted  = false;
    movement.ClearMove();
}

void BotController::ScriptMoveNear(const Vector& target, float radius)
{
    m_scriptControl.holdPosition = false;
    m_scriptControl.moveType     = BotScriptMoveType::MoveNear;
    m_scriptControl.moveTarget   = target;
    m_scriptControl.moveRadius   = radius < 0.0f ? 0.0f : radius;
    m_scriptControl.moveStarted  = false;
    movement.ClearMove();
}

void BotController::ScriptLookAt(const Vector& target)
{
    m_scriptControl.hasLookTarget = true;
    m_scriptControl.lookTarget    = target;
}

void BotController::ScriptClearLook(void)
{
    m_scriptControl.hasLookTarget = false;
    m_scriptControl.lookTarget    = vec_zero;
}

void BotController::ScriptWatchAt(const Vector& target)
{
    m_scriptControl.hasWatchTarget = true;
    m_scriptControl.watchTarget    = target;
}

void BotController::ScriptClearWatch(void)
{
    m_scriptControl.hasWatchTarget = false;
    m_scriptControl.watchTarget    = vec_zero;
}

void BotController::ScriptPrimaryFire(bool enabled)
{
    m_scriptControl.primaryFire = enabled;
}

void BotController::ScriptSecondaryFire(bool enabled)
{
    m_scriptControl.secondaryFire = enabled;
}

void BotController::ScriptUse(bool enabled)
{
    m_scriptControl.useButton = enabled;
    if (!enabled) {
        m_botCmd.buttons &= ~BUTTON_USE;
    }
}

void BotController::ScriptReload(void)
{
    m_scriptControl.reloadRequested = true;
}

void BotController::ScriptReleaseControl(void)
{
    if (m_scriptControl.posture == BotScriptPosture::Crouch || m_scriptControl.posture == BotScriptPosture::Prone) {
        BotApplyModHeight(controlledEnt, "stand");
    }

    m_scriptControl.reset();
    m_botCmd.buttons &= ~BUTTON_USE;
    movement.ClearMove();
    m_botCmd.forwardmove = 0;
    m_botCmd.rightmove   = 0;
    m_botCmd.upmove      = 0;
}

bool BotController::ScriptControlsUse(void) const
{
    return m_scriptControl.useButton;
}

/*
====================
AimAtAimNode

Make the bot face toward the current path
====================
*/
void BotController::AimAtAimNode(void)
{
    Vector goal;

    if (!movement.IsMoving()) {
        return;
    }

    //goal = movement.GetCurrentGoal();
    //if (goal != controlledEnt->origin) {
    //    rotation.AimAt(goal);
    //}

    if (controlledEnt->GetLadder()) {
        Vector vAngles = movement.GetCurrentPathDirection().toAngles();
        vAngles.x      = Q_clamp_float(vAngles.x, -80, 80);

        rotation.SetTargetAngles(vAngles);
        return;
    } else {
        Vector targetAngles;
        targetAngles   = movement.GetCurrentPathDirection().toAngles();
        targetAngles.x = 0;
        rotation.SetTargetAngles(targetAngles);
    }
}

/*
====================
CheckReload

Make the bot reload if necessary
====================
*/
void BotController::CheckReload(void)
{
    Weapon *weap;

    if (level.inttime < m_iLastFireTime + 2000) {
        // Don't reload while attacking
        return;
    }

    weap = controlledEnt->GetActiveWeapon(WEAPON_MAIN);

    if (weap && weap->CheckReload(FIRE_PRIMARY)) {
        SendCommand("reload");
    }
}

/*
====================
NoticeEvent

Warn the bot of an event
====================
*/
void BotController::NoticeEvent(Vector vPos, int iType, Entity *pEnt, float fDistanceSquared, float fRadiusSquared)
{
    Sentient *pSentOwner;
    float     fRangeFactor;
    Vector    delta1, delta2;

    fRangeFactor = 1.0 - (fDistanceSquared / fRadiusSquared);
    if (fRangeFactor < 0) {
        fRangeFactor = 0;
    }

    //
    // Resolve the entity that caused the sound
    //
    if (pEnt->IsSubclassOfSentient()) {
        pSentOwner = static_cast<Sentient *>(pEnt);
    } else if (pEnt->IsSubclassOfVehicleTurretGun()) {
        VehicleTurretGun *pVTG = static_cast<VehicleTurretGun *>(pEnt);
        pSentOwner             = pVTG->GetSentientOwner();
    } else if (pEnt->IsSubclassOfItem()) {
        Item *pItem = static_cast<Item *>(pEnt);
        pSentOwner  = pItem->GetOwner();
    } else if (pEnt->IsSubclassOfProjectile()) {
        Projectile *pProj = static_cast<Projectile *>(pEnt);
        pSentOwner        = pProj->GetOwner();
    } else {
        pSentOwner = NULL;
    }

    if (pSentOwner) {
        if (pSentOwner == controlledEnt) {
            // Ignore self
            return;
        }

        if ((pSentOwner->flags & FL_NOTARGET) || pSentOwner->getSolidType() == SOLID_NOT) {
            return;
        }

        // Ignore teammates
        if (pSentOwner->IsSubclassOfPlayer()) {
            Player *p = static_cast<Player *>(pSentOwner);

            if (g_gametype->integer >= GT_TEAM && p->GetTeam() == controlledEnt->GetTeam()) {
                return;
            }
        }
    }

    // Changed in OPM
    //  Always update beliefs from enemy sounds regardless of distance
    //  or whether the bot is already investigating something else.
    //  The belief map is the bot's spatial awareness — it should
    //  accumulate all heard information. Only the curiosity trigger
    //  (whether to actively investigate) is gated by probability.
    beliefMap.UpdateFromEvent(vPos, iType, fRangeFactor);

    //
    // Curiosity trigger — probability-gated
    //

    // Already curious about a closer sound? Don't switch target.
    if (m_curious.time) {
        delta1 = vPos - controlledEnt->origin;
        delta2 = m_curious.targetPos - controlledEnt->origin;
        if (delta1.lengthSquared() > delta2.lengthSquared()) {
            return;
        }
    }

    // Changed in OPM
    //  Close-range weapon fire and explosions bypass the probability gate.
    //  This ensures bots react immediately to nearby threats even if they
    //  weren't directly hit. A soldier would always notice gunfire 10 feet away.
    //  Note: WEAPON_IMPACT is excluded - we want the shooter position, not impact.
    bool bypassProbability = false;
    if (fRangeFactor > 0.7f) {
        // Close range (within ~30% of sound radius)
        switch (iType) {
        case AI_EVENT_WEAPON_FIRE:
        case AI_EVENT_EXPLOSION:
        case AI_EVENT_GRENADE:
            bypassProbability = true;
            break;
        default:
            break;
        }
    }

    if (!bypassProbability && fRangeFactor < random()) {
        return;
    }

    switch (iType) {
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
    case AI_EVENT_WEAPON_IMPACT:
        // Ignore bullet impacts - they indicate where the bullet hit, not where
        // the shooter is. React to WEAPON_FIRE instead which gives shooter position.
        break;
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_EXPLOSION:
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_GRENADE:
    default:
        m_curious.time               = level.inttime + 20000;
        m_curious.scanUntil          = 0;
        m_curious.targetPos          = vPos;
        m_curious.stimulusType       = iType;
        m_curious.stimulusDistanceSq = fDistanceSquared;
        if (g_bot_debug_state->integer >= 2) {
            gi.Printf(
                "BOT %s: NoticeEvent set curious (time=%d, pos=(%.0f,%.0f,%.0f))\n",
                controlledEnt->client->pers.netname,
                m_curious.time,
                vPos.x,
                vPos.y,
                vPos.z
            );
        }
        break;
    }

    // Added in OPM
    //  For close-range threat sounds, immediately turn toward the source.
    //  Don't wait for the state machine to process it - a soldier would
    //  instinctively look toward nearby gunfire. Skip if already in combat
    //  (attack state handles its own aiming).
    if (bypassProbability && !m_combat.attackTime) {
        if (g_bot_debug_reaction->integer) {
            gi.Printf(
                "BOT %s: Immediate reaction to close-range sound (type=%d, rangeFactor=%.2f) at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname,
                iType,
                fRangeFactor,
                vPos.x,
                vPos.y,
                vPos.z
            );
        }
        m_reaction.lookPos          = vPos;
        m_reaction.lookUntil        = level.inttime + 500;
        m_reaction.moveClearReason  = BotMoveClearReason::Reaction;
        m_idle.reset();
    }
}

/*
====================
ClearEnemy

Clear the bot's enemy
====================
*/
void BotController::ClearEnemy(void)
{
    m_combat.attackTime        = 0;
    m_combat.lastUnseenTime    = 0;
    m_combat.reactionReadyTime = 0;
    m_combat.losRecoverPos     = vec_zero;
    m_combat.losRecoverTime    = 0;
    ResetPassiveSpotAwareness();
    m_enemy.enemy              = NULL;
    m_enemy.eyesTag            = -1;
    m_enemy.oldPos             = vec_zero;
    m_enemy.lastPos            = vec_zero;
}

Weapon *BotController::FindWeaponWithAmmo()
{
    Weapon               *next;
    int                   n;
    int                   j;
    int                   bestrank;
    Weapon               *bestweapon;
    const Container<int>& inventory = controlledEnt->getInventory();

    n = inventory.NumObjects();

    // Search until we find the best weapon with ammo
    bestweapon = NULL;
    bestrank   = -999999;

    for (j = 1; j <= n; j++) {
        next = (Weapon *)G_GetEntity(inventory.ObjectAt(j));

        assert(next);
        if (!next->IsSubclassOfWeapon() || next->IsSubclassOfInventoryItem()) {
            continue;
        }

        if (next->GetWeaponClass() & WEAPON_CLASS_THROWABLE) {
            continue;
        }

        if (next->GetRank() < bestrank) {
            continue;
        }

        if (!next->HasAmmo(FIRE_PRIMARY)) {
            continue;
        }

        bestweapon = (Weapon *)next;
        bestrank   = bestweapon->GetRank();
    }

    return bestweapon;
}

Weapon *BotController::FindMeleeWeapon()
{
    Weapon               *next;
    int                   n;
    int                   j;
    int                   bestrank;
    Weapon               *bestweapon;
    const Container<int>& inventory = controlledEnt->getInventory();

    n = inventory.NumObjects();

    // Search until we find the best weapon with ammo
    bestweapon = NULL;
    bestrank   = -999999;

    for (j = 1; j <= n; j++) {
        next = (Weapon *)G_GetEntity(inventory.ObjectAt(j));

        assert(next);
        if (!next->IsSubclassOfWeapon() || next->IsSubclassOfInventoryItem()) {
            continue;
        }

        if (next->GetRank() < bestrank) {
            continue;
        }

        if (next->GetFireType(FIRE_SECONDARY) != FT_MELEE) {
            continue;
        }

        bestweapon = (Weapon *)next;
        bestrank   = bestweapon->GetRank();
    }

    return bestweapon;
}

void BotController::UseWeaponWithAmmo()
{
    Weapon *bestWeapon = FindWeaponWithAmmo();
    if (!bestWeapon) {
        //
        // If there is no weapon with ammo, fallback to a weapon that can melee
        //
        bestWeapon = FindMeleeWeapon();
    }

    if (!bestWeapon || bestWeapon == controlledEnt->GetActiveWeapon(WEAPON_MAIN)) {
        return;
    }

    controlledEnt->useWeapon(bestWeapon, WEAPON_MAIN);
}

void BotController::Spawned(void)
{
    m_randomSeed = ((controlledEnt ? controlledEnt->entnum : 0) + 1) * 1103515245u ^ level.inttime;
    ClearEnemy();
    ClearOverwatchAnchor("spawned", false);
    m_curious.reset();
    m_botCmd.buttons = 0;
    m_grenade.reset();
    movement.ClearMove();
    movement.ClearBannedZones();
    m_overwatch.reset();
    m_idle.reset();
    m_reaction.reset();
    m_scriptControl.reset();
    m_engagementMode = BotEngagementMode::None;
    m_tacticalMode   = BotTacticalMode::None;
    m_hazardMode     = BotHazardMode::None;

    // Added in OPM
    //  Assign a personality profile at first spawn and keep it across deaths.
    //  Profile drives per-bot aim tuning, combat timing, and weapon preference.
    if (m_bFirstSpawn) {
        m_profile = botProfileManager.PickProfile(g_bot_profile_override->string);
        rotation.SetAimParameters(
            m_profile.turnSpeed, m_profile.aimNoise, m_profile.aimOvershoot, m_profile.aimSettleSpeed
        );
        m_bFirstSpawn = false;

        if (g_bot_debug_state->integer) {
            gi.Printf("BOT %s: assigned profile '%s'\n", controlledEnt->client->pers.netname, m_profile.name.c_str());
        }
    }

    // Added in OPM
    //  Seed belief map with enemy spawn points so bots patrol toward
    //  likely spawn areas on round start.
    beliefMap.SeedFromSpawnPoints(controlledEnt);
}

void BotController::Think()
{
    usercmd_t  ucmd;
    usereyes_t eyeinfo;

    UpdateBotStates();
    GetUsercmd(&ucmd);
    GetEyeInfo(&eyeinfo);

    G_ClientThink(controlledEnt->edict, &ucmd, &eyeinfo);
}

void BotController::Killed(const Event& ev)
{
    Entity *attacker;

    ClearOverwatchAnchor("killed", false);
    ScriptReleaseControl();

    // send the respawn buttons
    if (!(m_botCmd.buttons & BUTTON_ATTACKLEFT)) {
        m_botCmd.buttons |= BUTTON_ATTACKLEFT;
    } else {
        m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
    }

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = 0;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    attacker = ev.GetEntity(1);

    // Added in OPM
    //  Record death location in belief map — persists across respawn
    beliefMap.UpdateFromDeath(controlledEnt->origin);

    if (attacker && BotRandomOneIn(5)) {
        // 1/5 chance to go back to the attacker position
        m_enemy.deathPos = attacker->origin;
    } else {
        m_enemy.deathPos = vec_zero;
    }

    // Re-apply the current profile so the next respawn loadout is profile-driven.
    ApplyProfilePrimaryWeapon(true);

    const char *userinfo = controlledEnt->client->pers.userinfo;
    if (!Info_ValueForKey(userinfo, "dm_playermodel")[0] || !Info_ValueForKey(userinfo, "dm_playergermanmodel")[0]) {
        //
        // This is useful to change nationality in Spearhead and Breakthrough
        // this allows the AI to use more weapons
        //
        Info_SetValueForKey(controlledEnt->client->pers.userinfo, "dm_playermodel", G_GetRandomAlliedPlayerModel());
        Info_SetValueForKey(
            controlledEnt->client->pers.userinfo, "dm_playergermanmodel", G_GetRandomGermanPlayerModel()
        );

        G_ClientUserinfoChanged(controlledEnt->edict, controlledEnt->client->pers.userinfo);
    }
}

// Added in OPM
//  React to being damaged - look toward attacker immediately
void BotController::Damaged(const Event& ev)
{
    Entity *attacker = ev.GetEntity(1);

    if (!attacker || attacker == controlledEnt) {
        return;
    }

    // Check if attacker is a valid enemy
    Sentient *sentAttacker = NULL;
    if (attacker->IsSubclassOfSentient()) {
        sentAttacker = static_cast<Sentient *>(attacker);

        if (!IsValidEnemy(sentAttacker)) {
            return;
        }
    }

    // Update belief map with attacker position - high confidence
    beliefMap.UpdateFromEvent(attacker->origin, AI_EVENT_WEAPON_FIRE, 1.0f);

    // If we already have this enemy targeted and can see them, don't interrupt
    if (m_enemy.enemy == sentAttacker && m_combat.lastSeenTime == level.inttime) {
        return;
    }

    m_reaction.lookPos          = attacker->centroid;
    m_reaction.lookUntil        = level.inttime + 750;
    m_reaction.moveClearReason  = BotMoveClearReason::Reaction;

    // If the attacker is a valid sentient enemy, enter attack mode
    if (sentAttacker) {
        if (g_bot_debug_reaction->integer) {
            gi.Printf(
                "BOT %s: Damaged by %s - entering attack mode, looking at (%.0f, %.0f, %.0f)\n",
                controlledEnt->client->pers.netname,
                sentAttacker->IsSubclassOfPlayer() ? static_cast<Player *>(sentAttacker)->client->pers.netname
                                                   : sentAttacker->targetname.c_str(),
                attacker->centroid.x,
                attacker->centroid.y,
                attacker->centroid.z
            );
        }

        // Set up enemy tracking
        ResetPassiveSpotAwareness();
        m_enemy.enemy   = sentAttacker;
        m_enemy.lastPos = sentAttacker->origin;
        m_enemy.eyesTag = gi.Tag_NumForName(sentAttacker->edict->tiki, "eyes bone");

        // Enter attack state - still need reaction time to aim before firing
        // Being shot tells you where the threat is, but you still need to turn and aim
        m_combat.attackTime        = level.inttime + 5000;
        m_combat.lastSeenTime      = level.inttime;
        m_combat.attackStopAimTime = level.inttime + 2000;
        StartCombatReactionDelay(); // Need time to turn and aim before firing.

        // Clear any curious state - we have a real threat now
        m_curious.time      = 0;
        m_curious.scanUntil = 0;
    } else {
        // Non-sentient attacker (e.g., explosion, trap) - go curious toward the position
        m_curious.targetPos = attacker->origin;
        m_curious.time      = level.inttime + 5000;
        m_curious.scanUntil = 0;
    }
}

void BotController::GotKill(const Event& ev)
{
    //
    // Changed in OPM
    //  Don't fully exit combat state after a kill - just clear the current
    //  enemy so RefreshAttackState can find a new target. Keep m_combat.attackTime
    //  active so the bot stays in combat mode and continues scanning for enemies.
    //
    m_enemy.enemy       = NULL;
    m_enemy.eyesTag     = -1;
    ResetPassiveSpotAwareness();
    m_curious.time      = 0;
    m_curious.scanUntil = 0;

    // Extend attack time briefly to allow scanning for new targets
    if (m_combat.attackTime) {
        m_combat.attackTime = level.inttime + 500 + (int)BotRandom(1000.0f);
    }

    if (g_bot_instamsg_chance->integer && level.inttime >= m_iNextTauntTime
        && BotRandomOneIn(g_bot_instamsg_chance->integer)) {
        //
        // Randomly play a taunt
        //
        Event event("dmmessage");

        event.AddInteger(0);

        if (g_protocol >= protocol_e::PROTOCOL_MOHTA_MIN) {
            event.AddString("*5" + str(1 + BotRandomInt(8)));
        } else {
            event.AddString("*4" + str(1 + BotRandomInt(9)));
        }

        controlledEnt->ProcessEvent(event);

        m_iNextTauntTime = level.inttime + g_bot_instamsg_delay->integer;
    }
}

void BotController::EventStuffText(const str& text)
{
    SendCommand(text);
}

void BotController::setControlledEntity(Player *player)
{
    if (controlledEnt && controlledEnt != player) {
        botManager.GetTacticalMemory().ReleaseOccupant(controlledEnt->entnum);
    }

    controlledEnt = player;
    movement.SetControlledEntity(player);
    rotation.SetControlledEntity(player);
    m_randomSeed = ((player ? player->entnum : 0) + 1) * 1103515245u;

    // Added in OPM
    //  Initialize belief map from spawn point bounds. We use spawn points
    //  rather than world->absmin/absmax because the world entity bounds
    //  don't reflect the actual playable area.
    if (!beliefMap.IsInitialized()) {
        Vector mapMins(999999, 999999, 999999);
        Vector mapMaxs(-999999, -999999, -999999);
        int    totalSpawns = 0;

        DM_Team *teams[] = {dmManager.GetTeamAllies(), dmManager.GetTeamAxis()};
        for (int t = 0; t < 2; t++) {
            for (int i = 1; i <= teams[t]->m_spawnpoints.NumObjects(); i++) {
                PlayerStart *spawn = teams[t]->m_spawnpoints.ObjectAt(i);
                Vector       pos   = spawn->origin;

                if (pos.x < mapMins.x) {
                    mapMins.x = pos.x;
                }
                if (pos.y < mapMins.y) {
                    mapMins.y = pos.y;
                }
                if (pos.z < mapMins.z) {
                    mapMins.z = pos.z;
                }
                if (pos.x > mapMaxs.x) {
                    mapMaxs.x = pos.x;
                }
                if (pos.y > mapMaxs.y) {
                    mapMaxs.y = pos.y;
                }
                if (pos.z > mapMaxs.z) {
                    mapMaxs.z = pos.z;
                }
                totalSpawns++;
            }
        }

        if (totalSpawns > 0) {
            // Pad bounds so edge spawns aren't at grid boundary
            float padding = 512.0f;
            mapMins.x -= padding;
            mapMins.y -= padding;
            mapMaxs.x += padding;
            mapMaxs.y += padding;

            beliefMap.Init(mapMins, mapMaxs, 512.0f);
        }
    }

    delegateHandle_gotKill =
        player->delegate_gotKill.Add(std::bind(&BotController::GotKill, this, std::placeholders::_1));
    delegateHandle_killed = player->delegate_killed.Add(std::bind(&BotController::Killed, this, std::placeholders::_1));
    delegateHandle_damage =
        player->delegate_damage.Add(std::bind(&BotController::Damaged, this, std::placeholders::_1));
    delegateHandle_stufftext =
        player->delegate_stufftext.Add(std::bind(&BotController::EventStuffText, this, std::placeholders::_1));
    delegateHandle_spawned = player->delegate_spawned.Add(std::bind(&BotController::Spawned, this));
}

Player *BotController::getControlledEntity() const
{
    return controlledEnt;
}

BotController *BotControllerManager::createController(Player *player)
{
    BotController *controller = new BotController();
    controller->setControlledEntity(player);

    controllers.AddObject(controller);

    return controller;
}

void BotControllerManager::removeController(BotController *controller)
{
    controllers.RemoveObject(controller);
    delete controller;
}

BotController *BotControllerManager::findController(Entity *ent)
{
    int i;

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        if (controller->getControlledEntity() == ent) {
            return controller;
        }
    }

    return nullptr;
}

const Container<BotController *>& BotControllerManager::getControllers() const
{
    return controllers;
}

BotControllerManager::~BotControllerManager()
{
    Cleanup();
}

void BotControllerManager::Init()
{
    BotController::Init();
}

void BotControllerManager::Cleanup()
{
    int i;

    BotController::Init();

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        delete controller;
    }

    controllers.FreeObjectList();
}

void BotControllerManager::ThinkControllers()
{
    int i;

    // Delete controllers that don't have associated player entity
    // This cannot happen unless some mods remove them
    for (i = controllers.NumObjects(); i > 0; i--) {
        BotController *controller = controllers.ObjectAt(i);
        if (!controller->getControlledEntity()) {
            gi.DPrintf(
                "Bot %d has no associated player entity. This shouldn't happen unless the entity has been removed by a "
                "script. The controller will be removed, please fix.\n",
                i
            );

            // Remove the controller, it will be recreated later to match `sv_numbots`
            delete controller;
            controllers.RemoveObjectAt(i);
        }
    }

    for (i = 1; i <= controllers.NumObjects(); i++) {
        BotController *controller = controllers.ObjectAt(i);
        controller->Think();
    }
}
