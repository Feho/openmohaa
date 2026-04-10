// playerbot_planner.h: Bot decision planner producing Goal objects.

#pragma once

#include "sentient.h"

class BotController;

// Added in OPM
//  A Goal is the planner's answer to "what should this bot do right
//  now?" The state machine's job is to execute it. Goals are cheap
//  value types; the planner replaces them freely.
enum class BotGoalType {
    Idle,
    Explore,
    Investigate, // curious -- chase a sense event
    Engage,      // attack a known enemy
    Flank,       // reach a position before engaging (future)
    Retreat,     // break contact (future)
};

struct BotGoal {
    BotGoalType       type;
    SafePtr<Sentient> targetEnemy;    // Engage
    Vector            targetPos;      // Explore / Investigate / Flank
    int               committedUntil; // level.inttime; don't downgrade before this
    int               createdAt;

    BotGoal()
        : type(BotGoalType::Idle)
        , targetPos(vec_zero)
        , committedUntil(0)
        , createdAt(0)
    {}
};

// Added in OPM
//  Produces a BotGoal at low frequency (~4 Hz). Reads m_senses, m_memory,
//  m_coverage, and the world; writes m_currentGoal. Never touches
//  rotation, movement, or fire buttons.
class BotPlanner
{
public:
    BotPlanner();

    void           SetController(BotController *ctrl);
    void           Tick(int now);
    void           Reset();
    const BotGoal& Current() const { return m_currentGoal; }

private:
    BotGoal   ChooseGoal(int now) const;
    bool      ShouldReplan(int now) const;
    bool      HasHardTrigger(int now) const;
    Sentient *FindBestVisibleEnemy() const;

    static int GoalPriority(BotGoalType type);

    BotController *m_controller;
    BotGoal        m_currentGoal;
    int            m_lastPlanTime;
};

const char *BotGoalTypeName(BotGoalType type);
