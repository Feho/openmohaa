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
#include "playerbot_profile.h"
#include "playerbot_tactical_memory.h"

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
static constexpr int BOT_STUCK_HISTORY = 5;

struct BotStuckState {
    Vector positions[BOT_STUCK_HISTORY]; // circular buffer, one sample per second
    int    nextSlot;                     // index of the oldest sample (next to overwrite)
    int    sampleCount;                  // samples recorded so far (0..BOT_STUCK_HISTORY)
    int    checkTime;                    // level.inttime of the last sample

    void reset()
    {
        for (int i = 0; i < BOT_STUCK_HISTORY; i++) {
            positions[i] = vec_zero;
        }
        nextSlot    = 0;
        sampleCount = 0;
        checkTime   = 0;
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

static constexpr int BOT_BANNED_ZONES_MAX        = 8;
static constexpr int BOT_BANNED_ZONE_RADIUS      = 128;
static constexpr int BOT_BANNED_ZONE_DURATION_MS = 120000; // 2 minutes

struct BotBannedZone {
    Vector origin;
    int    expireTime; // level.inttime when the ban expires (0 = inactive)
};

enum class BotStuckPolicy {
    TrackAndGiveUp,
    TrackAndRecover,
    Ignore
};

class BotMovement
{
public:
    BotMovement();
    ~BotMovement();

    void SetControlledEntity(Player *newEntity);

    void MoveThink(usercmd_t& botcmd);

    bool AvoidPath(
        Vector         vPos,
        float          fAvoidRadius,
        Vector         vPreferredDir = vec_zero,
        BotStuckPolicy stuckPolicy   = BotStuckPolicy::TrackAndGiveUp,
        float         *vLeashHome    = NULL,
        float          fLeashRadius  = 0.0f
    );
    bool MoveNear(
        Vector         vNear,
        float          fRadius,
        BotStuckPolicy stuckPolicy  = BotStuckPolicy::TrackAndGiveUp,
        float         *vLeashHome   = NULL,
        float          fLeashRadius = 0.0f
    );
    bool MoveTo(
        Vector         vPos,
        BotStuckPolicy stuckPolicy  = BotStuckPolicy::TrackAndGiveUp,
        float         *vLeashHome   = NULL,
        float          fLeashRadius = 0.0f
    );
    bool MoveToBestAttractivePoint(int iMinPriority = 0);

    bool   CanMoveTo(Vector vPos);
    bool   MoveDone() const;
    bool   IsMoving() const;
    bool   WasGivenUp() const;
    bool   IsPositionBanned(const Vector& pos) const;
    void   ClearBannedZones();
    void   ClearMove();
    Vector GetCurrentGoal() const;
    Vector GetCurrentPathDirection() const;

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
    float  m_fAttractTime;
    bool   m_bPathing;
    bool   m_bGaveUp;

    // Grouped state structs (prevents partial-reset bugs)
    BotStuckState     m_stuck;
    BotCollisionState m_collision;
    BotJumpState      m_jump;

    BotBannedZone  m_bannedZones[BOT_BANNED_ZONES_MAX];
    BotStuckPolicy m_stuckPolicy;

private:
    bool IsPathSegmentBanned(const Vector& start, const Vector& end) const;
    bool PathTouchesBannedZone() const;
    void BanCurrentZone();
};

class BotRotation
{
public:
    BotRotation();

    void SetControlledEntity(Player *newEntity);

    void          TurnThink(usercmd_t& botcmd, usereyes_t& eyeinfo);
    const Vector& GetTargetAngles() const;
    void          SetTargetAngles(Vector vAngles);
    void          AimAt(Vector vPos);

    // Added in OPM
    //  Per-bot aim parameters set from BotProfile at spawn time.
    //  Constructor initializes from cvar defaults so bots without profiles
    //  behave identically to the pre-profile baseline.
    void SetAimParameters(float turnSpeed, float aimNoise, float aimOvershoot, float aimSettleSpeed);

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

    // Added in OPM
    //  Per-bot aim parameters (set from BotProfile, fall back to cvar defaults)
    float m_fTurnSpeed;
    float m_fAimNoise;
    float m_fAimOvershoot;
    float m_fAimSettleSpeed;
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
    Vector losRecoverPos;        // Cached probe result for LOS recovery move
    int    losRecoverTime;       // level.inttime when probe ran; 0 = no valid cache

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
        losRecoverPos        = vec_zero;
        losRecoverTime       = 0;
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
    int    time;               // When curious state should expire
    int    stimulusType;       // AI_EVENT_* that triggered curiosity
    float  stimulusDistanceSq; // Distance to the stimulus when it triggered
    Vector lastPos;            // Last curious position investigated
    Vector targetPos;          // Current position to investigate
    Vector losProbePos;        // Probe result bot is actually moving toward (vec_zero = none)
    int    scanUntil;          // Scan-pause timer: hold and look around until this time

    void reset()
    {
        time               = 0;
        stimulusType       = 0;
        stimulusDistanceSq = 0.0f;
        lastPos            = vec_zero;
        targetPos          = vec_zero;
        losProbePos        = vec_zero;
        scanUntil          = 0;
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
 * @brief Window overwatch state — bot holds position at a window to scan.
 */
struct BotOverwatchState {
    Vector windowPos;     // World position of the window entity's centroid
    Vector standPos;      // Where the bot should stand (close to window, with sightline)
    Vector lookDir;       // Normalized direction from standPos through the window
    Vector anchorPos;     // Logical aim anchor for scan behavior
    int    dwellUntil;    // When to give up and resume patrol
    int    scanTime;      // When to next update scan angle
    int    cooldownUntil; // Earliest time this window can be used again (anti-reentry)
    int    displacedSince;
    int    committedSince;
    int    pathFailCount;
    int    spotIndex;

    void reset()
    {
        windowPos      = vec_zero;
        standPos       = vec_zero;
        lookDir        = vec_zero;
        anchorPos      = vec_zero;
        dwellUntil     = 0;
        scanTime       = 0;
        cooldownUntil  = 0;
        displacedSince = 0;
        committedSince = 0;
        pathFailCount  = 0;
        spotIndex      = -1;
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
    // Added in OPM
    //  Patrol look-around: occasionally glance at a point of interest while moving
    Vector scanTarget;   // World-space point the bot is currently staring at (vec_zero = not staring)
    int    scanUntil;    // Timestamp when the current stare ends
    int    scanNextTime; // Timestamp when the bot may pick the next point of interest

    void reset()
    {
        pauseTime    = 0;
        lookTime     = 0;
        walkTime     = 0;
        leanTime     = 0;
        leanDir      = 0;
        pausing      = false;
        walking      = false;
        scanTarget   = vec_zero;
        scanUntil    = 0;
        scanNextTime = 0;
    }
};

enum class BotEngagementMode {
    None,
    Attack,
    Curious
};

enum class BotTacticalMode {
    None,
    Idle,
    Overwatch
};

enum class BotHazardMode {
    None,
    Grenade
};

enum class BotMoveRequestType {
    None,
    Clear,
    MoveTo,
    MoveNear,
    AvoidPath
};

enum class BotAimDirective {
    None,
    AimAtPoint,
    AimAlongPath,
    SetAngles
};

enum class BotButtonAction {
    Leave,
    Clear,
    Hold,
    Toggle
};

struct BotReactionState {
    Vector lookPos;
    int    lookUntil;
    bool   clearMove;

    void reset()
    {
        lookPos   = vec_zero;
        lookUntil = 0;
        clearMove = false;
    }
};

struct BotPerceptionSnapshot {
    bool  attackActive;
    bool  curiousActive;
    bool  grenadeActive;
    bool  overwatchActive;
    bool  idleActive;
    bool  moving;
    bool  anchorActive;
    float anchorDistSq;
    float enemyAnchorDistSq;
};

struct BotCombatIntent {
    BotEngagementMode  mode;
    BotMoveRequestType moveType;
    BotStuckPolicy     stuckPolicy;
    Vector             moveTarget;
    Vector             preferredDir;
    float              radius;
    BotAimDirective    aimType;
    Vector             aimTarget;
    Vector             aimAngles;
    BotButtonAction    attackLeft;
    BotButtonAction    attackRight;
    int                rightmove;
    int                upmove;
    int                leanDir;
    bool               clearMove;
    bool               run;
    bool               updatedLastFireTime;

    void reset()
    {
        mode                = BotEngagementMode::None;
        moveType            = BotMoveRequestType::None;
        stuckPolicy         = BotStuckPolicy::TrackAndGiveUp;
        moveTarget          = vec_zero;
        preferredDir        = vec_zero;
        radius              = 0.0f;
        aimType             = BotAimDirective::None;
        aimTarget           = vec_zero;
        aimAngles           = vec_zero;
        attackLeft          = BotButtonAction::Leave;
        attackRight         = BotButtonAction::Leave;
        rightmove           = 0;
        upmove              = 0;
        leanDir             = 0;
        clearMove           = false;
        run                 = true;
        updatedLastFireTime = false;
    }
};

struct BotHazardIntent {
    BotHazardMode      mode;
    BotMoveRequestType moveType;
    BotStuckPolicy     stuckPolicy;
    Vector             moveTarget;
    Vector             preferredDir;
    float              radius;
    bool               clearMove;

    void reset()
    {
        mode         = BotHazardMode::None;
        moveType     = BotMoveRequestType::None;
        stuckPolicy  = BotStuckPolicy::TrackAndGiveUp;
        moveTarget   = vec_zero;
        preferredDir = vec_zero;
        radius       = 0.0f;
        clearMove    = false;
    }
};

struct BotTacticalIntent {
    BotTacticalMode    mode;
    BotMoveRequestType moveType;
    BotStuckPolicy     stuckPolicy;
    Vector             moveTarget;
    Vector             preferredDir;
    float              radius;
    BotAimDirective    aimType;
    Vector             aimTarget;
    Vector             aimAngles;
    BotButtonAction    attackLeft;
    BotButtonAction    attackRight;
    bool               reload;
    bool               clearMove;
    bool               run;
    bool               anchorActive;
    bool               anchorReturning;
    bool               lockPosition; // Prevent combat from overwriting the move (at anchor with visible enemy)
    bool               updatedLastFireTime;

    void reset()
    {
        mode                = BotTacticalMode::None;
        moveType            = BotMoveRequestType::None;
        stuckPolicy         = BotStuckPolicy::TrackAndGiveUp;
        moveTarget          = vec_zero;
        preferredDir        = vec_zero;
        radius              = 0.0f;
        aimType             = BotAimDirective::None;
        aimTarget           = vec_zero;
        aimAngles           = vec_zero;
        attackLeft          = BotButtonAction::Leave;
        attackRight         = BotButtonAction::Leave;
        reload              = false;
        clearMove           = false;
        run                 = true;
        anchorActive        = false;
        anchorReturning     = false;
        lockPosition        = false;
        updatedLastFireTime = false;
    }
};

struct BotResolvedCommand {
    BotEngagementMode  engagementMode;
    BotTacticalMode    tacticalMode;
    BotHazardMode      hazardMode;
    BotMoveRequestType moveType;
    BotStuckPolicy     stuckPolicy;
    Vector             moveTarget;
    Vector             preferredDir;
    float              radius;
    BotAimDirective    aimType;
    Vector             aimTarget;
    Vector             aimAngles;
    BotButtonAction    attackLeft;
    BotButtonAction    attackRight;
    int                rightmove;
    int                upmove;
    int                leanDir;
    bool               reload;
    bool               run;
    bool               clearMove;
    bool               updatedLastFireTime;

    void reset()
    {
        engagementMode      = BotEngagementMode::None;
        tacticalMode        = BotTacticalMode::None;
        hazardMode          = BotHazardMode::None;
        moveType            = BotMoveRequestType::None;
        stuckPolicy         = BotStuckPolicy::TrackAndGiveUp;
        moveTarget          = vec_zero;
        preferredDir        = vec_zero;
        radius              = 0.0f;
        aimType             = BotAimDirective::None;
        aimTarget           = vec_zero;
        aimAngles           = vec_zero;
        attackLeft          = BotButtonAction::Leave;
        attackRight         = BotButtonAction::Leave;
        rightmove           = 0;
        upmove              = 0;
        leanDir             = 0;
        reload              = false;
        run                 = true;
        clearMove           = false;
        updatedLastFireTime = false;
    }
};

class BotController : public Listener
{
private:
    BotMovement  movement;
    BotRotation  rotation;
    BotBeliefMap beliefMap;

    // Added in OPM
    //  Personality profile assigned at first spawn; kept for the lifetime of the bot.
    BotProfile m_profile;
    bool       m_bFirstSpawn;

    // Grouped state structs (prevents partial-reset bugs)
    BotCombatState    m_combat;
    BotEnemyState     m_enemy;
    BotCuriousState   m_curious;
    BotGrenadeState   m_grenade;
    BotOverwatchState m_overwatch;
    BotIdleBehavior   m_idle;
    BotReactionState  m_reaction;

    // Input
    usercmd_t  m_botCmd;
    usereyes_t m_botEyes;

    ScriptThreadLabel m_RunLabel;

    // Taunts
    int               m_iNextTauntTime;
    int               m_iLastFireTime;
    int               m_iLastPosDebugTime;
    BotEngagementMode m_engagementMode;
    BotTacticalMode   m_tacticalMode;
    BotHazardMode     m_hazardMode;

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
    void    ApplyProfilePrimaryWeapon(bool force = false);

    void CheckUse(void);
    bool CheckWindows(Vector *outWindowPos = nullptr, Vector *outLookDir = nullptr);
    void CheckValidWeapon(void);

    bool CheckCondition_Idle(void);
    void State_Idle(void);

    bool CheckCondition_Curious(void);
    void State_BeginCurious(void);
    void State_Curious(void);

    bool CheckCondition_Attack(void);
    void State_BeginAttack(void);
    void State_EndAttack(void);
    void State_Attack(void);
    bool IsValidEnemy(Sentient *sent) const;

    bool CheckCondition_Grenade(void);
    void State_BeginGrenade(void);
    void State_Grenade(void);

    bool CheckCondition_Overwatch(void);
    void State_BeginOverwatch(void);
    void State_Overwatch(void);


    BotPerceptionSnapshot BuildPerceptionSnapshot(void);
    void                  UpdateModeTransitions(const BotPerceptionSnapshot& snapshot);
    BotCombatIntent       BuildCombatIntent(const BotPerceptionSnapshot& snapshot);
    BotHazardIntent       BuildHazardIntent(const BotPerceptionSnapshot& snapshot);
    BotTacticalIntent     BuildTacticalIntent(const BotPerceptionSnapshot& snapshot);
    BotResolvedCommand
    ResolveIntents(const BotCombatIntent& combat, const BotHazardIntent& hazard, const BotTacticalIntent& tactical);
    void        ExecuteResolvedCommand(const BotResolvedCommand& command);
    void        DebugResolvedCommand(const BotResolvedCommand& command) const;
    void        ApplyButtonAction(int buttonMask, BotButtonAction action);
    const char *GetEngagementModeName(BotEngagementMode mode) const;
    const char *GetTacticalModeName(BotTacticalMode mode) const;
    const char *GetHazardModeName(BotHazardMode mode) const;
    void        ClearOverwatchAnchor(const char *reason, bool startCooldown);
    Vector      ProbeLOSPosition(const Vector& targetPos);

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

    BotMovement&  GetMovement();
    BotBeliefMap& GetBeliefMap();

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

    BotController                    *createController(Player *player);
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
    BotTacticalMemory&    GetTacticalMemory();

    void Init();
    void Cleanup();
    void Frame();
    void BroadcastEvent(Entity *originator, Vector origin, int iType, float radius);

private:
    BotControllerManager botControllerManager;
    BotTacticalMemory    m_tacticalMemory;
    int                  m_nextTacticalRevalidateTime;
};

extern BotManager botManager;
