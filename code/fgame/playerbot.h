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

/**
 * @file playerbot.h
 * @brief Bot AI controller and core bot system definitions
 *
 * This file contains the main BotController class and related bot AI structures.
 * The bot system uses a state-based architecture with event-driven behavior,
 * including Attack, Investigate, Curious, Grenade, and Idle states.
 *
 * The system supports:
 * - State-based AI with prioritized state transitions
 * - Squad coordination with role assignment
 * - Dynamic cover evaluation and usage
 * - Debug visualization and introspection
 */
// playerbot.h: Multiplayer bot system.

#pragma once

#include "player.h"
#include "navigate.h"
#include "navigation_path.h"

// Added in OPM - Phase 2B Task 2B.4
//  Include behavior tree and profile system for integration
#include "behavior_tree.h"
#include "bot_profile.h"

// Added in OPM - Phase 3 Task 3.4 Commit 5
//  Include utility AI system for strategy selection
#include "utility_evaluator.h"

#define MAX_BOT_FUNCTIONS 5

// Added in OPM
//  Bot AI Configuration Constants for improved code readability
namespace BotConstants
{
    // Vision and Perception
    constexpr float DEFAULT_FOV_DEGREES       = 100.0f; // Field of view in degrees
    constexpr float NARROW_FOV_DEGREES        = 20.0f;  // Narrow FOV for precise checks
    constexpr float FARPLANE_VISION_FACTOR    = 0.828f; // Factor of farplane distance for vision
    constexpr float VISIBILITY_THRESHOLD      = 0.1f;   // Minimum visibility factor to consider enemy visible
    constexpr float CENTRAL_FOV_DEGREES       = 80.0f;  // Central vision field of view
    constexpr float PERIPHERAL_FOV_DEGREES    = 180.0f; // Peripheral vision field of view
    constexpr float PERIPHERAL_CLARITY_FACTOR = 0.4f;   // Peripheral vision has 40% of central clarity

    // Combat Distances (units)
    constexpr float MELEE_RANGE                 = 64.0f;  // Maximum melee attack range
    constexpr float DEFAULT_MIN_ATTACK_DISTANCE = 128.0f; // Default minimum attack distance
    constexpr float MAX_MIN_ATTACK_DISTANCE     = 256.0f; // Maximum minimum attack distance
    constexpr float CLOSE_RANGE_THRESHOLD       = 384.0f; // Threshold for close range combat
    constexpr float IDEAL_COVER_DISTANCE        = 512.0f; // Ideal distance from enemy when in cover
    constexpr float AWARENESS_RADIUS            = 512.0f; // Radius for nearby enemies/allies
    constexpr float ATTACK_RANGE_DIVISOR        = 1.25f;  // Safety factor for primary weapon range

    // Movement Distances (units)
    constexpr float WAYPOINT_REACHED_DISTANCE   = 32.0f;  // Distance to consider waypoint reached
    constexpr float STUCK_CHECK_DISTANCE        = 10.0f;  // Distance threshold for stuck detection
    constexpr float SEARCH_PATTERN_STEP         = 256.0f; // Cardinal direction search step
    constexpr float SEARCH_PATTERN_DIAGONAL     = 181.0f; // Diagonal search step (256/√2 ≈ 181)
    constexpr float SEARCH_PATTERN_EXTENDED     = 512.0f; // Extended search distance
    constexpr float ESCAPE_ROUTE_TEST_DISTANCE  = 64.0f;  // Distance to test for escape routes
    constexpr float MOVEMENT_RANDOMNESS_RANGE   = 512.0f; // Range for random movement offsets
    constexpr float FLANK_POSITION_RADIUS       = 256.0f; // Radius for flanking position checks
    constexpr float SIGNIFICANT_DISTANCE_CHANGE = 256.0f; // Significant position change threshold
    constexpr float TRACE_GROUND_CHECK          = 128.0f; // Distance to trace down for ground
    constexpr float MOVEMENT_PREDICTION_FACTOR  = 0.5f;   // Factor for movement prediction
    constexpr float OBSTACLE_AVOIDANCE_DISTANCE = 128.0f; // Distance for obstacle avoidance
    constexpr float PREFERRED_DIRECTION_FACTOR  = 512.0f; // Factor for preferred movement direction

