#include "playerbot_tactical_memory.h"

#include "entity.h"
#include "g_local.h"
#include "gamecvars.h"
#include "level.h"
#include <climits>

static Vector YawOffsetDirection(const Vector& dir, float yawOffset)
{
    Vector angles;
    Vector shifted;

    vectoangles(dir, angles);
    angles.y += yawOffset;
    AngleVectors(angles, shifted, NULL, NULL);
    VectorNormalizeFast(shifted);
    return shifted;
}

void BotTacticalMemory::Init()
{
    Cleanup();
}

void BotTacticalMemory::Cleanup()
{
    m_numSpots = 0;

    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        m_spots[i].standPos           = vec_zero;
        m_spots[i].lookDir            = vec_zero;
        m_spots[i].forwardReach       = 0.0f;
        m_spots[i].score              = 0.0f;
        m_spots[i].teamnum            = 0;
        m_spots[i].lastUsedMs         = 0;
        m_spots[i].lastValidatedMs    = 0;
        m_spots[i].validationFailures = 0;
        m_spots[i].occupantEntNum     = -1;
        m_spots[i].active             = false;
    }
}

bool BotTacticalMemory::TryRecordSpot(const Vector& standPos, const Vector& lookDir, int teamnum, Entity *passEnt)
{
    if (g_gametype->integer < GT_TEAM) {
        return false;
    }

    if (lookDir.lengthSquared() < Square(0.01f)) {
        return false;
    }

    float score;
    float reach;

    if (!EvaluateSpot(standPos, lookDir, passEnt, score, reach)) {
        return false;
    }

    if (score < g_bot_tactical_min_score->value || reach < g_bot_tactical_min_reach->value) {
        return false;
    }

    if (IsDuplicate(standPos, lookDir, teamnum)) {
        return false;
    }

    const int slot = FindLRUSlot(teamnum);
    if (slot < 0) {
        return false;
    }

    TacticalSpot& spot = m_spots[slot];
    const bool    fresh = !spot.active;

    spot.standPos           = standPos;
    spot.lookDir            = lookDir;
    spot.forwardReach       = reach;
    spot.score              = score;
    spot.teamnum            = teamnum;
    spot.lastUsedMs         = level.inttime;
    spot.lastValidatedMs    = level.inttime;
    spot.validationFailures = 0;
    spot.occupantEntNum     = -1;
    spot.active             = true;

    if (fresh) {
        ++m_numSpots;
    }

    if (g_bot_debug_tactical_spots->integer) {
        gi.Printf(
            "BOT tactical: recorded team=%d slot=%d score=%.2f reach=%.0f pos=(%.0f, %.0f, %.0f)\n",
            teamnum,
            slot,
            score,
            reach,
            standPos.x,
            standPos.y,
            standPos.z
        );
    }

    return true;
}

int BotTacticalMemory::QueryBestSpot(
    int teamnum,
    const Vector& fromPos,
    float maxRadius,
    Entity *pathEnt,
    const PathSearchParameter& params
)
{
    if (g_gametype->integer < GT_TEAM) {
        return -1;
    }

    float    bestScore = -1.0f;
    int      bestIndex = -1;
    IPather *pather    = IPather::CreatePather();

    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        TacticalSpot& spot = m_spots[i];

        if (!spot.active || spot.teamnum != teamnum) {
            continue;
        }

        if (spot.occupantEntNum != -1 && (!pathEnt || spot.occupantEntNum != pathEnt->entnum)) {
            continue;
        }

        if ((spot.standPos - fromPos).lengthSquared() > Square(maxRadius)) {
            continue;
        }

        if (spot.lastUsedMs && level.inttime < spot.lastUsedMs + 10000) {
            continue;
        }

        if (!pather->TestPath(fromPos, spot.standPos, params)) {
            continue;
        }

        if (spot.score > bestScore) {
            bestScore = spot.score;
            bestIndex = i;
        }
    }

    delete pather;

    if (g_bot_debug_tactical_spots->integer >= 2) {
        gi.Printf("BOT tactical: query team=%d result=%d score=%.2f\n", teamnum, bestIndex, bestScore);
    }

    return bestIndex;
}

void BotTacticalMemory::SetOccupant(int spotIndex, int entNum)
{
    if (spotIndex < 0 || spotIndex >= MAX_SPOTS_TOTAL || !m_spots[spotIndex].active) {
        return;
    }

    m_spots[spotIndex].occupantEntNum = entNum;
    m_spots[spotIndex].lastUsedMs     = level.inttime;
}

void BotTacticalMemory::ReleaseOccupant(int entNum)
{
    if (entNum < 0) {
        return;
    }

    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        if (m_spots[i].active && m_spots[i].occupantEntNum == entNum) {
            m_spots[i].occupantEntNum = -1;
            m_spots[i].lastUsedMs     = level.inttime;
        }
    }
}

