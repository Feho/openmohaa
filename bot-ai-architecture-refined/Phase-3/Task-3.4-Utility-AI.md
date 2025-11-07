# Task 3.4: Utility AI System

**Status:** Ready to Execute  
**Duration:** 2 weeks  
**Priority:** MEDIUM  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Implements a Utility-based AI system that dynamically scores possible actions based on context, enabling more adaptive and unpredictable bot behavior. The utility system selects high-level strategies, while behavior trees execute the chosen strategy.

### Why This Matters
- **Unpredictability:** Bots don't follow rigid patterns, players can't exploit them
- **Context-Awareness:** Same situation yields different responses based on subtle factors
- **Emergent Behavior:** Complex tactics emerge from simple scoring rules
- **Tunability:** Easy to adjust via YAML configuration without code changes
- **Personality:** Profiles influence scoring, creating distinct bot playstyles

### What's Already Complete

**Phase 2A (Perception):**
- ✅ PerceptionSnapshot with enemy/ally/threat data
- ✅ ThreatLevel assessment
- ✅ Memory system with confidence tracking

**Phase 2B (Behavior Trees):**
- ✅ Complete BT framework
- ✅ BotProfile system with personality traits

**Phase 3.1-3.3 (Behaviors):**
- ✅ Combat behavior tree
- ✅ Investigation behavior tree
- ✅ Idle behavior tree
- ✅ Master behavior selector

**What Bots Can Do Now:**
- Fight, investigate, and patrol using predefined priority (combat > investigation > idle)

**What Bots Need:**
- Dynamic strategy selection based on context
- Smooth transitions between strategies
- Personality-driven decision making
- Emergent tactical behaviors

---

## Utility AI Theory

### Core Concept
Instead of rigid priorities, score each possible action based on current context:

```
Action Score = Σ (Consideration_i × Weight_i)
Best Action = max(Action Scores)
```

### Example: Should Bot Aggress?
```cpp
float health_factor = bot.health / bot.max_health;              // 0.0 - 1.0
float ammo_factor = bot.ammo / bot.max_ammo;                    // 0.0 - 1.0
float enemy_proximity = 1.0 - (enemy_dist / max_sight_range);  // 0.0 - 1.0
float ally_support = min(nearby_allies / 2.0, 1.0);            // 0.0 - 1.0

aggress_score = (health_factor * 0.3) +
                (ammo_factor * 0.3) +
                (enemy_proximity * 0.2) +
                (ally_support * 0.2);
```

**Result:** High health + good ammo + close enemy + allies = HIGH aggression score

### Consideration Curves
Transform raw values into scores using curves:

**Linear:** `output = input`
- Health: 50% health → 0.5 score
- Direct relationship

**Exponential:** `output = input^n`
- Ammo: 50% ammo → 0.25 score (with n=2)
- Emphasizes high values, penalizes low values

**Inverse Linear:** `output = 1.0 - input`
- Distance: Close enemy → high score
- Flips relationship

**Threshold:** `output = input > threshold ? 1.0 : 0.0`
- Critical health: < 25% → 1.0, else 0.0
- Binary decisions

---

## Technical Specification

### Actions to Score

1. **Aggress:** Push forward aggressively
2. **Defend:** Hold position, use cover
3. **Retreat:** Fall back to safety
4. **Investigate:** Search for lost enemies
5. **Support:** Help nearby ally
6. **Flank:** Circle around enemy

### Utility Evaluator Architecture

```cpp
// code/fgame/utility_evaluator.h

class UtilityEvaluator {
public:
    struct ScoredAction {
        std::string name;
        float score;            // 0.0 - 1.0
        std::string treeFile;   // Which BT to execute
    };
    
    // Evaluate all actions and return best
    ScoredAction SelectBestAction(
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
    
    // Get scores for all actions (for debugging)
    std::vector<ScoredAction> ScoreAllActions(
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
    
    // Load consideration curves from YAML
    void LoadFromFile(const char* filename);

private:
    struct Consideration {
        std::string name;
        CurveType curveType;
        float weight;
        float exponent;     // For exponential curves
        float threshold;    // For threshold curves
        float minValue;     // For normalization
        float maxValue;
    };
    
    struct ActionConfig {
        std::string name;
        std::string treeFile;
        std::vector<Consideration> considerations;
    };
    
    std::vector<ActionConfig> actions;
    
    // Score individual action
    float ScoreAction(
        const ActionConfig& action,
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
    
    // Evaluate single consideration
    float EvaluateConsideration(
        const Consideration& consideration,
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
    
    // Apply curve to raw value
    float ApplyCurve(float input, const Consideration& consideration);
};

enum class CurveType {
    LINEAR,
    EXPONENTIAL,
    INVERSE_LINEAR,
    THRESHOLD,
    LOGISTIC
};
```