    // Timing (milliseconds)
    constexpr int ATTACK_REACQUIRE_DELAY    = 1000; // Delay before reacquiring target
    constexpr int ATTACK_STOP_AIM_DURATION  = 3000; // How long to aim after stopping attack
    constexpr int TARGET_UNSEEN_THRESHOLD   = 2000; // Time before considering target truly lost
    constexpr int RECENT_FIRE_WINDOW        = 2000; // Window for considering recent fire
    constexpr int PATH_CHECK_INTERVAL       = 1000; // How often to check/recalculate path
    constexpr int STUCK_RECOVERY_DELAY      = 1000; // Delay for stuck recovery
    constexpr int JUMP_CHECK_INTERVAL       = 100;  // How often to check for jump opportunities
    constexpr int AIM_UPDATE_INTERVAL       = 100;  // How often to update aim offset
    constexpr int COLLISION_CHECK_INTERVAL  = 250;  // How often to check for collisions
    constexpr int ROLE_PERSISTENCE_DURATION = 5000; // How long to maintain assigned role
    constexpr int FLANK_EXECUTION_DURATION  = 3000; // How long to execute flanking maneuver
    constexpr int DAMAGE_TRACKING_WINDOW    = 2000; // Time window for tracking damage (2 seconds)
    constexpr int SECONDS_TO_MS             = 1000; // Conversion factor: seconds to milliseconds

    // Combat Behavior
    constexpr float WEAPON_SPREAD_THRESHOLD     = 0.25f;   // Max spread for accurate fire
    constexpr float AIM_OFFSET_BBOX_FACTOR      = 0.5f;    // Factor of bbox size for aim offset
    constexpr float COVER_QUALITY_EXCELLENT     = 0.7f;    // Quality threshold for good cover
    constexpr float COVER_QUALITY_BASE          = 0.5f;    // Base cover quality for obstruction
    constexpr float COVER_QUALITY_PROTECTION    = 0.3f;    // Quality bonus for protection angles
    constexpr float COVER_QUALITY_DISTANCE      = 0.2f;    // Quality bonus for ideal distance
    constexpr float COVER_QUALITY_ESCAPE        = 0.1f;    // Quality bonus for escape routes
    constexpr float COVER_DISTANCE_TOLERANCE    = 1024.0f; // Tolerance for cover distance evaluation
    constexpr float ESCAPE_ROUTE_MIN_FRACTION   = 0.5f;    // Min trace fraction for valid escape
    constexpr int   PROTECTION_ANGLE_SAMPLES    = 8;       // Number of angles to test for protection
    constexpr int   ESCAPE_ROUTE_DIRECTIONS     = 4;       // Number of directions to test for escape
    constexpr float HEALTH_RETREAT_THRESHOLD    = 0.5f;    // Health ratio to consider retreating
    constexpr float HEALTH_AGGRESSIVE_THRESHOLD = 0.7f;    // Health ratio for aggressive behavior
    constexpr float DAMAGE_RETREAT_THRESHOLD    = 30.0f;   // Damage amount to trigger retreat
    constexpr float COVER_DISTANCE_FACTOR       = 0.75f;   // Factor for cover search radius
    constexpr float REACTION_DISTANCE_MAX       = 2048.0f; // Max distance for reaction time calculation
    constexpr int   MAX_REACTION_TIME_MS        = 1000;    // Maximum reaction time in milliseconds

    // Movement Command Values
    constexpr signed char MAX_MOVE_SPEED     = 127;    // Maximum movement speed command
    constexpr float       MOVE_COMMAND_SCALE = 127.0f; // Scale factor for move commands

    // Angle Conversions
    constexpr float FULL_CIRCLE_DEGREES = 360.0f;        // Degrees in a full circle
    constexpr float DEGREES_TO_RADIANS  = M_PI / 180.0f; // Conversion factor

    // Directional Thresholds
    constexpr float FORWARD_BACKWARD_THRESHOLD = -0.75f; // Dot product threshold for forward/back
    constexpr float LATERAL_THRESHOLD          = 0.5f;   // Threshold for lateral movement

    // Utility Constants
    constexpr float EPSILON                = 0.0001f;     // Small value for float comparisons
    constexpr float TRACE_COMPLETE         = 1.0f;        // Trace fraction for complete pass-through
    constexpr float TRACE_ALMOST_COMPLETE  = 0.999f;      // Trace fraction for nearly complete
    constexpr float LARGE_DISTANCE_SQ      = 999999.0f;   // Large value for distance comparisons
    constexpr float VERY_LARGE_DISTANCE_SQ = 99999999.0f; // Very large value for initialization
    constexpr int   LARGE_NEGATIVE_RANK    = -999999;     // Large negative value for rank comparisons
    constexpr float PERCENT_CONVERSION     = 100.0f;      // Convert ratio to percentage

