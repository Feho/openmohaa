// playerbot_planner.cpp: Bot decision planner producing Goal objects.
//
// The planner runs at ~4 Hz and produces a single BotGoal. The state
// machine reads the current goal each frame and executes it. Decisions
// live here; execution lives in the state machine.

#include "playerbot.h"
#include "playerbot_planner.h"
#include "gamecvars.h"

static constexpr int kPlanIntervalMs = 250; // ~4 Hz

// Commitment durations per goal type (milliseconds).
// These prevent the planner from flipping between goals every tick
// when multiple options score closely.
static constexpr int kCommitEngage      = 2000;
static constexpr int kCommitInvestigate = 8000;
static constexpr int kCommitExplore     = 5000;
static constexpr int kCommitIdle        = 0;

const char *BotGoalTypeName(BotGoalType type)
{
    switch (type) {
    case BotGoalType::Idle:
        return "Idle";
    case BotGoalType::Explore:
        return "Explore";
    case BotGoalType::Investigate:
        return "Investigate";
    case BotGoalType::Engage:
        return "Engage";
    case BotGoalType::Flank:
        return "Flank";
    case BotGoalType::Retreat:
        return "Retreat";
    default:
        return "Unknown";
    }
}

int BotPlanner::GoalPriority(BotGoalType type)
{
    switch (type) {
    case BotGoalType::Idle:
        return 0;
    case BotGoalType::Explore:
        return 1;
    case BotGoalType::Investigate:
        return 2;
    case BotGoalType::Retreat:
        return 2;
    case BotGoalType::Engage:
        return 3;
    case BotGoalType::Flank:
        return 3;
    default:
        return 0;
    }
}

BotPlanner::BotPlanner()
    : m_controller(NULL)
    , m_lastPlanTime(0)
{}

void BotPlanner::SetController(BotController *ctrl)
{
    m_controller = ctrl;
}

void BotPlanner::Reset()
{
    m_currentGoal  = BotGoal();
    m_lastPlanTime = 0;
}

void BotPlanner::Tick(int now)
{
    if (!m_controller || !m_controller->getControlledEntity()) {
        return;
    }

    bool hardTrigger = HasHardTrigger(now);

    if (!ShouldReplan(now) && !hardTrigger) {
        return;
    }

    BotGoal candidate = ChooseGoal(now);

    // Commitment: don't downgrade goal type while committed
    if (!hardTrigger && now < m_currentGoal.committedUntil && candidate.type != m_currentGoal.type) {
        if (GoalPriority(candidate.type) < GoalPriority(m_currentGoal.type)) {
            m_lastPlanTime = now;
            return;
        }
    }

    // Log goal transitions
    if (candidate.type != m_currentGoal.type && g_bot_debug_planner->integer) {
        Player *ent = m_controller->getControlledEntity();

        if (candidate.type == BotGoalType::Engage && candidate.targetEnemy) {
            const char *enemyName = "unknown";
            if (candidate.targetEnemy->IsSubclassOfPlayer()) {
                enemyName = static_cast<Player *>(candidate.targetEnemy.Pointer())->client->pers.netname;
            }
            gi.Printf(
                "BOT %s: PLAN %s -> %s (target=%s, commit=%dms%s)\n",
                ent->client->pers.netname,
                BotGoalTypeName(m_currentGoal.type),
                BotGoalTypeName(candidate.type),
                enemyName,
                candidate.committedUntil - now,
                hardTrigger ? ", HARD" : ""
            );
        } else if (candidate.type == BotGoalType::Investigate) {
            gi.Printf(
                "BOT %s: PLAN %s -> %s (pos=(%.0f,%.0f,%.0f), commit=%dms%s)\n",
                ent->client->pers.netname,
                BotGoalTypeName(m_currentGoal.type),
                BotGoalTypeName(candidate.type),
                candidate.targetPos.x,
                candidate.targetPos.y,
                candidate.targetPos.z,
                candidate.committedUntil - now,
                hardTrigger ? ", HARD" : ""
            );
        } else {
            gi.Printf(
                "BOT %s: PLAN %s -> %s%s\n",
                ent->client->pers.netname,
                BotGoalTypeName(m_currentGoal.type),
                BotGoalTypeName(candidate.type),
                hardTrigger ? " (HARD)" : ""
            );
        }
    }

    m_currentGoal  = candidate;
    m_lastPlanTime = now;
}

bool BotPlanner::ShouldReplan(int now) const
{
    return now - m_lastPlanTime >= kPlanIntervalMs;
}

