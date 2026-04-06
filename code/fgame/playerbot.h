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
// playerbot.h: Multiplayer bot system.

#pragma once

#include "player.h"
#include "navigate.h"
#include "navigation_path.h"
#include "playerbot_beliefs.h"

// Added in OPM
//  Per-bot behavioral parameters. Initialized from cvars at spawn,
//  later modulated by personality traits.
struct BotParams {
    // Aim dynamics
    float turnSpeed;
    float aimOvershoot;
    float aimSettleSpeed;
    float aimNoise;
    float aimLerpSpeed;

    // Attack behavior
    float attackReactMinDelay;
    float attackReactRandomDelay;
    float attackBurstMinTime;
    float attackBurstRandomDelay;
    float attackContinuousFireMinTime;
    float attackContinuousFireRandomTime;
    float attackSpreadMult;
    int   crouchChance;

    // Combat positioning
    float standStillDistance; // Distance above which bot stops to shoot
    float engageDistanceMin;  // Minimum distance to maintain from enemies
    int   strafeChance;       // Chance (0-100) to strafe while firing

    // Grenade avoidance
    float grenadeAvoidRadius;

    // Belief map / patrol
    float beliefDecay;
    float beliefEventWeight;
    float beliefVisitPenalty;
    float beliefNoveltyBonus;
    float beliefScoreJitter;
    float beliefVisitDecay;
    float beliefPathBlockTime;
    // Added in OPM
    //  Bonus score per visible high-belief zone when selecting patrol target.
    float beliefOverwatchBonus;

    // Movement / stuck detection
    float progressStallTime;

    // Idle behavior pacing
    int   idlePauseChance;     // 1-in-N chance per frame to start a pause (lower = more frequent)
    float idlePauseMinTime;    // Minimum pause duration (seconds)
    float idlePauseRandomTime; // Additional random pause time (seconds)
    int   idleWalkChance;      // 1-in-N chance after a pause to start walking (lower = more frequent)
    float idleWalkMinTime;     // Minimum walk duration (seconds)
    float idleWalkRandomTime;  // Additional random walk time (seconds)

    // Death behavior
    int revengeChance; // 0-100 probability to hunt killer after death

    // Vision
    float visionDistance; // Max enemy detection range (units)

    // Sniper overwatch: hold position after a kill to watch for more targets
    float sniperOverwatchMin;    // Minimum overwatch duration (seconds)
    float sniperOverwatchRandom; // Additional random overwatch time (seconds)

    // Sniper behavior
    float scopedAimScale;   // Aim noise/spread multiplier when zoomed (< 1 = tighter)
    float scopeSettleDelay; // Seconds to hold fire after scoping in for first shot

    // Taunts
    int   instamsgChance;
    float instamsgDelay;

    void InitFromCvars();
    void ApplyPersonality(const struct BotPersonality& personality);
};

// Added in OPM
//  Bot personality: a set of behavioral traits assigned once at spawn.
//  Traits are floats in 0.0-1.0 range that modulate BotParams after
//  initialization from cvars. Each bot draws a personality randomly
//  from a preset pool and keeps it for the entire map.
struct BotPersonality {
    const char *name;

    // Behavioral traits (0.0 to 1.0)
    float accuracy;   // Higher = less aim noise, less spread, faster settle
    float aggression; // Higher = faster reactions, pushes toward enemies
    float patience;   // Higher = longer idle pauses, camps more
    float stealth;    // Higher = walks more, crouches more
    float strafing;   // Higher = more lateral movement during combat

    // Weapon and appearance
    int         preferredWeaponClass; // WEAPON_CLASS_* bitmask, 0 = no preference
    const char *alliedModel;          // Substring to match in model list, NULL = random
    const char *germanModel;          // Substring to match in model list, NULL = random

    // Selection weight (higher = more likely to be picked)
    int weight;
};

extern const BotPersonality botPersonalityPool[];
extern const int            botPersonalityPoolSize;

const BotPersonality& G_GetRandomBotPersonality();

#define MAX_BOT_FUNCTIONS 5

class BotController;

// Added in OPM
//  Grouped state structs to prevent partial-reset bugs

/**
 * @brief Tracks temporary "back away" behavior when the bot is blocked.
 *
 * When the bot gets stuck, it enters a temporary state where it backs away
 * from obstacles before re-pathfinding. This struct groups all related state
 * so it can be reset atomically.
 */
struct BotBlockedState {
    int state;     // 0 = normal, 1 = detected block, 2 = backing away
    int time;      // When current state began
    int lastTime;  // When block was first detected
    int numBlocks; // How many times we've been blocked (gives up after 5)