    // Audio Perception Constants (Phase 2 Task 2A.1.4)
    constexpr int   MAX_AUDIO_EVENTS         = 100;     // Maximum events in queue
    constexpr int   AUDIO_PRIORITY_MAX       = 2;       // Maximum priority value (high priority)
    constexpr float MAX_AUDIO_DISTANCE       = 2000.0f; // Maximum audio detection range (units)
    constexpr float AUDIO_REFERENCE_DISTANCE = 100.0f;  // Reference distance for attenuation
    constexpr float AUDIO_MIN_DISTANCE       = 1.0f;    // Minimum distance for attenuation

    // Memory System Constants (Phase 2 Task 2A.1.5)
    constexpr float MEMORY_CONFIDENCE_DECAY_RATE = 0.1f;  // 10% per second (full decay in 10s)
    constexpr float MEMORY_MIN_CONFIDENCE        = 0.1f;  // Filters memories older than 9 seconds
    constexpr float MEMORY_MAX_AGE_SECONDS       = 30.0f; // Hard cutoff prevents unbounded memory growth

    // Added in OPM - Phase 3 Task 3.1h
    //  Weapon switching thresholds
    constexpr float WEAPON_SWITCH_SCORE_THRESHOLD = 0.3f; // Score advantage needed to switch weapons

    // Added in OPM - Phase 3 Task 3.1g
    //  Grenade system constants
    constexpr float GRENADE_CLUSTER_RADIUS = 256.0f; // Max distance for enemies to be considered clustered
    constexpr float GRENADE_ALLY_SAFETY    = 384.0f; // Min distance from allies to safely throw grenade
    constexpr float GRENADE_COOLDOWN       = 10.0f;  // Seconds between grenade throws

    // Added in OPM - Phase 3 Task 3.3
    //  Idle behavior timing constants
    constexpr int   CURIOUS_INVESTIGATION_TIMEOUT = 5000;   // Curious investigation timeout (milliseconds)
    constexpr int   WAYPOINT_PAUSE_MIN            = 1000;   // Minimum pause at waypoint (milliseconds)
    constexpr int   WAYPOINT_PAUSE_MAX            = 3000;   // Maximum pause at waypoint (milliseconds)
    constexpr int   WANDER_PAUSE_MIN              = 2000;   // Minimum pause after wander (milliseconds)
    constexpr int   WANDER_PAUSE_MAX              = 5000;   // Maximum pause after wander (milliseconds)
    constexpr float WANDER_DISTANCE_MIN           = 256.0f; // Minimum wander distance (units)
    constexpr float WANDER_DISTANCE_MAX           = 768.0f; // Maximum wander distance (units)
    constexpr int   ATTRACTIVE_NODE_USE_MIN       = 10000;  // Minimum time at attractive node (milliseconds)
    constexpr int   ATTRACTIVE_NODE_USE_MAX       = 15000;  // Maximum time at attractive node (milliseconds)
    constexpr int   IDLE_LOOK_INTERVAL_MIN        = 3000;   // Minimum idle look interval (milliseconds)
    constexpr int   IDLE_LOOK_INTERVAL_MAX        = 6000;   // Maximum idle look interval (milliseconds)
    constexpr float CURIOUS_LOOK_DURATION         = 0.5f;   // Look duration per direction (seconds)
    constexpr int   CURIOUS_LOOK_DIRECTIONS       = 2;      // Number of directions to look (left/right)
    constexpr int   WANDER_TIMEOUT                = 10000;  // Wander movement timeout (milliseconds)
} // namespace BotConstants

typedef struct nodeAttract_s {
    float             m_fRespawnTime;
    AttractiveNodePtr m_pNode;
} nodeAttract_t;

class BotController;

class BotMovement
{
public:
    BotMovement();
    ~BotMovement();

    void SetControlledEntity(Player *newEntity);

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
    bool MoveToBestAttractivePoint(int iMinPriority = 0);

    bool CanMoveTo(Vector vPos);
    bool MoveDone();
    bool IsMoving(void);
    void ClearMove(void);

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

