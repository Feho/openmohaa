# Phase 4: Polish & Optimization - Production Ready

**Duration:** Ongoing (2-3 months for core features)
**Risk:** LOW
**Value:** MEDIUM-HIGH
**Goal:** Performance optimization, plugin system, full polish

**Prerequisites:** Phase 3 complete

---

## Task 4.1: LOD AI System
**Epic:** Performance Optimization
**Estimate:** 2 weeks
**Priority:** HIGH

### Subtasks

#### Week 1: LOD Framework
- [ ] **4.1.1** Design LOD system (4 hours)
  - Define LOD levels (HIGH, MEDIUM, LOW, SLEEPING)
  - Define criteria for each level
  - Design update scheduling

- [ ] **4.1.2** Implement `AIScheduler` (8 hours)
  ```cpp
  class AIScheduler {
  public:
      void RegisterBot(BotController* bot);
      void UnregisterBot(BotController* bot);
      void Update(float dt);

  private:
      AILOD DetermineLOD(BotController* bot);
      std::vector<BotController*> bots;
      int currentFrame = 0;
  };
  ```

- [ ] **4.1.3** Implement LOD determination (6 hours)
  - Check if in combat (HIGH)
  - Check distance to players (MEDIUM < 1000, LOW < 2000, SLEEPING > 2000)
  - Check visibility to players
  - Check if near objectives

- [ ] **4.1.4** Implement staggered updates (6 hours)
  - HIGH: Every frame
  - MEDIUM: Every 3 frames (20 Hz)
  - LOW: Every 6 frames (10 Hz)
  - SLEEPING: Every 60 frames (1 Hz)

#### Week 2: Integration & Tuning
- [ ] **4.1.5** Integrate with BotController (4 hours)
  - `BotController::SetLOD(AILOD level)`
  - Scale delta time based on LOD
  - Skip expensive operations at low LOD

- [ ] **4.1.6** Tune LOD thresholds (8 hours)
  - Test different distance thresholds
  - Test different update rates
  - Balance quality vs. performance

- [ ] **4.1.7** Test with 100+ bots (6 hours)
  - Measure FPS with LOD enabled/disabled
  - Verify bots still play well at low LOD
  - Tune transition smoothness

- [ ] **4.1.8** Debug visualization (2 hours)
  - Show LOD level above each bot
  - Show update frequency
  - Console command to force LOD level

**Deliverable:** 100+ bots at 60 FPS

---

## Task 4.2: Spatial Indexing
**Epic:** Performance Optimization
**Estimate:** 1 week
**Priority:** MEDIUM

### Subtasks
- [ ] **4.2.1** Implement `BotSpatialIndex` (8 hours)
  - Grid-based spatial hash
  - Insert/Remove operations
  - Range queries
  - Nearest neighbor queries

- [ ] **4.2.2** Integrate with BotManager (4 hours)
  - Bots register on spawn
  - Bots update on move
  - Bots unregister on death

- [ ] **4.2.3** Replace linear searches (4 hours)
  - FindBotsInRadius() uses spatial index
  - FindNearestBot() uses spatial index
  - Enemy detection uses spatial index

- [ ] **4.2.4** Performance testing (4 hours)
  - Benchmark before/after
  - Test with 200+ bots
  - Verify correctness

**Deliverable:** O(1) neighbor queries instead of O(n)

---

## Task 4.3: Async Pathfinding
**Epic:** Performance Optimization
**Estimate:** 2 weeks
**Priority:** MEDIUM

### Subtasks

#### Week 1: Async Infrastructure
- [ ] **4.3.1** Design async pathfinding (4 hours)
  - Thread pool for path computation
  - Request/result queue
  - Continuation callbacks

- [ ] **4.3.2** Implement `AsyncPathfinder` (12 hours)
  ```cpp
  class AsyncPathfinder {
  public:
      std::future<PathResult> RequestPath(PathRequest req);
      bool IsReady(std::future<PathResult>& future);
      PathResult GetResult(std::future<PathResult>& future);
  };
  ```

- [ ] **4.3.3** Create thread pool (6 hours)
  - Worker threads
  - Task queue
  - Thread-safe operations

- [ ] **4.3.4** Test async pathfinder (6 hours)
  - Unit tests
  - Load tests (many simultaneous requests)
  - Stress tests

