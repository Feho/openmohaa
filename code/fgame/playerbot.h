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

// Forward declarations for bot state classes
class AttackBotState;
class CuriousBotState;
class IdleBotState;
class GrenadeBotState;
class WeaponBotState;

class BotMovement
{
public:
    BotMovement();
    ~BotMovement();

    void SetControlledEntity(Player *newEntity);

    void MoveThink(usercmd_t& botcmd, float skillLevel = 1.0f);

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

    void          TurnThink(usercmd_t& botcmd, usereyes_t& eyeinfo, float skillLevel = 1.0f);
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

class BotController;

class BotState
{
public:
    BotState(BotController* pController) : m_pController(pController) {}
    virtual ~BotState() {}

    virtual void Enter() {}
    virtual void Think() = 0;
    virtual void Exit() {}
    virtual const char* GetName() = 0;

protected:
    BotController* m_pController;
};

class BotController : public Listener
{
    friend class AttackBotState;
    friend class CuriousBotState;
    friend class IdleBotState;
    friend class GrenadeBotState;
    friend class WeaponBotState;

private:
    BotMovement movement;
    BotRotation rotation;

    // States
    BotState* m_pCurrentState;
    AttackBotState* m_pAttackState;
    CuriousBotState* m_pCuriousState;
    IdleBotState* m_pIdleState;
    GrenadeBotState* m_pGrenadeState;
    WeaponBotState* m_pWeaponState;

    int    m_iCuriousTime;
    int    m_iAttackTime;
    int    m_iAttackStopAimTime;
    int    m_iLastBurstTime;
    int    m_iLastSeenTime;
    int    m_iLastUnseenTime;
    int    m_iContinuousFireTime;
    int    m_iLastStateChangeTime;  // Track when we last changed states
    Vector m_vAimOffset;
    int    m_iLastAimTime;

    Vector            m_vLastCuriousPos;
    Vector            m_vNewCuriousPos;
    Vector            m_vOldEnemyPos;
    Vector            m_vLastEnemyPos;
    Vector            m_vLastDeathPos;
    SafePtr<Sentient> m_pEnemy;
    int               m_iEnemyEyesTag;

    // Input
    usercmd_t  m_botCmd;
    usereyes_t m_botEyes;

    // Taunts
    int m_iNextTauntTime;
    int m_iLastFireTime;

    // Skill
    float m_fSkill;

    // Strafing behavior
    int   m_iLastStrafeTime;
    int   m_iStrafeDirection;
    int   m_iStrafeChangeTime;

    // Combat stance behavior
    int     m_iLastStanceChangeTime;
    int     m_iCrouchTime;
    int     m_iStandTime;
    bool    m_bIsCrouching;
    bool    m_bWantsToRun;
    
    // Performance optimization
    int     m_iLastFullUpdateTime;  // Track when we last did expensive operations

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
    void PerformCombatStrafing(void);
    void UpdateCombatStance(void);
    bool IsValidEnemy(Sentient *sent) const;
    bool CheckCondition_Attack(void);
    bool CheckCondition_Curious(void);

    void UpdateStates(void);
    void ChangeState(BotState* pNewState);
    bool ShouldTransitionToState(BotState* newState);
    void ValidateAndRecoverState(void);


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
    void GotKill(const Event& ev);
    void EventStuffText(const str& text);

    BotMovement& GetMovement();
    float        GetSkill() const;

	void AimAtEnemy(bool bCanSee, float fDistanceSquared, bool bFiring);
    bool PerformAttack(bool bCanSee, bool bCanAttack, float fDistanceSquared, bool& bMelee, bool& bNoMove, float& fMinDistanceSquared, class Weapon* pWeap);
    bool PerformCombatMovement(bool bCanSee, bool bMelee, bool bNoMove, bool bFiring, float fMinDistanceSquared, float fEnemyDistanceSquared);
    bool CheckReactionTime(float fDistanceSquared);

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