    Vector m_vCurrentOrigin;
    Vector m_vTargetPos;
    Vector m_vCurrentGoal;
    Vector m_vCurrentDir;
    Vector m_vLastCheckPos[2];
    float  m_fAttractTime;
    int    m_iTempAwayTime;
    int    m_iNumBlocks;
    int    m_iCheckPathTime;
    int    m_iLastBlockTime;
    int    m_iTempAwayState;
    bool   m_bPathing;

    ///
    /// Collision detection
    ///

    bool   m_bAvoidCollision;
    int    m_iCollisionCheckTime;
    Vector m_vTempCollisionAvoidance;

    ///
    /// Jump detection
    ///

    bool   m_bJump;
    int    m_iJumpCheckTime;
    Vector m_vJumpLocation;
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

private:
    SafePtr<Player> controlledEntity;

    Vector m_vTargetAng;
    Vector m_vCurrentAng;
    Vector m_vAngDelta;
    Vector m_vAngSpeed;
};

class BotState
{
public:
    virtual bool CheckCondition() const = 0;
    virtual void Begin()                = 0;
    virtual void End()                  = 0;
    virtual void Think()                = 0;
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

    // Changed in OPM
    //  Added in-class member initializers for safer default initialization
    struct EnemyMemory {
        SafePtr<Sentient> enemy                = nullptr;
        Vector            lastKnownPosition    = vec_zero;
        Vector            lastKnownVelocity    = vec_zero;
        float             lastSeenTime         = 0.0f;
        float             confidenceLevel      = 0.0f;
        bool              investigationStarted = false;
        int               searchAttempts       = 0;
    };

    struct CoverPoint {
        Vector position        = vec_zero;
        float  quality         = 0.0f; // 0.0-1.0 rating
        float  protectionAngle = 0.0f; // Angle of protection from enemy
        float  distanceToEnemy = 0.0f;
        bool   hasEscapeRoute  = false;
        int    evaluatedTime   = 0; // When this cover was evaluated
    };

    enum CoverState {
        COVER_NONE,
        COVER_MOVING_TO,
        COVER_IN_COVER,
        COVER_PEEKING,
        COVER_REPOSITIONING
    };

    enum FireMode {
        FIRE_ACCURATE,    // Aimed shots, low spread required
        FIRE_BURST,       // Short controlled bursts
        FIRE_SUPPRESSION, // Sustained fire at area, not specific target
        FIRE_MELEE        // Close combat
    };

    enum CombatProfile {
        AGGRESSIVE, // Push forward, sustained fire
        CAUTIOUS,   // Use cover, controlled bursts
        DEFENSIVE,  // Hold position, suppression
        RETREATING  // Fall back, covering fire
    };

    enum SquadRole {
        ROLE_NONE,
        ROLE_AGGRESSOR, // Direct assault
        ROLE_FLANKER,   // Circle to enemy sides
        ROLE_SUPPORT,   // Cover fire, hold position
        ROLE_DEFENDER   // Protect objective/teammate
    };

    // Changed in OPM
    //  Added in-class member initializers for safer default initialization
    struct SquadInfo {
        Container<BotController *> members;                // Bots within coordination range
        SafePtr<Sentient>          sharedTarget = nullptr; // Current squad target
        Vector                     rallyPoint   = vec_zero;
        int                        lastUpdate   = 0; // Last update time
    };

    // Added in OPM
    //  State data structures for improved organization
    //  Changed in OPM: Added in-class member initializers for safer default initialization
    struct CombatState {
        float recentDamage      = 0.0f;  // Accumulated damage in recent time window
        int   damageWindowStart = 0;     // Timestamp when damage tracking window started
        float burstDuration     = 1.0f;  // Duration of current burst firing
        float burstDelay        = 0.5f;  // Delay between bursts
        bool  requireLowSpread  = false; // Whether current fire mode requires low weapon spread
        bool  ammoLow           = false; // Whether bot is low on ammunition
    };

    struct CoverStateData {
        CoverPoint current;                    // Current cover point being used
        CoverState state         = COVER_NONE; // Current cover state (COVER_NONE, COVER_MOVING_TO, etc.)
        int        nextPeekTime  = 0;          // When bot should peek from cover next
        int        peekStartTime = 0;          // When current peek started
        float      peekDuration  = 0.0f;       // How long to peek for
    };

