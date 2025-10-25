# Utility AI - Reference & Theory

## Overview
Utility AI is a decision-making system that scores multiple actions and chooses the highest-rated one. Unlike rigid if/else logic, utility AI creates smooth, context-sensitive, emergent behaviors.

## Core Concepts

### Utility Function
Maps game state to action desirability (0.0 - 1.0 score).

```
Score = Σ (Consideration_i × Weight_i)
```

### Considerations
Individual factors that influence action desirability.

Example for "Attack" action:
- Health (healthy = good to attack)
- Ammo (lots of ammo = good to attack)
- Enemy count (1-2 enemies = good, many = bad)
- Distance to enemy (in weapon range = good)

### Response Curves
Functions that map raw input to normalized score (0-1).

Common curves:
- **Linear**: `y = x`
- **Exponential**: `y = x^n` (emphasize high values)
- **Inverse**: `y = 1 - x` (invert relationship)
- **Logistic**: `y = 1 / (1 + e^(-k(x-m)))` (S-curve)
- **Threshold**: Binary 0 or 1 based on cutoff

## Example: Attack Action Scoring

```cpp
float ScoreAttack(Context ctx) {
    float score = 0.0f;

    // Consideration 1: Health
    float healthFactor = ctx.health / ctx.maxHealth;
    float healthScore = pow(healthFactor, 2.0f);  // Exponential
    score += healthScore * 0.4f;  // Weight: 40%

    // Consideration 2: Ammo
    float ammoFactor = ctx.ammo / ctx.maxAmmo;
    float ammoScore = ammoFactor;  // Linear
    score += ammoScore * 0.3f;  // Weight: 30%

    // Consideration 3: Enemy Count
    float enemyScore;
    if (ctx.enemyCount == 1) enemyScore = 1.0f;
    else if (ctx.enemyCount == 2) enemyScore = 0.7f;
    else enemyScore = 0.3f;
    score += enemyScore * 0.3f;  // Weight: 30%

    return score;
}
```

## Designing Good Utility Functions

### 1. Normalize Inputs
All inputs should be 0.0 - 1.0 for consistent scoring.

```cpp
// Health: 0-100 → 0.0-1.0
float healthNormalized = currentHealth / maxHealth;

// Distance: 0-2000 → 0.0-1.0
float distanceNormalized = std::clamp(distance / 2000.0f, 0.0f, 1.0f);
```

### 2. Choose Appropriate Curves
Different relationships need different curves.

**Linear** (y = x): Proportional relationship
- More ammo = linearly better attack

**Exponential** (y = x^n): Emphasize extremes
- Low health (0.2) → 0.04 (very bad)
- High health (0.9) → 0.81 (very good)
- Exaggerates differences

**Inverse** (y = 1-x): Opposite relationship
- More enemies = worse for attacking

**Logistic** (S-curve): Smooth threshold
- Below threshold: gradually worse
- Above threshold: gradually better
- No hard cutoff

### 3. Balance Weights
Weights determine relative importance.

```cpp
// Critical: Health is most important
score += healthScore * 0.5f;  // 50%

// Important: Ammo matters
score += ammoScore * 0.3f;  // 30%

// Minor: Cover is nice but not critical
score += coverScore * 0.2f;  // 20%
```

Sum of weights typically = 1.0 (or normalize after).

### 4. Test Edge Cases
Verify scores make sense:

```cpp
// Low health, high ammo, 1 enemy
Context ctx1 = {health: 0.2f, ammo: 1.0f, enemies: 1};
float score1 = ScoreAttack(ctx1);
// Should score low (low health dominates)

// High health, low ammo, 3 enemies
Context ctx2 = {health: 0.9f, ammo: 0.1f, enemies: 3};
float score2 = ScoreAttack(ctx2);
// Should score medium-low (many enemies + low ammo)
```

## Utility vs. Behavior Trees

**Utility AI** picks *which* high-level action.
**Behavior Tree** executes the chosen action.

### Combined Approach (Best!)

```cpp
// Utility AI selects strategy
auto decision = utilityEvaluator.SelectBestAction(context);

// Behavior tree executes strategy
if (decision.name == "Aggress") {
    aggressTree->Execute(blackboard, dt);
} else if (decision.name == "Defend") {
    defendTree->Execute(blackboard, dt);
}
```

**Benefits:**
- Utility AI: Dynamic, context-aware strategy selection
- Behavior Trees: Clean execution of chosen strategy

## Advanced Techniques

### 1. Personality via Weights

```yaml
# Aggressive profile
aggress_action:
  health_weight: 0.3  # Less concerned about health
  ammo_weight: 0.2
  enemy_count_weight: 0.1  # Doesn't care about numbers
  aggression_trait: 0.4  # Personality dominates!

# Defensive profile
aggress_action:
  health_weight: 0.5  # Very health-conscious
  ammo_weight: 0.3
  enemy_count_weight: 0.2  # Cautious of outnumbering
  aggression_trait: 0.0  # Personality doesn't push aggression
```

Same action, different profiles = different behaviors!

### 2. Momentum / Inertia

Prevent rapid action switching:

```cpp
float ScoreAction(Action action, Context ctx) {
    float baseScore = CalculateBaseScore(action, ctx);

    // Bonus if this is current action
    if (action == currentAction) {
        baseScore += 0.2f;  // 20% inertia bonus
    }

    return baseScore;
}
```

