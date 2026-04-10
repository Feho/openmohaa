// playerbot_memory.cpp: Bot memory and coverage tracking.

// Added in OPM
//  BotMemory: ring buffer of recent perception events replacing
//  the old belief-map system for "what happened recently" queries.
//  BotCoverageMap: per-bot nav-node visit tracking so idle patrol
//  prefers unexplored areas instead of looping the same corridor.

#include "playerbot_memory.h"
#include "g_local.h"
#include "level.h"
#include "navigate.h"
#include "debuglines.h"

// ============================================================
// BotMemory
// ============================================================

BotMemory::BotMemory()
    : m_count(0)
    , m_head(0)
{
    memset(m_entries, 0, sizeof(m_entries));
}

void BotMemory::Remember(const Vector& pos, int type, float weight)
{
    BotRememberedEvent& e = m_entries[m_head];
    e.pos    = pos;
    e.type   = type;
    e.time   = level.inttime;
    e.weight = Q_clamp_float(weight, 0.0f, 1.0f);

    m_head = (m_head + 1) % kCapacity;
    if (m_count < kCapacity) {
        m_count++;
    }
}

void BotMemory::Tick(int now)
{
    for (int i = 0; i < m_count; i++) {
        BotRememberedEvent& e = m_entries[i];
        if (e.time == 0) {
            continue;
        }

        int age = now - e.time;
        if (age > kMaxAgeMs) {
            e.time   = 0;
            e.weight = 0.0f;
        }
    }
}

bool BotMemory::HasRecent(int maxAgeMs) const
{
    for (int i = 0; i < m_count; i++) {
        if (m_entries[i].time && level.inttime - m_entries[i].time < maxAgeMs) {
            return true;
        }
    }
    return false;
}

/*
====================
GetMostRelevantPos

Score each remembered event by recency and proximity:
  score = weight * exp(-age / decay) / (1 + distance / scale)
Returns true and writes outPos if something recent enough exists.
====================
*/
bool BotMemory::GetMostRelevantPos(const Vector& origin, int now, Vector& outPos) const
{
    float bestScore = 0.0f;
    bool  found     = false;

    for (int i = 0; i < m_count; i++) {
        const BotRememberedEvent& e = m_entries[i];
        if (e.time == 0) {
            continue;
        }

        int age = now - e.time;
        if (age > kMaxAgeMs) {
            continue;
        }

        float ageFactor  = expf(-(float)age / (float)kDecayMs);
        float dist       = (e.pos - origin).length();
        float distFactor = 1.0f / (1.0f + dist / kDistScale);

        float score = e.weight * ageFactor * distFactor;

        if (score > bestScore) {
            bestScore = score;
            outPos    = e.pos;
            found     = true;
        }
    }

    return found;
}

int BotMemory::Count() const
{
    int active = 0;
    for (int i = 0; i < m_count; i++) {
        if (m_entries[i].time) {
            active++;
        }
    }
    return active;
}

void BotMemory::Clear()
{
    memset(m_entries, 0, sizeof(m_entries));
    m_count = 0;
    m_head  = 0;
}

// ============================================================
// BotCoverageMap
// ============================================================

BotCoverageMap::BotCoverageMap()
    : m_bInitialized(false)
{}

// Called on map change or controller destruction, not on respawn.
void BotCoverageMap::Reset()
{
    m_lastVisitedTime.FreeObjectList();
    m_bInitialized = false;
}

void BotCoverageMap::MarkVisited(int nodeIdx, int time)
{
    int nodeCount = PathSearch::nodecount;
    if (nodeCount <= 0) {
        return;
    }

    if (!m_bInitialized || m_lastVisitedTime.NumObjects() != nodeCount) {
        m_lastVisitedTime.FreeObjectList();
        m_lastVisitedTime.Resize(nodeCount);
        for (int i = 0; i < nodeCount; i++) {
            m_lastVisitedTime.AddObject(0);
        }
        m_bInitialized = true;
    }

    if (nodeIdx >= 0 && nodeIdx < m_lastVisitedTime.NumObjects()) {
        m_lastVisitedTime.ObjectAt(nodeIdx + 1) = time;
    }
}

