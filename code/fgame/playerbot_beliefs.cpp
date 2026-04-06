// playerbot_beliefs.cpp: Spatial belief map for bot decision making.

// Added in OPM
//  Implements a spatial belief map that discretizes the map into zones and
//  tracks the estimated probability of enemy presence in each zone. The map
//  is updated from events (weapon fire, footsteps, explosions), direct
//  sightings, and death locations. Beliefs decay over time without
//  reinforcement. Used to drive idle patrol, pre-aiming, and curious state.

#include "g_local.h"
#include "playerbot.h"
#include "playerbot_beliefs.h"
#include "gamecvars.h"
#include "player.h"
#include "sentient.h"
#include "dm_manager.h"
#include "playerstart.h"
#include "navigation_recast_load.h"
#include "navigation_recast_helpers.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

// Added in OPM
//  Static shared visibility table — one instance for all bot belief maps.
ZoneVisibilityTable BotBeliefMap::s_visibility;

/*
====================
ZoneVisibilityTable::Init

Added in OPM
  Allocate packed bitset storage for zoneCount x zoneCount zone pairs.
  Each row occupies ceil(zoneCount / 32) uint32_t words.
====================
*/
void ZoneVisibilityTable::Init(int zoneCount)
{
    m_zoneCount = zoneCount;
    m_rowWords  = (zoneCount + 31) / 32;

    int totalWords = m_rowWords * zoneCount;

    m_sampled.FreeObjectList();
    m_visible.FreeObjectList();

    m_sampled.Resize(totalWords);
    m_visible.Resize(totalWords);

    for (int i = 0; i < totalWords; i++) {
        m_sampled.AddObject(0u);
        m_visible.AddObject(0u);
    }
}

/*
====================
ZoneVisibilityTable::Clear

Added in OPM
  Zero all bitset entries without re-allocating.
====================
*/
void ZoneVisibilityTable::Clear()
{
    for (int i = 1; i <= m_sampled.NumObjects(); i++) {
        m_sampled.ObjectAt(i) = 0u;
    }
    for (int i = 1; i <= m_visible.NumObjects(); i++) {
        m_visible.ObjectAt(i) = 0u;
    }
    m_zoneCount = 0;
    m_rowWords  = 0;
}

bool ZoneVisibilityTable::IsInitialized() const
{
    return m_zoneCount > 0 && m_sampled.NumObjects() > 0;
}

void ZoneVisibilityTable::SetBit(Container<uint32_t>& bits, int a, int b)
{
    int wordIdx = a * m_rowWords + b / 32;
    bits.ObjectAt(wordIdx + 1) |= (1u << (b % 32));
}

bool ZoneVisibilityTable::GetBit(const Container<uint32_t>& bits, int a, int b) const
{
    int wordIdx = a * m_rowWords + b / 32;
    return (bits.ObjectAt(wordIdx + 1) & (1u << (b % 32))) != 0;
}

bool ZoneVisibilityTable::IsSampled(int a, int b) const
{
    if (a < 0 || b < 0 || a >= m_zoneCount || b >= m_zoneCount) {
        return false;
    }
    return GetBit(m_sampled, a, b);
}

bool ZoneVisibilityTable::IsVisible(int a, int b) const
{
    if (a < 0 || b < 0 || a >= m_zoneCount || b >= m_zoneCount) {
        return false;
    }
    return GetBit(m_visible, a, b);
}

void ZoneVisibilityTable::Record(int a, int b, bool visible)
{
    if (a < 0 || b < 0 || a >= m_zoneCount || b >= m_zoneCount) {
        return;
    }
    SetBit(m_sampled, a, b);
    SetBit(m_sampled, b, a);
    if (visible) {
        SetBit(m_visible, a, b);
        SetBit(m_visible, b, a);
    }
}

int ZoneVisibilityTable::CountOversightZones(int from, const Container<BeliefZone>& zones, float minBelief) const
{
    int count = 0;
    for (int j = 0; j < m_zoneCount; j++) {
        if (j == from) {
            continue;
        }
        if (!IsSampled(from, j) || !IsVisible(from, j)) {
            continue;
        }
        if (j < zones.NumObjects() && zones.ObjectAt(j + 1).belief >= minBelief) {
            count++;
        }
    }
    return count;
}

BotBeliefMap::BotBeliefMap()
    : m_bInitialized(false)
    , m_iVisClearIndex(0)
    , m_fCellSize(0)
    , m_iGridWidth(0)
    , m_iGridHeight(0)
    , m_bNavMeshMode(false)
    , m_iCurrentTargetZone(-1)
    , m_iTargetLockTime(0)
    , m_pParams(NULL)
    , m_debugName(NULL)
{}

// Added in OPM
//  Reset the shared visibility table — called on map reload.
void BotBeliefMap::ClearSharedVisibility()
{
    s_visibility.Clear();
}

void BotBeliefMap::SetParams(const BotParams *params)
{
    m_pParams = params;
}