#### Week 2: Integration
- [ ] **4.3.5** Integrate with BotMovement (8 hours)
  - Request path asynchronously
  - Continue current path while waiting
  - Apply new path when ready

- [ ] **4.3.6** Handle edge cases (6 hours)
  - Path request times out
  - Path invalidated before ready
  - Bot killed while path pending

- [ ] **4.3.7** Performance testing (4 hours)
  - Measure frame time improvement
  - Verify no hitches
  - Test with 100+ bots

- [ ] **4.3.8** Tune thread count (2 hours)
  - Test 1, 2, 4, 8 threads
  - Find optimal for typical CPU counts

**Deliverable:** Zero frame hitches from pathfinding

---

## Task 4.4: Plugin System
**Epic:** Modularity & Plugins
**Estimate:** 3 weeks
**Priority:** LOW

### Subtasks

#### Week 1: Plugin Interface
- [ ] **4.4.1** Design plugin system (6 hours)
  - Define `IBehaviorPlugin` interface
  - Define plugin lifecycle
  - Define registration API

- [ ] **4.4.2** Implement `PluginManager` (8 hours)
  ```cpp
  class PluginManager {
  public:
      void LoadPlugin(const char* path);
      void UnloadPlugin(const char* name);
      void RegisterPlugin(IBehaviorPlugin* plugin);
      void InitializePlugins();
  };
  ```

- [ ] **4.4.3** Dynamic library loading (8 hours)
  - Windows: LoadLibrary/GetProcAddress
  - Linux: dlopen/dlsym
  - Cross-platform wrapper

#### Week 2: Core Plugins
- [ ] **4.4.4** Extract core combat as plugin (8 hours)
  - Move combat actions to plugin
  - Test loading as plugin
  - Verify functionality unchanged

- [ ] **4.4.5** Create example plugin (8 hours)
  - VehicleBehaviorPlugin (if vehicles exist)
  - Or ObjectiveGameModePlugin
  - Document plugin creation process

- [ ] **4.4.6** Plugin configuration (4 hours)
  ```yaml
  plugins:
    - core/combat
    - core/movement
    - gamemodes/objective
    - optional/taunts
  ```

#### Week 3: Integration & Documentation
- [ ] **4.4.7** Hot-reload plugins (6 hours)
  - Unload/reload without restart
  - Handle state migration
  - Safety checks

- [ ] **4.4.8** Plugin documentation (6 hours)
  - How to create a plugin
  - API reference
  - Example plugin with comments

- [ ] **4.4.9** Test plugin system (6 hours)
  - Load/unload multiple times
  - Test with multiple plugins
  - Verify no memory leaks

**Deliverable:** Extensible plugin system with examples

---

## Task 4.5: Component-Based Architecture (ECS)
**Epic:** Component-Based Architecture
**Estimate:** 4 weeks (OPTIONAL - big refactor)
**Priority:** LOW

### Subtasks

#### Week 1: ECS Library Integration
- [ ] **4.5.1** Evaluate ECS libraries (4 hours)
  - EnTT
  - EntityX
  - Custom lightweight ECS

- [ ] **4.5.2** Integrate chosen library (4 hours)
  - Add to build system
  - Basic example
  - Performance testing

- [ ] **4.5.3** Design component breakdown (8 hours)
  - List all BotController data
  - Group into logical components
  - Design system responsibilities

#### Week 2-3: Refactor to Components
- [ ] **4.5.4** Create components (16 hours)
  - TransformComponent
  - HealthComponent
  - WeaponComponent
  - CombatComponent
  - PerceptionComponent
  - MemoryComponent
  - BehaviorTreeComponent
  - SquadComponent

- [ ] **4.5.5** Create systems (16 hours)
  - PerceptionSystem (refactor existing)
  - BehaviorTreeSystem
  - CombatSystem
  - MovementSystem
  - SquadSystem

#### Week 4: Migration & Testing
- [ ] **4.5.6** Migrate BotController to ECS (12 hours)
  - Create entity per bot
  - Attach components
  - Systems update components

- [ ] **4.5.7** Test extensively (8 hours)
  - All behaviors still work
  - Performance equal or better
  - Memory usage acceptable

**Note:** This is a major refactor. Consider deferring or skipping if timeline is tight.

---

