// playerbot_visibility.cpp: Precomputed tactical visibility matrix between navigation nodes.

#include "playerbot_visibility.h"
#include "g_local.h"
#include "g_utils.h"
#include "navigate.h"
#include "debuglines.h"

// Added in OPM
//  Bakes a per-map tactical visibility matrix at map load.
//  For every pair of navigation nodes within maxTacticalRange,
//  traces are performed at standing and crouch eye heights.
//  Results are stored sparsely for O(1) amortized lookup.

static constexpr float MAX_TACTICAL_RANGE    = 4096.0f;
static constexpr float MAX_TACTICAL_RANGE_SQ = MAX_TACTICAL_RANGE * MAX_TACTICAL_RANGE;
static constexpr float STANDING_EYE_OFFSET   = 82.0f;
static constexpr float CROUCH_EYE_OFFSET     = 48.0f;

static bool IsFiniteVec3(const vec3_t vec)
{
    return isfinite(vec[0]) && isfinite(vec[1]) && isfinite(vec[2]);
}

BotVisibilityMatrix::BotVisibilityMatrix()
    : m_bBaked(false)
{}

void BotVisibilityMatrix::Reset()
{
    m_entries.FreeObjectList();
    m_bBaked = false;
}

void BotVisibilityMatrix::Bake()
{
    int nodeCount = PathSearch::nodecount;
    if (nodeCount <= 0) {
        Reset();
        gi.Printf("BotVisibilityMatrix: no path nodes, skipping bake\n");
        return;
    }

    int startTime = gi.Milliseconds();

    // Allocate per-node entry lists
    Reset();
    m_entries.Resize(nodeCount);
    for (int i = 0; i < nodeCount; i++) {
        m_entries.AddObject();
    }

    vec3_t mins = {0, 0, 0};
    vec3_t maxs = {0, 0, 0};

    int totalPairsChecked = 0;
    int totalVisible       = 0;

    for (int i = 0; i < nodeCount; i++) {
        PathNode *nodeA = PathSearch::pathnodes[i];
        if (!nodeA) {
            continue;
        }

        vec3_t posAStand;
        vec3_t posACrouch;
        posAStand[0] = posACrouch[0] = nodeA->origin[0];
        posAStand[1] = posACrouch[1] = nodeA->origin[1];
        posAStand[2]  = nodeA->origin[2] + STANDING_EYE_OFFSET;
        posACrouch[2] = nodeA->origin[2] + CROUCH_EYE_OFFSET;

        if (!IsFiniteVec3(posAStand) || !IsFiniteVec3(posACrouch)) {
            continue;
        }

        for (int j = i + 1; j < nodeCount; j++) {
            PathNode *nodeB = PathSearch::pathnodes[j];
            if (!nodeB) {
                continue;
            }

            float dx = nodeA->origin[0] - nodeB->origin[0];
            float dy = nodeA->origin[1] - nodeB->origin[1];
            float dz = nodeA->origin[2] - nodeB->origin[2];
            float distSq = dx * dx + dy * dy + dz * dz;

            if (!isfinite(distSq) || distSq <= 0.0f || distSq > MAX_TACTICAL_RANGE_SQ) {
                continue;
            }

            totalPairsChecked++;

            vec3_t posBStand;
            vec3_t posBCrouch;
            posBStand[0] = posBCrouch[0] = nodeB->origin[0];
            posBStand[1] = posBCrouch[1] = nodeB->origin[1];
            posBStand[2]  = nodeB->origin[2] + STANDING_EYE_OFFSET;
            posBCrouch[2] = nodeB->origin[2] + CROUCH_EYE_OFFSET;

            if (!IsFiniteVec3(posBStand) || !IsFiniteVec3(posBCrouch)) {
                continue;
            }

            float dist = sqrtf(distSq);
            if (!isfinite(dist)) {
                continue;
            }

            // Trace standing eye height
            qboolean standVisible = G_SightTrace(
                Vector(posAStand),
                Vector(mins),
                Vector(maxs),
                Vector(posBStand),
                (gentity_t *)NULL,
                (gentity_t *)NULL,
                MASK_CANSEE_NOENTS,
                qfalse,
                "BotVisibilityMatrix::Bake standing"
            );

            if (standVisible) {
                // Fully visible at standing height
                VisibilityEntry entryAB;
                entryAB.otherNode  = j;
                entryAB.distance   = dist;
                entryAB.crouchOnly = false;

                VisibilityEntry entryBA;
                entryBA.otherNode  = i;
                entryBA.distance   = dist;
                entryBA.crouchOnly = false;

                m_entries.ObjectAt(i + 1).AddObject(entryAB);
                m_entries.ObjectAt(j + 1).AddObject(entryBA);

                totalVisible++;
                continue;
            }

            // Standing blocked: check crouch height
            qboolean crouchVisible = G_SightTrace(
                Vector(posACrouch),
                Vector(mins),
                Vector(maxs),
                Vector(posBCrouch),
                (gentity_t *)NULL,
                (gentity_t *)NULL,
                MASK_CANSEE_NOENTS,
                qfalse,
                "BotVisibilityMatrix::Bake crouch"
            );

            if (crouchVisible) {
                VisibilityEntry entryAB;
                entryAB.otherNode  = j;
                entryAB.distance   = dist;
                entryAB.crouchOnly = true;

                VisibilityEntry entryBA;
                entryBA.otherNode  = i;
                entryBA.distance   = dist;
                entryBA.crouchOnly = true;

                m_entries.ObjectAt(i + 1).AddObject(entryAB);
                m_entries.ObjectAt(j + 1).AddObject(entryBA);

                totalVisible++;
            }
        }
    }

    m_bBaked = true;

    int elapsed = gi.Milliseconds() - startTime;
    gi.Printf(
        "BotVisibilityMatrix: baked %d nodes, %d pairs checked, %d visible, %.1f KB, %d ms\n",
        nodeCount,
        totalPairsChecked,
        totalVisible,
        MemoryFootprint() / 1024.0f,
        elapsed
    );
}