    struct MemoryState {
        EnemyMemory enemyMemory;                     // Memory of last seen enemy
        int         investigateStartTime = 0;        // When investigation state started
        int         investigateEventTime = 0;        // When high-priority sound was heard
        Vector      investigateEventPos  = vec_zero; // Location of sound event being investigated
        int         currentEventPriority = 0;        // Priority level (0=none, 1=curious, 2=investigate)
    };

    struct SquadState {
        SquadInfo squad;                           // Squad information (members, shared target, etc.)
        SquadRole role                = ROLE_NONE; // Bot's role in the squad
        int       lastSquadUpdateTime = 0;         // Last time squad awareness was updated
        int       roleAssignmentTime  = 0;         // When current role was assigned
        Vector    flankPosition       = vec_zero;  // Target position for flanking maneuver
        bool      flankPositionValid  = false;     // Whether flank position is valid
    };

private:
    static botfunc_t botfuncs[];

    BotMovement movement;
    BotRotation rotation;

    // States
    int    m_iCuriousTime;
    int    m_iAttackTime;
    int    m_iAttackStopAimTime;
    int    m_iLastBurstTime;
    int    m_iLastSeenTime;
    int    m_iLastUnseenTime;
    int    m_iContinuousFireTime;
    Vector m_vAimOffset;
    int    m_iLastAimTime;
    int    m_iStateEntryTime[MAX_BOT_FUNCTIONS]; // Track when each state was entered (for minimum duration)
    int    m_iTargetLockTime;                    // Track when current target was acquired (for target stickiness)

    Vector            m_vLastCuriousPos;
    Vector            m_vNewCuriousPos;
    Vector            m_vOldEnemyPos;
    Vector            m_vLastEnemyPos;
    Vector            m_vLastDeathPos;
    SafePtr<Sentient> m_pEnemy;
    int               m_iEnemyEyesTag;

    // Changed in OPM
    //  Refactored state variables into logical structs for improved organization
    CombatState    combatState;
    CoverStateData coverState;
    MemoryState    memoryState;
    SquadState     squadState;

    int m_iLastCoverSearchTime;

    // Tactical combat system
    FireMode      m_fireMode;
    CombatProfile m_combatProfile;
    int           m_iSuppressionEndTime;

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
    void        State_Curious(void);

    static void InitState_Attack(botfunc_t *func);
    bool        CheckCondition_Attack(void);
    void        State_EndAttack(void);
    void        State_Attack(void);

    /**
     * @brief Check if a sentient entity is a valid enemy target
     *
     * Validates target based on:
     * - Not self
     * - Not hidden or flagged NOTARGET
     * - Not dead
     * - Solid entity
     * - Different team (in team modes)
     *
     * @param sent The sentient entity to validate
     * @return true if valid enemy, false otherwise
     */
    bool IsValidEnemy(Sentient *sent) const;

    // Added in OPM
    //  Extracted functions from State_Attack for improved readability
    bool      ValidateAttackPreconditions(void);
    Sentient *SelectBestTarget(float maxDistance, float& outDistanceSq);
    Sentient *SelectBestTarget(const Container<Sentient *>& sentients, float maxDistance, float& outDistanceSq);
    void      AimAtTarget(bool canSee);
    void      HandleMeleeAttack(bool canSee, float distanceSq, float secondaryRangeSq, Weapon *weapon, bool& outMelee);
    void      HandleBurstControl(bool firing, int fireDelay, int maxContinuousFireTime, int maxBurstTime);
    void      HandleWeaponFiring(
             bool    canSee,
             float   distanceSq,
             float   primaryRangeSq,
             float   secondaryRangeSq,
             Weapon *weapon,
             bool&   outNoMove,
             bool&   outFiring,
             bool&   outMelee
         );
    float ExecuteFiring(bool canSee, float distanceSq, bool& outNoMove, bool& outFiring, bool& outMelee);
    void  UpdateAttackMovement(bool noMove, bool melee, bool canSee, float minDistanceSq);

    static void InitState_Investigate(botfunc_t *func);
    bool        CheckCondition_Investigate(void);
    void        State_EndInvestigate(void);
    void        State_Investigate(void);
    Vector      CalculateSearchPosition(void);

