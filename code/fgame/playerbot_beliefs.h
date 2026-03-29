// playerbot_beliefs.h: Spatial belief map for bot decision making.

#pragma once

#include "../corepp/vector.h"
#include "../corepp/container.h"

class Player;

// Added in OPM
//  Spatial belief map: a per-bot probability grid estimating where enemies
//  are likely to be. Each zone stores a belief value (0.0-1.0) representing
//  the estimated probability of enemy presence. Updated from events,
//  sightings, and deaths; decays over time without reinforcement.

struct BeliefZone {
    Vector centroid;
    float  belief;
    int    lastUpdateTime;
};

class BotBeliefMap
{
public:
    BotBeliefMap();

    void Init(const Vector& worldMins, const Vector& worldMaxs, float cellSize);
    void Decay(float dt);

    void UpdateFromEvent(Vector pos, int iType, float fRangeFactor);
    void UpdateFromSighting(Vector pos);
    void UpdateFromDeath(Vector pos);
    void ClearZone(Vector pos);
    void ClearZonesVisibleFrom(Player *player);

    int    GetHighestBeliefZone() const;
    float  GetBeliefAtPos(Vector pos) const;
    Vector GetHighestBeliefDir(Vector myPos) const;
    Vector GetHighestBeliefPos() const;

    int                          GetZoneCount() const;
    const Container<BeliefZone>& GetZones() const;

    bool IsInitialized() const;

private:
    int   FindZoneForPos(const Vector& pos) const;
    void  AddBelief(int zoneIndex, float amount);
    float GetDeathDecayRate() const;

    Container<BeliefZone> m_zones;
    bool                  m_bInitialized;
    int                   m_iVisClearIndex;

    // Grid parameters
    Vector m_vWorldMins;
    Vector m_vWorldMaxs;
    float  m_fCellSize;
    int    m_iGridWidth;
    int    m_iGridHeight;
};
