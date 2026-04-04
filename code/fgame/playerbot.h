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

    // Grenade avoidance
    float grenadeAvoidRadius;

    // Belief map / patrol
    float beliefDecay;
    float beliefEventWeight;
    float beliefMinPatrol;
    float beliefVisitPenalty;
    float beliefNoveltyBonus;
    float beliefScoreJitter;
    float beliefVisitDecay;
    float beliefPathBlockTime;

    // Movement / stuck detection
    float progressStallTime;
    float stuckRadius;
    float stuckTime;

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

    // Weapon and appearance
    int         preferredWeaponClass; // WEAPON_CLASS_* bitmask, 0 = no preference
    const char *alliedModel;          // Substring to match in model list, NULL = random
    const char *germanModel;          // Substring to match in model list, NULL = random

    // Selection weight (higher = more likely to be picked)
    int weight;
};

extern const BotPersonality  botPersonalityPool[];
extern const int             botPersonalityPoolSize;

const BotPersonality& G_GetRandomBotPersonality();

#define MAX_BOT_FUNCTIONS 5

typedef struct nodeAttract_s {
    float             m_fRespawnTime;
    AttractiveNodePtr m_pNode;
} nodeAttract_t;

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
    bool MoveToBestAttractivePoint(const BotBeliefMap *beliefMap = NULL, int iMinPriority = 0);

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
    void   CheckAttractiveNodes();
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
    AttractiveNodePtr          m_pPrimaryAttract;
    Container<nodeAttract_t *> m_attractList;
    IPather                   *m_pPath;
    int                        m_iLastMoveTime;

    // Core movement state
    Vector m_vCurrentOrigin;
    Vector m_vTargetPos;
    Vector m_vCurrentGoal;
    Vector m_vCurrentDir;
    Vector m_vLastCheckPos[2];
    int    m_iCheckPathTime;
    float  m_fAttractTime;
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
    virtual bool CheckCondition() const = 0;
    virtual void Begin()                = 0;
    virtual void End()                  = 0;
    virtual void Think()                = 0;
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
 * @brief Curious state for investigating sounds/events.
 */
struct BotCuriousState {
    int    time;         // When curious state should expire
    int    cooldownTime; // When cooldown ends (ignore all sounds until then)
    Vector lastPos;      // Last curious position investigated
    Vector targetPos;    // Current position to investigate

    void reset()
    {
        time         = 0;
        cooldownTime = 0;
        lastPos      = vec_zero;
        targetPos    = vec_zero;
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

class BotController : public Listener
{
public:
    struct botfunc_t {
        bool (BotController::*CheckCondition)(void);
        void (BotController::*BeginState)(void);
        void (BotController::*EndState)(void);
        void (BotController::*ThinkState)(void);
    };

private:
    static botfunc_t botfuncs[];

    BotParams       m_params;
    BotPersonality  m_personality;
    BotMovement     movement;
    BotRotation  rotation;
    BotBeliefMap beliefMap;

    // Grouped state structs (prevents partial-reset bugs)
    BotCombatState  m_combat;
    BotEnemyState   m_enemy;
    BotCuriousState m_curious;
    BotGrenadeState m_grenade;
    BotIdleBehavior m_idle;

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

    void State_DefaultBegin(void);
    void State_DefaultEnd(void);
    void State_Reset(void);

    static void InitState_Idle(botfunc_t *func);
    bool        CheckCondition_Idle(void);
    void        State_Idle(void);

    static void InitState_Curious(botfunc_t *func);
    bool        CheckCondition_Curious(void);
    void        State_BeginCurious(void);
    void        State_Curious(void);

    static void InitState_Attack(botfunc_t *func);
    bool        CheckCondition_Attack(void);
    void        State_BeginAttack(void);
    void        State_EndAttack(void);
    void        State_Attack(void);
    bool        IsValidEnemy(Sentient *sent) const;

    static void InitState_Grenade(botfunc_t *func);
    bool        CheckCondition_Grenade(void);
    void        State_BeginGrenade(void);
    void        State_Grenade(void);

    static void InitState_Weapon(botfunc_t *func);
    bool        CheckCondition_Weapon(void);
    void        State_BeginWeapon(void);
    void        State_Weapon(void);

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

    BotMovement&        GetMovement();
    BotBeliefMap&       GetBeliefMap();
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
