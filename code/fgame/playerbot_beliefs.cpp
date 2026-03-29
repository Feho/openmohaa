// playerbot_beliefs.cpp: Spatial belief map for bot decision making.

// Added in OPM
//  Implements a spatial belief map that discretizes the map into zones and
//  tracks the estimated probability of enemy presence in each zone. The map
//  is updated from events (weapon fire, footsteps, explosions), direct
//  sightings, and death locations. Beliefs decay over time without
//  reinforcement. Used to drive idle patrol, pre-aiming, and curious state.

#include "g_local.h"
#include "playerbot_beliefs.h"
#include "gamecvars.h"
#include "player.h"
#include "sentient.h"

BotBeliefMap::BotBeliefMap()
    : m_bInitialized(false)
    , m_iVisClearIndex(0)
    , m_fCellSize(0)
    , m_iGridWidth(0)
    , m_iGridHeight(0)
{}

/*
====================
Init

Discretize the map into a 2D grid of zones. The grid is flat (XY plane)
since vertical variation in MOHAA maps is modest relative to cell size.
Cell size is clamped so the total zone count stays in the ~50-200 range.
====================
*/
void BotBeliefMap::Init(const Vector& worldMins, const Vector& worldMaxs, float cellSize)
{
    m_vWorldMins = worldMins;
    m_vWorldMaxs = worldMaxs;

    float worldWidth  = worldMaxs.x - worldMins.x;
    float worldHeight = worldMaxs.y - worldMins.y;

    // Clamp cell size so we get a reasonable number of zones
    float minCellSize = Q_max(worldWidth, worldHeight) / 14.0f;
    float maxCellSize = Q_max(worldWidth, worldHeight) / 5.0f;
    m_fCellSize       = Q_clamp_float(cellSize, minCellSize, maxCellSize);

    m_iGridWidth  = Q_max(1, (int)ceilf(worldWidth / m_fCellSize));
    m_iGridHeight = Q_max(1, (int)ceilf(worldHeight / m_fCellSize));

    int totalZones = m_iGridWidth * m_iGridHeight;

    m_zones.Resize(totalZones);

    for (int y = 0; y < m_iGridHeight; y++) {
        for (int x = 0; x < m_iGridWidth; x++) {
            BeliefZone zone;
            zone.centroid.x      = worldMins.x + (x + 0.5f) * m_fCellSize;
            zone.centroid.y      = worldMins.y + (y + 0.5f) * m_fCellSize;
            zone.centroid.z      = (worldMins.z + worldMaxs.z) * 0.5f;
            zone.belief          = 0.0f;
            zone.lastUpdateTime  = 0;
            m_zones.AddObject(zone);
        }
    }

    m_bInitialized = true;
}

/*
====================
FindZoneForPos

Map a world position to the zone index that contains it.
Returns -1 if outside the grid.
====================
*/
int BotBeliefMap::FindZoneForPos(const Vector& pos) const
{
    if (!m_bInitialized) {
        return -1;
    }

    int x = (int)((pos.x - m_vWorldMins.x) / m_fCellSize);
    int y = (int)((pos.y - m_vWorldMins.y) / m_fCellSize);

    x = Q_clamp(x, 0, m_iGridWidth - 1);
    y = Q_clamp(y, 0, m_iGridHeight - 1);

    return y * m_iGridWidth + x;
}

/*
====================
AddBelief

Add belief to a zone, clamping to [0.0, 1.0].
====================
*/
void BotBeliefMap::AddBelief(int zoneIndex, float amount)
{
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone = m_zones.ObjectAt(zoneIndex + 1);
    zone.belief      = Q_clamp_float(zone.belief + amount, 0.0f, 1.0f);
    zone.lastUpdateTime = level.inttime;
}

/*
====================
Decay

Per-frame exponential decay of all zone beliefs.
belief *= decayRate ^ dt
====================
*/
void BotBeliefMap::Decay(float dt)
{
    if (!m_bInitialized) {
        return;
    }

    float decayRate = g_bot_belief_decay->value;
    float factor    = powf(decayRate, dt);

    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        BeliefZone& zone = m_zones.ObjectAt(i);
        zone.belief *= factor;

        // Snap to zero to avoid lingering tiny values
        if (zone.belief < 0.01f) {
            zone.belief = 0.0f;
        }
    }
}

