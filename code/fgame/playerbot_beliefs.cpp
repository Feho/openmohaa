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

BotBeliefMap::BotBeliefMap()
    : m_bInitialized(false)
    , m_iVisClearIndex(0)
    , m_fCellSize(0)
    , m_iGridWidth(0)
    , m_iGridHeight(0)
    , m_iCurrentTargetZone(-1)
    , m_iTargetLockTime(0)
    , m_pParams(NULL)
{}

void BotBeliefMap::SetParams(const BotParams *params)
{
    m_pParams = params;
}

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
            zone.visitCount      = 0;
            zone.lastVisitTime   = 0;
            zone.pathBlockedTime = 0;
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
        // Also check if zone is near a failed target
        bool nearFailedTarget = IsNearFailedTarget(currentZone.centroid);
        if (!isBlocked && !nearFailedTarget && currentZone.belief >= m_pParams->beliefMinPatrol
            && level.inttime < m_iTargetLockTime) {
            return m_iCurrentTargetZone;
        }
    }

    float minBelief = m_pParams->beliefMinPatrol;
    int   bestIndex = -1;
    float bestScore = -999.0f;

    // Calculate max distance for normalization (diagonal of world)
    float worldWidth  = m_vWorldMaxs.x - m_vWorldMins.x;
    float worldHeight = m_vWorldMaxs.y - m_vWorldMins.y;
    float maxDist     = sqrtf(worldWidth * worldWidth + worldHeight * worldHeight);

    // Visit decay time in milliseconds
    int visitDecayTime = (int)(m_pParams->beliefVisitDecay * 1000.0f);

    for (int i = 1; i <= m_zones.NumObjects(); i++) {
        const BeliefZone& zone = m_zones.ObjectAt(i);

        // Skip zones that are currently path-blocked
        if (zone.pathBlockedTime > 0 && level.inttime - zone.pathBlockedTime < pathBlockDuration) {
            continue;
        }

        // Added in OPM
        //  Skip zones near failed target positions (unreachable areas)
        if (IsNearFailedTarget(zone.centroid)) {
            continue;
        }

        // Novelty bonus: attract to never-visited zones (even if belief is 0)
        float noveltyBonus = (zone.visitCount == 0) ? m_pParams->beliefNoveltyBonus : 0.0f;

        // Skip zones with no belief AND no novelty bonus
        if (zone.belief < minBelief && noveltyBonus <= 0.0f) {
            continue;
        }

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

/*
====================
AddFailedTarget

// Added in OPM
//  Record a target position the bot couldn't reach. Any future destination
//  within a large radius of this position will be rejected. This blocks
//  entire areas (like the space behind a wall) rather than single points.
====================
*/
void BotBeliefMap::AddFailedTarget(Vector targetPos)
{
    int   stuckTime = (int)(m_pParams->stuckTime * 1000.0f);
    float radius    = m_pParams->stuckRadius;

    // Remove expired entries
    for (int i = m_failedTargets.NumObjects(); i >= 1; i--) {
        if (level.inttime - m_failedTargets.ObjectAt(i).time > stuckTime) {
            m_failedTargets.RemoveObjectAt(i);
        }
    }

    // Check if we already have a failed target nearby (avoid duplicates)
    float radiusSq = radius * radius;
    for (int i = 1; i <= m_failedTargets.NumObjects(); i++) {
        Vector delta = m_failedTargets.ObjectAt(i).pos - targetPos;
        if (delta.lengthXYSquared() < radiusSq) {
            // Update existing entry's time
            m_failedTargets.ObjectAt(i).time = level.inttime;
            return;
        }
    }

    // Add new failed target
    FailedTarget ft;
    ft.pos  = targetPos;
    ft.time = level.inttime;
    m_failedTargets.AddObject(ft);
}

/*
====================
IsNearFailedTarget

// Added in OPM
//  Check if a position is near any failed target. Used to reject destinations
//  that are close to places the bot already knows it can't reach.
====================
*/
bool BotBeliefMap::IsNearFailedTarget(Vector pos) const
{
    if (m_failedTargets.NumObjects() == 0) {
        return false;
    }

    int   stuckTime = (int)(m_pParams->stuckTime * 1000.0f);
    float radiusSq  = m_pParams->stuckRadius * m_pParams->stuckRadius;

    for (int i = 1; i <= m_failedTargets.NumObjects(); i++) {
        const FailedTarget& ft = m_failedTargets.ObjectAt(i);

        // Skip expired entries
        if (level.inttime - ft.time > stuckTime) {
            continue;
        }

        Vector delta = ft.pos - pos;
        if (delta.lengthXYSquared() < radiusSq) {
            return true;
        }
    }

    return false;
}

/*
====================
PrintGrid

// Added in OPM
//  Print the belief grid to the console as a text map.
//  Each cell shows its belief intensity: . low, o medium, O high, X blocked.
//  The bot's position is marked with @.
====================
*/
void BotBeliefMap::PrintGrid(Vector botPos) const
{
    if (!m_bInitialized) {
        gi.Printf("Belief map not initialized\n");
        return;
    }

    int botZone = FindZoneForPos(botPos);
    int botX    = (botZone >= 0) ? (botZone % m_iGridWidth) : -1;
    int botY    = (botZone >= 0) ? (botZone / m_iGridWidth) : -1;

    int   pathBlockDuration = (int)(m_pParams->beliefPathBlockTime * 1000.0f);
    float maxBelief         = 0.0f;

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
            bool isFailed = IsNearFailedTarget(zone.centroid);

            char c;
            if (x == botX && y == botY) {
                c = '@';
            } else if (isBlocked) {
                c = 'X';
            } else if (isFailed) {
                c = 'F';
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

    // Print failed targets
    if (m_failedTargets.NumObjects() > 0) {
        int stuckTime = (int)(m_pParams->stuckTime * 1000.0f);

        gi.Printf("Failed targets:\n");
        for (int i = 1; i <= m_failedTargets.NumObjects(); i++) {
            const FailedTarget& ft      = m_failedTargets.ObjectAt(i);
            int                 elapsed = level.inttime - ft.time;

            if (elapsed < stuckTime) {
                gi.Printf(
                    "  (%.0f, %.0f) %ds ago, expires in %ds\n",
                    ft.pos.x,
                    ft.pos.y,
                    elapsed / 1000,
                    (stuckTime - elapsed) / 1000
                );
            }
        }
    }
}
