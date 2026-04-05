// playerbot_beliefs.h: Spatial belief map for bot decision making.

#pragma once

#include "../corepp/vector.h"
#include "../corepp/container.h"

class Player;
class DM_Team;
struct BotParams;

// Added in OPM
//  Spatial belief map: a per-bot probability grid estimating where enemies
//  are likely to be. Each zone stores a belief value (0.0-1.0) representing
//  the estimated probability of enemy presence. Updated from events,
//  sightings, and deaths; decays over time without reinforcement.

struct BeliefZone {
    Vector centroid;
    float  belief;
    int    lastUpdateTime;
    int    visitCount;      // Times searched without finding enemy
    int    lastVisitTime;   // For visit decay
    int    pathBlockedTime; // When path to this zone was marked blocked (0 = not blocked)
};

// Added in OPM
//  Failed target: a position the bot tried to reach but couldn't.
//  Any destination near this position will be rejected.
struct FailedTarget {
    Vector pos; // The unreachable target position
    int    time;
};

class BotBeliefMap
{
public:
    BotBeliefMap();

    void SetParams(const BotParams *params);
    void Init(const Vector& worldMins, const Vector& worldMaxs, float cellSize);
    void Decay(float dt);

    void UpdateFromEvent(Vector pos, int iType, float fRangeFactor);
    void UpdateFromSighting(Vector pos);
    void UpdateFromDeath(Vector pos);
    void SeedFromSpawnPoints(Player *player);
    void ClearZone(Vector pos);
    void ClearZonesVisibleFrom(Player *player);

    // Added in OPM
    //  Visit-based belief suppression: tracks how many times a zone was
    //  searched without finding enemies. Repeated visits reduce effective
    //  belief, encouraging exploration of new areas.
    void MarkVisited(Vector pos);
    void ResetVisitsOnSighting(Vector pos);

    // Added in OPM
    //  Path-blocked zone tracking: when a bot fails to reach a zone after
    //  repeated attempts, mark it as blocked so the bot stops trying.
    void MarkPathBlocked(Vector pos);
    bool IsPathBlocked(Vector pos) const;

    // Added in OPM
    //  Failed target tracking: record targets the bot couldn't reach.
    //  Reject any destination near a failed target.
    void AddFailedTarget(Vector targetPos);
    bool IsNearFailedTarget(Vector pos) const;

    // Changed in OPM
    //  Zone selection now considers distance from bot position and uses
    //  hysteresis to prevent flip-flopping between zones.
    int    GetBestZone(Vector myPos);
    float  GetBeliefAtPos(Vector pos) const;
    Vector GetHighestBeliefDir(Vector myPos);
    Vector GetHighestBeliefPos(Vector myPos);

    int                          GetZoneCount() const;
    const Container<BeliefZone>& GetZones() const;

    void PrintGrid(Vector botPos) const;

    bool IsInitialized() const;

private:
    int   FindZoneForPos(const Vector& pos) const;
    void  AddBelief(int zoneIndex, float amount);
    float GetDeathDecayRate() const;

    Container<BeliefZone>   m_zones;
    Container<FailedTarget> m_failedTargets;
    bool                    m_bInitialized;
    int                     m_iVisClearIndex;

    // Grid parameters
    Vector m_vWorldMins;
    Vector m_vWorldMaxs;
    float  m_fCellSize;
    int    m_iGridWidth;
    int    m_iGridHeight;

    // Hysteresis - prevent flip-flopping between zones
    int m_iCurrentTargetZone;
    int m_iTargetLockTime;

    const BotParams *m_pParams;
};
