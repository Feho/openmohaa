# Migration Strategy - From Current to Ideal

## Core Principle: Incremental, Risk-Managed Evolution

**Never "big bang" rewrite.** Build new system alongside old, migrate feature by feature. Always keep the game shippable.

## Migration Philosophy

### The Strangler Fig Pattern
Like the strangler fig tree that grows around an existing tree:
1. New code grows alongside old code
2. New features use new architecture
3. Old features gradually migrated
4. Eventually, old code can be removed

### Key Rules
1. **Always Shippable:** Every commit leaves game in playable state
2. **Feature Flags:** New system toggleable via cvar for testing
3. **Gradual Migration:** One state/behavior at a time
4. **Measure Everything:** Performance, AI quality, code metrics
5. **Reversible:** Can roll back if something breaks

## Current vs. Ideal: The Gap

| Component | Current | Ideal | Migration Complexity |
|-----------|---------|-------|---------------------|
| Decision Making | State machine | Behavior Trees + Utility AI | HIGH |
| Architecture | Monolithic | 3-layer pipeline | MEDIUM |
| Data | Hardcoded cvars | YAML configs | LOW |
| Code Quality | Long functions | Small, focused | LOW |
| Testing | Manual | Automated | MEDIUM |
| Debugging | Printf | Visual tools | HIGH |
| Performance | All bots full AI | LOD + async | MEDIUM |
| Modularity | Single files | Plugin system | HIGH |

## Four-Phase Migration Plan

---

## Phase 1: Foundation (1-2 weeks)
**Goal:** Improve current code quality, prepare for bigger changes

**Risk:** LOW
**Value:** MEDIUM (immediate code quality improvements)
**Breaking Changes:** NONE

### 1.1 Code Cleanup

#### Extract Functions
Current problem: 200+ line functions mixing concerns

**Before:**
```cpp
void BotController::State_Attack() {
    // 200 lines of enemy validation, aiming, firing, movement...
}
```

**After:**
```cpp
void BotController::State_Attack() {
    if (!ValidateAttackPreconditions()) return;

    Sentient* target = SelectBestTarget();
    if (!target) return;

    AimAtTarget(target);
    UpdateAttackMovement(target);
    ExecuteFiring(target);
}

bool BotController::ValidateAttackPreconditions() {
    return m_pEnemy && controlledEnt && HasValidWeapon();
}

Sentient* BotController::SelectBestTarget() {
    // 20 lines of target selection logic
}

void BotController::AimAtTarget(Sentient* target) {
    // 30 lines of aiming logic
}

void BotController::UpdateAttackMovement(Sentient* target) {
    // 25 lines of movement logic
}

void BotController::ExecuteFiring(Sentient* target) {
    // 40 lines of firing logic
}
```

**Tasks:**
- Extract `State_Attack()` into 5-6 small functions
- Extract `State_Idle()` into 3-4 small functions
- Extract `State_Curious()` into 3-4 small functions
- Similar for other states

#### Create Data Structures
Current problem: Loose primitive variables

**Before:**
```cpp
class BotController {
    float m_fRecentDamage;
    int m_iDamageWindowStart;
    float m_fBurstDuration;
    float m_fBurstDelay;
    bool m_bRequireLowSpread;
    bool m_bAmmoLow;
    // ... 40 more member variables
};
```

**After:**
```cpp
struct CombatState {
    float recentDamage;
    int damageWindowStart;
    float burstDuration;
    float burstDelay;
    bool requireLowSpread;
    bool ammoLow;
};

struct CoverState {
    CoverPoint current;
    CoverState state;
    int nextPeekTime;
    float peekDuration;
};

class BotController {
    CombatState combat;
    CoverState cover;
    MemoryState memory;
    // Much clearer!
};
```

**Tasks:**
- Group related variables into structs
- Replace individual members with struct members
- Update all code to use new structure

### 1.2 Add Basic Testing

#### Unit Test Framework
Set up GoogleTest or similar.

```cpp
// tests/bot_pathfinding_test.cpp
TEST(BotMovementTest, CalculatesDirCorrectly) {
    BotMovement movement;
    Vector delta(100, 0, 0);

    Vector dir = movement.CalculateDir(delta);

    EXPECT_NEAR(dir.x, 1.0f, 0.01f);
    EXPECT_NEAR(dir.y, 0.0f, 0.01f);
    EXPECT_NEAR(dir.z, 0.0f, 0.01f);
}

TEST(BotControllerTest, IsValidEnemyRejectsTeammates) {
    BotController bot;
    Sentient teammate = CreateMockSentient(TEAM_ALLIES);

    EXPECT_FALSE(bot.IsValidEnemy(&teammate));
}
```

