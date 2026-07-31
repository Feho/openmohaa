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
    Vector positions[BOT_STUCK_HISTORY] = {}; // circular buffer, one sample per second
    int    nextSlot                     = 0;  // index of the oldest sample (next to overwrite)
    int    sampleCount                  = 0;  // samples recorded so far (0..BOT_STUCK_HISTORY)
    int    checkTime                    = 0;  // level.inttime of the last sample

    void reset()
    {
        *this = BotStuckState();
    }
};

/**
 * @brief Collision avoidance state for navigating around obstacles.
 *
 * When the bot detects an obstacle in front, it calculates an avoidance
 * position to the left or right. This struct groups the avoidance state.
 */
struct BotCollisionState {
    bool   active       = false;    // Currently avoiding collision
    int    checkTime    = 0;        // Last time we checked for collisions
    int    crouchUntil  = 0;        // Try crouching briefly after getting stuck
    Vector avoidancePos = vec_zero; // Position to move to for avoidance

    void reset()
    {
        *this = BotCollisionState();
    }
};

/**
 * @brief Jump detection state for obstacle traversal.
 *
 * Tracks whether the bot needs to jump and validates the jump is making progress.
 */
struct BotJumpState {
    bool   active    = false;    // Currently trying to jump
    int    checkTime = 0;        // When jump was initiated
    Vector startPos  = vec_zero; // Position when jump started (to detect progress)

    void reset()
    {
        *this = BotJumpState();
    }
};

struct BotLadderState {
    bool   active           = false;
    bool   respawnQueued    = false;
    int    lastProgressTime = 0;
    Vector lastProgressPos  = vec_zero;

    void reset()
    {
        *this = BotLadderState();
    }
};

struct BotYieldState {
    bool   active        = false;
    int    expireTime    = 0;
    int    cooldownUntil = 0;
    Vector direction     = vec_zero;
    Vector destination   = vec_zero;

    void reset()
    {
        *this = BotYieldState();
    }
};

static constexpr int BOT_BANNED_ZONES_MAX        = 8;
static constexpr int BOT_BANNED_ZONE_RADIUS      = 96;
static constexpr int BOT_BANNED_ZONE_DURATION_MS = 20000;
static constexpr int BOT_LADDER_RESPAWN_TIME_MS  = 10000;
static constexpr int BOT_LADDER_PROGRESS_DIST    = 32;

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
    bool   ReachedMoveGoal() const;
    bool   CompletedMove() const;
    bool   IsMoving() const;
    bool   WasGivenUp() const;
    bool   IsPositionBanned(const Vector& pos) const;
    void   ClearBannedZones();
    void   ClearMove(bool completed = false);
    Vector GetCurrentGoal() const;
    Vector GetCurrentPathDirection() const;
    void   SetWaitingForLadder(FuncLadder *ladder);