/*
====================
UpdateFromEvent

Update beliefs from a NoticeEvent (weapon fire, footstep, explosion, etc).
The weight depends on the event type, scaled by range falloff and the
global event weight cvar.
====================
*/
void BotBeliefMap::UpdateFromEvent(Vector pos, int iType, float fRangeFactor)
{
    if (!m_bInitialized) {
        return;
    }

    float weight = 0.0f;

    switch (iType) {
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_WEAPON_IMPACT:
        weight = 0.6f;
        break;
    case AI_EVENT_EXPLOSION:
        weight = 0.4f;
        break;
    case AI_EVENT_FOOTSTEP:
        weight = 0.2f;
        break;
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
        weight = 0.3f;
        break;
    case AI_EVENT_GRENADE:
        weight = 0.5f;
        break;
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
    default:
        weight = 0.15f;
        break;
    }

    weight *= fRangeFactor * g_bot_belief_event_weight->value;

    int zoneIndex = FindZoneForPos(pos);
    AddBelief(zoneIndex, weight);
}

/*
====================
UpdateFromSighting

Direct visual contact with an enemy: set belief to 1.0 (certainty).
====================
*/
void BotBeliefMap::UpdateFromSighting(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone    = m_zones.ObjectAt(zoneIndex + 1);
    zone.belief         = 1.0f;
    zone.lastUpdateTime = level.inttime;
}

/*
====================
UpdateFromDeath

Where this bot died. High belief that persists longer (slower decay
handled by the decay rate difference in the calling code — death beliefs
use a higher initial value).
====================
*/
void BotBeliefMap::UpdateFromDeath(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    AddBelief(zoneIndex, 0.8f);
}

/*
====================
ClearZone

Mark a zone as visually confirmed empty.
====================
*/
void BotBeliefMap::ClearZone(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone    = m_zones.ObjectAt(zoneIndex + 1);
    zone.belief         = 0.0f;
    zone.lastUpdateTime = level.inttime;
}

/*
====================
ClearZonesVisibleFrom

Periodic visibility sweep: clear belief for zones the bot can currently see
are empty. Only checks a subset of zones per call to avoid per-frame cost.
====================
*/
void BotBeliefMap::ClearZonesVisibleFrom(Player *player)
{
    if (!m_bInitialized || !player) {
        return;
    }

    // Check a rolling window of zones each frame to spread cost
    int count = Q_min(8, m_zones.NumObjects());

    for (int n = 0; n < count; n++) {
        int i = ((m_iVisClearIndex + n) % m_zones.NumObjects());

        BeliefZone& zone = m_zones.ObjectAt(i + 1);
        if (zone.belief <= 0.0f) {
            continue;
        }

        if (player->CanSee(zone.centroid, 80, 2048, false)) {
            zone.belief         = 0.0f;
            zone.lastUpdateTime = level.inttime;
        }
    }

    m_iVisClearIndex = (m_iVisClearIndex + count) % Q_max(1, m_zones.NumObjects());
}

/*
====================
GetHighestBeliefZone

Return the index of the zone with the highest belief.
Returns -1 if no zone has belief above the patrol threshold.
====================
*/
int BotBeliefMap::GetHighestBeliefZone() const
{
    if (!m_bInitialized) {
        return -1;
    }

    float minBelief = g_bot_belief_min_patrol->value;
    int   bestIndex = -1;
    float bestBelief = 0.0f;

    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        const BeliefZone& zone = m_zones.ObjectAt(i);
        if (zone.belief > bestBelief && zone.belief >= minBelief) {
            bestBelief = zone.belief;
            bestIndex  = i - 1;
        }
    }

    return bestIndex;
}

/*
====================
GetBeliefAtPos

Return the belief value at a given position.
====================
*/
float BotBeliefMap::GetBeliefAtPos(Vector pos) const
{
    if (!m_bInitialized) {
        return 0.0f;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return 0.0f;
    }

    return m_zones.ObjectAt(zoneIndex + 1).belief;
}

/*
====================
GetHighestBeliefDir

Return the direction from myPos toward the highest-belief zone.
Returns vec_zero if no zone has significant belief.
====================
*/
Vector BotBeliefMap::GetHighestBeliefDir(Vector myPos) const
{
    int bestIndex = GetHighestBeliefZone();
    if (bestIndex < 0) {
        return vec_zero;
    }

    Vector dir = m_zones.ObjectAt(bestIndex + 1).centroid - myPos;
    dir.z      = 0;
    VectorNormalize(dir);
    return dir;
}

/*
====================
GetHighestBeliefPos

Return the centroid of the highest-belief zone.
Returns vec_zero if no zone has significant belief.
====================
*/
Vector BotBeliefMap::GetHighestBeliefPos() const
{
    int bestIndex = GetHighestBeliefZone();
    if (bestIndex < 0) {
        return vec_zero;
    }

    return m_zones.ObjectAt(bestIndex + 1).centroid;
}

int BotBeliefMap::GetZoneCount() const
{
    return m_zones.NumObjects();
}

const Container<BeliefZone>& BotBeliefMap::GetZones() const
{
    return m_zones;
}

bool BotBeliefMap::IsInitialized() const
{
    return m_bInitialized;
}
