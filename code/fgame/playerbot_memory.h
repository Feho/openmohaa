// playerbot_memory.h: Bot memory and coverage tracking.

#pragma once

#include "../corepp/vector.h"
#include "../corepp/container.h"

// Added in OPM
//  Fixed-capacity ring buffer of recent perception events. Replaces
//  the old belief-map system for the "what happened recently" query.
//  Consumers are the state machine (pre-aim, curious fallback) and the planner (future).

struct BotRememberedEvent {
    Vector pos;
    int    type;    // AI_EVENT_* -- stored for future scoring, not yet used
    int    time;
    float  weight;
};

class BotMemory
{
public:
    BotMemory();

    void Remember(const Vector& pos, int type, float weight);
    void Tick(int now);

    bool HasRecent(int maxAgeMs) const;
    bool GetMostRelevantPos(const Vector& origin, int now, Vector& outPos) const;
    int  Count() const;
    void Clear();

private:
    static constexpr int kCapacity   = 8;
    static constexpr int kMaxAgeMs   = 30000;
    static constexpr int kDecayMs    = 15000;
    static constexpr float kDistScale = 2048.0f;

    BotRememberedEvent m_entries[kCapacity];
    int                m_count;
    int                m_head;
};

// Added in OPM
//  Tracks when each navigation node was last visited by this bot,
//  so idle patrol prefers unexplored areas. Backed by the existing
//  navigation graph — no new spatial structure.

class BotCoverageMap
{
public:
    BotCoverageMap();

    void MarkVisited(int nodeIdx, int time);
    int  GetLastVisitedTime(int nodeIdx) const;

    int PickExplorationTarget(const Vector& origin, float radius, int now) const;

    void Reset();
    void DrawDebug(const Vector& botOrigin, int now) const;

private:
    static constexpr int kCoverageDecayMs = 120000;

    Container<int> m_lastVisitedTime;
    bool           m_bInitialized;
};