private:
    Vector CalculateDir(const Vector& delta) const;
    Vector CalculateRelativeWishDirection(const Vector& dir) const;
    void   CheckAttractiveNodes();
    void   CheckEndPos(Entity *entity);
    void   CheckJump(usercmd_t& botcmd);
    void   CheckJumpOverEdge(usercmd_t& botcmd);
    void   NewMove();
    bool   CheckLadderRespawnFallback(usercmd_t& botcmd);
    bool   ApplyPlayerYield(usercmd_t& botcmd);
    bool   FindPlayerPushDirection(Vector& pushDirection) const;
    bool   FindYieldDestination(const Vector& pushDirection, Vector& destination) const;
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
    bool   m_bMoveCompleted;

    // Grouped state structs (prevents partial-reset bugs)
    BotStuckState     m_stuck;
    BotCollisionState m_collision;
    BotJumpState      m_jump;
    BotLadderState    m_ladder;
    BotYieldState     m_yield;

    BotBannedZone      m_bannedZones[BOT_BANNED_ZONES_MAX];
    BotStuckPolicy     m_stuckPolicy;
    SafePtr<FuncLadder> m_pWaitingForLadder;

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
    int    attackTime           = 0;        // When attack state should expire
    int    attackStopAimTime    = 0;        // When to stop aiming at last known position
    int    lastBurstTime        = 0;        // When last burst fire pause started
    int    lastSeenTime         = 0;        // When enemy was last seen
    int    lastUnseenTime       = 0;        // When enemy became unseen
    int    reactionReadyTime    = 0;        // Fixed sampled time when bot may fire after reacquiring an enemy
    int    continuousFireTime   = 0;        // How long we've been firing continuously
    int    lastWeaponSwitchTime = 0;        // When last weapon switch was attempted
    Vector aimOffset            = vec_zero; // Current aim offset from target center
    Vector aimOffsetTarget      = vec_zero; // Target aim offset (lerped toward)
    int    lastAimTime          = 0;        // Last time aim offset was updated
    int    aimLerpStartTime     = 0;        // When aim lerp started
    int    strafeTime           = 0;        // When to change strafe direction
    int    strafeDir            = 0;        // Current strafe direction
    bool   standingStill        = false;    // Standing still to aim
    bool   crouching            = false;    // Currently crouching in combat
    bool   crouchDecided        = false;    // Whether crouch decision was made
    Vector losRecoverPos        = vec_zero; // Cached probe result for LOS recovery move
    int    losRecoverTime       = 0;        // level.inttime when probe ran; 0 = no valid cache
    SafePtr<Sentient> spotAwarenessEnemy = NULL;
    float             spotAwareness      = 0.0f;

    void reset()
    {
        *this = BotCombatState();
    }
};

/**
 * @brief Enemy tracking state.
 */
struct BotEnemyState {
    SafePtr<Sentient> enemy    = NULL;
    int               eyesTag  = -1;       // Bone tag for enemy's eyes
    Vector            oldPos   = vec_zero; // Previous known enemy position
    Vector            lastPos  = vec_zero; // Last known enemy position
    Vector            deathPos = vec_zero; // Where enemy died (for avoidance)

    void reset()
    {
        *this = BotEnemyState();
    }
};

/**
 * @brief Curious state for investigating sounds/events.
 */
struct BotCuriousState {
    int    time               = 0;        // When curious state should expire
    int    stimulusType       = 0;        // AI_EVENT_* that triggered curiosity
    float  stimulusDistanceSq = 0.0f;     // Distance to the stimulus when it triggered
    Vector lastPos            = vec_zero; // Last curious position investigated
    Vector targetPos          = vec_zero; // Current position to investigate
    Vector losProbePos        = vec_zero; // Probe result bot is actually moving toward (vec_zero = none)
    int    scanUntil          = 0;        // Scan-pause timer: hold and look around until this time

    void reset()
    {
        *this = BotCuriousState();
    }
};

/**
 * @brief Grenade avoidance state.
 */
struct BotGrenadeState {
    SafePtr<Entity> grenade   = NULL; // The grenade being avoided
    int             avoidTime = 0;    // When to stop avoiding

    void reset()
    {
        *this = BotGrenadeState();
    }
};

/**
 * @brief Window overwatch state — bot holds position at a window to scan.
 */
struct BotOverwatchState {
    Vector windowPos      = vec_zero; // World position of the window entity's centroid
    Vector standPos       = vec_zero; // Where the bot should stand (close to window, with sightline)
    Vector lookDir        = vec_zero; // Normalized direction from standPos through the window
    Vector anchorPos      = vec_zero; // Logical aim anchor for scan behavior
    int    dwellUntil     = 0;        // When to give up and resume patrol
    int    scanTime       = 0;        // When to next update scan angle
    int    cooldownUntil  = 0;        // Earliest time this window can be used again (anti-reentry)
    int    displacedSince = 0;
    int    committedSince = 0;
    int    pathFailCount  = 0;
    int    spotIndex      = -1;

    void reset()
    {
        *this = BotOverwatchState();
    }
};

/**
 * @brief Human-like idle/movement behavior state.
 */