// Added in OPM
//  Human-readable name for an AI event type, used in belief debug output.
static const char *AIEventTypeName(int type)
{
    switch (type) {
    case AI_EVENT_WEAPON_FIRE:     return "WEAPON_FIRE";
    case AI_EVENT_EXPLOSION:       return "EXPLOSION";
    case AI_EVENT_FOOTSTEP:        return "FOOTSTEP";
    case AI_EVENT_AMERICAN_VOICE:  return "AMERICAN_VOICE";
    case AI_EVENT_GERMAN_VOICE:    return "GERMAN_VOICE";
    case AI_EVENT_AMERICAN_URGENT: return "AMERICAN_URGENT";
    case AI_EVENT_GERMAN_URGENT:   return "GERMAN_URGENT";
    case AI_EVENT_GRENADE:         return "GRENADE";
    case AI_EVENT_MISC:            return "MISC";
    case AI_EVENT_MISC_LOUD:       return "MISC_LOUD";
    default:                       return "UNKNOWN";
    }
}

void BotBeliefMap::SetDebugName(const char *name)
{
    m_debugName = name;
}

/*
====================
Init

Discretize the map into belief zones. If a valid Recast navmesh is available,
zones are derived from navigation mesh polygons (multi-floor aware). Otherwise,
falls back to a flat XY grid.
====================
*/
void BotBeliefMap::Init(const Vector& worldMins, const Vector& worldMaxs, float cellSize)
{
    m_vWorldMins = worldMins;
    m_vWorldMaxs = worldMaxs;

    // Added in OPM
    //  Reset the shared visibility table on every map reload so stale
    //  zone-pair data from the previous map does not carry over.
    s_visibility.Clear();

    // Added in OPM
    //  Prefer navmesh-based zone init so centroids land on walkable surfaces
    //  and multi-floor maps get per-floor zones. If the navmesh hasn't been
    //  built yet (sv_maxbots was 0 at map load time, so LoadWorldMap skipped
    //  it), trigger a build now that we know there is at least one bot.
    if (!navigationMap.IsValid() && !g_navigation_legacy->integer
        && g_gametype->integer != GT_SINGLE_PLAYER) {
        navigationMap.LoadWorldMap(level.m_mapfile);
    }

    if (navigationMap.IsValid()) {
        InitFromNavMesh(worldMins, worldMaxs, cellSize);
    } else {
        InitFlatGrid(worldMins, worldMaxs, cellSize);
    }

    m_bInitialized = true;
}

/*
====================
InitFlatGrid

Flat XY-grid fallback used when no navmesh is available. Zone centroids are
pinned to mid-height of the map bbox (the pre-navmesh behavior).
====================
*/
void BotBeliefMap::InitFlatGrid(const Vector& worldMins, const Vector& worldMaxs, float cellSize)
{
    m_bNavMeshMode = false;
    m_polyToZone.FreeObjectList();

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
            zone.visitCount      = 0;
            zone.lastVisitTime   = 0;
            zone.pathBlockedTime = 0;
            m_zones.AddObject(zone);
        }
    }

    // Added in OPM
    //  Initialize shared visibility table now that zone count is known.
    s_visibility.Init(m_zones.NumObjects());
}

// Internal struct used only during navmesh zone construction.
struct ZoneBuildData {
    float sumX, sumY, sumZ;
    int   count;
    int   gridX, gridY;
};