bool BotPlanner::HasHardTrigger(int now) const
{
    // Damage since last plan
    if (m_controller->m_senses.damagedTime > m_lastPlanTime) {
        return true;
    }

    // Current engage target became invalid (died, disconnected, etc.)
    if (m_currentGoal.type == BotGoalType::Engage && m_currentGoal.targetEnemy
        && !m_controller->IsValidEnemy(m_currentGoal.targetEnemy)) {
        return true;
    }

    // State finished its work but planner hasn't switched yet
    if (m_currentGoal.type == BotGoalType::Engage && !m_controller->m_combat.attackTime
        && !m_controller->m_enemy.enemy) {
        return true;
    }
    if (m_currentGoal.type == BotGoalType::Investigate && m_currentGoal.createdAt != 0
        && !m_controller->m_curious.time && now > m_currentGoal.createdAt + kPlanIntervalMs) {
        return true;
    }

    return false;
}

Sentient *BotPlanner::FindBestVisibleEnemy() const
{
    Player *controlledEnt = m_controller->getControlledEntity();
    if (!controlledEnt) {
        return NULL;
    }

    float     maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828);
    Sentient *bestEnemy   = NULL;
    float     bestDistSq  = 999999999.0f;

    for (int i = 1; i <= SentientList.NumObjects(); i++) {
        Sentient *sent = SentientList.ObjectAt(i);

        if (!m_controller->IsValidEnemy(sent)) {
            continue;
        }

        float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

        // Match combat visibility so the planner does not grant wider acquisition
        // than the execution layer can sustain.
        if (controlledEnt->CanSee(sent, 120, maxDistance, false)) {
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestEnemy  = sent;
            }
        }
    }

    return bestEnemy;
}

BotGoal BotPlanner::ChooseGoal(int now) const
{
    BotGoal goal;
    goal.createdAt = now;

    Player *ent = m_controller->getControlledEntity();
    if (!ent || ent->IsDead() || ent->IsSpectator()) {
        goal.type = BotGoalType::Idle;
        return goal;
    }

    // --- Engage priority (highest) ---

    // 1. Visible enemy
    Sentient *bestEnemy = FindBestVisibleEnemy();
    if (bestEnemy) {
        goal.type           = BotGoalType::Engage;
        goal.targetEnemy    = bestEnemy;
        goal.targetPos      = bestEnemy->origin;
        goal.committedUntil = now + kCommitEngage;
        return goal;
    }

    // 2. Damaged by a valid enemy we can't see yet
    if (m_controller->m_senses.damagedBy && m_controller->IsValidEnemy(m_controller->m_senses.damagedBy)
        && now - m_controller->m_senses.damagedTime < 5000) {
        goal.type           = BotGoalType::Engage;
        goal.targetEnemy    = m_controller->m_senses.damagedBy;
        goal.targetPos      = m_controller->m_senses.damagedFrom;
        goal.committedUntil = now + kCommitEngage;
        return goal;
    }

    // 3. Maintain engagement if attack timer still active (mirrors existing
    //    CheckCondition_Attack fallthrough when m_combat.attackTime hasn't expired)
    if (m_currentGoal.type == BotGoalType::Engage && m_controller->m_combat.attackTime
        && now <= m_controller->m_combat.attackTime) {
        goal.type           = BotGoalType::Engage;
        goal.targetEnemy    = m_controller->m_enemy.enemy;
        goal.targetPos      = m_controller->m_enemy.lastPos;
        goal.committedUntil = m_controller->m_combat.attackTime;
        return goal;
    }

    // --- Investigate priority ---

    // Heard something recently
    if (m_controller->m_senses.heardTime && now - m_controller->m_senses.heardTime < 20000) {
        goal.type           = BotGoalType::Investigate;
        goal.targetPos      = m_controller->m_senses.heardPos;
        goal.committedUntil = now + kCommitInvestigate;
        return goal;
    }

    // Ongoing investigation (state still has time on its timer)
    if (m_currentGoal.type == BotGoalType::Investigate && m_controller->m_curious.time
        && now <= m_controller->m_curious.time) {
        goal.type           = BotGoalType::Investigate;
        goal.targetPos      = m_controller->m_curious.targetPos;
        goal.committedUntil = m_controller->m_curious.time;
        return goal;
    }

    // Damage source we can't identify as a valid enemy (non-sentient, etc.)
    // Still worth investigating the direction
    if (m_controller->m_senses.damagedTime && now - m_controller->m_senses.damagedTime < 5000
        && m_controller->m_senses.damagedFrom != vec_zero) {
        goal.type           = BotGoalType::Investigate;
        goal.targetPos      = m_controller->m_senses.damagedFrom;
        goal.committedUntil = now + kCommitInvestigate;
        return goal;
    }

    // --- Idle fallback ---
    goal.type           = BotGoalType::Idle;
    goal.committedUntil = kCommitIdle;
    return goal;
}