bool BotVisibilityMatrix::CanSee(int nodeA, int nodeB) const
{
    return FindEntry(nodeA, nodeB) != NULL;
}

bool BotVisibilityMatrix::CanSeeCrouch(int nodeA, int nodeB) const
{
    const VisibilityEntry *entry = FindEntry(nodeA, nodeB);
    return entry != NULL && entry->crouchOnly;
}

float BotVisibilityMatrix::Distance(int nodeA, int nodeB) const
{
    const VisibilityEntry *entry = FindEntry(nodeA, nodeB);
    if (entry) {
        return entry->distance;
    }
    return -1.0f;
}

void BotVisibilityMatrix::GetVisibleNodes(int fromNode, float maxRange, Container<int>& out) const
{
    out.FreeObjectList();

    if (!m_bBaked || fromNode < 0 || fromNode >= m_entries.NumObjects()) {
        return;
    }

    const Container<VisibilityEntry>& nodeEntries = m_entries.ObjectAt(fromNode + 1);
    for (int i = 1; i <= nodeEntries.NumObjects(); i++) {
        const VisibilityEntry& entry = nodeEntries.ObjectAt(i);
        if (entry.distance <= maxRange) {
            out.AddObject(entry.otherNode);
        }
    }
}

size_t BotVisibilityMatrix::MemoryFootprint() const
{
    size_t total = sizeof(*this);

    for (int i = 1; i <= m_entries.NumObjects(); i++) {
        const Container<VisibilityEntry>& nodeEntries = m_entries.ObjectAt(i);
        total += sizeof(Container<VisibilityEntry>);
        total += nodeEntries.NumObjects() * sizeof(VisibilityEntry);
    }

    return total;
}

const VisibilityEntry *BotVisibilityMatrix::FindEntry(int nodeA, int nodeB) const
{
    if (!m_bBaked || nodeA < 0 || nodeA >= m_entries.NumObjects() || nodeB < 0) {
        return NULL;
    }

    const Container<VisibilityEntry>& nodeEntries = m_entries.ObjectAt(nodeA + 1);
    for (int i = 1; i <= nodeEntries.NumObjects(); i++) {
        if (nodeEntries.ObjectAt(i).otherNode == nodeB) {
            return &nodeEntries.ObjectAt(i);
        }
    }

    return NULL;
}

void BotVisibilityMatrix::DrawDebug(const Vector& fromPos)
{
    if (!m_bBaked) {
        return;
    }

    // Find the nearest node to fromPos
    int   bestNode = -1;
    float bestDist = 999999.0f;

    for (int i = 0; i < PathSearch::nodecount; i++) {
        PathNode *node = PathSearch::pathnodes[i];
        if (!node) {
            continue;
        }

        float dx = fromPos.x - node->origin[0];
        float dy = fromPos.y - node->origin[1];
        float dz = fromPos.z - node->origin[2];
        float d  = dx * dx + dy * dy + dz * dz;

        if (d < bestDist) {
            bestDist = d;
            bestNode = i;
        }
    }

    if (bestNode < 0) {
        return;
    }

    const Container<VisibilityEntry>& nodeEntries = m_entries.ObjectAt(bestNode + 1);
    Vector startPos(
        PathSearch::pathnodes[bestNode]->origin[0],
        PathSearch::pathnodes[bestNode]->origin[1],
        PathSearch::pathnodes[bestNode]->origin[2] + STANDING_EYE_OFFSET
    );

    for (int i = 1; i <= nodeEntries.NumObjects(); i++) {
        const VisibilityEntry& entry = nodeEntries.ObjectAt(i);
        PathNode              *other = PathSearch::pathnodes[entry.otherNode];
        if (!other) {
            continue;
        }

        Vector endPos(other->origin[0], other->origin[1], other->origin[2] + STANDING_EYE_OFFSET);

        if (entry.crouchOnly) {
            // Yellow for crouch-only visibility
            G_DebugLine(startPos, endPos, 1.0f, 1.0f, 0.0f, 0.5f);
        } else {
            // Green for full visibility
            G_DebugLine(startPos, endPos, 0.0f, 1.0f, 0.0f, 0.5f);
        }
    }
}