/*
====================
InitFromNavMesh

Added in OPM
  Build belief zones from Recast navigation mesh polygons.

  Each ground polygon's centroid (in game coordinates, guaranteed walkable)
  is bucketed by (XY grid cell, Z floor). Polys that share the same XY cell
  and whose Z centroids are within Z_CLUSTER_THRESHOLD of an existing zone in
  that cell are merged into it; otherwise a new zone is created. This produces
  one zone per floor per map area rather than a single flat zone per cell.

  A poly-index -> zone-index table is built so FindZoneForPos can use
  dtNavMeshQuery::findNearestPoly for constant-time, multi-floor-correct
  lookup instead of raw XY arithmetic.
====================
*/
void BotBeliefMap::InitFromNavMesh(const Vector& worldMins, const Vector& worldMaxs, float cellSize)
{
    const dtNavMesh  *navMesh = navigationMap.GetNavMesh();
    const dtMeshTile *tile   = navMesh->getTile(0);
    if (!tile || !tile->header || tile->header->polyCount == 0) {
        // No polys in tile 0 — degrade gracefully to flat grid
        InitFlatGrid(worldMins, worldMaxs, cellSize);
        return;
    }

    // Derive XY grid dimensions (same clamping as flat-grid mode)
    float worldWidth  = worldMaxs.x - worldMins.x;
    float worldHeight = worldMaxs.y - worldMins.y;
    float minCellSize = Q_max(worldWidth, worldHeight) / 14.0f;
    float maxCellSize = Q_max(worldWidth, worldHeight) / 5.0f;
    m_fCellSize       = Q_clamp_float(cellSize, minCellSize, maxCellSize);
    m_iGridWidth      = Q_max(1, (int)ceilf(worldWidth / m_fCellSize));
    m_iGridHeight     = Q_max(1, (int)ceilf(worldHeight / m_fCellSize));

    // Z tolerance: polys within this vertical distance share a floor zone
    static const float Z_CLUSTER_THRESHOLD = 200.0f;

    int polyCount = tile->header->polyCount;

    // Pre-fill poly-to-zone map with -1 (off-mesh or unassigned)
    m_polyToZone.FreeObjectList();
    m_polyToZone.Resize(polyCount);
    for (int i = 0; i < polyCount; i++) {
        m_polyToZone.AddObject(-1);
    }

    // Accumulate zone centroids from poly centroids
    Container<ZoneBuildData> buildData;

    for (int i = 0; i < polyCount; i++) {
        const dtPoly& poly = tile->polys[i];

        // Skip off-mesh connections — they have no walkable surface
        if (poly.getType() != DT_POLYTYPE_GROUND) {
            continue;
        }
        if (poly.vertCount == 0) {
            continue;
        }

        // Compute centroid in Recast space (X=right, Y=up, Z=-fwd)
        float rcx = 0, rcy = 0, rcz = 0;
        for (int v = 0; v < poly.vertCount; v++) {
            int vi = poly.verts[v];
            rcx += tile->verts[vi * 3 + 0];
            rcy += tile->verts[vi * 3 + 1];
            rcz += tile->verts[vi * 3 + 2];
        }
        rcx /= poly.vertCount;
        rcy /= poly.vertCount;
        rcz /= poly.vertCount;

        // Convert centroid to game coordinates (X=right, Y=fwd, Z=up)
        float recastPos[3] = {rcx, rcy, rcz};
        float gamePos[3];
        ConvertRecastToGameCoord(recastPos, gamePos);

        // Determine XY grid cell for this poly
        int gx = (int)((gamePos[0] - worldMins.x) / m_fCellSize);
        int gy = (int)((gamePos[1] - worldMins.y) / m_fCellSize);
        gx = Q_clamp(gx, 0, m_iGridWidth - 1);
        gy = Q_clamp(gy, 0, m_iGridHeight - 1);

        // Search for a matching zone: same XY cell, Z centroid within threshold
        int matchedZone = -1;
        for (int z = 1; z <= buildData.NumObjects(); z++) {
            ZoneBuildData& zd = buildData.ObjectAt(z);
            if (zd.gridX == gx && zd.gridY == gy) {
                float zCentroid = zd.sumZ / zd.count;
                if (fabsf(gamePos[2] - zCentroid) < Z_CLUSTER_THRESHOLD) {
                    matchedZone = z - 1; // convert to 0-based
                    break;
                }
            }
        }

        if (matchedZone < 0) {
            // No matching zone — create a new one
            ZoneBuildData zd;
            zd.sumX  = gamePos[0];
            zd.sumY  = gamePos[1];
            zd.sumZ  = gamePos[2];
            zd.count = 1;
            zd.gridX = gx;
            zd.gridY = gy;
            matchedZone = buildData.NumObjects(); // 0-based index of the new entry
            buildData.AddObject(zd);
        } else {
            ZoneBuildData& zd = buildData.ObjectAt(matchedZone + 1);
            zd.sumX += gamePos[0];
            zd.sumY += gamePos[1];
            zd.sumZ += gamePos[2];
            zd.count++;
        }

        m_polyToZone.ObjectAt(i + 1) = matchedZone;
    }

    // Finalize zones from accumulated sums
    int zoneCount = buildData.NumObjects();
    m_zones.Resize(zoneCount);

    for (int i = 1; i <= zoneCount; i++) {
        const ZoneBuildData& zd = buildData.ObjectAt(i);
        BeliefZone zone;
        zone.centroid.x      = zd.sumX / zd.count;
        zone.centroid.y      = zd.sumY / zd.count;
        zone.centroid.z      = zd.sumZ / zd.count;
        zone.belief          = 0.0f;
        zone.lastUpdateTime  = 0;
        zone.visitCount      = 0;
        zone.lastVisitTime   = 0;
        zone.pathBlockedTime = 0;
        m_zones.AddObject(zone);
    }

    m_bNavMeshMode = true;

    gi.DPrintf(
        "BotBeliefMap: navmesh mode — %d zones from %d polys (grid %dx%d cell=%.0f)\n",
        zoneCount,
        polyCount,
        m_iGridWidth,
        m_iGridHeight,
        m_fCellSize
    );

    // Added in OPM
    //  Initialize shared visibility table now that zone count is known.
    s_visibility.Init(m_zones.NumObjects());
}