int BotCoverageMap::GetLastVisitedTime(int nodeIdx) const
{
    if (!m_bInitialized || nodeIdx < 0 || nodeIdx >= m_lastVisitedTime.NumObjects()) {
        return 0;
    }

    return m_lastVisitedTime.ObjectAt(nodeIdx + 1);
}

/*
====================
PickExplorationTarget

Pick the oldest-visited reachable node within radius of origin,
biased by proximity so patrol doesn't send the bot across the map.

Score = staleness * proximityFactor
  staleness = how long since visited (or kCoverageDecayMs if never visited)
  proximityFactor = 1.0 at origin, 0.3 at edge of radius
====================
*/
int BotCoverageMap::PickExplorationTarget(const Vector& origin, float radius, int now) const
{
    if (!m_bInitialized || radius <= 0.0f) {
        return -1;
    }

    int   nodeCount = PathSearch::nodecount;
    int   visitCount = m_lastVisitedTime.NumObjects();
    float radiusSq  = radius * radius;
    int   bestNode  = -1;
    float bestScore = -1.0f;

    if (visitCount <= 0) {
        return -1;
    }

    if (nodeCount > visitCount) {
        nodeCount = visitCount;
    }

    for (int i = 0; i < nodeCount; i++) {
        PathNode *node = PathSearch::pathnodes[i];
        if (!node) {
            continue;
        }

        float dx = node->origin.x - origin.x;
        float dy = node->origin.y - origin.y;
        float dz = node->origin.z - origin.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq > radiusSq) {
            continue;
        }

        float dist = sqrtf(distSq);

        int lastVisited = m_lastVisitedTime.ObjectAt(i + 1);
        int staleness;
        if (lastVisited == 0) {
            staleness = kCoverageDecayMs;
        } else {
            staleness = now - lastVisited;
            if (staleness > kCoverageDecayMs) {
                staleness = kCoverageDecayMs;
            }
        }

        float proximityFactor = 1.0f - (dist / radius) * 0.7f;
        float score           = (float)staleness * proximityFactor;

        if (score > bestScore) {
            bestScore = score;
            bestNode  = i;
        }
    }

    return bestNode;
}

/*
====================
DrawDebug

Draw debug visualization of coverage state. Nodes are colored by
how recently they were visited: green = recently visited, red = stale/unvisited.
====================
*/
void BotCoverageMap::DrawDebug(const Vector& botOrigin, int now) const
{
    if (!m_bInitialized) {
        return;
    }

    int nodeCount = PathSearch::nodecount;

    for (int i = 0; i < nodeCount; i++) {
        PathNode *node = PathSearch::pathnodes[i];
        if (!node) {
            continue;
        }

        float dx = node->origin.x - botOrigin.x;
        float dy = node->origin.y - botOrigin.y;
        float distSq = dx * dx + dy * dy;

        // Only draw nearby nodes to avoid visual clutter
        if (distSq > 2048.0f * 2048.0f) {
            continue;
        }

        int lastVisited = m_lastVisitedTime.ObjectAt(i + 1);
        float staleness;
        if (lastVisited == 0) {
            staleness = 1.0f;
        } else {
            staleness = (float)(now - lastVisited) / (float)kCoverageDecayMs;
            if (staleness > 1.0f) {
                staleness = 1.0f;
            }
            if (staleness < 0.0f) {
                staleness = 0.0f;
            }
        }

        // Green = recently visited, red = stale
        float r = staleness;
        float g = 1.0f - staleness;
        float b = 0.0f;

        Vector pos = node->origin;
        G_DebugCircle((float *)pos, 24.0f, r, g, b, 0.6f, qtrue);
    }
}