### YAML Configuration Format

```yaml
# utility/bot_utility.yaml
utility:
  # Action 1: Aggress
  - action:
      name: "Aggress"
      tree: "behaviors/combat_aggressive.btree"
      considerations:
        - name: "health_factor"
          curve: "linear"
          weight: 0.3
          min: 0.0
          max: 1.0
        
        - name: "ammo_factor"
          curve: "exponential"
          exponent: 2.0
          weight: 0.3
          min: 0.0
          max: 1.0
        
        - name: "enemy_proximity"
          curve: "inverse_linear"
          weight: 0.2
          min: 0.0
          max: 2048.0  # Max sight range
        
        - name: "ally_support"
          curve: "linear"
          weight: 0.2
          min: 0.0
          max: 3.0  # Max allies to count

  # Action 2: Defend
  - action:
      name: "Defend"
      tree: "behaviors/combat_defensive.btree"
      considerations:
        - name: "health_factor"
          curve: "inverse_linear"
          weight: 0.3
        
        - name: "in_cover"
          curve: "threshold"
          threshold: 0.5
          weight: 0.4
        
        - name: "enemy_count"
          curve: "exponential"
          exponent: 1.5
          weight: 0.3
          min: 0.0
          max: 5.0

  # Action 3: Retreat
  - action:
      name: "Retreat"
      tree: "behaviors/combat_retreat.btree"
      considerations:
        - name: "health_factor"
          curve: "inverse_linear"
          weight: 0.5
        
        - name: "outnumbered"
          curve: "threshold"
          threshold: 2.0  # More than 2 enemies vs. us
          weight: 0.3
        
        - name: "cover_nearby"
          curve: "linear"
          weight: 0.2

  # Action 4: Investigate
  - action:
      name: "Investigate"
      tree: "behaviors/investigation.btree"
      considerations:
        - name: "memory_confidence"
          curve: "exponential"
          exponent: 2.0
          weight: 0.5
        
        - name: "no_visible_enemies"
          curve: "threshold"
          threshold: 0.5
          weight: 0.3
        
        - name: "personality_curiosity"
          curve: "linear"
          weight: 0.2

  # Action 5: Support
  - action:
      name: "Support"
      tree: "behaviors/combat_support.btree"
      considerations:
        - name: "ally_in_trouble"
          curve: "exponential"
          exponent: 2.0
          weight: 0.4
        
        - name: "distance_to_ally"
          curve: "inverse_linear"
          weight: 0.3
          max: 1024.0
        
        - name: "personality_teamwork"
          curve: "linear"
          weight: 0.3

  # Action 6: Flank
  - action:
      name: "Flank"
      tree: "behaviors/combat_flank.btree"
      considerations:
        - name: "enemy_distracted"
          curve: "linear"
          weight: 0.3
        
        - name: "flank_path_available"
          curve: "threshold"
          threshold: 0.5
          weight: 0.4
        
        - name: "personality_creativity"
          curve: "linear"
          weight: 0.3
```

### Implementation