    void reset()
    {
        state     = 0;
        time      = 0;
        lastTime  = 0;
        numBlocks = 0;
    }
};

/**
 * @brief Collision avoidance state for navigating around obstacles.
 *
 * When the bot detects an obstacle in front, it calculates an avoidance
 * position to the left or right. This struct groups the avoidance state.
 */
struct BotCollisionState {
    bool   active;       // Currently avoiding collision
    int    checkTime;    // Last time we checked for collisions
    Vector avoidancePos; // Position to move to for avoidance

    void reset()
    {
        active       = false;
        checkTime    = 0;
        avoidancePos = vec_zero;
    }
};

/**
 * @brief Jump detection state for obstacle traversal.
 *
 * Tracks whether the bot needs to jump and validates the jump is making progress.
 */
struct BotJumpState {
    bool   active;    // Currently trying to jump
    int    checkTime; // When jump was initiated
    Vector startPos;  // Position when jump started (to detect progress)

    void reset()
    {
        active    = false;
        checkTime = 0;
        startPos  = vec_zero;
    }
};

// Added in OPM
/**
 * @brief Progress tracking state for detecting oscillating/stuck bots.
 *
 * Tracks whether the bot is making progress toward its destination.
 * If the bot hasn't gotten closer for a configurable time, it's considered stuck.
 */
struct BotProgressState {
    Vector targetPos;    // The destination we're tracking progress toward
    int    startTime;    // When we started trying to reach this destination
    float  bestDist;     // Closest squared distance we've achieved
    int    lastProgress; // Last time we made progress (got closer)

    void reset()
    {
        targetPos    = vec_zero;
        startTime    = 0;
        bestDist     = 999999.0f;
        lastProgress = 0;
    }
};

class BotMovement
{
public:
    BotMovement();
    ~BotMovement();

    void SetControlledEntity(Player *newEntity, const BotParams *params);

    void MoveThink(usercmd_t& botcmd);

    void AvoidPath(
        Vector vPos,
        float  fAvoidRadius,
        Vector vPreferredDir = vec_zero,
        float *vLeashHome    = NULL,
        float  fLeashRadius  = 0.0f
    );
    void MoveNear(Vector vNear, float fRadius, float *vLeashHome = NULL, float fLeashRadius = 0.0f);
    void MoveTo(Vector vPos, float *vLeashHome = NULL, float fLeashRadius = 0.0f);

    bool   CanMoveTo(Vector vPos) const;
    bool   MoveDone() const;
    bool   IsMoving() const;
    void   ClearMove();
    Vector GetCurrentGoal() const;
    Vector GetCurrentPathDirection() const;

    // Added in OPM
    //  Path blocking: when the bot gives up trying to reach a destination,
    //  return the blocked position so the belief map can mark it unreachable
    bool   DidGiveUpPath() const;
    Vector GetBlockedDestination() const;

private:
    Vector CalculateDir(const Vector& delta) const;
    Vector CalculateRelativeWishDirection(const Vector& dir) const;
    void   CheckEndPos(Entity *entity);
    void   CheckJump(usercmd_t& botcmd);
    void   CheckJumpOverEdge(usercmd_t& botcmd);
    void   NewMove();
    Vector FixDeltaFromCollision(const Vector& delta);
    void   CalculateBestFrontAvoidance(
          const Vector& targetOrg,
          float         maxDist,
          const Vector& forward,
          const Vector& right,
          float&        bestFrac,
          Vector&       bestPos
      );

private:
    SafePtr<Player>            controlledEntity;
    IPather                   *m_pPath;
    int                        m_iLastMoveTime;

    // Core movement state
    Vector m_vCurrentOrigin;
    Vector m_vTargetPos;
    Vector m_vCurrentGoal;
    Vector m_vCurrentDir;
    Vector m_vLastCheckPos[2];
    int    m_iCheckPathTime;
    bool   m_bPathing;

    // Grouped state structs (prevents partial-reset bugs)
    BotBlockedState   m_blocked;
    BotCollisionState m_collision;
    BotJumpState      m_jump;
    BotProgressState  m_progress;

    // Added in OPM
    //  Path blocking: track when we give up on a destination
    bool   m_bGaveUpPath;
    Vector m_vBlockedDest;

    const BotParams *m_pParams;
};

class BotRotation
{
public:
    BotRotation();

    void SetControlledEntity(Player *newEntity, const BotParams *params);