struct BotIdleBehavior {
    int  pauseTime = 0;    // When current idle pause ends
    int  lookTime  = 0;    // When to change look direction during pause
    int  walkTime  = 0;    // When to stop walking and run again
    int  leanTime  = 0;    // When to change lean state
    int  leanDir   = 0;    // Current lean direction: -1 left, 0 none, 1 right
    bool pausing   = false; // Currently in idle pause
    bool walking   = false; // Currently walking instead of running
    // Added in OPM
    //  Patrol look-around: occasionally glance at a point of interest while moving
    Vector scanTarget   = vec_zero; // World-space point the bot is currently staring at (vec_zero = not staring)
    int    scanUntil    = 0;        // Timestamp when the current stare ends
    int    scanNextTime = 0;        // Timestamp when the bot may pick the next point of interest

    void reset()
    {
        *this = BotIdleBehavior();
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

enum class BotMoveClearReason {
    None,
    ModeTransition,
    AttackExpired,
    CuriousExpired,
    CombatStop,
    OverwatchAnchor,
    IdlePause,
    Reaction,
    ScriptHold,
    ScriptMoveComplete
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

enum class BotScriptMoveType {
    None,
    MoveTo,
    MoveNear
};

enum class BotScriptPosture {
    None,
    Stand,
    Crouch,
    Prone
};

struct BotScriptControlState {
    bool              holdPosition    = false;
    // Set when a scripted move must latch into a hold on arrival. The hold duration only
    // starts once the bot gets there, so travel time never eats into the guard time.
    bool              holdAfterMove   = false;
    float             holdDuration    = 0.0f;
    int               holdUntil       = 0;
    BotScriptMoveType moveType        = BotScriptMoveType::None;
    Vector            moveTarget      = vec_zero;
    float             moveRadius      = 0.0f;
    bool              moveStarted     = false;
    bool              hasLookTarget   = false;
    Vector            lookTarget      = vec_zero;
    bool              hasWatchTarget  = false;
    Vector            watchTarget     = vec_zero;
    BotScriptPosture  posture         = BotScriptPosture::None;
    bool              primaryFire     = false;
    bool              secondaryFire   = false;
    bool              useButton       = false;
    bool              reloadRequested = false;

    void reset()
    {
        *this = BotScriptControlState();
    }
};

enum class BotScriptCommandStatus {
    Unknown,
    Running,
    Reached,
    Failed,
    Cancelled,
    Superseded
};

struct BotScriptCommandRecord {
    int                    id     = 0;
    BotScriptCommandStatus status = BotScriptCommandStatus::Unknown;
};

static constexpr int BOT_SCRIPT_COMMAND_HISTORY_MAX = 16;

struct BotScriptCommandState {
    BotScriptCommandRecord history[BOT_SCRIPT_COMMAND_HISTORY_MAX] = {};
    int                    historyNext                            = 0;
    int                    nextId                                 = 1;
    int                    activeId                               = 0;
};

struct BotReactionState {
    Vector             lookPos          = vec_zero;
    int                lookUntil        = 0;
    BotMoveClearReason moveClearReason  = BotMoveClearReason::None;

    void reset()
    {
        *this = BotReactionState();
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
    BotMoveClearReason moveClearReason;
    bool               run;
    bool               visibleEnemy;
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
        moveClearReason     = BotMoveClearReason::None;
        run                 = true;
        visibleEnemy        = false;
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
    BotMoveClearReason moveClearReason;

    void reset()
    {
        mode            = BotHazardMode::None;
        moveType        = BotMoveRequestType::None;
        stuckPolicy     = BotStuckPolicy::TrackAndGiveUp;
        moveTarget      = vec_zero;
        preferredDir    = vec_zero;
        radius          = 0.0f;
        moveClearReason = BotMoveClearReason::None;
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
    BotMoveClearReason moveClearReason;
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
        moveClearReason     = BotMoveClearReason::None;
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
    BotButtonAction    useButton;
    int                rightmove;
    int                upmove;
    int                leanDir;
    bool               reload;
    bool               run;
    BotMoveClearReason moveClearReason;
    bool               visibleEnemy;
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
        useButton           = BotButtonAction::Leave;
        rightmove           = 0;
        upmove              = 0;
        leanDir             = 0;
        reload              = false;
        run                 = true;
        moveClearReason     = BotMoveClearReason::None;
        visibleEnemy        = false;
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
    int                   m_iNextTauntTime;
    int                   m_iLastFireTime;
    int                   m_iLastPosDebugTime;
    int                   m_randomSeed;
    BotEngagementMode     m_engagementMode;
    BotTacticalMode       m_tacticalMode;
    BotHazardMode         m_hazardMode;
    BotScriptControlState m_scriptControl;
    BotScriptCommandState m_scriptCommands;

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

    bool IsValidEnemy(Sentient *sent) const;
    float GetVisionDistance(void) const;
    float GetHorizontalViewOffset(const Vector& pos) const;
    float GetPassiveSpotRate(Sentient *sent, float distSq, float *traceFov, bool *immediate, bool *forceLook) const;
    void  DecayPassiveSpotAwareness(void);
    bool  AdvancePassiveSpotAwareness(Sentient *sent, float rate);
    void  ResetPassiveSpotAwareness(void);

    BotMoveClearReason    RefreshPerceptionState(void);
    BotMoveClearReason    RefreshAttackState(void);
    BotMoveClearReason    RefreshCuriousState(void);
    void                  RefreshGrenadeState(void);
    void                  RefreshOverwatchState(void);
    BotPerceptionSnapshot BuildPerceptionSnapshot(void) const;
    BotMoveClearReason    UpdateModeTransitions(const BotPerceptionSnapshot& snapshot);
    BotCombatIntent       AdvanceCombatStateAndBuildIntent(const BotPerceptionSnapshot& snapshot);
    BotHazardIntent       BuildHazardIntent(const BotPerceptionSnapshot& snapshot);
    BotTacticalIntent     AdvanceTacticalStateAndBuildIntent(const BotPerceptionSnapshot& snapshot);
    BotResolvedCommand
    ResolveIntents(
        const BotCombatIntent& combat,
        const BotHazardIntent& hazard,
        const BotTacticalIntent& tactical,
        BotMoveClearReason perceptionClearReason,
        BotMoveClearReason transitionClearReason
    );
    void        ApplyScriptControl(BotResolvedCommand& command);
    void        ExecuteResolvedCommand(const BotResolvedCommand& command);
    void        DebugResolvedCommand(const BotResolvedCommand& command) const;
    void        ApplyButtonAction(int buttonMask, BotButtonAction action);
    const char *GetEngagementModeName(BotEngagementMode mode) const;
    const char *GetTacticalModeName(BotTacticalMode mode) const;
    const char *GetHazardModeName(BotHazardMode mode) const;
    const char *GetMoveClearReasonName(BotMoveClearReason reason) const;
    void        ClearOverwatchAnchor(const char *reason, bool startCooldown);
    void        StartCombatReactionDelay(void);
    Vector      ProbeLOSPosition(const Vector& targetPos);
    float       BotRandom(void);
    float       BotRandom(float n);
    float       BotCRandom(void);
    int         BotRandomInt(int upperExclusive);
    bool        BotRandomOneIn(int n);
    bool        BotRandomPercent(float percent);
    BotScriptCommandRecord       *FindScriptCommand(int commandId);
    const BotScriptCommandRecord *FindScriptCommand(int commandId) const;
    int                           BeginScriptMoveCommand(void);
    void                          FinishActiveScriptCommand(BotScriptCommandStatus status);

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

    void BeginLatchedHold(BotResolvedCommand& command);

    void ScriptHoldPosition(bool enabled);
    int  ScriptHoldPositionAt(const Vector& target, float duration, float radius);
    void ScriptStop(void);
    void ScriptSetPosture(BotScriptPosture posture, bool enabled);
    int  ScriptMoveTo(const Vector& target);
    int  ScriptMoveNear(const Vector& target, float radius);
    void ScriptLookAt(const Vector& target);
    void ScriptClearLook(void);
    void ScriptWatchAt(const Vector& target);
    void ScriptClearWatch(void);
    void ScriptPrimaryFire(bool enabled);
    void ScriptSecondaryFire(bool enabled);
    void ScriptUse(bool enabled);
    void ScriptReload(void);
    void ScriptReleaseControl(void);
    bool ScriptControlsUse(void) const;
    const char *ScriptCommandStatus(int commandId) const;

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