```cpp
// code/fgame/utility_evaluator.cpp

UtilityEvaluator::ScoredAction UtilityEvaluator::SelectBestAction(
    const PerceptionSnapshot& perception,
    const Player* bot,
    const BotProfile* profile
) {
    std::vector<ScoredAction> scores = ScoreAllActions(perception, bot, profile);
    
    if (scores.empty()) {
        return {"idle", 0.0f, "behaviors/idle.btree"};
    }
    
    // Find highest scoring action
    auto best = std::max_element(scores.begin(), scores.end(),
        [](const ScoredAction& a, const ScoredAction& b) {
            return a.score < b.score;
        });
    
    return *best;
}

std::vector<UtilityEvaluator::ScoredAction> UtilityEvaluator::ScoreAllActions(
    const PerceptionSnapshot& perception,
    const Player* bot,
    const BotProfile* profile
) {
    std::vector<ScoredAction> results;
    
    for (const auto& action : actions) {
        float score = ScoreAction(action, perception, bot, profile);
        
        results.push_back({
            action.name,
            score,
            action.treeFile
        });
    }
    
    return results;
}

float UtilityEvaluator::ScoreAction(
    const ActionConfig& action,
    const PerceptionSnapshot& perception,
    const Player* bot,
    const BotProfile* profile
) {
    float totalScore = 0.0f;
    float totalWeight = 0.0f;
    
    for (const auto& consideration : action.considerations) {
        float value = EvaluateConsideration(consideration, perception, bot, profile);
        totalScore += value * consideration.weight;
        totalWeight += consideration.weight;
    }
    
    // Normalize by total weight
    if (totalWeight > 0.0f) {
        return totalScore / totalWeight;
    }
    
    return 0.0f;
}

float UtilityEvaluator::EvaluateConsideration(
    const Consideration& consideration,
    const PerceptionSnapshot& perception,
    const Player* bot,
    const BotProfile* profile
) {
    float rawValue = 0.0f;
    
    // Extract raw value based on consideration name
    if (consideration.name == "health_factor") {
        rawValue = bot->health / bot->max_health;
    }
    else if (consideration.name == "ammo_factor") {
        Weapon* weapon = bot->GetActiveWeapon(WEAPON_MAIN);
        if (weapon) {
            int ammo = weapon->AmmoAvailable(PRIMARY_MODE);
            int clipSize = weapon->GetClipSize(PRIMARY_MODE);
            rawValue = (clipSize > 0) ? (float)ammo / clipSize : 0.0f;
        }
    }
    else if (consideration.name == "enemy_proximity") {
        const EnemyInfo* enemy = perception.GetClosestEnemy();
        if (enemy) {
            rawValue = enemy->distance;  // Will be normalized by curve
        }
    }
    else if (consideration.name == "ally_support") {
        rawValue = std::min((float)perception.visibleAllies.size(), 3.0f);
    }
    else if (consideration.name == "in_cover") {
        // IsInCover requires bot position and enemy position
        BotController* botCtrl = GetBotController(bot);
        const EnemyInfo* enemy = perception.GetClosestEnemy();
        if (botCtrl && enemy && enemy->entity) {
            rawValue = botCtrl->IsInCover(bot->origin, enemy->position) ? 1.0f : 0.0f;
        }
    }
    else if (consideration.name == "enemy_count") {
        rawValue = (float)perception.GetEnemyCount();
    }
    else if (consideration.name == "outnumbered") {
        int enemies = perception.GetEnemyCount();
        int allies = perception.visibleAllies.size() + 1;  // +1 for self
        rawValue = (float)(enemies - allies);
    }
    else if (consideration.name == "cover_nearby") {
        // FindNearestCover returns PathNode*, not CoverPoint*
        const EnemyInfo* enemy = perception.GetClosestEnemy();
        if (enemy && enemy->entity) {
            Vector botPos = bot->origin;
            PathNode* coverNode = PathNode::FindNearestCover(bot, botPos, enemy->entity);
            rawValue = coverNode ? 1.0f : 0.0f;
        }
    }
    else if (consideration.name == "memory_confidence") {
        // Highest confidence memory
        float maxConfidence = 0.0f;
        for (const auto& memory : perception.knownEnemies) {
            maxConfidence = std::max(maxConfidence, memory.confidenceLevel);
        }
        rawValue = maxConfidence;
    }
    else if (consideration.name == "no_visible_enemies") {
        rawValue = perception.HasVisibleEnemy() ? 0.0f : 1.0f;
    }
    else if (consideration.name == "personality_curiosity") {
        rawValue = profile->GetCreativity();  // Use Creativity for curiosity
    }
    else if (consideration.name == "ally_in_trouble") {
        // Check if any ally has low health
        float maxTrouble = 0.0f;
        for (const auto& ally : perception.visibleAllies) {
            if (ally.entity && ally.entity->health / ally.entity->max_health < 0.5f) {
                maxTrouble = 1.0f;
                break;
            }
        }
        rawValue = maxTrouble;
    }
    else if (consideration.name == "distance_to_ally") {
        const AllyInfo* ally = perception.GetClosestAlly();
        if (ally) {
            rawValue = ally->distance;
        }
    }
    else if (consideration.name == "personality_teamwork") {
        rawValue = profile->GetTeamwork();
    }
    else if (consideration.name == "enemy_distracted") {
        // Check if enemy is engaged with someone else (m_Enemy is public member)
        const EnemyInfo* enemy = perception.GetClosestEnemy();
        if (enemy && enemy->entity) {
            Sentient* enemyTarget = enemy->entity->m_Enemy;
            rawValue = (enemyTarget != bot) ? 1.0f : 0.0f;
        }
    }
    else if (consideration.name == "flank_path_available") {
        // Check if flank route exists (requires helper function)
        const EnemyInfo* enemy = perception.GetClosestEnemy();
        if (enemy && enemy->entity) {
            Vector flankPos = BT::Combat::CalculateFlankPosition(bot->origin, enemy->position);
            rawValue = BT::Combat::PathExistsTo(bot, flankPos) ? 1.0f : 0.0f;
        }
    }
    else if (consideration.name == "personality_creativity") {
        rawValue = profile->GetCreativity();
    }
    
    // Apply curve to transform raw value
    return ApplyCurve(rawValue, consideration);
}

float UtilityEvaluator::ApplyCurve(float input, const Consideration& consideration) {
    // Normalize input to 0-1 range
    float normalized = (input - consideration.minValue) / 
                       (consideration.maxValue - consideration.minValue);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    
    float output = 0.0f;
    
    switch (consideration.curveType) {
        case CurveType::LINEAR:
            output = normalized;
            break;
        
        case CurveType::EXPONENTIAL:
            output = std::pow(normalized, consideration.exponent);
            break;
        
        case CurveType::INVERSE_LINEAR:
            output = 1.0f - normalized;
            break;
        
        case CurveType::THRESHOLD:
            output = normalized > consideration.threshold ? 1.0f : 0.0f;
            break;
        
        case CurveType::LOGISTIC:
            // S-curve: 1 / (1 + e^(-k*(x-0.5)))
            float k = consideration.exponent;  // Steepness
            output = 1.0f / (1.0f + std::exp(-k * (normalized - 0.5f)));
            break;
    }
    
    return std::clamp(output, 0.0f, 1.0f);
}

void UtilityEvaluator::LoadFromFile(const char* filename) {
    // Parse YAML file and populate actions vector
    YAML::Node config = YAML::LoadFile(filename);
    
    for (const auto& actionNode : config["utility"]) {
        ActionConfig action;
        action.name = actionNode["action"]["name"].as<std::string>();
        action.treeFile = actionNode["action"]["tree"].as<std::string>();
        
        for (const auto& consNode : actionNode["action"]["considerations"]) {
            Consideration cons;
            cons.name = consNode["name"].as<std::string>();
            cons.weight = consNode["weight"].as<float>();
            
            std::string curveStr = consNode["curve"].as<std::string>();
            if (curveStr == "linear") cons.curveType = CurveType::LINEAR;
            else if (curveStr == "exponential") cons.curveType = CurveType::EXPONENTIAL;
            else if (curveStr == "inverse_linear") cons.curveType = CurveType::INVERSE_LINEAR;
            else if (curveStr == "threshold") cons.curveType = CurveType::THRESHOLD;
            else if (curveStr == "logistic") cons.curveType = CurveType::LOGISTIC;
            
            cons.exponent = consNode["exponent"].as<float>(1.0f);
            cons.threshold = consNode["threshold"].as<float>(0.5f);
            cons.minValue = consNode["min"].as<float>(0.0f);
            cons.maxValue = consNode["max"].as<float>(1.0f);
            
            action.considerations.push_back(cons);
        }
        
        actions.push_back(action);
    }
}
```