    void          TurnThink(usercmd_t& botcmd, usereyes_t& eyeinfo);
    const Vector& GetTargetAngles() const;
    void          SetTargetAngles(Vector vAngles);
    void          AimAt(Vector vPos);

private:
    SafePtr<Player> controlledEntity;

    Vector m_vTargetAng;
    Vector m_vCurrentAng;
    Vector m_vAngDelta;
    Vector m_vAngSpeed;

    // Added in OPM
    //  Aim dynamics: overshoot and settle model
    Vector m_vPrevTargetAng;  // Previous target to detect new aim requests
    bool   m_bOvershootPhase; // Currently in overshoot phase
    float  m_fSettleFrac;     // 0..1 progress through settle phase
    float  m_fOvershootYaw;   // Overshoot amount applied to yaw
    float  m_fOvershootPitch; // Overshoot amount applied to pitch

    const BotParams *m_pParams;
};

class BotState
{
public:
    virtual ~BotState() {}

    virtual const char *GetName() const { return "Unknown"; }

    virtual bool CheckCondition() = 0;

    virtual void Begin() {}

    virtual void End() {}

    virtual void Think() = 0;
};

// Added in OPM
//  Grouped state structs for BotController to prevent partial-reset bugs

/**
 * @brief Combat/attack state including aiming and firing behavior.
 */
struct BotCombatState {
    int    attackTime;           // When attack state should expire
    int    attackStopAimTime;    // When to stop aiming at last known position
    int    lastBurstTime;        // When last burst fire pause started
    int    lastSeenTime;         // When enemy was last seen
    int    lastUnseenTime;       // When enemy became unseen
    int    continuousFireTime;   // How long we've been firing continuously
    int    lastWeaponSwitchTime; // When last weapon switch was attempted
    Vector aimOffset;            // Current aim offset from target center
    Vector aimOffsetTarget;      // Target aim offset (lerped toward)
    int    lastAimTime;          // Last time aim offset was updated
    int    aimLerpStartTime;     // When aim lerp started
    int    strafeTime;           // When to change strafe direction
    int    strafeDir;            // Current strafe direction
    bool   standingStill;        // Standing still to aim
    bool   crouching;            // Currently crouching in combat
    bool   crouchDecided;        // Whether crouch decision was made
    int    overwatchUntil;       // Hold position after kill until this time (sniper overwatch)
    int    scopeInTime;          // When scope zoom-in completed (for settle delay)

    void reset()
    {
        attackTime           = 0;
        attackStopAimTime    = 0;
        lastBurstTime        = 0;
        lastSeenTime         = 0;
        lastUnseenTime       = 0;
        continuousFireTime   = 0;
        lastWeaponSwitchTime = 0;
        aimOffset            = vec_zero;
        aimOffsetTarget      = vec_zero;
        lastAimTime          = 0;
        aimLerpStartTime     = 0;
        strafeTime           = 0;
        strafeDir            = 0;
        standingStill        = false;
        crouching            = false;
        crouchDecided        = false;
        overwatchUntil       = 0;
        scopeInTime          = 0;
    }
};

/**
 * @brief Enemy tracking state.
 */
struct BotEnemyState {
    SafePtr<Sentient> enemy;
    int               eyesTag;  // Bone tag for enemy's eyes
    Vector            oldPos;   // Previous known enemy position
    Vector            lastPos;  // Last known enemy position
    Vector            deathPos; // Where enemy died (for avoidance)

    void reset()
    {
        enemy    = NULL;
        eyesTag  = -1;
        oldPos   = vec_zero;
        lastPos  = vec_zero;
        deathPos = vec_zero;
    }
};

/**
 * @brief Grenade avoidance state.
 */
struct BotGrenadeState {
    SafePtr<Entity> grenade;   // The grenade being avoided
    int             avoidTime; // When to stop avoiding

    void reset()
    {
        grenade   = NULL;
        avoidTime = 0;
    }
};

/**
 * @brief Human-like idle/movement behavior state.
 */
struct BotIdleBehavior {
    int  pauseTime; // When current idle pause ends
    int  lookTime;  // When to change look direction during pause
    int  walkTime;  // When to stop walking and run again
    int  leanTime;  // When to change lean state
    int  leanDir;   // Current lean direction: -1 left, 0 none, 1 right
    bool pausing;   // Currently in idle pause
    bool walking;   // Currently walking instead of running

    void reset()
    {
        pauseTime = 0;
        lookTime  = 0;
        walkTime  = 0;
        leanTime  = 0;
        leanDir   = 0;
        pausing   = false;
        walking   = false;
    }
};

