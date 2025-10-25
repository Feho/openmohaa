# Phase 2: Core Systems - New Architecture Implementation

**Duration:** 1-2 months
**Risk:** MEDIUM
**Value:** HIGH
**Goal:** Implement new architecture alongside old (no breaking changes)

**Prerequisites:** Phase 1 complete

---

## Task 2.1: Perception System Implementation
**Epic:** Perception System (`epics/02-perception-system.md`)
**Estimate:** 2 weeks
**Priority:** HIGH

### Subtasks

#### Week 1: Core Structure
- [ ] **2.1.1** Create `PerceptionSystem` class (4 hours)
  - Define interface
  - Create `PerceptionSnapshot` struct
  - Basic Update() method

- [ ] **2.1.2** Implement `VisionSensor` (8 hours)
  - Extract existing `CanSee()` logic
  - Add FOV calculation
  - Add occlusion testing
  - Add distance attenuation
  - Add peripheral vision detection

- [ ] **2.1.3** Create `EnemyInfo` struct (2 hours)
  ```cpp
  struct EnemyInfo {
      Sentient* entity;
      Vector position;
      Vector velocity;
      float distance;
      float visibilityFactor;
      float angleFromForward;
      bool isInPeripheral;
  };
  ```

- [ ] **2.1.4** Write vision tests (4 hours)
  - Test FOV detection
  - Test occlusion
  - Test distance limits
  - Test peripheral vision

#### Week 2: Audio & Memory
- [ ] **2.1.5** Implement `AudioSensor` (6 hours)
  - Refactor `NoticeEvent()` into sensor
  - 3D positional audio calculations
  - Priority filtering
  - Direction estimation

- [ ] **2.1.6** Implement `MemorySystem` (8 hours)
  - Store enemy positions
  - Confidence decay over time
  - Predicted positions (velocity extrapolation)
  - Clean up old memories

- [ ] **2.1.7** Integration with `BotController` (4 hours)
  - Add `PerceptionSystem` member
  - Update `Think()` to use perception
  - Populate blackboard/state from snapshot
  - Feature flag: `g_bot_use_perception_system`

- [ ] **2.1.8** Write perception tests (6 hours)
  - Test audio direction estimation
  - Test memory decay
  - Test predicted positions
  - Integration test with BotController

**Files Created:**
- `code/fgame/perception.h`
- `code/fgame/perception.cpp`
- `tests/test_perception.cpp`

---

## Task 2.2: Data-Driven Configuration
**Epic:** Data-Driven Configuration (`epics/04-data-driven-configuration.md`)
**Estimate:** 1.5 weeks
**Priority:** HIGH

### Subtasks

#### Week 1: YAML Integration
- [ ] **2.2.1** Integrate yaml-cpp library (4 hours)
  - Add to build system
  - Test basic parsing
  - Error handling

- [ ] **2.2.2** Create `BotProfile` class (8 hours)
  ```cpp
  class BotProfile {
  public:
      static BotProfile LoadFromFile(const char* path);

      // Personality
      float GetAggression() const;
      float GetCaution() const;
      float GetTeamwork() const;
      float GetCreativity() const;

      // Combat
      float GetPreferredRange() const;
      float GetFireDiscipline() const;
      std::pair<float, float> GetBurstLength() const;

      // More accessors...
  };
  ```

- [ ] **2.2.3** Implement YAML parsing (8 hours)
  - Parse personality section
  - Parse combat section
  - Parse movement section
  - Parse aim section
  - Parse tactics section
  - Validation with error messages

- [ ] **2.2.4** Create 5 default profiles (4 hours)
  - `profiles/aggressive.yaml`
  - `profiles/balanced.yaml`
  - `profiles/defensive.yaml`
  - `profiles/sniper.yaml`
  - `profiles/rusher.yaml`

#### Week 2: Integration
- [ ] **2.2.5** Integrate profiles with `BotController` (6 hours)
  - Load profile in constructor
  - Replace cvar usage with profile values
  - Test each profile in-game

- [ ] **2.2.6** Hot-reload support (4 hours)
  - File watcher (or manual reload command)
  - `bot_reload_profiles` command
  - Validate before applying

- [ ] **2.2.7** Runtime profile switching (3 hours)
  - `bot_setprofile <botnum> <profile>`
  - `bot_listprofiles` command

- [ ] **2.2.8** Write profile tests (3 hours)
  - Test loading valid profiles
  - Test error handling (invalid files)
  - Test profile switching

**Files Created:**
- `code/fgame/bot_profile.h`
- `code/fgame/bot_profile.cpp`
- `profiles/*.yaml` (5 files)
- `tests/test_bot_profile.cpp`

---

## Task 2.3: Basic Behavior Tree Framework
**Epic:** Behavior Tree System (`epics/01-behavior-tree-system.md`)
**Estimate:** 2 weeks
**Priority:** HIGH

### Subtasks

#### Week 1: Core Nodes
- [ ] **2.3.1** Create `BTNode` base class (2 hours)
  ```cpp
  class BTNode {
  public:
      enum Status { SUCCESS, FAILURE, RUNNING };
      virtual Status Execute(Blackboard& bb, float dt) = 0;
      virtual void Reset() = 0;
  };
  ```

- [ ] **2.3.2** Implement `BTSelector` (3 hours)
  - Tries children left-to-right
  - Returns first SUCCESS/RUNNING
  - Unit tests

- [ ] **2.3.3** Implement `BTSequence` (3 hours)
  - Executes children left-to-right
  - Returns first FAILURE/RUNNING
  - Unit tests

- [ ] **2.3.4** Implement `BTParallel` (4 hours)
  - Executes all children
  - Configurable success policy
  - Unit tests

