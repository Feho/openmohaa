# Architecture Vision - Detailed Technical Design

## Executive Summary

This document describes the **ideal bot AI architecture** for OpenMoHAA, designed from first principles with unlimited budget and time. The architecture combines:

- **Behavior Trees** for hierarchical decision-making
- **Utility AI** for dynamic action selection
- **GOAP** for tactical planning
- **ECS** (Entity-Component-System) for data-oriented design
- **Data-driven configuration** for designer empowerment
- **Professional testing** at all levels

## 1. Core Architecture: Three-Layer Pipeline

### 1.1 Perception Layer
**Responsibility:** What can the bot sense about the world?

```cpp
class PerceptionSystem {
public:
    PerceptionSnapshot Update(float dt);

private:
    VisionSensor vision;
    AudioSensor hearing;
    MemorySystem memory;
    WorldKnowledgeBase worldKnowledge;
};

struct PerceptionSnapshot {
    std::vector<EnemyInfo> visibleEnemies;
    std::vector<EnemyInfo> knownEnemies;      // From memory
    std::vector<AudioEvent> recentSounds;
    std::vector<AllyInfo> nearbyAllies;
    std::vector<TacticalPoint> interestPoints; // Cover, objectives, items
    EnvironmentInfo environment;               // Lighting, weather, terrain
    ThreatAssessment threatLevel;
};
```

**Key Features:**
- **Vision Sensor:** FOV-based, handles occlusion, distance attenuation, lighting conditions
- **Audio Sensor:** 3D positional audio, priority-based filtering, direction estimation
- **Memory System:** Stores last known positions, velocity predictions, confidence decay over time
- **World Knowledge:** Static info (map layout, item locations, chokepoints) learned over time

**Benefits:**
- Bots can't "see through walls" (proper vision simulation)
- Memory creates realistic searching behavior
- Sound creates tactical opportunities (hear footsteps, gunfire)
- Clean interface to decision layer

### 1.2 Decision Layer
**Responsibility:** What should the bot do based on what it senses?

Three decision-making systems work together:

#### 1.2.1 Behavior Trees (Primary)
Hierarchical, modular decision structures.

```cpp
class BehaviorTree {
public:
    NodeStatus Execute(Blackboard& blackboard, float dt);
    void SetRootNode(std::unique_ptr<Node> root);

private:
    std::unique_ptr<Node> rootNode;
};

// Node types
class Selector : public CompositeNode {
    // Tries children until one succeeds (OR logic)
};

class Sequence : public CompositeNode {
    // Executes children until one fails (AND logic)
};

class Parallel : public CompositeNode {
    // Executes multiple children simultaneously
};

class Condition : public LeafNode {
    // Evaluates blackboard state, returns success/failure
};

class Action : public LeafNode {
    // Performs actual behavior (move, shoot, etc.)
};
```

**Example Tree:**
```
Selector (Pick first that succeeds)
├─ Sequence (Emergency)
│  ├─ Condition: Health < 25%
│  ├─ Action: FindCover
│  └─ Action: Heal
├─ Sequence (Combat)
│  ├─ Condition: HasVisibleEnemy
│  ├─ Parallel
│  │  ├─ Action: MaintainDistance
│  │  ├─ Action: AimAtEnemy
│  │  └─ Action: Fire
│  └─ Action: SuppressFire
└─ Sequence (Patrol/Idle)
   ├─ Condition: NoThreats
   └─ Action: PatrolArea
```

**Benefits:**
- Visual, hierarchical structure (easy to understand)
- Composable (reuse subtrees)
- Reactive (re-evaluates every frame)
- Designer-friendly (can be edited visually)

#### 1.2.2 Utility AI (Action Scoring)
Dynamically scores actions based on context.