**Tasks:**
- Set up test infrastructure
- Write 10-15 unit tests for critical functions
- Add to build system
- Set up CI to run tests

### 1.3 Basic Debug Improvements

#### Console Commands
```cpp
// In-game commands for debugging
void Bot_DebugInfo(int clientNum) {
    // Print current state, target, path, etc.
}

void Bot_ForceState(int clientNum, int stateIndex) {
    // Force bot into specific state for testing
}

void Bot_ShowPath(int clientNum, bool show) {
    // Toggle path visualization
}
```

**Tasks:**
- Add console commands for common debug tasks
- Improve existing debug drawing
- Add more informative debug output

### Phase 1 Deliverables
- [ ] All functions < 100 lines (target: < 50)
- [ ] Related data grouped into structs
- [ ] 10-15 passing unit tests
- [ ] Improved debug commands
- [ ] Zero functional regressions

**Outcome:** Cleaner code that's easier to work with, foundation for Phase 2.

---

## Phase 2: Core Systems (1-2 months)
**Goal:** Implement new architecture alongside old

**Risk:** MEDIUM
**Value:** HIGH (enables new features impossible in old system)
**Breaking Changes:** NONE (new system is opt-in)

### 2.1 Perception System

#### Extract Perception Logic
Create new `PerceptionSystem` class, initially just wrapping existing code.

```cpp
// New class
class PerceptionSystem {
public:
    void Update(Player* bot, float dt);
    PerceptionSnapshot GetSnapshot() const;

private:
    std::vector<Sentient*> visibleEnemies;
    std::vector<AudioEvent> recentSounds;
};

// In BotController
class BotController {
    PerceptionSystem perception;  // New!

    void Think() {
        perception.Update(controlledEnt, dt);
        auto snapshot = perception.GetSnapshot();

        // Old state machine can use snapshot
        m_pEnemy = snapshot.closestEnemy;
        // ...
    }
};
```

**Migration Steps:**
1. Create `PerceptionSystem` class
2. Move enemy detection logic to `PerceptionSystem::UpdateVision()`
3. Move event handling to `PerceptionSystem::UpdateHearing()`
4. Create `PerceptionSnapshot` struct
5. Update `BotController` to use `PerceptionSystem`
6. Test: AI behaves identically

**Tasks:**
- Create `perception.h` and `perception.cpp`
- Implement `VisionSensor` (initially wraps existing `CanSee()`)
- Implement `AudioSensor` (wraps existing `NoticeEvent()`)
- Create `PerceptionSnapshot` data structure
- Integrate with `BotController`
- Add tests for perception logic

### 2.2 Data-Driven Configuration

#### Create Configuration System
```cpp
// Load bot profiles from YAML
class BotProfile {
public:
    static BotProfile LoadFromFile(const char* path);

    float GetAggression() const { return personality.aggression; }
    float GetCaution() const { return personality.caution; }
    // ...

private:
    struct Personality {
        float aggression;
        float caution;
        float teamwork;
    } personality;

    struct CombatParams {
        float preferredRange;
        float fireDiscipline;
        // ...
    } combat;
};

// In BotController
class BotController {
    BotProfile profile;

    BotController() {
        // Load profile based on difficulty or randomize
        profile = BotProfile::LoadFromFile("profiles/default.yaml");
    }

    void State_Attack() {
        float minRange = weapon->GetRange() * profile.GetCombatParam("min_range_factor");
        // Use profile values instead of hardcoded cvars
    }
};
```

**Migration Steps:**
1. Choose YAML library (e.g., yaml-cpp)
2. Create `BotProfile` class
3. Create default profiles (aggressive, balanced, defensive)
4. Gradually replace cvar usage with profile values
5. Add runtime profile switching for testing

**Tasks:**
- Integrate YAML parsing library
- Create `bot_profile.h` and `bot_profile.cpp`
- Create 3 default profiles
- Replace 10 most common cvar usages
- Add console command to change bot profile

### 2.3 Basic Behavior Tree Framework

#### Implement Core BT Classes
Don't migrate existing behaviors yet, just create infrastructure.

