# Behavior Trees - Reference & Theory

## Overview
Behavior Trees (BTs) are a hierarchical decision-making structure used extensively in game AI. Unlike finite state machines, BTs are modular, composable, and easy to visualize.

## Core Concepts

### Node Types

#### 1. Composite Nodes
Parent nodes that have one or more children.

**Selector (OR Node)**
- Executes children left-to-right
- Returns SUCCESS on first child success
- Returns RUNNING if child returns RUNNING
- Returns FAILURE only if all children fail
- Use case: "Try this, if it fails try that"

**Sequence (AND Node)**
- Executes children left-to-right
- Returns FAILURE on first child failure
- Returns RUNNING if child returns RUNNING
- Returns SUCCESS only if all children succeed
- Use case: "Do this, then this, then this"

**Parallel**
- Executes all children simultaneously
- Success policy determines when to return SUCCESS
  - RequireAll: All must succeed
  - RequireOne: At least one must succeed
  - RequireN: N specific children must succeed
- Use case: "Do these things at the same time"

#### 2. Leaf Nodes
Nodes that actually do something.

**Condition**
- Tests a boolean condition
- Returns SUCCESS or FAILURE immediately
- Never returns RUNNING
- Examples: HasEnemy(), LowHealth(), HasAmmo()

**Action**
- Performs actual behavior
- Can return RUNNING for multi-frame actions
- Examples: MoveTo(), AimAt(), Fire(), Search()

#### 3. Decorator Nodes (Optional)
Modify behavior of single child.

**Inverter**
- SUCCESS → FAILURE, FAILURE → SUCCESS
- Use case: "Not" logic

**Repeat**
- Repeats child N times or forever
- Use case: Patrol loops

**UntilFail**
- Keeps executing child until it fails
- Use case: Persistent behaviors

## Execution Model

### Tree Traversal
1. Start at root
2. Execute current node
3. Based on result:
   - SUCCESS/FAILURE: Move to next sibling or return to parent
   - RUNNING: Pause here, resume next frame

### Example Execution

```
Selector
├─ Sequence (Combat)
│  ├─ Condition: HasEnemy ✓ → SUCCESS
│  └─ Action: Attack ⏵ → RUNNING
└─ Action: Patrol

Frame 1: HasEnemy returns SUCCESS, Attack starts, returns RUNNING
Frame 2: Resume Attack, still RUNNING
Frame 3: Attack completes, returns SUCCESS, Sequence returns SUCCESS, Selector returns SUCCESS
Frame 4: Tree restarts, HasEnemy checked again...
```

## Blackboard Pattern

Behavior trees use a "blackboard" for shared state.

```cpp
Blackboard blackboard;
blackboard.Set("target", enemy);
blackboard.Set("destination", Vector(100, 0, 0));

// Actions read from blackboard
auto target = blackboard.Get<Sentient*>("target");
auto dest = blackboard.Get<Vector>("destination");
```

**Benefits:**
- Nodes communicate without tight coupling
- Easy to test (inject blackboard state)
- Clear data dependencies

## Design Patterns

### Pattern 1: Priority Selector
```
Selector
├─ Emergency (high priority)
├─ Combat (medium priority)
└─ Patrol (low priority)
```

First valid option wins. Natural priority ordering.

### Pattern 2: Sequence with Validation
```
Sequence
├─ Condition: ValidateTarget
├─ Condition: HasAmmo
├─ Action: Aim
└─ Action: Fire
```

Only fires if all preconditions met.

### Pattern 3: Parallel Behaviors
```
Parallel (RequireAll)
├─ Action: MaintainDistance
├─ Action: Aim
└─ Action: Fire
```

Movement, aiming, and firing happen simultaneously.

### Pattern 4: Subtree Reuse
```
Combat Tree:
  - include: "check_weapon.btree"
  - include: "engage_target.btree"

Search Tree:
  - include: "check_weapon.btree"  # Same subtree!
  - include: "search_area.btree"
```

DRY principle: define once, use everywhere.

## Advantages over State Machines

| Aspect | State Machine | Behavior Tree |
|--------|--------------|---------------|
| Structure | Flat, spaghetti transitions | Hierarchical, clear |
| Modularity | Difficult | Easy (subtrees) |
| Visual | Hard to visualize | Tree diagram |
| Priority | Manual management | Implicit (left-to-right) |
| Reusability | Copy-paste | Include subtrees |
| Debugging | Breakpoints | Visual tree state |

