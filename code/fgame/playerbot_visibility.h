// playerbot_visibility.h: Precomputed tactical visibility matrix between navigation nodes.

#pragma once

#include "../corepp/vector.h"
#include "../corepp/container.h"

// Added in OPM
//  Precomputed sightline data between navigation nodes, baked once
//  per map at load time. Queried by the planner and target scorer.
//
//  For every pair of navigation nodes within maxTacticalRange,
//  a single trace determines whether they can see each other.
//  Results are stored in a sparse per-node list for O(1) amortized lookup.

struct VisibilityEntry {
    int   otherNode;
    float distance;
    bool  crouchOnly; // visible when crouched but not when standing
};

class BotVisibilityMatrix
{
public:
    BotVisibilityMatrix();

    void Reset();
    void Bake();
    bool CanSee(int nodeA, int nodeB) const;
    bool CanSeeCrouch(int nodeA, int nodeB) const;

    float Distance(int nodeA, int nodeB) const;

    void GetVisibleNodes(int fromNode, float maxRange, Container<int>& out) const;

    size_t MemoryFootprint() const;
    bool   IsBaked() const { return m_bBaked; }

    void DrawDebug(const Vector& fromPos);

private:
    const VisibilityEntry *FindEntry(int nodeA, int nodeB) const;

    Container<Container<VisibilityEntry>> m_entries;
    bool                                  m_bBaked;
};