```cpp
// Behavior tree nodes
class BTNode {
public:
    enum Status { SUCCESS, FAILURE, RUNNING };
    virtual Status Execute(Blackboard& bb, float dt) = 0;
};

class BTSequence : public BTNode {
    Status Execute(Blackboard& bb, float dt) override {
        for (auto& child : children) {
            Status s = child->Execute(bb, dt);
            if (s != SUCCESS) return s;
        }
        return SUCCESS;
    }
private:
    std::vector<std::unique_ptr<BTNode>> children;
};

class BTSelector : public BTNode {
    Status Execute(Blackboard& bb, float dt) override {
        for (auto& child : children) {
            Status s = child->Execute(bb, dt);
            if (s != FAILURE) return s;
        }
        return FAILURE;
    }
private:
    std::vector<std::unique_ptr<BTNode>> children;
};

class BTCondition : public BTNode {
    using ConditionFunc = std::function<bool(Blackboard&)>;

    BTCondition(ConditionFunc func) : checkFunc(func) {}

    Status Execute(Blackboard& bb, float dt) override {
        return checkFunc(bb) ? SUCCESS : FAILURE;
    }
private:
    ConditionFunc checkFunc;
};

class BTAction : public BTNode {
    using ActionFunc = std::function<Status(Blackboard&, float)>;

    BTAction(ActionFunc func) : actionFunc(func) {}

    Status Execute(Blackboard& bb, float dt) override {
        return actionFunc(bb, dt);
    }
private:
    ActionFunc actionFunc;
};
```

**Create ONE New Behavior:** Implement simple patrol using BT

```cpp
// Create a simple patrol behavior as proof-of-concept
std::unique_ptr<BTNode> CreatePatrolTree() {
    auto root = std::make_unique<BTSequence>();

    root->AddChild(std::make_unique<BTAction>([](Blackboard& bb, float dt) {
        if (bb.Get<bool>("hasDestination")) return BTNode::SUCCESS;

        Vector dest = FindRandomPatrolPoint();
        bb.Set("patrolDestination", dest);
        bb.Set("hasDestination", true);
        return BTNode::SUCCESS;
    }));

    root->AddChild(std::make_unique<BTAction>([](Blackboard& bb, float dt) {
        Vector dest = bb.Get<Vector>("patrolDestination");
        MoveTo(dest);

        if (MoveDone()) {
            bb.Set("hasDestination", false);
            return BTNode::SUCCESS;
        }
        return BTNode::RUNNING;
    }));

    return root;
}
```

**Feature Flag:** `g_bot_use_behaviortree = 0/1` to toggle

**Migration Steps:**
1. Implement core BT node types
2. Implement Blackboard (simple key-value store)
3. Create simple patrol tree
4. Add cvar to toggle BT vs. old idle state
5. Test both paths work correctly

**Tasks:**
- Create `behavior_tree.h` and `behavior_tree.cpp`
- Implement 5 core node types (Sequence, Selector, Parallel, Condition, Action)
- Implement Blackboard
- Create proof-of-concept patrol behavior
- Add feature flag
- Add tests for BT execution

### Phase 2 Deliverables
- [ ] PerceptionSystem functional, passes all tests
- [ ] 3 bot profiles loaded from YAML
- [ ] Basic BT framework implemented
- [ ] One behavior (patrol) working in BT
- [ ] Feature flags allow toggling new systems
- [ ] No regressions in existing gameplay

**Outcome:** New architecture proven, ready for full migration.

---

## Phase 3: Migration & Advanced Features (1-2 months)
**Goal:** Migrate behaviors to new system, add advanced AI

**Risk:** MEDIUM
**Value:** VERY HIGH (significantly better AI)
**Breaking Changes:** OPTIONAL (old system can be removed or kept as fallback)

### 3.1 Migrate Behaviors to Behavior Trees

#### One State at a Time
Migrate in order of complexity (simple → complex):

1. **Idle State** → Simple patrol tree ✅ (done in Phase 2)
2. **Curious State** → Investigation tree
3. **Attack State** → Combat tree
4. **Investigate State** → Search tree

**Example: Combat Behavior Tree**