    // Cover system
    CoverPoint FindBestCover(Vector enemyPos);
    float      EvaluateCoverQuality(Vector pos, Vector enemyPos);
    bool       IsInCover(Vector pos, Vector enemyPos);
    bool       IsCoverCompromised(void);
    void       UpdateCoverBehavior(void);

    // Tactical combat system
    void          UpdateTacticalCombat(void);
    void          UpdateSuppressionFire(void);
    void          ExecuteRetreat(void);
    void          CalculateBurstTiming(void);
    void          CheckAmmoConservation(void);
    CombatProfile DetermineCombatProfile(void);
    int           CountEnemiesInRadius(float radius);
    int           CountAlliesInRadius(float radius);
    void          SetFireMode(FireMode mode);

    // Squad coordination system
    void      UpdateSquadAwareness(void);
    SquadRole AssignSquadRole(void);
    void      ExecuteFlankingManeuver(void);
    void      ShareEnemyInformation(void);
    void      ReceiveEnemyInfo(Sentient *enemy, Vector position);
    void      CoordinateAttack(void);
    void      CheckStaggeredEngagement(void);
    int       CountAlliesNearPosition(Vector pos, float radius);
    SquadRole GetSquadRole(void) const;
    bool      HasEnemy(void) const;
    Sentient *GetEnemy(void) const;

    // Added in OPM - Phase 2B Task 2B.4
    //  Behavior tree execution helpers
    void PopulateBlackboard(void);
    void ExecuteBehaviorTree(float deltaTime);

    // Added in OPM - Phase 3 Task 3.4 Commit 5
    //  Utility AI strategy evaluation
    void EvaluateStrategy(float deltaTime);
    void SwitchStrategy(const std::string& strategyName, const std::string& treeFile);

    static void InitState_Grenade(botfunc_t *func);
    bool        CheckCondition_Grenade(void);
    void        State_Grenade(void);

    static void InitState_Weapon(botfunc_t *func);
    bool        CheckCondition_Weapon(void);
    void        State_BeginWeapon(void);

    void CheckStates(void);
    bool CanExitState(int stateIndex);

public:
    CLASS_PROTOTYPE(BotController);

    BotController();
    ~BotController();

    static void Init(void);

    /**
     * @brief Get bot's eye information for view rendering
     *
     * @param eyeinfo Pointer to usereyes_t structure to fill
     */
    void GetEyeInfo(usereyes_t *eyeinfo);

    /**
     * @brief Get bot's user command for this frame
     *
     * @param ucmd Pointer to usercmd_t structure to fill
     */
    void GetUsercmd(usercmd_t *ucmd);

    /**
     * @brief Update all bot state machines and check state transitions
     *
     * Called each frame to update bot behavior states.
     */
    void UpdateBotStates(void);

    /**
     * @brief Check if bot needs to reload and trigger reload if safe
     */
    void CheckReload(void);

    /**
     * @brief Make the bot face toward the current path direction
     */
    void AimAtAimNode(void);

    /**
     * @brief Track damage taken for tactical retreat decisions
     * 
     * Updates recentDamage accumulator and tracks damage in 2-second window.
     * Called automatically when bot takes damage.
     * 
     * @param damage Amount of damage taken
     */
    void TrackDamage(float damage);

    /**
     * @brief Check if bot should retreat based on tactical situation
     * 
     * Returns true if:
     * - Health < retreat threshold (default 25%)
     * - Recent damage > damage threshold (default 30)
     * - Outnumbered (3+ enemies)
     * 
     * @return true if bot should retreat
     */
    bool ShouldRetreat(void);

    /**
     * @brief Notify bot of a game event (sound, visual, etc.)
     *
     * @param vPos Position of the event
     * @param iType Event type (AI_EVENT_*)
     * @param pEnt Entity that caused the event
     * @param fDistanceSquared Squared distance from bot to event
     * @param fRadiusSquared Squared radius of event influence
     */
    void NoticeEvent(const Vector& vPos, int iType, Entity *pEnt, float fDistanceSquared, float fRadiusSquared);

    /**
     * @brief Get priority level for event type
     *
     * @param eventType Event type (AI_EVENT_*)
     * @return 0=none, 1=low priority (Curious state), 2=high priority (Investigation state)
     */
    int GetEventPriority(int eventType) const;

    /**
     * @brief Clear the bot's current enemy target
     */
    void ClearEnemy(void);

    /**
     * @brief Send a console command for the bot to execute
     *
     * @param text Command string to execute
     */
    void SendCommand(const char *text);

