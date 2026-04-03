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
#include "dm_manager.h"
#include "playerstart.h"

BotBeliefMap::BotBeliefMap()
    : m_bInitialized(false)
    , m_iVisClearIndex(0)
    , m_fCellSize(0)
    , m_iGridWidth(0)
    , m_iGridHeight(0)
    , m_iCurrentTargetZone(-1)
    , m_iTargetLockTime(0)
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
            zone.centroid.x     = worldMins.x + (x + 0.5f) * m_fCellSize;
            zone.centroid.y     = worldMins.y + (y + 0.5f) * m_fCellSize;
            zone.centroid.z     = (worldMins.z + worldMaxs.z) * 0.5f;
            zone.belief         = 0.0f;
            zone.lastUpdateTime = 0;
            zone.visitCount     = 0;
            zone.lastVisitTime  = 0;
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

    // Hysteresis: if we have a current target and it still has belief, stick with it
    // for a minimum time to avoid flip-flopping
    if (m_iCurrentTargetZone >= 0 && m_iCurrentTargetZone < m_zones.NumObjects()) {
        const BeliefZone& currentZone = m_zones.ObjectAt(m_iCurrentTargetZone + 1);
        if (currentZone.belief >= g_bot_belief_min_patrol->value && level.inttime < m_iTargetLockTime) {
            return m_iCurrentTargetZone;
        }
    }

    float minBelief = g_bot_belief_min_patrol->value;
    int   bestIndex = -1;
    float bestScore = -999.0f;

    // Calculate max distance for normalization (diagonal of world)
    float worldWidth  = m_vWorldMaxs.x - m_vWorldMins.x;
    float worldHeight = m_vWorldMaxs.y - m_vWorldMins.y;
    float maxDist     = sqrtf(worldWidth * worldWidth + worldHeight * worldHeight);

    // Visit decay time in milliseconds
    int visitDecayTime = (int)(g_bot_belief_visit_decay->value * 1000.0f);

    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        const BeliefZone& zone = m_zones.ObjectAt(i);

        // Novelty bonus: attract to never-visited zones (even if belief is 0)
        float noveltyBonus = (zone.visitCount == 0) ? g_bot_belief_novelty_bonus->value : 0.0f;

        // Skip zones with no belief AND no novelty bonus
        if (zone.belief < minBelief && noveltyBonus <= 0.0f) {
            continue;
        }

        // Calculate distance factor: closer zones score higher
        // Range: 1.0 (at bot position) to 0.3 (at max distance)
        Vector delta     = zone.centroid - myPos;
        delta.z          = 0; // 2D distance
        float dist       = delta.length();
        float distFactor = 1.0f - (dist / maxDist) * 0.7f;

        // Calculate effective visit count after decay
        int effectiveVisits = zone.visitCount;
        if (visitDecayTime > 0 && zone.lastVisitTime > 0) {
            int timeSinceVisit = level.inttime - zone.lastVisitTime;
            int decayedVisits  = timeSinceVisit / visitDecayTime;
            effectiveVisits    = Q_max(0, zone.visitCount - decayedVisits);
        }

        // Visit penalty: diminishing returns for repeated visits
        float visitPenalty = 1.0f / (1.0f + effectiveVisits * g_bot_belief_visit_penalty->value);

        // Jitter: prevent teammate clustering by adding random variance
        float jitter = G_CRandom(g_bot_belief_score_jitter->value);

        // Final score
        float score = (zone.belief * visitPenalty * distFactor) + noveltyBonus + jitter;

        if (score > bestScore) {
            bestScore = score;
            bestIndex = i - 1;
        }
    }

    // Update hysteresis state if we found a new target
    if (bestIndex >= 0 && bestIndex != m_iCurrentTargetZone) {
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