```yaml
# behaviors/combat.btree
tree:
  root:
    type: selector
    children:
      # Emergency
      - type: sequence
        name: "EmergencyRetreat"
        children:
          - type: condition
            check: "health < 0.25"
          - type: action
            action: "FindCover"
          - type: action
            action: "RetreatToCover"

      # Main combat
      - type: sequence
        name: "EngageEnemy"
        children:
          - type: condition
            check: "HasVisibleEnemy"

          - type: parallel
            policy: "RequireAll"
            children:
              - type: sequence
                name: "ManageDistance"
                children:
                  - type: selector
                    children:
                      - type: sequence
                        children:
                          - type: condition
                            check: "EnemyDistance < weapon.minRange"
                          - type: action
                            action: "BackAway"
                      - type: sequence
                        children:
                          - type: condition
                            check: "EnemyDistance > weapon.maxRange"
                          - type: action
                            action: "MoveCloser"
                      - type: action
                        action: "MaintainDistance"

              - type: action
                action: "AimAtEnemy"

              - type: action
                action: "Fire"
```

**Migration Steps:**
1. Load combat tree from YAML
2. Implement tree actions (BackAway, MoveCloser, AimAtEnemy, Fire)
3. Test with `g_bot_use_behaviortree_combat = 1`
4. Compare AI performance old vs. new
5. Fix issues, tune parameters
6. Make new system default

### 3.2 Implement Utility AI

#### Action Scoring System
```cpp
class UtilityEvaluator {
public:
    struct ScoredAction {
        std::string name;
        float score;
        ActionContext context;
    };

    ScoredAction SelectBestAction(const PerceptionSnapshot& perception) {
        std::vector<ScoredAction> scores;

        scores.push_back(ScoreAction("Aggress", perception));
        scores.push_back(ScoreAction("Defend", perception));
        scores.push_back(ScoreAction("Retreat", perception));
        scores.push_back(ScoreAction("Investigate", perception));

        return *std::max_element(scores.begin(), scores.end(),
            [](auto& a, auto& b) { return a.score < b.score; });
    }

private:
    ScoredAction ScoreAction(const std::string& action, const PerceptionSnapshot& perc) {
        float score = 0.0f;

        if (action == "Aggress") {
            float healthFactor = bot.health / bot.maxHealth;
            float ammoFactor = bot.ammo / bot.maxAmmo;
            float enemyFactor = std::min(perc.visibleEnemies.size() / 3.0f, 1.0f);

            score = healthFactor * 0.4f + ammoFactor * 0.3f + enemyFactor * 0.3f;
        }
        // ... score other actions

        return {action, score, {}};
    }
};
```

**Integration with BT:**
Use utility AI to select *which* behavior tree to run.

```cpp
void BotController::Think() {
    auto perception = perceptionSystem.GetSnapshot();

    // Utility AI picks high-level strategy
    auto decision = utilityEvaluator.SelectBestAction(perception);

    // Behavior tree executes chosen strategy
    if (decision.name == "Aggress") {
        aggressTree->Execute(blackboard, dt);
    } else if (decision.name == "Defend") {
        defendTree->Execute(blackboard, dt);
    }
    // ...
}
```

### 3.3 Advanced Debugging Tools

#### Behavior Tree Visualizer
```cpp
void DebugDrawBehaviorTree(BotController* bot) {
    auto tree = bot->GetActiveBehaviorTree();
    Vector screenPos(10, 100, 0);

    DrawTreeNode(tree->GetRoot(), screenPos, 0);
}

void DrawTreeNode(BTNode* node, Vector pos, int depth) {
    Color color = GetNodeColor(node->GetLastStatus());
    DrawBox(pos, pos + Vector(100, 20, 0), color);
    DrawText(pos, node->GetName());

    // Draw children recursively
    for (int i = 0; i < node->GetChildCount(); i++) {
        Vector childPos = pos + Vector(20, 30 + i * 30, 0);
        DrawTreeNode(node->GetChild(i), childPos, depth + 1);
    }
}
```

#### Utility Score Overlay
```cpp
void DebugDrawUtilityScores(BotController* bot) {
    auto scores = bot->GetLastUtilityScores();

    Vector pos(10, 300, 0);
    for (auto& score : scores) {
        DrawBar(pos, score.score, score.name);
        pos.y += 25;
    }
}
```

### Phase 3 Deliverables
- [ ] All behaviors migrated to behavior trees
- [ ] Utility AI selecting high-level strategies
- [ ] Behavior tree visualizer functional
- [ ] AI quality measurably improved
- [ ] Old state machine can be removed (or kept as fallback)

**Outcome:** Significantly smarter, more dynamic AI. Clean, maintainable codebase.

---

## Phase 4: Polish & Optimization (ongoing)
**Goal:** Performance, tools, extensibility

**Risk:** LOW
**Value:** MEDIUM-HIGH
**Breaking Changes:** NONE