### Integration with BotController

```cpp
// code/fgame/playerbot.cpp

class BotController {
private:
    UtilityEvaluator utilityEvaluator;
    BehaviorTree* currentTree;
    std::string currentStrategy;
    float strategyChangeTimer;

public:
    void Think(float dt) {
        // Update perception
        auto perception = perceptionSystem.Update(bot, dt);
        
        // Populate blackboard
        blackboard.Set("bot", bot);
        blackboard.Set("perception", &perception);
        blackboard.Set("profile", &profile);
        
        // Every 0.5 seconds, re-evaluate strategy
        strategyChangeTimer += dt;
        if (strategyChangeTimer > 0.5f) {
            auto bestAction = utilityEvaluator.SelectBestAction(perception, bot, &profile);
            
            // Switch strategy if significantly better (hysteresis)
            if (bestAction.name != currentStrategy) {
                float currentScore = GetActionScore(currentStrategy);
                
                // Require 20% improvement to switch (prevents oscillation)
                if (bestAction.score > currentScore * 1.2f) {
                    SwitchStrategy(bestAction.name, bestAction.treeFile);
                }
            }
            
            strategyChangeTimer = 0.0f;
        }
        
        // Execute current behavior tree
        if (currentTree) {
            currentTree->Execute(blackboard, dt);
        }
    }
    
    void SwitchStrategy(const std::string& strategy, const std::string& treeFile) {
        currentStrategy = strategy;
        currentTree = LoadBehaviorTree(treeFile);
        
        // Reset tree state
        if (currentTree) {
            currentTree->Reset();
        }
        
        // Debug output
        if (g_bot_debug->integer) {
            gi.DPrintf("Bot %d switching to strategy: %s\n", bot->client, strategy.c_str());
        }
    }
};
```

