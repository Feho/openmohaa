// playerbot_planner.cpp: Bot decision planner producing Goal objects.
//
// The planner runs at ~4 Hz and produces a single BotGoal. The state
// machine reads the current goal each frame and executes it. Decisions
// live here; execution lives in the state machine.

#include "playerbot.h"
#include "playerbot_planner.h"
#include "playerbot_target_scorer.h"
#include "gamecvars.h"

static constexpr int kPlanIntervalMs = 250; // ~4 Hz

// Commitment durations per goal type (milliseconds).
// These prevent the planner from flipping between goals every tick
// when multiple options score closely.
static constexpr int kCommitEngage      = 2000;
static constexpr int kCommitFlank       = 3000;
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

    // Current engage/flank target became invalid (died, disconnected, etc.)
    if ((m_currentGoal.type == BotGoalType::Engage || m_currentGoal.type == BotGoalType::Flank)
        && m_currentGoal.targetEnemy && !m_controller->IsValidEnemy(m_currentGoal.targetEnemy)) {
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

BotPlanner::TargetPick BotPlanner::FindBestTarget() const
{
    TargetPick best;

    Player *controlledEnt = m_controller->getControlledEntity();
    if (!controlledEnt) {
        return best;
    }

    BotTargetScorer scorer;
    bool            debug = g_bot_debug_scorer->integer != 0;

    for (int i = 1; i <= SentientList.NumObjects(); i++) {
        Sentient *sent = SentientList.ObjectAt(i);

        if (!m_controller->IsValidEnemy(sent)) {
            continue;
        }

        BotTargetScorer::Score score = scorer.Evaluate(*m_controller, sent);

        if (debug) {
            const char *enemyName = "unknown";
            if (sent->IsSubclassOfPlayer()) {
                enemyName = static_cast<Player *>(sent)->client->pers.netname;
            }
            gi.Printf(
                "BOT %s: SCORE %s ttk=%.2f travel=%.2f aim=%.2f dmg=%.2f shootNode=%d%s\n",
                controlledEnt->client->pers.netname,
                enemyName,
                score.ttk,
                score.travelTime,
                score.aimTime,
                score.damageTime,
                score.shootFromNode,
                score.reachable ? "" : " (unreachable)"
            );
        }

        if (!score.reachable) {
            continue;
        }

        if (!best.valid || score.ttk < best.ttk) {
            best.valid         = true;
            best.enemy         = sent;
            best.shootFromNode = score.shootFromNode;
            best.ttk           = score.ttk;
        }
    }

    return best;
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

    // 1. Best TTK target (visible or reachable via flanking)
    TargetPick picked = FindBestTarget();
    if (picked.valid && picked.enemy) {
        goal.targetEnemy = picked.enemy;

        if (picked.shootFromNode < 0) {
            // Already at a shooting position: engage directly.
            goal.type           = BotGoalType::Engage;
            goal.targetPos      = picked.enemy->origin;
            goal.committedUntil = now + kCommitEngage;
            return goal;
        }

        // Need to reposition first. Emit a Flank goal pointing at the
        // nav node with a sightline; once there (or once the enemy
        // becomes visible along the way) the next planner tick will
        // promote to Engage.
        PathNode *node = PathSearch::pathnodes[picked.shootFromNode];
        if (node) {
            goal.type           = BotGoalType::Flank;
            goal.targetPos      = Vector(node->origin[0], node->origin[1], node->origin[2]);
            goal.committedUntil = now + kCommitFlank;
            return goal;
        }

        // Defensive fallback: the scorer validated the node, but if
        // it's gone for some reason, still engage directly.
        goal.type           = BotGoalType::Engage;
        goal.targetPos      = picked.enemy->origin;
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