void BotTacticalMemory::RevalidateSpots(Entity *passEnt)
{
    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        TacticalSpot& spot = m_spots[i];

        if (!spot.active) {
            continue;
        }

        float score;
        float reach;

        if (!EvaluateSpot(spot.standPos, spot.lookDir, passEnt, score, reach)
            || score < g_bot_tactical_min_score->value
            || reach < g_bot_tactical_min_reach->value) {
            ++spot.validationFailures;
            spot.lastValidatedMs = level.inttime;

            if (spot.validationFailures >= 3) {
                if (g_bot_debug_tactical_spots->integer) {
                    gi.Printf("BOT tactical: evict slot=%d after %d validation failures\n", i, spot.validationFailures);
                }

                spot.active         = false;
                spot.occupantEntNum = -1;
                --m_numSpots;
            }

            continue;
        }

        spot.score              = score;
        spot.forwardReach       = reach;
        spot.lastValidatedMs    = level.inttime;
        spot.validationFailures = 0;
    }
}

const TacticalSpot& BotTacticalMemory::GetSpot(int index) const
{
    assert(index >= 0 && index < MAX_SPOTS_TOTAL);
    return m_spots[index];
}

bool BotTacticalMemory::IsDuplicate(const Vector& standPos, const Vector& lookDir, int teamnum) const
{
    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        const TacticalSpot& spot = m_spots[i];

        if (!spot.active || spot.teamnum != teamnum) {
            continue;
        }

        if ((spot.standPos - standPos).lengthSquared() > Square(64.0f)) {
            continue;
        }

        if (DotProduct(spot.lookDir, lookDir) > 0.95f) {
            return true;
        }
    }

    return false;
}

int BotTacticalMemory::FindLRUSlot(int teamnum) const
{
    int inactiveSlot = -1;
    int oldestSlot   = -1;
    int oldestTime   = INT_MAX;
    int teamCount    = 0;

    for (int i = 0; i < MAX_SPOTS_TOTAL; ++i) {
        const TacticalSpot& spot = m_spots[i];

        if (!spot.active) {
            if (inactiveSlot < 0) {
                inactiveSlot = i;
            }
            continue;
        }

        if (spot.teamnum != teamnum) {
            continue;
        }

        ++teamCount;
        if (spot.lastUsedMs < oldestTime) {
            oldestTime = spot.lastUsedMs;
            oldestSlot = i;
        }
    }

    if (teamCount < MAX_SPOTS_PER_TEAM && inactiveSlot >= 0) {
        return inactiveSlot;
    }

    return oldestSlot;
}

bool BotTacticalMemory::EvaluateSpot(const Vector& standPos, const Vector& lookDir, Entity *passEnt, float& outScore, float& outReach)
{
    Vector origin = standPos + Vector(0, 0, TACTICAL_SPOT_VIEWHEIGHT);
    Vector dir    = lookDir;
    Vector backDir = dir * -1.0f;
    VectorNormalizeFast(dir);
    VectorNormalizeFast(backDir);

    const struct {
        Vector direction;
        float  dist;
    } traces[] = {
        {dir, 4096.0f},
        {YawOffsetDirection(dir, 15.0f), 2048.0f},
        {YawOffsetDirection(dir, 30.0f), 2048.0f},
        {YawOffsetDirection(dir, -15.0f), 2048.0f},
        {YawOffsetDirection(dir, -30.0f), 2048.0f},
        {backDir, 128.0f},
        {YawOffsetDirection(backDir, 45.0f), 128.0f},
        {YawOffsetDirection(backDir, -45.0f), 128.0f}
    };

    trace_t results[ARRAY_LEN(traces)];

    for (unsigned int i = 0; i < ARRAY_LEN(traces); ++i) {
        Vector end = origin + traces[i].direction * traces[i].dist;
        results[i] = G_Trace(origin, vec_zero, vec_zero, end, passEnt, MASK_SOLID, false, "BotTacticalMemory");
    }

    outReach = results[0].fraction * 4096.0f;
    if (outReach < g_bot_tactical_min_reach->value) {
        return false;
    }

    float latScore = 0.0f;
    for (int i = 1; i <= 4; ++i) {
        latScore += results[i].fraction;
    }
    latScore *= 0.25f;

    float rearCovered = 0.0f;
    for (int i = 5; i <= 7; ++i) {
        if (results[i].fraction < 0.75f) {
            rearCovered += 1.0f;
        }
    }

    const float rearScore    = rearCovered / 3.0f;
    const float forwardScore = Q_clamp_float(outReach / 4096.0f, 0.0f, 1.0f);

    outScore = forwardScore * 0.60f + latScore * 0.25f + rearScore * 0.15f;
    return true;
}
