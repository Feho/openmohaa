#pragma once

#include "../corepp/vector.h"
#include "navigation_path.h"

class Entity;

static constexpr float TACTICAL_SPOT_VIEWHEIGHT = 56.0f;

struct TacticalSpot {
    Vector standPos;
    Vector lookDir;
    float  forwardReach;
    float  score;
    int    teamnum;
    int    lastUsedMs;
    int    lastValidatedMs;
    int    validationFailures;
    int    occupantEntNum;
    bool   active;
};

class BotTacticalMemory
{
public:
    static const int MAX_SPOTS_PER_TEAM = 16;
    static const int MAX_SPOTS_TOTAL    = MAX_SPOTS_PER_TEAM * 2;

    void Init();
    void Cleanup();

    bool TryRecordSpot(const Vector& standPos, const Vector& lookDir, int teamnum, Entity *passEnt);

    int QueryBestSpot(
        int teamnum,
        const Vector& fromPos,
        float maxRadius,
        Entity *pathEnt,
        const PathSearchParameter& params
    );

    void SetOccupant(int spotIndex, int entNum);
    void ReleaseOccupant(int entNum);
    void RevalidateSpots(Entity *passEnt);

    const TacticalSpot& GetSpot(int index) const;

private:
    bool IsDuplicate(const Vector& standPos, const Vector& lookDir, int teamnum) const;
    int  FindLRUSlot(int teamnum) const;
    static bool EvaluateSpot(const Vector& standPos, const Vector& lookDir, Entity *passEnt, float& outScore, float& outReach);

private:
    TacticalSpot m_spots[MAX_SPOTS_TOTAL];
    int          m_numSpots;
};