/*
====================
FindZoneForPos

Map a world position to the zone index that contains it.
Returns -1 if no zone is found.

In navmesh mode (Added in OPM):
  Converts pos to Recast space and calls findNearestPoly. The nearest poly's
  index is used to look up the pre-built poly-to-zone table. This handles
  multi-floor maps correctly because each floor has its own zone.

In flat-grid mode (fallback):
  Simple XY grid arithmetic as before.
====================
*/
int BotBeliefMap::FindZoneForPos(const Vector& pos) const
{
    if (!m_bInitialized) {
        return -1;
    }

    // Added in OPM
    //  Navmesh-based lookup: use findNearestPoly to resolve which poly (and
    //  therefore which zone) is beneath the position. Half-extents match the
    //  player bounding box used everywhere else in the navigation system.
    if (m_bNavMeshMode) {
        static const float halfExtents[3] = {15.f, 47.f, 15.f};

        float recastPos[3];
        ConvertGameToRecastCoord(pos, recastPos);

        dtPolyRef polyRef  = 0;
        float     nearest[3];
        navigationMap.GetNavMeshQuery()->findNearestPoly(
            recastPos, halfExtents, navigationMap.GetQueryFilter(), &polyRef, nearest
        );

        if (polyRef == 0) {
            return -1;
        }

        unsigned int polyIdx = navigationMap.GetNavMesh()->decodePolyIdPoly(polyRef);
        if ((int)polyIdx < m_polyToZone.NumObjects()) {
            return m_polyToZone.ObjectAt(polyIdx + 1);
        }
        return -1;
    }

    // Flat-grid fallback
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

    BeliefZone& zone    = m_zones.ObjectAt(zoneIndex + 1);
    zone.belief         = Q_clamp_float(zone.belief + amount, 0.0f, 1.0f);
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

    float decayRate = m_pParams->beliefDecay;
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
        weight = 0.6f;
        break;
    case AI_EVENT_WEAPON_IMPACT:
        // Ignore bullet impacts - they indicate where the bullet hit, not where
        // the shooter is. This would cause bots to investigate walls.
        return;
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

    weight *= fRangeFactor * m_pParams->beliefEventWeight;

    int zoneIndex = FindZoneForPos(pos);
    AddBelief(zoneIndex, weight);

    // Added in OPM
    //  g_bot_debug_beliefs 3: log every significant belief update so the
    //  player can see what the bot is hearing and how much it matters.
    if (g_bot_debug_beliefs->integer >= 3 && m_debugName && zoneIndex >= 0 && weight >= 0.05f) {
        const BeliefZone& zone = m_zones.ObjectAt(zoneIndex + 1);
        gi.Printf(
            "[%s] heard %-15s @ (%.0f %.0f %.0f)  zone %3d @ (%.0f %.0f %.0f)  +%.2f -> %.2f\n",
            m_debugName, AIEventTypeName(iType),
            pos.x, pos.y, pos.z,
            zoneIndex, zone.centroid.x, zone.centroid.y, zone.centroid.z,
            weight, zone.belief
        );
    }
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

    // Added in OPM
    if (g_bot_debug_beliefs->integer >= 3 && m_debugName) {
        gi.Printf(
            "[%s] SIGHTING @ (%.0f %.0f %.0f)  zone %3d @ (%.0f %.0f %.0f)  belief=1.00\n",
            m_debugName, pos.x, pos.y, pos.z,
            zoneIndex, zone.centroid.x, zone.centroid.y, zone.centroid.z
        );
    }

    // Added in OPM
    //  Seed adjacent visible zones with partial belief — an enemy that was
    //  spotted may retreat to a nearby zone, so pre-warm those zones.
    DiffuseFromZone(zoneIndex, 0.4f);
}