## Task 4.6: Full Test Coverage
**Epic:** Testing Infrastructure
**Estimate:** 2 weeks
**Priority:** MEDIUM

### Subtasks
- [ ] **4.6.1** Identify untested code (4 hours)
  - Run coverage tool
  - List functions with < 50% coverage
  - Prioritize by criticality

- [ ] **4.6.2** Write missing unit tests (20 hours)
  - Target: 80%+ coverage
  - Focus on core AI algorithms
  - ~40 new tests

- [ ] **4.6.3** Write missing integration tests (12 hours)
  - Complex behavior scenarios
  - Edge cases
  - ~15 new tests

- [ ] **4.6.4** Set up coverage CI (4 hours)
  - Generate coverage reports
  - Fail if coverage drops
  - Show trend over time

**Deliverable:** 80%+ code coverage

---

## Task 4.7: Documentation
**Epic:** Code Quality
**Estimate:** 1.5 weeks
**Priority:** MEDIUM

### Subtasks

#### Week 1: Code Documentation
- [ ] **4.7.1** Doxygen setup (2 hours)
  - Configure Doxygen
  - Generate initial docs
  - Set up auto-generation

- [ ] **4.7.2** Document all public APIs (12 hours)
  - BotController
  - PerceptionSystem
  - BehaviorTree
  - UtilityEvaluator
  - All other public classes

- [ ] **4.7.3** Architecture diagrams (6 hours)
  - System overview diagram
  - Data flow diagram
  - Class hierarchy diagram

#### Week 2: User Documentation
- [ ] **4.7.4** Designer guide (8 hours)
  - How to create bot profiles
  - How to create behavior trees
  - How to tune parameters

- [ ] **4.7.5** Developer guide (6 hours)
  - Architecture overview
  - How to add new actions
  - How to add new systems

- [ ] **4.7.6** Troubleshooting guide (2 hours)
  - Common issues
  - Debug techniques
  - Performance tips

**Deliverable:** Comprehensive documentation

---

## Task 4.8: Profile & Behavior Library
**Epic:** Data-Driven Configuration
**Estimate:** 1 week
**Priority:** LOW

### Subtasks
- [ ] **4.8.1** Create 10+ bot profiles (8 hours)
  - Aggressive, Defensive, Balanced (done in Phase 2)
  - Sniper, Rusher (done in Phase 2)
  - Flanker, Support, Defender, Patrol, Sentry

- [ ] **4.8.2** Create behavior tree library (8 hours)
  - Combat variants (defensive, aggressive, suppressive)
  - Movement patterns (patrol, guard, roam)
  - Squad behaviors (formation, coordinated assault)

- [ ] **4.8.3** Document profiles & behaviors (4 hours)
  - When to use each profile
  - What each behavior does
  - How to customize

**Deliverable:** Rich library for designers to use

---

## Phase 4 Deliverables

### Performance
- [ ] LOD system: 100+ bots at 60 FPS
- [ ] Spatial indexing: O(1) queries
- [ ] Async pathfinding: Zero hitches
- [ ] Total AI budget: < 5ms for 100 bots

### Extensibility
- [ ] Plugin system functional
- [ ] Example plugins
- [ ] Plugin documentation

### Quality
- [ ] 80%+ test coverage
- [ ] Full API documentation
- [ ] User guides

### Library
- [ ] 10+ bot profiles
- [ ] Rich behavior tree library
- [ ] Well-documented

## Success Criteria
- [ ] 100+ bots at 60 FPS on mid-range hardware
- [ ] Plugin system enables easy extensions
- [ ] New developers productive in < 1 day (good docs)
- [ ] Zero P0/P1 bugs

## Risks
- **Scope Creep:** Too many polish items
  - Mitigation: Prioritize ruthlessly, ship incrementally

- **Diminishing Returns:** Optimizations take longer than expected
  - Mitigation: Set time budgets, accept "good enough"

---

## Total Phase 4 Estimate: 12-16 weeks (depending on optional tasks)
## Recommended Team Size: 2-3 developers
## Prerequisites: Phase 3 complete

---

## Post-Phase 4: Maintenance Mode

After Phase 4, the system is production-ready. Future work:
- Bug fixes
- New behaviors as needed
- Community plugin support
- Performance tuning for new maps
- Balance adjustments

The architecture is now clean, tested, performant, and extensible. Enjoy!
