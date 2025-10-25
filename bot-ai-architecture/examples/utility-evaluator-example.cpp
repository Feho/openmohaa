// Utility AI Evaluator Example
// Shows how to score actions dynamically based on context

#include "utility_evaluator.h"
#include "bot_profile.h"
#include "perception.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// Consideration Curves
// These map input values (0.0-1.0) to output scores (0.0-1.0)
// ============================================================================

namespace Curves {

// Linear curve: y = x
float Linear(float x) {
    return std::clamp(x, 0.0f, 1.0f);
}

// Exponential curve: y = x^exponent
float Exponential(float x, float exponent) {
    return std::pow(std::clamp(x, 0.0f, 1.0f), exponent);
}

// Inverse linear: y = 1 - x
float InverseLinear(float x) {
    return 1.0f - std::clamp(x, 0.0f, 1.0f);
}

// Threshold: binary 0 or 1
float Threshold(float x, float threshold) {
    return (x >= threshold) ? 1.0f : 0.0f;
}

// Logistic/S-curve: smooth transition
float Logistic(float x, float midpoint, float steepness) {
    float k = steepness;
    return 1.0f / (1.0f + std::exp(-k * (x - midpoint)));
}

}  // namespace Curves

// ============================================================================
// Utility Evaluator Implementation
// ============================================================================

class UtilityEvaluator {
public:
    struct ActionScore {
        std::string name;
        float score;
        std::string reasoning;  // For debugging
    };

    ActionScore SelectBestAction(
        const PerceptionSnapshot& perception,
        const BotState& state,
        const BotProfile& profile
    ) {
        std::vector<ActionScore> scores;

        // Score each possible high-level action
        scores.push_back(ScoreAggress(perception, state, profile));
        scores.push_back(ScoreDefend(perception, state, profile));
        scores.push_back(ScoreRetreat(perception, state, profile));
        scores.push_back(ScoreInvestigate(perception, state, profile));
        scores.push_back(ScoreSupport(perception, state, profile));
        scores.push_back(ScoreFlank(perception, state, profile));

        // Find highest score
        auto best = std::max_element(scores.begin(), scores.end(),
            [](const ActionScore& a, const ActionScore& b) {
                return a.score < b.score;
            });

        return *best;
    }

private:
    // ========================================================================
    // Action Scorers
    // Each considers multiple factors and combines them
    // ========================================================================

    ActionScore ScoreAggress(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: Health (healthy = aggressive)
        float healthFactor = state.health / state.maxHealth;
        float healthScore = Curves::Exponential(healthFactor, 2.0f);
        score += healthScore * 0.3f;

        // Consideration 2: Ammo (lots of ammo = aggressive)
        float ammoFactor = (float)state.ammo / state.maxAmmo;
        float ammoScore = Curves::Exponential(ammoFactor, 1.5f);
        score += ammoScore * 0.2f;

        // Consideration 3: Enemy count (1-2 enemies = aggressive, many = cautious)
        int enemyCount = perc.visibleEnemies.size();
        float enemyScore = 0.0f;
        if (enemyCount == 1) enemyScore = 1.0f;
        else if (enemyCount == 2) enemyScore = 0.8f;
        else if (enemyCount >= 3) enemyScore = 0.3f;
        score += enemyScore * 0.2f;

        // Consideration 4: Ally support (allies nearby = more confident)
        int allyCount = perc.nearbyAllies.size();
        float allyScore = std::min((float)allyCount / 2.0f, 1.0f);
        score += allyScore * 0.15f;

        // Consideration 5: Personality (aggression trait)
        float personalityScore = profile.GetAggression();
        score += personalityScore * 0.15f;

        return {
            "Aggress",
            score,
            "health=" + std::to_string(healthScore) +
            " ammo=" + std::to_string(ammoScore) +
            " enemies=" + std::to_string(enemyScore)
        };
    }

    ActionScore ScoreDefend(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: In cover? (in cover = defend is good)
        float coverScore = state.isInCover ? 1.0f : 0.0f;
        score += coverScore * 0.3f;

        // Consideration 2: Many enemies (outnumbered = defend)
        int enemyCount = perc.visibleEnemies.size();
        float enemyScore = Curves::Logistic((float)enemyCount / 5.0f, 0.4f, 10.0f);
        score += enemyScore * 0.25f;

        // Consideration 3: Near objective (defending objective = good)
        float objectiveDistance = state.distanceToObjective;
        float objectiveScore = Curves::InverseLinear(objectiveDistance / 500.0f);
        score += objectiveScore * 0.2f;

        // Consideration 4: Personality (caution trait)
        float personalityScore = profile.GetCaution();
        score += personalityScore * 0.25f;

        return {
            "Defend",
            score,
            "cover=" + std::to_string(coverScore) +
            " enemies=" + std::to_string(enemyScore) +
            " objective=" + std::to_string(objectiveScore)
        };
    }

    ActionScore ScoreRetreat(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: Low health (critical!)
        float healthFactor = state.health / state.maxHealth;
        float healthScore = Curves::InverseLinear(healthFactor);
        score += healthScore * 0.5f;  // Heavy weight

        // Consideration 2: Taking damage
        float recentDamage = state.damageLastSecond / state.maxHealth;
        float damageScore = Curves::Exponential(recentDamage, 2.0f);
        score += damageScore * 0.2f;

        // Consideration 3: Outnumbered
        int enemyCount = perc.visibleEnemies.size();
        int allyCount = perc.nearbyAllies.size();
        float outnumbered = std::max(0.0f, (float)(enemyCount - allyCount - 1) / 3.0f);
        score += Curves::Linear(outnumbered) * 0.15f;

        // Consideration 4: Low ammo
        float ammoFactor = (float)state.ammo / state.maxAmmo;
        float ammoScore = Curves::Threshold(ammoFactor, 0.2f);  // Low ammo = retreat
        score += (1.0f - ammoScore) * 0.15f;

        return {
            "Retreat",
            score,
            "health=" + std::to_string(healthScore) +
            " damage=" + std::to_string(damageScore) +
            " outnumbered=" + std::to_string(outnumbered)
        };
    }