    /**
     * @brief Main bot think function, called each frame
     */
    void Think();

    void Spawned(void);

    void Killed(const Event& ev);
    void GotKill(const Event& ev);
    void EventStuffText(const str& text);

    BotMovement& GetMovement();
    BotRotation& GetRotation();

    // Added in OPM - Phase 2B Task 2B.2
    //  Helper methods for behavior tree actions
    void SetEnemy(Sentient *enemy) { m_pEnemy = enemy; }

    void PressFireButton() { m_botCmd.buttons |= BUTTON_ATTACKLEFT; }

    usercmd_t& GetBotCmd() { return m_botCmd; }

    // Added in OPM - Phase 2B Task 2B.4
    //  Profile and behavior tree integration
    void LoadProfile(const char *profileName);
    void ReloadProfile();

    BehaviorTree *GetBehaviorTree() { return behaviorTree.get(); }

    Blackboard& GetBlackboard() { return blackboard; }

    BotProfile *GetProfile() { return profile.get(); }

    // Added in OPM
    //  Debug visualization and introspection methods

    /**
     * @brief Print detailed debug information about this bot
     *
     * Outputs to console:
     * - Current state and timers
     * - Enemy/target information
     * - Weapon and ammo status
     * - Movement goals
     * - Squad information
     */
    void PrintDebugInfo(void);

    /**
     * @brief Force bot into a specific state for testing
     *
     * @param stateIndex State to force: 0=Attack, 1=Investigate, 2=Curious, 3=Grenade, 4=Idle
     */
    void ForceState(int stateIndex);

    /**
     * @brief Toggle debug perception visualization for this bot
     *
     * Enables/disables visual debug overlays including:
     * - Path visualization (goal, direction)
     * - Enemy tracking (visible/remembered)
     * - State information
     * - Perception indicators (FOV, audio radius)
     */
    void TogglePerceptionVisualization(void);

    /**
     * @brief Draw debug visualization for the bot (called each frame)
     *
     * Renders debug overlays when visualization is enabled.
     * Called automatically by the bot system.
     */
    void DrawDebugVisualization(void);

    // Debug visualization flags
    bool m_bShowPerception;
    bool m_bShowPath;
    bool m_bShowEnemy;
    bool m_bShowState;

    // Added in OPM - Phase 3 Task 3.4 Commit 3
    //  Public accessors for utility AI system

    /**
     * @brief Check if given position provides cover from enemy position
     *
     * @param pos Position to check for cover
     * @param enemyPos Enemy position to check cover against
     * @return true if position is in cover from enemy
     */
    bool CheckCover(Vector pos, Vector enemyPos) { return IsInCover(pos, enemyPos); }

public:
    /**
     * @brief Set the player entity controlled by this bot
     *
     * @param player The player entity to control
     */
    void setControlledEntity(Player *player);

    /**
     * @brief Get the player entity controlled by this bot
     *
     * @return Pointer to the controlled Player entity
     */
    Player *getControlledEntity() const;

    // Added in OPM - Phase 3 Task 3.3
    //  Public access for behavior tree actions
    Container<PathNode *> m_patrolRoute; // Waypoints for patrol behavior

    // Added in OPM - Phase 3 Task 3.4 Commit 6
    //  Public accessors for utility AI debug command
    const std::string& GetCurrentStrategy() const { return currentStrategy; }

private:
    SafePtr<Player> controlledEnt;

    // Added in OPM - Phase 2B Task 2B.4
    //  Behavior tree system integration
    std::unique_ptr<BotProfile>   profile;      // Bot personality profile
    std::unique_ptr<BehaviorTree> behaviorTree; // Current behavior tree
    Blackboard                    blackboard;   // Shared data for BT nodes

    // Added in OPM - Phase 3 Task 3.4 Commit 5
    //  Utility AI system integration
    UtilityEvaluator utilityEvaluator;    // Utility-based action selection
    std::string      currentStrategy;     // Currently selected strategy name
    float            strategyChangeTimer; // Time until next strategy evaluation
    float            lastStrategyScore;   // Score of current strategy (for hysteresis)
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

    void Init();
    void Cleanup();
    void Frame();
    void BroadcastEvent(Entity *originator, Vector origin, int iType, float radius);

private:
    BotControllerManager botControllerManager;
};

extern BotManager botManager;
