# Epic 3: Utility AI

## Overview
Implement Utility-based AI for dynamic action selection based on context-sensitive scoring, creating more unpredictable and adaptive bot behaviors.

## Business Value
- **Unpredictability:** Bots don't follow rigid patterns, players can't exploit them
- **Context-Aware:** Same situation can yield different responses based on subtle factors
- **Emergent Behavior:** Complex tactics emerge from simple scoring rules
- **Tunability:** Easy to adjust via configuration without code changes

## Current State
Hard-coded priority system: Attack > Investigate > Curious > Idle

**Problems:**
- Rigid, predictable behavior
- Binary decisions (all-or-nothing)
- Difficult to add new actions without breaking priority order

## Target State
```cpp
class UtilityEvaluator {
    struct ScoredAction {
        std::string name;
        float score;  // 0.0 - 1.0
    };

    ScoredAction SelectBestAction(const PerceptionSnapshot& perc, const BotState& state);
};
```

Dynamic scoring considers: health, ammo, enemy count, cover availability, ally positions, personality traits.

## Acceptance Criteria
- [ ] Utility evaluator scores 5+ actions
- [ ] Consideration curves configurable in YAML
- [ ] Integration with behavior trees (utility picks tree, BT executes)
- [ ] Measurable behavior diversity (bots don't all act identically)
- [ ] Performance: < 0.2ms per evaluation

## Technical Components

### Consideration Curves
```yaml
considerations:
  health_factor:
    curve: linear
    weight: 0.4

  ammo_factor:
    curve: exponential
    exponent: 2.0
    weight: 0.3

  enemy_proximity:
    curve: inverse_linear
    min: 0
    max: 1000
    weight: 0.3
```

### Actions to Score
- Aggress (push forward)
- Defend (hold position, use cover)
- Retreat (fall back)
- Investigate (search area)
- Support (help teammate)
- Flank (circle around enemy)

## Related Epics
- Epic 1 (Behavior Trees - utility picks tree)
- Epic 2 (Perception - provides context for scoring)

## References
- `references/utility-ai.md`
- [Utility AI for Decision Making (GDC)](https://www.gdcvault.com/play/1021848/)