    ActionScore ScoreInvestigate(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: Enemy memory with high confidence
        float bestConfidence = 0.0f;
        for (const auto& memory : perc.knownEnemies) {
            bestConfidence = std::max(bestConfidence, memory.confidenceLevel);
        }
        score += bestConfidence * 0.4f;

        // Consideration 2: Recent high-priority sound
        float soundScore = 0.0f;
        if (perc.loudestSound && perc.loudestSound->priority > 0.5f) {
            float age = level.time - perc.loudestSound->timestamp;
            soundScore = Curves::InverseLinear(age / 10.0f);  // Decay over 10s
        }
        score += soundScore * 0.3f;

        // Consideration 3: Not in immediate danger
        bool inDanger = !perc.visibleEnemies.empty() || state.recentlyDamaged;
        float safetyScore = inDanger ? 0.0f : 1.0f;
        score += safetyScore * 0.2f;

        // Consideration 4: Personality (creativity/curiosity)
        float personalityScore = profile.GetCreativity();
        score += personalityScore * 0.1f;

        return {
            "Investigate",
            score,
            "memory=" + std::to_string(bestConfidence) +
            " sound=" + std::to_string(soundScore) +
            " safe=" + std::to_string(safetyScore)
        };
    }

    ActionScore ScoreSupport(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: Ally in combat nearby
        int alliesInCombat = 0;
        for (const auto& ally : perc.nearbyAllies) {
            if (ally.isInCombat) alliesInCombat++;
        }
        float allyScore = std::min((float)alliesInCombat / 2.0f, 1.0f);
        score += allyScore * 0.4f;

        // Consideration 2: We're healthy (can afford to help)
        float healthFactor = state.health / state.maxHealth;
        score += Curves::Exponential(healthFactor, 1.5f) * 0.3f;

        // Consideration 3: Personality (teamwork trait)
        float personalityScore = profile.GetTeamwork();
        score += personalityScore * 0.3f;

        return {
            "Support",
            score,
            "allies=" + std::to_string(allyScore) +
            " health=" + std::to_string(healthFactor) +
            " teamwork=" + std::to_string(personalityScore)
        };
    }

    ActionScore ScoreFlank(
        const PerceptionSnapshot& perc,
        const BotState& state,
        const BotProfile& profile
    ) {
        float score = 0.0f;

        // Consideration 1: Enemy engaged with ally (distracted)
        bool enemyDistracted = false;
        for (const auto& enemy : perc.visibleEnemies) {
            if (enemy.targetingAlly) {
                enemyDistracted = true;
                break;
            }
        }
        score += (enemyDistracted ? 1.0f : 0.0f) * 0.35f;

        // Consideration 2: Good health/ammo (can execute maneuver)
        float resourceScore = (state.health / state.maxHealth) *
                             ((float)state.ammo / state.maxAmmo);
        score += Curves::Exponential(resourceScore, 1.5f) * 0.3f;

        // Consideration 3: Flank path available
        float flankScore = state.hasFlankPath ? 1.0f : 0.0f;
        score += flankScore * 0.2f;

        // Consideration 4: Personality (creativity + aggression)
        float personalityScore = (profile.GetCreativity() + profile.GetAggression()) / 2.0f;
        score += personalityScore * 0.15f;

        return {
            "Flank",
            score,
            "distracted=" + std::to_string(enemyDistracted) +
            " resources=" + std::to_string(resourceScore) +
            " path=" + std::to_string(flankScore)
        };
    }
};

// ============================================================================
// Usage Example
// ============================================================================

void BotController::SelectStrategy() {
    // Get current perception and state
    auto perception = perceptionSystem.GetSnapshot();
    BotState state = GetCurrentState();

    // Evaluate utility of all actions
    UtilityEvaluator evaluator;
    auto decision = evaluator.SelectBestAction(perception, state, profile);

    // Log decision for debugging
    Com_Printf("Bot selected: %s (score=%.2f) - %s\n",
        decision.name.c_str(),
        decision.score,
        decision.reasoning.c_str());

    // Execute chosen strategy via behavior tree
    if (decision.name == "Aggress") {
        aggressTree->Execute(blackboard, dt);
    } else if (decision.name == "Defend") {
        defendTree->Execute(blackboard, dt);
    } else if (decision.name == "Retreat") {
        retreatTree->Execute(blackboard, dt);
    } else if (decision.name == "Investigate") {
        investigateTree->Execute(blackboard, dt);
    } else if (decision.name == "Support") {
        supportTree->Execute(blackboard, dt);
    } else if (decision.name == "Flank") {
        flankTree->Execute(blackboard, dt);
    }
}

// ============================================================================
// Notes
// ============================================================================

/*
Utility AI Benefits:
- Dynamic: Same situation can yield different actions based on subtle differences
- Unpredictable: Players can't memorize bot patterns
- Tuneable: Adjust weights/curves to change behavior
- Debuggable: Can see exact scores and reasoning

Designing Good Considerations:
- Keep them simple and independent
- Use curves to shape responses (linear, exponential, threshold)
- Weight important factors higher
- Combine personality traits for variety

Testing:
- Unit test each scorer with mock data
- Verify scores make intuitive sense
- Test edge cases (no health, no ammo, many enemies)
- Visualize scores in-game for tuning
*/