- [ ] **2.3.5** Implement `BTCondition` (3 hours)
  - Lambda-based conditions
  - Common conditions library
  - Unit tests

- [ ] **2.3.6** Implement `BTAction` (3 hours)
  - Lambda-based actions
  - Support RUNNING status (multi-frame)
  - Unit tests

- [ ] **2.3.7** Implement `Blackboard` (4 hours)
  - Type-safe key-value store
  - std::any or variant-based
  - Get/Set/Has methods
  - Unit tests

#### Week 2: Integration & First Behavior
- [ ] **2.3.8** Create `BehaviorTreeBuilder` (4 hours)
  - Fluent interface for tree construction
  - Validation
  - Examples

- [ ] **2.3.9** Implement simple patrol tree (6 hours)
  ```cpp
  auto patrolTree = BehaviorTreeBuilder()
      .Sequence()
          .Condition([](auto& bb) { return !bb.Has("destination"); })
          .Action([](auto& bb, float dt) {
              bb.Set("destination", FindRandomPoint());
              return BTNode::SUCCESS;
          })
      .End()
      .Action([](auto& bb, float dt) {
          MoveTo(bb.Get<Vector>("destination"));
          if (MoveDone()) {
              bb.Remove("destination");
              return BTNode::SUCCESS;
          }
          return BTNode::RUNNING;
      })
      .Build();
  ```

- [ ] **2.3.10** Integrate with `BotController` (4 hours)
  - Add `BehaviorTree` member
  - Feature flag: `g_bot_use_behaviortree`
  - Execute tree in `Think()`
  - Fallback to old state machine if disabled

- [ ] **2.3.11** YAML tree loading (basic) (6 hours)
  - Parse simple tree structure
  - Map action/condition names to C++ functions
  - Load patrol tree from YAML

- [ ] **2.3.12** Write BT tests (4 hours)
  - Test each node type
  - Test tree execution
  - Test blackboard operations
  - Integration test with BotController

**Files Created:**
- `code/fgame/behavior_tree.h`
- `code/fgame/behavior_tree.cpp`
- `code/fgame/behavior_tree_builder.h`
- `code/fgame/behavior_tree_builder.cpp`
- `behaviors/patrol.btree` (YAML)
- `tests/test_behavior_tree.cpp`

---

## Task 2.4: Integration Testing
**Epic:** Testing (`epics/06-testing-infrastructure.md`)
**Estimate:** 3 days
**Priority:** MEDIUM

### Subtasks
- [ ] **2.4.1** Create integration test framework (4 hours)
  - `BotTestHarness` class
  - `MockWorld` class
  - Time fast-forwarding

- [ ] **2.4.2** Write perception integration tests (4 hours)
  - Bot detects visible enemies
  - Bot remembers lost enemies
  - Bot investigates sounds

- [ ] **2.4.3** Write profile integration tests (2 hours)
  - Different profiles behave differently
  - Profile switching works correctly

- [ ] **2.4.4** Write behavior tree integration tests (4 hours)
  - Patrol tree executes correctly
  - Tree switches based on conditions
  - Blackboard state persists

**Files Created:**
- `tests/bot_test_harness.h`
- `tests/bot_test_harness.cpp`
- `tests/integration_test_perception.cpp`
- `tests/integration_test_profiles.cpp`
- `tests/integration_test_behaviortree.cpp`

---

## Task 2.5: Debug Visualization Improvements
**Epic:** Debug Visualization (`epics/07-debug-visualization.md`)
**Estimate:** 1 week
**Priority:** MEDIUM

### Subtasks
- [ ] **2.5.1** Behavior Tree visualizer (8 hours)
  - Draw tree structure
  - Highlight active nodes
  - Color-code node status
  - Show blackboard values

- [ ] **2.5.2** Perception visualizer (6 hours)
  - Draw FOV cone
  - Draw visible enemies (green)
  - Draw remembered enemies (orange, fading)
  - Draw audio events (blue)

- [ ] **2.5.3** Profile info display (2 hours)
  - Show current profile name
  - Show key personality traits
  - Toggle via console command

- [ ] **2.5.4** Console commands (4 hours)
  - `bot_debug <num> <mode>` (perception, behavior, all)
  - `bot_step <num>` (step through BT execution)
  - `bot_blackboard <num>` (print blackboard contents)

**Files Changed:**
- `playerbot_util.cpp` (debug rendering)
- `g_cmds.cpp` (console commands)

---

## Phase 2 Deliverables

### New Systems
- [ ] PerceptionSystem with vision, audio, memory
- [ ] BotProfile system with 5 default profiles
- [ ] Behavior Tree framework with core nodes
- [ ] Simple patrol behavior working via BT

### Configuration
- [ ] Profiles loaded from YAML
- [ ] Hot-reload support
- [ ] Runtime profile switching

### Testing
- [ ] 30+ unit tests
- [ ] 10+ integration tests
- [ ] Test harness for future testing

### Debugging
- [ ] BT visualization
- [ ] Perception visualization
- [ ] Enhanced console commands

### Feature Flags
- [ ] `g_bot_use_perception_system`
- [ ] `g_bot_use_behaviortree`
- [ ] Old system still works when flags disabled

## Success Criteria
- [ ] New systems functional and tested
- [ ] Zero regressions when features disabled
- [ ] At least one behavior (patrol) works better with new system
- [ ] Performance acceptable (< 10% overhead)

## Risks
- **Complexity:** Three major systems at once
  - Mitigation: Implement sequentially, not parallel

- **Performance:** New systems add overhead
  - Mitigation: Profile early, optimize hot paths

- **Integration Issues:** New systems conflict
  - Mitigation: Feature flags, gradual integration

---

## Total Phase 2 Estimate: 6-8 weeks
## Recommended Team Size: 2-3 developers
## Prerequisites: Phase 1 complete