---

## Implementation Steps

### Week 1: Core Utility System

#### Day 1-2: Core Classes (12 hours)
- [ ] Create `UtilityEvaluator` class structure
- [ ] Implement `ScoreAction` method
- [ ] Implement `EvaluateConsideration` method
- [ ] Implement `SelectBestAction` method
- [ ] Implement `ScoreAllActions` (for debugging)
- [ ] **Write unit tests** (5 tests: score calculation, best selection, normalization, empty actions, edge cases)

#### Day 3: Curve Implementation (6 hours)
- [ ] Implement `ApplyCurve` with all curve types
- [ ] Test linear curve
- [ ] Test exponential curve
- [ ] Test inverse linear curve
- [ ] Test threshold curve
- [ ] Test logistic curve (S-curve)
- [ ] **Write unit tests** (6 tests: one per curve type)

#### Day 4: Consideration Extractors (8 hours)
- [ ] Implement all 15+ consideration extractors
- [ ] Add helper methods to BotProfile for personality traits
- [ ] Add helper: `CalculateFlankPosition`
- [ ] **Write unit tests** (8 tests: sample considerations)

#### Day 5: YAML Parsing (6 hours)
- [ ] Implement `LoadFromFile` method
- [ ] Parse action configurations
- [ ] Parse consideration configurations
- [ ] Add error handling and validation
- [ ] **Write unit tests** (3 tests: valid config, invalid config, missing fields)

### Week 2: Integration & Tuning

#### Day 6: Create Utility Configurations (8 hours)
- [ ] Create `utility/bot_utility.yaml` with 6 actions
- [ ] Define considerations for Aggress action
- [ ] Define considerations for Defend action
- [ ] Define considerations for Retreat action
- [ ] Define considerations for Investigate action
- [ ] Define considerations for Support action
- [ ] Define considerations for Flank action
- [ ] Test parsing of all configurations

#### Day 7-8: Integration with BotController (12 hours)
- [ ] Add `UtilityEvaluator` member to BotController
- [ ] Load utility configuration on bot spawn
- [ ] Implement strategy switching logic with hysteresis
- [ ] Add `SwitchStrategy` method
- [ ] Populate blackboard with utility data
- [ ] Test smooth strategy transitions

#### Day 9: Strategy-Specific Trees (8 hours)
- [ ] Create `behaviors/combat_aggressive.btree`
- [ ] Create `behaviors/combat_defensive.btree`
- [ ] Create `behaviors/combat_retreat.btree`
- [ ] Create `behaviors/combat_support.btree`
- [ ] Create `behaviors/combat_flank.btree`
- [ ] (Reuse existing investigation.btree and idle.btree)

#### Day 10-11: Testing & Tuning (12 hours)
- [ ] **Write integration tests** (6 tests):
  - Bot selects aggress when healthy and well-armed
  - Bot selects retreat when low health
  - Bot selects defend when outnumbered
  - Bot selects investigate when enemy lost
  - Bot selects support when ally in trouble
  - Bot selects flank when enemy distracted
- [ ] Test with different bot profiles
- [ ] Tune consideration weights
- [ ] Tune curve parameters (exponents, thresholds)
- [ ] Adjust hysteresis threshold (20% improvement)
- [ ] Verify emergent behaviors
- [ ] Fix bugs

#### Day 12: Debug Visualization (4 hours)
- [ ] Add console command `bot_utility_scores <botnum>`
- [ ] Display all action scores in overlay
- [ ] Highlight selected action
- [ ] Show consideration breakdown
- [ ] Add to existing debug visualization system

---