### 3. Composite Considerations

Combine multiple factors:

```cpp
// "Safety" = health + cover + allies
float safetyScore = (healthScore + coverScore + allyScore) / 3.0f;
score += safetyScore * 0.5f;
```

### 4. Contextual Modifiers

Adjust scores based on game mode:

```cpp
if (gameMode == CAPTURE_FLAG) {
    defendScore *= 1.5f;  // Defend more important in CTF
}
```

## Common Patterns

### Pattern 1: Graduated Response

```cpp
// Retreat score increases with damage
float recentDamage = ctx.damageLastSecond;
float retreatScore = pow(recentDamage, 2.0f);  // Exponential

// Light damage (0.1) → 0.01 (don't retreat)
// Heavy damage (0.8) → 0.64 (strongly consider retreat)
```

### Pattern 2: Sweet Spot

```cpp
// Attack best at medium range, worse at extremes
float distanceScore;
float optimalRange = 500.0f;
float actualRange = ctx.distanceToEnemy;
float deviation = abs(actualRange - optimalRange) / optimalRange;
distanceScore = 1.0f - deviation;  // Best at optimal, worse further away
```

### Pattern 3: Combo Multiplier

```cpp
// Flank is MUCH better if enemy distracted
float flankScore = baseFlankScore;
if (enemyDistracted) {
    flankScore *= 3.0f;  // Huge multiplier!
}
```

## Debugging Utility AI

### Visualization

Show scores as bars:
```
Aggress     |████████░░| 0.8
Defend      |██████░░░░| 0.6
Retreat     |███░░░░░░░| 0.3  ← Selected
Investigate |████░░░░░░| 0.4
```

### Reasoning Output

```cpp
struct ScoredAction {
    std::string name;
    float score;
    std::string reasoning;  // For debugging
};

return {
    "Aggress",
    0.82f,
    "health=0.9 ammo=0.8 enemies=1 → 0.82"
};
```

### Historical Tracking

Record decisions over time:
```
Frame 100: Aggress (0.85)
Frame 120: Aggress (0.82)
Frame 140: Defend (0.78) ← Switched!
Frame 160: Defend (0.81)
```

Helps identify thrashing or instability.

## Performance Tips

### 1. Cache Expensive Calculations

```cpp
// Don't recalculate every action
float distanceToEnemy = CalculateOnce();
context.distanceToEnemy = distanceToEnemy;

// All actions use cached value
```

### 2. Early Out

```cpp
// If action clearly invalid, return 0 immediately
if (action == "Attack" && !hasWeapon) {
    return 0.0f;  // Don't bother scoring
}
```

### 3. LOD Utility

Low LOD: Score every N frames, use cached result.

### 4. Lazy Evaluation

Only score top N most promising actions.

## Testing Utility AI

### Unit Tests

```cpp
TEST(UtilityTest, AttackScoreHighWhenHealthy) {
    Context ctx = {health: 0.9f, ammo: 1.0f, enemies: 1};
    float score = ScoreAttack(ctx);
    EXPECT_GT(score, 0.7f);  // Should score high
}

TEST(UtilityTest, RetreatScoreHighWhenLowHealth) {
    Context ctx = {health: 0.1f, ammo: 1.0f, enemies: 3};
    float score = ScoreRetreat(ctx);
    EXPECT_GT(score, 0.8f);  // Should definitely retreat
}
```

### Integration Tests

```cpp
TEST(UtilityIntegrationTest, BotRetreatsWhenDamaged) {
    Bot bot;
    bot.SetHealth(0.2f);
    bot.AddEnemies(3);

    auto decision = utilityEvaluator.SelectBestAction(bot.GetContext());

    EXPECT_EQ(decision.name, "Retreat");
}
```

## Resources

### Papers
- "Behavioral Mathematics for Game AI" - Dave Mark
- "Building a Better Centaur" - Kevin Dill (F.E.A.R. AI)

### Books
- "Behavioral Mathematics for Game AI" - Dave Mark & Kevin Dill
- "Game AI Pro" series - Utility AI chapters

### Talks
- GDC 2010: "Building Better Behavior with Utility Theory" - Dave Mark
- GDC 2012: "Embracing the Dark Art of Mathematical Modeling in AI" - Dave Mark

### Tools
- Curvaceous: Utility curve editor
- UtilityAI library (C++)

## OpenMoHAA Implementation

For our bots:
1. Score 5-6 high-level actions (Aggress, Defend, Retreat, etc.)
2. Pick highest score
3. Execute via behavior tree
4. Visualize scores in debug overlay
5. Configure curves in YAML

See `examples/utility-evaluator-example.cpp` for complete implementation.

## Key Takeaways

✅ **Do:**
- Normalize all inputs (0-1)
- Use appropriate curves for relationships
- Balance weights thoughtfully
- Test edge cases
- Visualize scores for debugging
- Combine with behavior trees

❌ **Don't:**
- Mix raw values (normalize!)
- Over-complicate (start simple)
- Forget inertia (prevent thrashing)
- Ignore personality (use weights!)

**Utility AI creates unpredictable, context-aware, emergent behaviors that feel intelligent and reactive.**
