// Added in OPM - Phase 3 Task 3.1a
// bt_combat_helpers.cpp: Combat-related helper functions for behavior tree actions

#include "bt_combat_helpers.h"
#include "playerbot.h"

namespace BT
{
namespace Combat
{

// Added in OPM - Phase 3 Task 3.1a
//  Migrated from BotController::IsValidEnemy() in playerbot_attack.cpp
bool IsValidEnemy(const Player *bot, Sentient *enemy)
{
    if (!bot || !enemy) {
        return false;
    }

    if (enemy == bot) {
        return false;
    }

    if (enemy->hidden() || (enemy->flags & FL_NOTARGET)) {
        // Ignore hidden / non-target enemies
        return false;
    }

    if (enemy->IsDead()) {
        // Ignore dead enemies
        return false;
    }

    if (enemy->getSolidType() == SOLID_NOT) {
        // Ignore non-solid, like spectators
        return false;
    }

    // Team check
    if (enemy->IsSubclassOfPlayer()) {
        Player *enemyPlayer = static_cast<Player *>(enemy);

        if (g_gametype->integer >= GT_TEAM && enemyPlayer->GetTeam() == bot->GetTeam()) {
            return false; // Same team
        }
    } else {
        if (enemy->m_Team == bot->m_Team) {
            return false; // Same team
        }
    }

    return true;
}

// Added in OPM - Phase 3 Task 3.1a
//  Calculate target priority score with stickiness behavior
float CalculateTargetScore(
    Sentient *enemy,
    Sentient *currentTarget,
    float     distance,
    float     lockTime,
    float     lockDuration,
    float     switchThreshold
)
{
    if (!enemy) {
        return 0.0f;
    }

    // Base score: inverse of distance (closer = higher score)
    // Use a large constant to keep scores positive and meaningful
    float score = 10000.0f / (distance + 1.0f);

    // Target stickiness: bonus for current target
    if (currentTarget && enemy == currentTarget) {
        // Check if lock time still active
        float timeSinceLock = (level.svsTime - lockTime) / 1000.0f; // Convert to seconds

        if (timeSinceLock < lockDuration) {
            // Give significant bonus to current target while locked
            score += switchThreshold * 2.0f;
        }
    }

    return score;
}

// Added in OPM - Phase 3 Task 3.1a
//  Find closest enemy from perception snapshot
const EnemyInfo *FindClosestVisibleEnemy(const PerceptionSnapshot *perception)
{
    if (!perception || perception->visibleEnemies.empty()) {
        return nullptr;
    }

    return perception->GetClosestEnemy();
}

} // namespace Combat
} // namespace BT