/*
====================
DiffuseFromZone

Added in OPM
  Propagate partial belief from zoneIndex to all zones that are visible
  from it according to the shared visibility table. Belief falls off with
  distance so far-away zones receive very little seeding.
====================
*/
void BotBeliefMap::DiffuseFromZone(int zoneIndex, float baseAmount)
{
    if (!s_visibility.IsInitialized()) {
        return;
    }
    for (int j = 0; j < m_zones.NumObjects(); j++) {
        if (j == zoneIndex) {
            continue;
        }
        if (!s_visibility.IsSampled(zoneIndex, j) || !s_visibility.IsVisible(zoneIndex, j)) {
            continue;
        }
        float dist        = (m_zones.ObjectAt(j + 1).centroid - m_zones.ObjectAt(zoneIndex + 1).centroid).length();
        float attenuation = 1.0f / (1.0f + dist / 1000.0f);
        float amount      = baseAmount * attenuation * 0.3f;
        AddBelief(j, amount);

        // Added in OPM
        if (g_bot_debug_beliefs->integer >= 3 && m_debugName && amount >= 0.02f) {
            const BeliefZone& dst = m_zones.ObjectAt(j + 1);
            gi.Printf(
                "[%s] diffuse z%d -> z%d @ (%.0f %.0f %.0f)  +%.2f -> %.2f\n",
                m_debugName, zoneIndex, j,
                dst.centroid.x, dst.centroid.y, dst.centroid.z,
                amount, dst.belief
            );
        }
    }
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
SeedFromSpawnPoints

// Added in OPM
//  Seed the belief map with low belief at enemy spawn points so bots
//  patrol toward likely spawn areas even before hearing anything.
//  In team games, seeds only opposing team spawns.
//  In FFA, seeds all deathmatch spawns (everyone is an enemy).
====================
*/
void BotBeliefMap::SeedFromSpawnPoints(Player *player)
{
    if (!m_bInitialized || !player) {
        return;
    }

    const float spawnBelief = 0.8f;

    teamtype_t botTeam = player->GetTeam();

    if (g_gametype->integer >= GT_TEAM && botTeam >= TEAM_ALLIES) {
        // Team game: seed enemy team spawn points
        DM_Team *enemyTeam;
        if (botTeam == TEAM_ALLIES) {
            enemyTeam = dmManager.GetTeamAxis();
        } else {
            enemyTeam = dmManager.GetTeamAllies();
        }

        for (int i = 1; i <= enemyTeam->m_spawnpoints.NumObjects(); i++) {
            PlayerStart *spawn     = enemyTeam->m_spawnpoints.ObjectAt(i);
            int          zoneIndex = FindZoneForPos(spawn->origin);
            // Seed all spawn points regardless of m_bForbidSpawns — disabled
            // spawns may be enabled later (objective mode) and still represent
            // likely enemy positions the bot should be aware of.
            AddBelief(zoneIndex, spawnBelief);
        }
    } else {
        // FFA: seed all deathmatch spawns except our own zone
        DM_Team *freeForAll = dmManager.GetTeamAllies();
        int      myZone     = FindZoneForPos(player->origin);

        for (int i = 1; i <= freeForAll->m_spawnpoints.NumObjects(); i++) {
            PlayerStart *spawn     = freeForAll->m_spawnpoints.ObjectAt(i);
            int          zoneIndex = FindZoneForPos(spawn->origin);
            // Skip the zone the bot is currently in
            if (zoneIndex != myZone) {
                AddBelief(zoneIndex, spawnBelief);
            }
        }
    }
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

// Changed in OPM
//  Periodic visibility sweep: clear belief for zones the bot can currently
//  see are empty. Only checks a rolling window of zones per call to spread
//  cost. Cache-first: if s_visibility has a recorded result for this zone
//  pair, skip the raycast and use the cached result directly.
====================
*/
void BotBeliefMap::ClearZonesVisibleFrom(Player *player)
{
    if (!m_bInitialized || !player) {
        return;
    }

    // Check a rolling window of zones each frame to spread cost
    int count = Q_min(8, m_zones.NumObjects());

    // Added in OPM
    //  Resolve the bot's current zone once for the whole window so we can
    //  look up cached visibility results without redundant navmesh queries.
    int botZone = FindZoneForPos(player->origin);

    for (int n = 0; n < count; n++) {
        int i = ((m_iVisClearIndex + n) % m_zones.NumObjects());

        BeliefZone& zone = m_zones.ObjectAt(i + 1);
        if (zone.belief <= 0.0f) {
            continue;
        }

        // Added in OPM
        //  Don't clear zones that were recently reinforced by a sound event.
        //  Seeing the centroid from afar is not the same as physically
        //  searching the area — the bot must walk to the zone and trigger
        //  MarkVisited to legitimately lower belief. Without this guard, a
        //  fresh gunshot belief is wiped within a frame on flat maps where
        //  the centroid is trivially visible.
        if (level.inttime - zone.lastUpdateTime < 15000) {
            continue;
        }

        // Added in OPM
        //  Cache-first: reuse a previously recorded visibility result to
        //  skip the CanSee raycast entirely for this zone pair.
        if (botZone >= 0 && s_visibility.IsSampled(botZone, i)) {
            if (s_visibility.IsVisible(botZone, i)) {
                zone.belief         = 0.0f;
                zone.lastUpdateTime = level.inttime;
            }
            continue;
        }

        // Cache miss: perform the raycast and record the result.
        bool canSee = player->CanSee(zone.centroid, 80, 2048, false);
        if (botZone >= 0) {
            s_visibility.Record(botZone, i, canSee);
        }
        if (canSee) {
            zone.belief         = 0.0f;
            zone.lastUpdateTime = level.inttime;
        }
    }

    m_iVisClearIndex = (m_iVisClearIndex + count) % Q_max(1, m_zones.NumObjects());
}

/*
====================
GetBestZone

Return the index of the best zone to investigate, considering both
belief level and distance from the bot. Uses hysteresis to prevent
flip-flopping between zones.

// Changed in OPM
//  Now applies visit-based belief suppression: zones that have been
//  searched repeatedly without finding enemies become less attractive.
//  Also adds novelty bonus for unvisited zones and random jitter to
//  prevent teammate clustering.

Score = (belief * visitPenalty * distanceFactor) + noveltyBonus + jitter
====================
*/
int BotBeliefMap::GetBestZone(Vector myPos)
{
    if (!m_bInitialized) {
        return -1;
    }

    // Path block duration in milliseconds (needed for hysteresis check)
    int pathBlockDuration = (int)(m_pParams->beliefPathBlockTime * 1000.0f);

    // Hysteresis: if we have a current target and it still has belief, stick with it
    // for a minimum time to avoid flip-flopping
    if (m_iCurrentTargetZone >= 0 && m_iCurrentTargetZone < m_zones.NumObjects()) {
        const BeliefZone& currentZone = m_zones.ObjectAt(m_iCurrentTargetZone + 1);
        // Fixed in OPM
        //  Check if zone is path-blocked before returning it - don't keep targeting unreachable zones
        bool isBlocked =
            currentZone.pathBlockedTime > 0 && level.inttime - currentZone.pathBlockedTime < pathBlockDuration;
        if (!isBlocked && currentZone.belief > 0.0f && level.inttime < m_iTargetLockTime) {
            return m_iCurrentTargetZone;
        }
    }

    int   bestIndex = -1;
    float bestScore = -999.0f;

    // Calculate max distance for normalization (diagonal of world)
    float worldWidth  = m_vWorldMaxs.x - m_vWorldMins.x;
    float worldHeight = m_vWorldMaxs.y - m_vWorldMins.y;
    float maxDist     = sqrtf(worldWidth * worldWidth + worldHeight * worldHeight);

    // Visit decay time in milliseconds
    int visitDecayTime = (int)(m_pParams->beliefVisitDecay * 1000.0f);

    // Added in OPM
    //  Resolve bot zone once before the scoring loop for overwatch bonus.
    int botZone = s_visibility.IsInitialized() ? FindZoneForPos(myPos) : -1;

    // Added in OPM
    //  Track the winner's score components and runner-up for debug output.
    bool  doDebug     = g_bot_debug_beliefs->integer >= 3 && m_debugName != NULL;
    float winBelief   = 0, winDistFactor = 0, winVisitPenalty = 0;
    float winNovelty  = 0, winJitter = 0;
    int   winOverwatch = 0;

    struct DebugRunner { int idx; float score; };
    DebugRunner runner[2] = {{-1, -999.f}, {-1, -999.f}};

    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        const BeliefZone& zone = m_zones.ObjectAt(i);

        // Skip zones that are currently path-blocked
        if (zone.pathBlockedTime > 0 && level.inttime - zone.pathBlockedTime < pathBlockDuration) {
            continue;
        }

        // Novelty bonus: attract to never-visited zones (even if belief is 0)
        float noveltyBonus = (zone.visitCount == 0) ? m_pParams->beliefNoveltyBonus : 0.0f;

        // Calculate distance factor: closer zones score higher
        // Range: 1.0 (at bot position) to 0.3 (at max distance)
        Vector delta     = zone.centroid - myPos;
        float dist       = delta.lengthXY();
        float distFactor = 1.0f - (dist / maxDist) * 0.7f;

        // Calculate effective visit count after decay
        int effectiveVisits = zone.visitCount;
        if (visitDecayTime > 0 && zone.lastVisitTime > 0) {
            int timeSinceVisit = level.inttime - zone.lastVisitTime;
            int decayedVisits  = timeSinceVisit / visitDecayTime;
            effectiveVisits    = Q_max(0, zone.visitCount - decayedVisits);
        }

        // Visit penalty: diminishing returns for repeated visits
        float visitPenalty = 1.0f / (1.0f + effectiveVisits * m_pParams->beliefVisitPenalty);

        // Jitter: prevent teammate clustering by adding random variance
        float jitter = G_CRandom(m_pParams->beliefScoreJitter);

        // Final score
        float score = (zone.belief * visitPenalty * distFactor) + noveltyBonus + jitter;

        // Added in OPM
        //  Overwatch bonus: zones from which many high-belief zones are
        //  visible score higher, encouraging bots to hold tactically
        //  dominant positions.
        int oversight = 0;
        if (s_visibility.IsInitialized() && botZone >= 0) {
            oversight = s_visibility.CountOversightZones(i - 1, m_zones, 0.3f);
            score += oversight * m_pParams->beliefOverwatchBonus;
        }

        if (score > bestScore) {
            // Demote current winner to runner-up list before replacing it
            if (doDebug && bestIndex >= 0) {
                if (score > runner[0].score) {
                    runner[1] = runner[0];
                    runner[0] = {bestIndex, bestScore};
                } else if (score > runner[1].score) {
                    runner[1] = {bestIndex, bestScore};
                }
            }
            bestScore       = score;
            bestIndex       = i - 1;
            winBelief       = zone.belief;
            winDistFactor   = distFactor;
            winVisitPenalty = visitPenalty;
            winNovelty      = noveltyBonus;
            winJitter       = jitter;
            winOverwatch    = oversight;
        } else if (doDebug) {
            if (score > runner[0].score) {
                runner[1] = runner[0];
                runner[0] = {i - 1, score};
            } else if (score > runner[1].score) {
                runner[1] = {i - 1, score};
            }
        }
    }

    // Update hysteresis state if we found a new target
    if (bestIndex >= 0 && bestIndex != m_iCurrentTargetZone) {
        // Added in OPM
        //  Print full scoring breakdown when the patrol target changes.
        if (doDebug) {
            const BeliefZone& wz = m_zones.ObjectAt(bestIndex + 1);
            gi.Printf(
                "[%s] patrol -> zone %3d @ (%.0f %.0f %.0f)  score=%.2f\n"
                "  bel=%.2f  dist=%.2f  visitPen=%.2f  novelty=%.2f  jitter=%.2f  overwatch=%d\n",
                m_debugName,
                bestIndex, wz.centroid.x, wz.centroid.y, wz.centroid.z, bestScore,
                winBelief, winDistFactor, winVisitPenalty, winNovelty, winJitter, winOverwatch
            );
            for (int r = 0; r < 2; r++) {
                if (runner[r].idx < 0) {
                    continue;
                }
                const BeliefZone& rz = m_zones.ObjectAt(runner[r].idx + 1);
                gi.Printf(
                    "  runner-up: zone %3d @ (%.0f %.0f %.0f)  score=%.2f  bel=%.2f\n",
                    runner[r].idx, rz.centroid.x, rz.centroid.y, rz.centroid.z,
                    runner[r].score, rz.belief
                );
            }
        }

        m_iCurrentTargetZone = bestIndex;
        // Lock to this target for 2-4 seconds
        m_iTargetLockTime = level.inttime + 2000 + (int)G_Random(2000);
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

Return the direction from myPos toward the best zone.
Returns vec_zero if no zone has significant belief.
====================
*/
Vector BotBeliefMap::GetHighestBeliefDir(Vector myPos)
{
    int bestIndex = GetBestZone(myPos);
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

Return the centroid of the best zone to investigate.
Returns vec_zero if no zone has significant belief.
====================
*/
Vector BotBeliefMap::GetHighestBeliefPos(Vector myPos)
{
    int bestIndex = GetBestZone(myPos);
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

/*
====================
MarkVisited

// Added in OPM
//  Increment visit count for a zone when the bot searches it and finds
//  no enemies. Higher visit counts reduce the zone's attractiveness,
//  encouraging exploration of new areas.
====================
*/
void BotBeliefMap::MarkVisited(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone = m_zones.ObjectAt(zoneIndex + 1);
    zone.visitCount++;
    zone.lastVisitTime = level.inttime;
}

/*
====================
ResetVisitsOnSighting

// Added in OPM
//  Reset visit count for a zone when an enemy is actually found there.
//  This makes the zone attractive again since it proved to be dangerous.
====================
*/
void BotBeliefMap::ResetVisitsOnSighting(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone = m_zones.ObjectAt(zoneIndex + 1);
    zone.visitCount  = 0;
}

/*
====================
MarkPathBlocked

// Added in OPM
//  Mark a zone as unreachable when the bot fails to path there after
//  repeated attempts. The zone will be skipped in GetBestZone() until
//  the block duration expires.
====================
*/
void BotBeliefMap::MarkPathBlocked(Vector pos)
{
    if (!m_bInitialized) {
        return;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return;
    }

    BeliefZone& zone     = m_zones.ObjectAt(zoneIndex + 1);
    zone.pathBlockedTime = level.inttime;
}

/*
====================
IsPathBlocked

// Added in OPM
//  Check if a position is in a path-blocked zone.
====================
*/
bool BotBeliefMap::IsPathBlocked(Vector pos) const
{
    if (!m_bInitialized) {
        return false;
    }

    int zoneIndex = FindZoneForPos(pos);
    if (zoneIndex < 0 || zoneIndex >= m_zones.NumObjects()) {
        return false;
    }

    const BeliefZone& zone              = m_zones.ObjectAt(zoneIndex + 1);
    int               pathBlockDuration = (int)(m_pParams->beliefPathBlockTime * 1000.0f);

    return zone.pathBlockedTime > 0 && level.inttime - zone.pathBlockedTime < pathBlockDuration;
}

// Removed in OPM
//  AddFailedTarget and IsNearFailedTarget removed — the failed target system
//  poisoned large map areas. Zone-level IsPathBlocked is sufficient.

/*
====================
ResetTargetLock

// Added in OPM
//  Break hysteresis so GetBestZone re-evaluates on the next call.
====================
*/
void BotBeliefMap::ResetTargetLock()
{
    m_iTargetLockTime = 0;
}

/*
====================
PrintGrid

// Added in OPM
//  Print the belief map to the console.
//
//  Navmesh mode: lists the top 16 zones by belief score, showing position
//  (X Y Z), belief value, visit count, and whether the zone is blocked. The
//  bot's zone is highlighted with @. This replaces the flat 2D grid since
//  zones no longer map 1:1 to XY cells.
//
//  Flat-grid mode: prints the original top-down 2D grid with symbols.
====================
*/
void BotBeliefMap::PrintGrid(Vector botPos) const
{
    if (!m_bInitialized) {
        gi.Printf("Belief map not initialized\n");
        return;
    }

    int pathBlockDuration = (int)(m_pParams->beliefPathBlockTime * 1000.0f);
    int botZone           = FindZoneForPos(botPos);

    // Added in OPM
    //  Navmesh mode: print per-zone list sorted by belief (top 16).
    if (m_bNavMeshMode) {
        float maxBelief = 0.0f;
        for (int i = 1; i <= m_zones.NumObjects(); i++) {
            if (m_zones.ObjectAt(i).belief > maxBelief) {
                maxBelief = m_zones.ObjectAt(i).belief;
            }
        }

        gi.Printf(
            "Belief map (navmesh) %d zones  max=%.2f  bot=%d\n", m_zones.NumObjects(), maxBelief, botZone
        );

        // Collect indices of zones with non-zero belief, sorted descending
        // Use simple insertion sort (zone count is small)
        int   sortedIdx[16];
        float sortedBelief[16];
        int   sortedCount = 0;

        for (int i = 1; i <= m_zones.NumObjects(); i++) {
            const BeliefZone& zone = m_zones.ObjectAt(i);
            if (zone.belief < 0.01f && i - 1 != botZone) {
                continue;
            }
            // Insert into sorted list (max 16 entries)
            bool insert = sortedCount < 16;
            if (!insert && zone.belief > sortedBelief[15]) {
                insert = true;
            }
            if (insert) {
                // Find insertion position
                int ins = (sortedCount < 16) ? sortedCount : 15;
                for (int j = 0; j < sortedCount && j < 16; j++) {
                    if (zone.belief > sortedBelief[j]) {
                        ins = j;
                        break;
                    }
                }
                // Shift down
                int cap = Q_min(sortedCount, 15);
                for (int j = cap; j > ins; j--) {
                    sortedIdx[j]    = sortedIdx[j - 1];
                    sortedBelief[j] = sortedBelief[j - 1];
                }
                sortedIdx[ins]    = i - 1; // 0-based
                sortedBelief[ins] = zone.belief;
                if (sortedCount < 16) {
                    sortedCount++;
                }
            }
        }

        for (int s = 0; s < sortedCount; s++) {
            int               idx  = sortedIdx[s];
            const BeliefZone& zone = m_zones.ObjectAt(idx + 1);
            bool              isBlocked =
                zone.pathBlockedTime > 0 && level.inttime - zone.pathBlockedTime < pathBlockDuration;
            char marker = (idx == botZone) ? '@' : ' ';
            gi.Printf(
                " %c zone %3d  pos(%.0f %.0f %.0f)  belief=%.2f  visits=%d%s\n",
                marker,
                idx,
                zone.centroid.x,
                zone.centroid.y,
                zone.centroid.z,
                zone.belief,
                zone.visitCount,
                isBlocked ? "  BLOCKED" : ""
            );
        }
        return;
    }

    // Flat-grid mode: print original 2D grid
    int botX = (botZone >= 0) ? (botZone % m_iGridWidth) : -1;
    int botY = (botZone >= 0) ? (botZone / m_iGridWidth) : -1;

    float maxBelief = 0.0f;
    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        if (m_zones.ObjectAt(i).belief > maxBelief) {
            maxBelief = m_zones.ObjectAt(i).belief;
        }
    }

    gi.Printf("Belief grid %dx%d  cell=%.0f  max=%.2f\n", m_iGridWidth, m_iGridHeight, m_fCellSize, maxBelief);

    // Print top-down, Y descending so north is up
    for (int y = m_iGridHeight - 1; y >= 0; y--) {
        char line[256];
        int  len = 0;

        for (int x = 0; x < m_iGridWidth && len < 254; x++) {
            int               idx  = y * m_iGridWidth + x;
            const BeliefZone& zone = m_zones.ObjectAt(idx + 1);

            bool isBlocked = zone.pathBlockedTime > 0
                          && level.inttime - zone.pathBlockedTime < pathBlockDuration;

            char c;
            if (x == botX && y == botY) {
                c = '@';
            } else if (isBlocked) {
                c = 'X';
            } else if (zone.belief < 0.01f) {
                c = '.';
            } else if (zone.belief < 0.25f) {
                c = 'o';
            } else if (zone.belief < 0.5f) {
                c = 'O';
            } else {
                c = '#';
            }

            line[len++] = c;
            line[len++] = ' ';
        }

        line[len] = '\0';
        gi.Printf("%s\n", line);
    }
}