## Files to Create/Modify

### New Files
```
code/fgame/utility_evaluator.h           # UtilityEvaluator class
code/fgame/utility_evaluator.cpp         # Implementation
code/fgame/utility_curves.h              # Curve type enum and functions
code/fgame/utility_curves.cpp            # Curve implementations
code/fgame/utility_considerations.h      # Consideration extraction functions
code/fgame/utility_considerations.cpp    # Consideration extractors
code/fgame/bt_combat_helpers.h           # Combat helper functions (CalculateFlankPosition, etc.)
code/fgame/bt_combat_helpers.cpp         # Combat helper implementations
utility/bot_utility.yaml                 # Utility configuration
behaviors/combat_aggressive.btree        # Aggressive combat variant
behaviors/combat_defensive.btree         # Defensive combat variant
behaviors/combat_retreat.btree           # Retreat behavior
behaviors/combat_support.btree           # Support ally behavior
behaviors/combat_flank.btree             # Flanking behavior
tests/test_utility_evaluator.cpp         # Unit tests
tests/test_utility_curves.cpp            # Curve tests
tests/integration_test_utility.cpp       # Integration tests
```

### Modified Files
```
code/fgame/playerbot.h                # Add UtilityEvaluator member, strategy switching
code/fgame/playerbot.cpp              # Integrate utility AI, GetBotController helper
code/fgame/bt_blackboard_keys.h       # Add utility-related blackboard keys
code/fgame/bot_profile.h              # Personality trait accessors (already exist)
code/fgame/bot_profile.cpp            # No changes needed (traits already implemented)
code/fgame/CMakeLists.txt             # Add new source files to build
```

---

## Testing Strategy

### Unit Tests (22 tests)
- 5 core evaluation tests
- 6 curve tests
- 8 consideration tests
- 3 YAML parsing tests

### Integration Tests (6 tests)
- Action selection scenarios for each strategy

### Manual Testing
- [ ] Bot behavior varies with health/ammo
- [ ] Different profiles show different tendencies
- [ ] Bot adapts to changing situations
- [ ] No oscillation between strategies
- [ ] Smooth strategy transitions
- [ ] Emergent tactical behaviors visible

---

## Acceptance Criteria

### Functionality
- [ ] UtilityEvaluator scores all actions correctly
- [ ] All curve types work as expected
- [ ] 15+ considerations implemented
- [ ] YAML configuration loading works
- [ ] Integration with BotController functional
- [ ] Strategy switching with hysteresis
- [ ] 6 strategies (aggress, defend, retreat, investigate, support, flank)

### Quality
- [ ] 28 total tests pass (22 unit + 6 integration)
- [ ] Smooth strategy transitions
- [ ] No rapid oscillation
- [ ] Behavior feels dynamic and adaptive
- [ ] Code follows OpenMoHAA standards

### Performance
- [ ] Utility evaluation < 0.2ms per bot
- [ ] Strategy re-evaluation every 0.5s (not every frame)

---

## Success Metrics

### AI Quality
- **Adaptability:** Bot responds appropriately to changing context
- **Unpredictability:** Players can't exploit fixed patterns
- **Personality:** Different profiles show distinct behaviors
- **Emergence:** Complex tactics from simple rules

### Tuning Guide
- **Weights:** Adjust relative importance of considerations
- **Curves:** Change how raw values map to scores
- **Hysteresis:** Control strategy switching frequency (10-30% improvement)
- **Personality:** Profile traits influence decision making

---

## Troubleshooting

### Bot Oscillates Between Strategies
- Increase hysteresis threshold (try 30%)
- Reduce strategy re-evaluation frequency
- Smooth consideration values over time

### Bot Always Chooses Same Strategy
- Check consideration weights (too dominant?)
- Verify curve types are appropriate
- Test with different contexts

### Scores Don't Make Sense
- Print consideration breakdown for debugging
- Verify raw value extraction
- Check normalization (min/max values)

### Performance Issues
- Cache consideration values
- Reduce re-evaluation frequency
- Profile hot paths

---

## Dependencies

### Requires
- Complete behavior trees (combat, investigation, idle)
- Perception system
- BotProfile system with personality traits
- YAML parsing library

### Provides
- Dynamic strategy selection
- Emergent tactical behaviors
- Foundation for advanced AI features

---

**Next Task:** Task 3.5 - Debug Visualization (enhanced BT visualizer, utility overlay, tactical display)