// Added in OPM
//  Forward declarations for concrete bot state classes.
//  Defined in playerbot_states.cpp; BotController befriends them so they
//  can access its private members directly.
class BotStateIdle;
class BotStateAttack;
class BotStateGrenade;
class BotStateWeapon;

class BotController : public Listener
{
    // Added in OPM
    //  Concrete state classes are defined in playerbot_states.cpp.
    //  Friend access lets them read/write BotController internals directly
    //  without exposing everything publicly.
    friend class BotStateIdle;
    friend class BotStateAttack;
    friend class BotStateGrenade;
    friend class BotStateWeapon;

private:
    // Added in OPM
    //  OOP state instances, one per slot. Created in InitStates(), deleted in destructor.
    BotState *m_states[MAX_BOT_FUNCTIONS];

    BotParams      m_params;
    BotPersonality m_personality;
    BotMovement    movement;
    BotRotation    rotation;
    BotBeliefMap   beliefMap;

    // Grouped state structs (prevents partial-reset bugs)
    BotCombatState  m_combat;
    BotEnemyState   m_enemy;
    BotGrenadeState m_grenade;
    BotIdleBehavior m_idle;

    // Added in OPM
    //  Belief spike flag: set by NoticeEvent/Damaged when a sound passes the
    //  probability gate. Tells idle patrol to immediately re-evaluate
    //  its target instead of waiting for the current one to complete.
    //  m_iBeliefSpikeCooldown prevents flip-flopping when sounds arrive
    //  from multiple zones in quick succession — once a spike fires, the
    //  bot commits to the chosen zone for ~4 seconds before another spike
    //  can interrupt it.
    bool m_bBeliefSpiked;
    int  m_iBeliefSpikeCooldown;

    // Input
    usercmd_t  m_botCmd;
    usereyes_t m_botEyes;

    // States
    int               m_StateCount;
    unsigned int      m_StateFlags;
    ScriptThreadLabel m_RunLabel;

    // Taunts
    int m_iNextTauntTime;
    int m_iLastFireTime;

private:
    DelegateHandle delegateHandle_gotKill;
    DelegateHandle delegateHandle_killed;
    DelegateHandle delegateHandle_damage;
    DelegateHandle delegateHandle_stufftext;
    DelegateHandle delegateHandle_spawned;

private:
    Weapon *FindWeaponWithAmmo(void);
    Weapon *FindMeleeWeapon(void);
    void    UseWeaponWithAmmo(void);

    void CheckUse(void);
    bool CheckWindows(void);
    void CheckValidWeapon(void);

    void State_Reset(void);
    bool IsValidEnemy(Sentient *sent) const;

    void InitStates(void);
    void CheckStates(void);

public:
    CLASS_PROTOTYPE(BotController);

    BotController();
    ~BotController();

    static void Init(void);

    void GetEyeInfo(usereyes_t *eyeinfo);
    void GetUsercmd(usercmd_t *ucmd);

    void UpdateBotStates(void);
    void CheckReload(void);

    void AimAtAimNode(void);

    void NoticeEvent(Vector vPos, int iType, Entity *pEnt, float fDistanceSquared, float fRadiusSquared);
    void ClearEnemy(void);

    void SendCommand(const char *text);

    void Think();

    void Spawned(void);

    void Killed(const Event& ev);
    void Damaged(const Event& ev);
    void GotKill(const Event& ev);
    void EventStuffText(const str& text);

    BotMovement&          GetMovement();
    BotBeliefMap&         GetBeliefMap();
    const BotPersonality& GetPersonality() const;

    void SetPersonality(const BotPersonality& personality);
    void DrawDebugBeliefs();

public:
    void    setControlledEntity(Player *player);
    Player *getControlledEntity() const;

private:
    SafePtr<Player> controlledEnt;
};

class BotControllerManager : public Listener
{
public:
    CLASS_PROTOTYPE(BotControllerManager);

public:
    ~BotControllerManager();

    BotController                    *createController(Player *player, const BotPersonality& personality);
    void                              removeController(BotController *controller);
    BotController                    *findController(Entity *ent);
    const Container<BotController *>& getControllers() const;

    void Init();
    void Cleanup();
    void ThinkControllers();

private:
    Container<BotController *> controllers;
};

class BotManager : public Listener
{
public:
    CLASS_PROTOTYPE(BotManager);

public:
    BotControllerManager& getControllerManager();

    void Init();
    void Cleanup();
    void Frame();
    void BroadcastEvent(Entity *originator, Vector origin, int iType, float radius);

private:
    BotControllerManager botControllerManager;
};

extern BotManager botManager;