```cpp
class UtilityEvaluator {
public:
    struct ScoredAction {
        ActionType type;
        float score;        // 0.0 - 1.0
        ActionContext context;
    };

    std::vector<ScoredAction> ScoreAllActions(
        const PerceptionSnapshot& perception,
        const BotState& state
    );

private:
    std::vector<ConsiderationCurve> considerations;
};

// Example considerations
float ScoreTakeCover(const Context& ctx) {
    float healthFactor = 1.0f - (ctx.health / ctx.maxHealth);
    float exposureFactor = ctx.isExposed ? 1.0f : 0.0f;
    float enemyCountFactor = std::min(ctx.visibleEnemies / 3.0f, 1.0f);

    return (healthFactor * 0.5f + exposureFactor * 0.3f + enemyCountFactor * 0.2f);
}

float ScoreAggress(const Context& ctx) {
    float healthFactor = ctx.health / ctx.maxHealth;
    float ammoFactor = ctx.ammo / ctx.maxAmmo;
    float allyFactor = std::min(ctx.nearbyAllies / 2.0f, 1.0f);

    return (healthFactor * 0.4f + ammoFactor * 0.3f + allyFactor * 0.3f);
}
```

**Benefits:**
- Smooth, context-sensitive decisions (not binary)
- Emergent behavior from simple rules
- Easy to tune (adjust weights)
- Unpredictable (players can't memorize patterns)

#### 1.2.3 GOAP (Goal-Oriented Action Planning)
Plans multi-step action sequences to achieve goals.

```cpp
class TacticalPlanner {
public:
    struct Action {
        std::string name;
        WorldState preconditions;
        WorldState effects;
        float cost;
    };

    std::vector<Action> PlanActionSequence(
        const WorldState& current,
        const WorldState& goal
    );

private:
    std::vector<Action> availableActions;
    AStarPlanner planner;
};

// Example: Bot needs to kill enemy but out of ammo
// Current: { hasAmmo: false, enemyAlive: true, weaponLoaded: false }
// Goal:    { enemyAlive: false }
//
// Planner finds sequence:
// 1. FindAmmo     (hasAmmo: true)
// 2. ReloadWeapon (weaponLoaded: true)
// 3. KillEnemy    (enemyAlive: false)
```

**Benefits:**
- Solves complex problems autonomously
- Flexible (add new actions, planner adapts)
- Emergent tactics (finds creative solutions)
- Handles resource constraints naturally

### 1.3 Action Layer
**Responsibility:** How does the bot physically execute decisions?

```cpp
class ActionSystem {
public:
    void Execute(const Decision& decision, float dt);

private:
    MovementController movement;
    AimController aim;
    WeaponController weapon;
    AnimationController animation;
};

class MovementController {
    void MoveTo(Vector target);
    void MoveNear(Vector target, float radius);
    void AvoidPath(Vector danger, Vector preferredDir);
    void Stop();

private:
    AsyncPathfinder pathfinder;
    CollisionAvoidance collision;
    JumpDetector jumpDetector;
};

class AimController {
    void AimAt(Vector target);
    void AimSmooth(Vector target, float smoothness);
    void AddRecoil(Vector recoilPattern);
    void AddHumanError(float inaccuracy);

private:
    Vector currentAim;
    Vector targetAim;
    float turnSpeed;
};

class WeaponController {
    void Fire(FireMode mode);
    void Reload();
    void SwitchTo(WeaponType type);

    bool HasAmmo();
    float GetAccuracy();
    float GetEffectiveRange();
};
```

**Benefits:**
- Clean interface (decision layer doesn't care about implementation)
- Controllers handle complexities (pathfinding, aiming, recoil)
- Easy to test in isolation
- Can swap implementations (e.g., different pathfinding algorithms)

## 2. Data-Driven Configuration

### 2.1 Bot Profiles
Defines bot personality and behavior parameters.

```yaml
# profiles/aggressive.yaml
profile:
  name: "Aggressive Rusher"
  description: "Charges enemies, high risk/reward playstyle"

  personality:
    aggression: 0.9        # 0.0 (defensive) - 1.0 (aggressive)
    caution: 0.2           # Willingness to take cover
    teamwork: 0.5          # Follows squad tactics
    creativity: 0.7        # Tries unconventional approaches

  combat:
    preferred_range: 256   # Optimal combat distance
    fire_discipline: 0.3   # 0.0 (spray) - 1.0 (precise)
    burst_length: [0.5, 1.5]
    reload_under_fire: true
    ammo_conservation: 0.2  # Low = doesn't care about ammo

  movement:
    speed_preference: 1.3  # Multiplier on base speed
    jump_frequency: 0.8    # How often to jump
    path_deviation: 0.3    # Randomness in pathing
    crouch_usage: 0.1      # Rarely crouches

  aim:
    reaction_time: [0.1, 0.3]    # Min/max seconds to acquire target
    tracking_smoothness: 0.6     # 0.0 (instant) - 1.0 (smooth)
    spread_multiplier: 1.3       # Higher = less accurate
    headshot_bias: 0.4           # Tendency to aim for head

  tactics:
    cover_usage: 0.3             # Rarely uses cover
    retreat_threshold: 0.15      # HP % before retreating
    flank_preference: 0.8        # Loves flanking
    grenade_frequency: 0.6
```

### 2.2 Behavior Tree Definitions
Define decision logic in data files.

```yaml
# behaviors/combat.btree
tree:
  root:
    type: selector
    children:
      # Emergency behaviors
      - type: sequence
        name: "Emergency Retreat"
        children:
          - type: condition
            check: "health < profile.retreat_threshold"
          - type: action
            action: "FindNearestCover"
          - type: action
            action: "RetreatToCover"

      # Combat behaviors
      - type: sequence
        name: "Engage Enemy"
        children:
          - type: condition
            check: "HasVisibleEnemy()"
          - type: selector
            children:
              # Too close, back up
              - type: sequence
                children:
                  - type: condition
                    check: "EnemyDistance() < weapon.minRange"
                  - type: action
                    action: "BackAwayFromEnemy"

              # Good range, attack
              - type: parallel
                policy: "RequireAll"
                children:
                  - type: action
                    action: "MaintainOptimalRange"
                  - type: action
                    action: "AimAtEnemy"
                  - type: action
                    action: "FireWeapon"

      # Patrol/search
      - type: action
        action: "PatrolArea"
```

### 2.3 Consideration Curves
Define utility scoring curves.

```yaml
# utility/aggressive_combat.yaml
considerations:
  - name: "HealthFactor"
    inputs: ["botHealth", "maxHealth"]
    curve:
      type: "linear"
      slope: 1.0
      exponent: 2.0  # Exponential curve
    weight: 0.3

  - name: "AmmoFactor"
    inputs: ["currentAmmo", "maxAmmo"]
    curve:
      type: "threshold"
      threshold: 0.3
      below_value: 0.0
      above_value: 1.0
    weight: 0.4

  - name: "EnemyProximity"
    inputs: ["enemyDistance"]
    curve:
      type: "inverse_linear"
      min_distance: 0
      max_distance: 1000
    weight: 0.3
```

**Benefits:**
- Non-programmers can create bot variations
- Quick iteration (no recompile)
- Version control for balance changes
- Easy A/B testing
- Hot-reload during development

## 3. Component-Based Entity System (ECS)

### 3.1 Why ECS?
Current monolithic `BotController` has 50+ member variables. ECS separates data from logic.

### 3.2 Components (Data)
```cpp
struct TransformComponent {
    Vector position;
    Vector velocity;
    Vector angles;
};

struct HealthComponent {
    float current;
    float max;
    float regenRate;
};

struct WeaponComponent {
    WeaponType currentWeapon;
    std::map<WeaponType, int> ammo;
    float accuracy;
    float recoilAccumulation;
};

struct CombatComponent {
    FireMode fireMode;
    CombatProfile profile;
    float burstTimer;
    float reloadTimer;
};

struct PerceptionComponent {
    std::vector<EntityID> visibleEnemies;
    std::vector<EntityID> knownEnemies;
    float visionRange;
    float hearingRange;
};

struct MemoryComponent {
    std::map<EntityID, EnemyMemory> enemyMemories;
    std::vector<AudioEvent> recentEvents;
};

struct BehaviorTreeComponent {
    std::unique_ptr<BehaviorTree> tree;
    Blackboard blackboard;
};
```

### 3.3 Systems (Logic)
```cpp
class PerceptionSystem : public System {
    void Update(Registry& registry, float dt) {
        auto view = registry.view<TransformComponent, PerceptionComponent>();
        for (auto [entity, transform, perception] : view) {
            UpdateVision(entity, transform, perception);
            UpdateHearing(entity, transform, perception);
        }
    }
};

class BehaviorTreeSystem : public System {
    void Update(Registry& registry, float dt) {
        auto view = registry.view<BehaviorTreeComponent, PerceptionComponent>();
        for (auto [entity, btree, perception] : view) {
            // Populate blackboard with perception data
            PopulateBlackboard(btree.blackboard, perception);

            // Execute behavior tree
            btree.tree->Execute(btree.blackboard, dt);
        }
    }
};

class CombatSystem : public System {
    void Update(Registry& registry, float dt) {
        auto view = registry.view<CombatComponent, WeaponComponent, PerceptionComponent>();
        for (auto [entity, combat, weapon, perception] : view) {
            UpdateFireMode(combat, weapon, perception);
            UpdateBurstTiming(combat, dt);
            ExecuteFiring(combat, weapon, perception);
        }
    }
};
```

### 3.4 Benefits
- **Performance:** Data-oriented = cache-friendly = faster
- **Flexibility:** Add/remove components = add/remove capabilities
- **Clarity:** Systems are stateless and single-purpose
- **Parallelization:** Systems can run concurrently
- **Serialization:** Easy to save/load component data

## 4. Testing Infrastructure

### 4.1 Unit Tests
Test individual algorithms.

```cpp
TEST(PathfindingTest, FindsOptimalPath) {
    NavMesh mesh = CreateSimpleMesh();
    PathRequest request{start: {0,0,0}, end: {100,0,0}};

    PathResult result = pathfinder.FindPath(mesh, request);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.waypoints.size(), 3);
    EXPECT_NEAR(result.length, 100.0f, 1.0f);
}

TEST(UtilityAITest, ScoresActionsCorrectly) {
    Context ctx = {health: 0.2f, ammo: 1.0f, visibleEnemies: 3};

    auto scores = evaluator.ScoreAllActions(ctx);

    // Low health + enemies = should prioritize cover
    EXPECT_GT(scores["TakeCover"], scores["Aggress"]);
}

TEST(BehaviorTreeTest, EmergencyOverridesCombat) {
    Blackboard bb;
    bb.Set("health", 0.1f);
    bb.Set("hasVisibleEnemy", true);

    NodeStatus status = tree.Execute(bb, 0.1f);

    EXPECT_TRUE(bb.Get<bool>("isRetreating"));  // Should retreat, not fight
}
```

### 4.2 Integration Tests
Test system interactions.

```cpp
TEST(CombatIntegrationTest, BotEngagesAndRetreatsCorrectly) {
    TestWorld world;
    BotEntity bot = world.SpawnBot({health: 1.0f});
    EnemyEntity enemy = world.SpawnEnemy({position: {100, 0, 0}});

    // Bot should engage
    world.Update(1.0f);
    EXPECT_TRUE(bot.IsAttacking());

    // Damage bot
    bot.TakeDamage(0.8f);
    world.Update(1.0f);

    // Bot should retreat
    EXPECT_TRUE(bot.IsRetreating());
    EXPECT_GT(bot.GetDistanceToEnemy(), 100.0f);
}
```

### 4.3 Scenario Tests
Test gameplay situations.

```cpp
TEST(ScenarioTest, BotCompletesObjectiveMission) {
    World world = LoadMap("test_objective");
    Bot bot = world.SpawnBot(world.GetSpawnPoint());
    Objective obj = world.GetObjective();

    // Simulate until bot reaches objective or timeout
    float elapsed = SimulateUntil([&]() {
        return bot.IsAt(obj.position);
    }, maxTime: 60.0f);

    ASSERT_LT(elapsed, 60.0f) << "Bot failed to reach objective";
    EXPECT_TRUE(obj.IsCompleted());
}

TEST(ScenarioTest, BotDealsWithAmbush) {
    World world = LoadMap("ambush_scenario");
    Bot bot = world.SpawnBot({100, 0, 0});

    // Wait for bot to enter kill zone
    world.UpdateUntil([&]() { return bot.position.x > 200; });

    // Spawn enemies behind bot
    world.SpawnEnemies({
        {{150, -50, 0}, {150, 50, 0}}  // Flanking positions
    });

    world.Update(5.0f);

    // Bot should have detected threat and taken action
    EXPECT_TRUE(bot.IsAlive());  // Survived
    EXPECT_GT(bot.GetKills(), 0);  // Fought back
}
```

## 5. Debug Visualization

### 5.1 In-Game Overlays
```cpp
class DebugRenderer {
    void DrawBehaviorTreeState(BotEntity bot) {
        // Shows behavior tree with active nodes highlighted
        // Green = success, Red = failure, Yellow = running
    }

    void DrawUtilityScores(BotEntity bot) {
        // Bar chart of action scores
        // Shows why bot chose current action
    }

    void DrawPerceptionData(BotEntity bot) {
        // Vision cone (FOV)
        // Audio radius
        // Known enemy positions (with confidence decay)
    }

    void DrawTacticalOverlay(BotEntity bot) {
        // Cover points (colored by quality)
        // Threat map (danger areas)
        // Flank routes
        // Squad coordination lines
    }
};
```

### 5.2 External Tools
- **Behavior Tree Editor:** Drag-drop nodes, live preview
- **Bot Profile Editor:** Sliders for personality traits, instant preview
- **Playback Recorder:** Record bot sessions, replay with scrubbing
- **Heatmap Generator:** Where bots go, where they die, combat density

## 6. Performance Optimizations

### 6.1 Level-of-Detail (LOD) AI
```cpp
enum class AILOD {
    HIGH,      // Full AI, 60 FPS updates
    MEDIUM,    // Simplified AI, 20 FPS updates
    LOW,       // Basic AI, 10 FPS updates
    SLEEPING   // Minimal AI, 1 FPS updates (just exists)
};

AILOD DetermineAILOD(BotEntity bot) {
    if (bot.IsInCombat()) return AILOD::HIGH;
    if (bot.IsNearPlayer(1000.0f)) return AILOD::MEDIUM;
    if (bot.IsVisibleToPlayer()) return AILOD::LOW;
    return AILOD::SLEEPING;
}
```

### 6.2 Spatial Indexing
```cpp
class BotSpatialIndex {
    // Grid-based spatial hash for fast neighbor queries
    std::vector<BotEntity> FindBotsInRadius(Vector pos, float radius);
    BotEntity FindNearestBot(Vector pos, TeamID team);

    // Update incrementally (bots register when they move)
};
```

### 6.3 Async Operations
```cpp
class AsyncPathfinder {
    std::future<PathResult> RequestPath(PathRequest req) {
        return std::async(std::launch::async, [=]() {
            return ComputePath(req);  // Runs on worker thread
        });
    }
};

// Bot continues current path while waiting for new one
```

## 7. Module System

### 7.1 Plugin Architecture
```cpp
class IBehaviorModule {
    virtual void Initialize(BotEntity bot) = 0;
    virtual void Update(float dt) = 0;
    virtual float EvaluateUtility(const Context& ctx) = 0;
    virtual void OnEvent(const AIEvent& event) = 0;
};

class VehicleBehaviorModule : public IBehaviorModule {
    // Handles vehicle entry, driving, turret control
};

class ObjectiveBehaviorModule : public IBehaviorModule {
    // CTF, bomb planting, zone capture logic
};

// Loaded dynamically based on config
```

### 7.2 Benefits
- Game modes add behaviors without modifying core
- Optional features don't bloat base system
- Third-party mods can extend AI
- Easy experimentation

## 8. Migration Path

See `MIGRATION_STRATEGY.md` for detailed plan on moving from current to ideal architecture.

**Key Principle:** Incremental, always-shippable changes. Build new alongside old, migrate piece by piece.

## Conclusion

This architecture represents the **ideal** - what we'd build with unlimited resources. In practice:

- Start with Phase 1 (foundation)
- Implement pieces incrementally
- Keep old system working
- Migrate behavior by behavior

Even partial implementation provides massive value. The key is **modularity** - each piece works independently and improves the whole.

---

**Next:** Read `MIGRATION_STRATEGY.md` to see how to get from current code to this vision.