## Best Practices

### 1. Keep Trees Shallow
❌ Bad: 10+ levels deep
✅ Good: 3-5 levels max

Deep trees are hard to understand. Use subtrees to manage complexity.

### 2. Single Responsibility
❌ Bad: Action that does movement + aiming + firing
✅ Good: Separate actions, combined with Parallel

Small, focused nodes are easier to test and reuse.

### 3. Validate Early
```
Sequence
├─ Condition: ValidateEverything  ✅ Fail fast!
├─ Action: ExpensiveOperation
└─ Action: AnotherExpensiveOperation
```

Put conditions before expensive actions.

### 4. Name Clearly
❌ Bad: `Sequence1`, `Action4`
✅ Good: `EmergencyRetreat`, `AimAtEnemy`

Trees are self-documenting with good names.

### 5. Avoid Deep State
Keep state in blackboard, not in actions.

❌ Bad:
```cpp
class AttackAction {
    float timer;  // Internal state
    Vector lastPos;  // More state
};
```

✅ Good:
```cpp
class AttackAction {
    Status Execute(Blackboard& bb, float dt) {
        // State in blackboard
        float timer = bb.Get<float>("attackTimer");
        bb.Set("attackTimer", timer + dt);
    }
};
```

## Common Pitfalls

### 1. Too Much Logic in Conditions
❌ Bad:
```cpp
bool HasGoodShotCondition() {
    // 50 lines of complex logic
}
```

✅ Good:
```cpp
bool HasGoodShotCondition() {
    return blackboard.Get<float>("shotQuality") > 0.7f;
}
// Calculate shotQuality elsewhere
```

### 2. Forgetting to Reset
Actions must reset state when restarted.

```cpp
void OnEnter() {
    timer = 0.0f;  // Reset!
}
```

### 3. Blocking Actions
❌ Bad:
```cpp
Status MoveToAction() {
    while (!arrived) {
        Move();  // Blocks entire tree!
    }
    return SUCCESS;
}
```

✅ Good:
```cpp
Status MoveToAction() {
    Move();
    return arrived ? SUCCESS : RUNNING;
}
```

## Performance Tips

### 1. Cache Results
Don't recalculate conditions every frame if result won't change.

### 2. LOD
High LOD: Execute every frame
Low LOD: Execute every N frames

### 3. Early Out
Selectors naturally early-out (stop after first success).

### 4. Avoid Allocations
Pre-allocate nodes, don't create during execution.

## Testing Behavior Trees

### Unit Tests
```cpp
TEST(BehaviorTreeTest, SelectorReturnsFirstSuccess) {
    auto tree = Selector()
        .AddChild(FailureNode())
        .AddChild(SuccessNode())
        .AddChild(SuccessNode())  // Never reached
        .Build();

    EXPECT_EQ(tree->Execute(bb, 0.1f), SUCCESS);
}
```

### Integration Tests
```cpp
TEST(CombatBehaviorTest, RetreatsWhenLowHealth) {
    Blackboard bb;
    bb.Set("health", 0.1f);

    auto result = combatTree->Execute(bb, 0.1f);

    EXPECT_TRUE(bb.Get<bool>("isRetreating"));
}
```

## Resources

### Books
- "Behavior Trees for Robotics and AI" - Michele Colledanchise
- "Game AI Pro" series - various authors (chapters on BTs)

### Papers
- "Behavior Trees for AI in Games" - Alex J. Champandard
- "A Comparative Evaluation of Behavior Trees and Finite State Machines" - Iovino et al.

### Tools
- Unreal Engine Behavior Tree Editor
- Unity Behavior Designer plugin
- BehaviorTree.CPP library (C++ implementation)

### Online
- [AIGameDev.com](http://aigamedev.com) - Behavior tree tutorials
- [GDC Vault](https://gdcvault.com) - Search "behavior trees"
- [Gamasutra](https://gamasutra.com) - AI architecture articles

## OpenMoHAA Implementation Notes

For our implementation:
- Start with simple nodes (Selector, Sequence, Condition, Action)
- Use YAML for tree definition
- Blackboard for state sharing
- Parallel node for combat (aim + move + fire)
- Subtrees for reusable behaviors
- Visual debugger to show active nodes

See `examples/behavior-tree-example.yaml` for complete example.
