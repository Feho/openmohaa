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

        // Added in OPM - Phase 3 Task 3.1g
        //  Grenade system helpers

        // Changed in OPM
        //  Improved clustering algorithm using density-based approach (Gemini review feedback)
        //  Checks if any enemy has at least 1 other enemy within maxRadius
        bool AreEnemiesClustered(const std::vector<EnemyInfo>& enemies, float maxRadius)
        {
            if (enemies.size() < 2) {
                return false;
            }

            float maxRadiusSq = maxRadius * maxRadius;

            // Density-based approach: find if any enemy has at least 1 neighbor within radius
            for (size_t i = 0; i < enemies.size(); i++) {
                int nearbyCount = 0;

                for (size_t j = 0; j < enemies.size(); j++) {
                    if (i == j) {
                        continue; // Skip self
                    }

                    float distanceSq = (enemies[i].position - enemies[j].position).lengthSquared();
                    if (distanceSq <= maxRadiusSq) {
                        nearbyCount++;

                        // Found at least one neighbor - this is a valid cluster
                        if (nearbyCount >= 1) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        // Changed in OPM
        //  Returns the position of the enemy with the most neighbors (Gemini review feedback)
        //  This provides a better grenade target than geometric center
        Vector CalculateClusterCenter(const std::vector<EnemyInfo>& enemies)
        {
            if (enemies.empty()) {
                return Vector(0, 0, 0);
            }

            if (enemies.size() == 1) {
                return enemies[0].position;
            }

            // Find the enemy with the most neighbors within cluster radius
            float  clusterRadiusSq = BotConstants::GRENADE_CLUSTER_RADIUS * BotConstants::GRENADE_CLUSTER_RADIUS;
            int    maxNeighbors    = -1;
            Vector bestPosition    = enemies[0].position;

            for (size_t i = 0; i < enemies.size(); i++) {
                int neighborCount = 0;

                for (size_t j = 0; j < enemies.size(); j++) {
                    if (i == j) {
                        continue; // Skip self
                    }

                    float distanceSq = (enemies[i].position - enemies[j].position).lengthSquared();
                    if (distanceSq <= clusterRadiusSq) {
                        neighborCount++;
                    }
                }

                if (neighborCount > maxNeighbors) {
                    maxNeighbors = neighborCount;
                    bestPosition = enemies[i].position;
                }
            }

            return bestPosition;
        }

        bool HasAlliesNearPosition(Vector position, float safetyRadius, PerceptionSnapshot *perception)
        {
            if (!perception) {
                return false;
            }

            float safetyRadiusSq = safetyRadius * safetyRadius;

            for (const auto& ally : perception->visibleAllies) {
                float distanceSq = (ally.position - position).lengthSquared();
                if (distanceSq < safetyRadiusSq) {
                    return true; // Ally too close - unsafe to throw grenade
                }
            }

            return false;
        }

        // Added in OPM - Phase 3 Task 3.4
        //  Calculate flanking position using perpendicular vector method
        //  Based on logic from playerbot_squad.cpp ExecuteFlankingManeuver()
        Vector CalculateFlankPosition(const Vector& botPos, const Vector& enemyPos, float radius)
        {
            // Calculate vector from bot to enemy
            Vector toEnemy = enemyPos - botPos;
            toEnemy[2]     = 0; // Ignore vertical component for flanking
            toEnemy.normalize();

            // Create perpendicular vectors (90 degrees left and right)
            // Rotate 2D vector: (x, y) → (-y, x) for left, (y, -x) for right
            Vector flankLeft(-toEnemy[1], toEnemy[0], 0);
            Vector flankRight(toEnemy[1], -toEnemy[0], 0);

            // Calculate both flank positions
            Vector leftFlank  = enemyPos + (flankLeft * radius);
            Vector rightFlank = enemyPos + (flankRight * radius);

            // Choose side closer to bot's current position
            // This provides more natural flanking behavior
            float leftDistSq  = (leftFlank - botPos).lengthSquared();
            float rightDistSq = (rightFlank - botPos).lengthSquared();

            return (leftDistSq <= rightDistSq) ? leftFlank : rightFlank;
        }

        // Added in OPM - Phase 3 Task 3.4
        //  Test if navigation path exists using pathfinding system
        // Fixed in OPM
        //  Use unique_ptr for automatic cleanup and exception safety
        bool PathExistsTo(const Player *bot, const Vector& targetPos)
        {
            if (!bot) {
                return false;
            }

            // Use unique_ptr for automatic cleanup
            std::unique_ptr<IPather> testPather(IPather::CreatePather());
            if (!testPather) {
                return false;
            }

            // Set up path search parameters
            PathSearchParameter params;
            params.leashHome  = bot->origin;
            params.leashDist  = 0.0f;                      // No leash for test
            params.fallHeight = 400;                       // Standard fall height
            params.entity     = const_cast<Player *>(bot); // PathSearchParameter requires non-const

            return testPather->TestPath(bot->origin, targetPos, params);
        }

    } // namespace Combat
} // namespace BT
