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

#define MAX_BOT_FUNCTIONS 5

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

    struct EnemyMemory {
        SafePtr<Sentient> enemy;
        Vector            lastKnownPosition;
        Vector            lastKnownVelocity;
        float             lastSeenTime;
        float             confidenceLevel;
        bool              investigationStarted;
        int               searchAttempts;
    };

    struct CoverPoint {
        Vector position;
        float  quality;           // 0.0-1.0 rating
        float  protectionAngle;   // Angle of protection from enemy
        float  distanceToEnemy;
        bool   hasEscapeRoute;
        int    evaluatedTime;     // When this cover was evaluated
    };

    enum CoverState {
        COVER_NONE,
        COVER_MOVING_TO,
        COVER_IN_COVER,
        COVER_PEEKING,
        COVER_REPOSITIONING
    };

    enum FireMode {
        FIRE_ACCURATE,      // Aimed shots, low spread required
        FIRE_BURST,         // Short controlled bursts
        FIRE_SUPPRESSION,   // Sustained fire at area, not specific target
        FIRE_MELEE          // Close combat
    };

    enum CombatProfile {
        AGGRESSIVE,    // Push forward, sustained fire
        CAUTIOUS,      // Use cover, controlled bursts
        DEFENSIVE,     // Hold position, suppression
        RETREATING     // Fall back, covering fire
    };

    enum SquadRole {
        ROLE_NONE,
        ROLE_AGGRESSOR,   // Direct assault
        ROLE_FLANKER,     // Circle to enemy sides
        ROLE_SUPPORT,     // Cover fire, hold position
        ROLE_DEFENDER     // Protect objective/teammate
    };

    struct SquadInfo {
        Container<BotController*> members;      // Bots within coordination range
        SafePtr<Sentient>         sharedTarget; // Current squad target
        Vector                    rallyPoint;
        int                       lastUpdate;   // Last update time
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
    int    m_iStateEntryTime[MAX_BOT_FUNCTIONS];  // Track when each state was entered (for minimum duration)
    int    m_iTargetLockTime;                      // Track when current target was acquired (for target stickiness)

    Vector            m_vLastCuriousPos;
    Vector            m_vNewCuriousPos;
    Vector            m_vOldEnemyPos;
    Vector            m_vLastEnemyPos;
    Vector            m_vLastDeathPos;
    SafePtr<Sentient> m_pEnemy;
    int               m_iEnemyEyesTag;

    // Enemy memory system for investigation
    EnemyMemory m_enemyMemory;
    int         m_iInvestigateStartTime;

    // Sound-based investigation tracking
    int    m_iInvestigateEventTime;  // When high-priority sound was heard (for sound investigation mode)
    Vector m_vInvestigateEventPos;   // Location of high-priority sound event
    int    m_iCurrentEventPriority;  // Priority of current investigation (0=none, 1=low/curious, 2=high/investigate)

    // Cover system
    CoverPoint  m_currentCover;
    CoverState  m_coverState;
    int         m_iNextPeekTime;
    int         m_iPeekStartTime;
    float       m_fPeekDuration;
    int         m_iLastCoverSearchTime;

    // Tactical combat system
    FireMode      m_fireMode;
    CombatProfile m_combatProfile;
    int           m_iSuppressionEndTime;
    float         m_fRecentDamage;
    int           m_iDamageWindowStart;
    float         m_fBurstDuration;
    float         m_fBurstDelay;
    bool          m_bRequireLowSpread;
    bool          m_bAmmoLow;

    // Squad coordination system
    SquadInfo  m_squad;
    SquadRole  m_squadRole;
    int        m_iLastSquadUpdateTime;
    int        m_iRoleAssignmentTime;
    Vector     m_vFlankPosition;
    bool       m_bFlankPositionValid;

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
    bool        IsValidEnemy(Sentient *sent) const;

    // Added in OPM
    //  Extracted functions from State_Attack for improved readability
    bool      ValidateAttackPreconditions(void);
    Sentient *SelectBestTarget(float maxDistance, float& outDistanceSq);
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
    float ExecuteFiring(
             bool  canSee,
             float distanceSq,
             bool& outNoMove,
             bool& outFiring,
             bool& outMelee
         );
    void UpdateAttackMovement(bool noMove, bool melee, bool canSee, float minDistanceSq);

    static void InitState_Investigate(botfunc_t *func);
    bool        CheckCondition_Investigate(void);
    void        State_EndInvestigate(void);
    void        State_Investigate(void);
    Vector      CalculateSearchPosition(void);

    // Cover system
    CoverPoint  FindBestCover(Vector enemyPos);
    float       EvaluateCoverQuality(Vector pos, Vector enemyPos);
    bool        IsInCover(Vector pos, Vector enemyPos);
    bool        IsCoverCompromised(void);
    void        UpdateCoverBehavior(void);

    // Tactical combat system
    void          UpdateTacticalCombat(void);
    void          UpdateSuppressionFire(void);
    bool          ShouldRetreat(void);
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
    void      ReceiveEnemyInfo(Sentient* enemy, Vector position);
    void      CoordinateAttack(void);
    void      CheckStaggeredEngagement(void);
    int       CountAlliesNearPosition(Vector pos, float radius);
    SquadRole GetSquadRole(void) const;
    bool      HasEnemy(void) const;
    Sentient* GetEnemy(void) const;

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

    void GetEyeInfo(usereyes_t *eyeinfo);
    void GetUsercmd(usercmd_t *ucmd);

    void UpdateBotStates(void);
    void CheckReload(void);

    void AimAtAimNode(void);

    void NoticeEvent(Vector vPos, int iType, Entity *pEnt, float fDistanceSquared, float fRadiusSquared);
    int  GetEventPriority(int eventType);
    void ClearEnemy(void);

    void SendCommand(const char *text);

    void Think();

    void Spawned(void);

    void Killed(const Event& ev);
    void GotKill(const Event& ev);
    void EventStuffText(const str& text);

    BotMovement& GetMovement();

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

    void Init();
    void Cleanup();
    void Frame();
    void BroadcastEvent(Entity *originator, Vector origin, int iType, float radius);

private:
    BotControllerManager botControllerManager;
};

extern BotManager botManager;
