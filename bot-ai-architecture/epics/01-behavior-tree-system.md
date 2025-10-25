# Epic 1: Behavior Tree System

## Overview
Replace priority-based state machine with hierarchical Behavior Tree system for more maintainable, composable AI decision-making.

## Business Value
- **Maintainability:** Visual, hierarchical structure easier to understand than procedural code
- **Composability:** Reuse subtrees across different bot types
- **Designer Empowerment:** Non-programmers can create/modify behaviors
- **Debugging:** Clear visualization of decision flow

## Current State
```cpp
// Priority-based state machine in BotController::CheckStates()
static botfunc_t botfuncs[MAX_BOT_FUNCTIONS] = {
    {&CheckCondition_Attack, &BeginAttack, &EndAttack, &State_Attack},      // Priority 0 (highest)
    {&CheckCondition_Investigate, ...},                                      // Priority 1
    {&CheckCondition_Curious, ...},                                          // Priority 2
    {&CheckCondition_Grenade, ...},                                          // Priority 3
    {&CheckCondition_Idle, ...}                                              // Priority 4 (lowest)
};

// States evaluated sequentially, first valid condition wins
```

**Problems:**
- Hard to visualize decision logic
- Adding new behavior requires modifying array order
- No composition (can't reuse attack logic in different contexts)
- Debugging requires printf and breakpoints

## Target State
```cpp
// Behavior Tree with composite nodes
class BehaviorTree {
    std::unique_ptr<BTNode> root;
    Blackboard blackboard;

    NodeStatus Execute(float dt);
};

// Example tree structure:
//  Selector (choose first success)
//  ├─ Sequence (Emergency)
//  │  ├─ Condition: Health < 25%
//  │  ├─ Action: FindCover
//  │  └─ Action: Retreat
//  ├─ Sequence (Combat)
//  │  ├─ Condition: HasEnemy
//  │  └─ Parallel
//  │     ├─ Action: Aim
//  │     ├─ Action: Fire
//  │     └─ Action: Move
//  └─ Action: Patrol
```

**Benefits:**
- Visual tree structure
- Composable nodes
- Data-driven (trees defined in YAML)
- Runtime debugging tools

## Acceptance Criteria
- [ ] Core BT node types implemented (Selector, Sequence, Parallel, Condition, Action)
- [ ] Blackboard system for shared state
- [ ] At least one complete behavior (e.g., patrol) works via BT
- [ ] YAML parsing for tree definition
- [ ] Debug visualization shows active nodes
- [ ] Performance: BT execution < 0.5ms per bot
- [ ] Feature flag allows toggling BT vs. old state machine
- [ ] Zero AI regressions when BT enabled

## Technical Components

### 1. Core Node Types
```cpp
class BTNode {
public:
    enum Status { SUCCESS, FAILURE, RUNNING };
    virtual Status Execute(Blackboard& bb, float dt) = 0;
    virtual void Reset() = 0;
};

class BTSelector : public BTNode {
    // Tries children left-to-right until one succeeds
    // Returns: First child SUCCESS/RUNNING, or FAILURE if all fail
};

class BTSequence : public BTNode {
    // Executes children left-to-right until one fails
    // Returns: First child FAILURE/RUNNING, or SUCCESS if all succeed
};

class BTParallel : public BTNode {
    // Executes all children simultaneously
    // Policy: RequireAll, RequireOne, etc.
};

class BTCondition : public BTNode {
    // Evaluates condition, returns SUCCESS/FAILURE
};

class BTAction : public BTNode {
    // Performs action, may return RUNNING for multi-frame actions
};
```

### 2. Blackboard System
```cpp
class Blackboard {
public:
    template<typename T>
    void Set(const std::string& key, const T& value);

    template<typename T>
    T Get(const std::string& key) const;

    bool Has(const std::string& key) const;

private:
    std::unordered_map<std::string, std::any> data;
};
```

### 3. Tree Builder
```cpp
class BehaviorTreeBuilder {
public:
    BehaviorTreeBuilder& Sequence();
    BehaviorTreeBuilder& Selector();
    BehaviorTreeBuilder& Parallel(ParallelPolicy policy);
    BehaviorTreeBuilder& Condition(ConditionFunc func);
    BehaviorTreeBuilder& Action(ActionFunc func);
    BehaviorTreeBuilder& End();

    std::unique_ptr<BTNode> Build();
};

// Usage:
auto tree = BehaviorTreeBuilder()
    .Selector()
        .Sequence()
            .Condition([](auto& bb) { return bb.Get<float>("health") < 0.25f; })
            .Action([](auto& bb, float dt) { /* retreat */ })
        .End()
        .Action([](auto& bb, float dt) { /* patrol */ })
    .End()
    .Build();
```

### 4. YAML Definition
```yaml
# combat.btree
tree:
  type: selector
  children:
    - type: sequence
      name: "Emergency Retreat"
      children:
        - type: condition
          check: "health < 0.25"
        - type: action
          action: "FindCover"
        - type: action
          action: "Retreat"

    - type: sequence
      name: "Combat"
      children:
        - type: condition
          check: "HasEnemy()"
        - type: parallel
          policy: "RequireAll"
          children:
            - type: action
              action: "AimAtEnemy"
            - type: action
              action: "Fire"
```

## Dependencies
- YAML parsing library (yaml-cpp or similar)
- Blackboard key-value store
- Debug rendering system

## Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Performance regression | HIGH | MEDIUM | Profile early, optimize node execution, cache results |
| Steep learning curve | MEDIUM | HIGH | Good documentation, examples, visual editor (Phase 4) |
| Migration complexity | HIGH | MEDIUM | Gradual migration, keep old system working, feature flags |
| Hard to debug | MEDIUM | MEDIUM | Invest in visualization tools early |

## Implementation Phases

### Phase 1: Core Framework (Week 1-2)
- Implement base `BTNode` class
- Implement 5 core node types
- Implement Blackboard
- Write unit tests

### Phase 2: Integration (Week 3)
- Create `BehaviorTreeBuilder`
- Integrate with `BotController`
- Feature flag to toggle BT
- Migrate one simple behavior (patrol)

### Phase 3: Data-Driven (Week 4)
- YAML parsing
- Load trees from files
- Hot-reload support

### Phase 4: Migration (Week 5-8)
- Migrate remaining behaviors one-by-one
- Test each migration
- Performance tuning

## Success Metrics
- **Code Quality:** Behavior logic in YAML, not C++
- **Performance:** < 0.5ms per bot BT execution
- **Maintainability:** Add new behavior in < 1 hour
- **AI Quality:** No regressions vs. old system

## Related Epics
- **Epic 2:** Perception System (provides data to BT)
- **Epic 3:** Utility AI (can work alongside BT for action selection)
- **Epic 7:** Debug Visualization (BT viewer)

## References
- `references/behavior-trees.md`
- [Behavior Trees in Robotics and AI](https://arxiv.org/abs/1709.00084)
- [Unreal Engine Behavior Trees](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/ArtificialIntelligence/BehaviorTrees/)

## Open Questions
- [ ] Should we support decorators (nodes that modify child behavior)?
- [ ] Do we need services (background tasks that run alongside tree)?
- [ ] Should blackboard be per-bot or shared across squad?
- [ ] How to handle tree switching (e.g., different trees for different situations)?

## Example Use Cases

### Use Case 1: Patrol Behavior
Bot patrols area, investigates sounds, returns to patrol.

```yaml
patrol_tree:
  type: selector
  children:
    - type: sequence
      name: "Investigate Sound"
      children:
        - type: condition
          check: "HeardRecentSound()"
        - type: action
          action: "MoveToSoundLocation"
        - type: action
          action: "LookAround"

    - type: action
      action: "PatrolWaypoints"
```

### Use Case 2: Combat Behavior
Engages enemy with cover usage and tactical retreat.

```yaml
combat_tree:
  type: selector
  children:
    - type: sequence
      name: "Emergency"
      children:
        - type: condition
          check: "health < 0.25"
        - type: action
          action: "Retreat"

    - type: sequence
      name: "Combat"
      children:
        - type: condition
          check: "HasEnemy()"
        - type: parallel
          children:
            - type: sequence
              name: "Movement"
              children:
                - type: selector
                  children:
                    - type: sequence
                      children:
                        - type: condition
                          check: "!IsInCover()"
                        - type: action
                          action: "FindCover"
                    - type: action
                      action: "PeekFromCover"

            - type: action
              action: "AimAndFire"
```

## Notes
- Start simple: basic nodes, one behavior
- Iterate based on feedback
- Visual editor can come later (Phase 4)
- Focus on correctness first, performance second