### 4.1 Performance Optimizations

#### LOD System
```cpp
class AIScheduler {
    void Update(float dt) {
        for (auto& bot : bots) {
            AILOD lod = DetermineLOD(bot);

            switch (lod) {
                case AILOD::HIGH:    bot->Think(dt); break;                // 60 FPS
                case AILOD::MEDIUM:  if (frame % 3 == 0) bot->Think(dt * 3); break;   // 20 FPS
                case AILOD::LOW:     if (frame % 6 == 0) bot->Think(dt * 6); break;   // 10 FPS
                case AILOD::SLEEPING: if (frame % 60 == 0) bot->Think(dt * 60); break; // 1 FPS
            }
        }
        frame++;
    }
};
```

#### Async Pathfinding
```cpp
class AsyncPathfinder {
    std::future<PathResult> RequestPath(PathRequest req) {
        return std::async(std::launch::async, [=]() {
            return pathfinder.ComputePath(req);
        });
    }
};
```

### 4.2 Plugin System

```cpp
class IBehaviorPlugin {
    virtual void Initialize() = 0;
    virtual void RegisterBehaviors(BehaviorTreeFactory& factory) = 0;
    virtual void RegisterUtilityActions(UtilityEvaluator& evaluator) = 0;
};

// Example plugin
class ObjectiveGameModePlugin : public IBehaviorPlugin {
    void RegisterBehaviors(BehaviorTreeFactory& factory) override {
        factory.RegisterAction("CaptureFlag", &CaptureFlag);
        factory.RegisterAction("DefendFlag", &DefendFlag);
    }
};
```

### Phase 4 Deliverables
- [ ] LOD system: 100+ bots at 60 FPS
- [ ] Async pathfinding (no frame hitches)
- [ ] Plugin system functional
- [ ] Full documentation
- [ ] Example custom behaviors

**Outcome:** Production-ready, scalable, extensible AI system.

---

## Migration Metrics

### How to Measure Success

**Code Quality:**
```
- Average function length: < 50 lines
- Cyclomatic complexity: < 10
- Test coverage: > 80%
- Compile warnings: 0
```

**Performance:**
```
- Bot count at 60 FPS: 100+ (vs. current ~30)
- Average Think() time: < 1ms
- Memory usage: +10% max (for better architecture)
- Pathfinding frame time: < 0.1ms (async)
```

**AI Quality:**
```
- Player survey: "AI feels human" > 70%
- Behavior diversity: 5+ distinct playstyles measurable
- Bug count: < current (more testable code)
```

**Development Velocity:**
```
- Time to add new behavior: 1 hour (vs. current ~8 hours)
- Time to fix typical bug: 15 min (vs. current ~2 hours)
- Time to create bot profile: 5 min (no code)
```

## Risk Mitigation

### What Could Go Wrong?

**Risk:** Performance regression
**Mitigation:** Profile continuously, LOD from day 1, feature flags allow rollback

**Risk:** AI quality worse after migration
**Mitigation:** A/B testing, keep old system as fallback, gradual migration

**Risk:** Team doesn't adopt new architecture
**Mitigation:** Documentation, examples, pair programming, show quick wins

**Risk:** Scope creep
**Mitigation:** Stick to phase plan, ship incrementally, resist "while we're at it..."

**Risk:** Bugs in production
**Mitigation:** Extensive testing, gradual rollout, monitoring, quick rollback ability

## Timeline Summary

| Phase | Duration | Risk | Value | Outcome |
|-------|----------|------|-------|---------|
| Phase 1: Foundation | 1-2 weeks | LOW | MEDIUM | Cleaner code, testable |
| Phase 2: Core Systems | 1-2 months | MEDIUM | HIGH | New architecture proven |
| Phase 3: Migration | 1-2 months | MEDIUM | VERY HIGH | Superior AI, clean codebase |
| Phase 4: Polish | Ongoing | LOW | MEDIUM-HIGH | Production-ready, scalable |

**Total:** 3-5 months for core migration, then ongoing polish.

## Conclusion

This migration strategy gets you from current state to ideal architecture in **safe, incremental steps**. Each phase delivers value, and you can stop at any point with a working system.

The key is discipline:
- Don't skip phases
- Don't rush
- Test everything
- Measure continuously
- Ship frequently

**Start with Phase 1 today. You'll see benefits within a week.**

---

**Next:** Review epics and tasks to begin implementation.
