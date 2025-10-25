# Phase 1: Foundation - Quick Wins & Groundwork

**Duration:** 1-2 weeks
**Risk:** LOW
**Value:** MEDIUM
**Goal:** Improve code quality, add basic testing, prepare for bigger changes

---

## Task 1.1: Extract Long Functions
**Epic:** Code Quality (`epics/10-code-quality-refactoring.md`)
**Estimate:** 3 days
**Priority:** HIGH

### Subtasks
- [ ] **1.1.1** Extract `State_Attack()` into smaller functions (2 hours)
  - ValidateAttackPreconditions()
  - SelectBestTarget()
  - AimAtTarget()
  - UpdateAttackMovement()
  - ExecuteFiring()

- [ ] **1.1.2** Extract `State_Idle()` into smaller functions (1 hour)
  - CheckRevengeTarget()
  - SelectWanderDestination()
  - ExecutePatrol()

- [ ] **1.1.3** Extract `State_Curious()` into smaller functions (1 hour)
  - ValidateCuriousEvent()
  - MoveToEventLocation()

- [ ] **1.1.4** Extract `State_Investigate()` into smaller functions (1.5 hours)
  - CalculateSearchPosition()
  - ExecuteSearchPattern()
  - EvaluateSearchComplete()

- [ ] **1.1.5** Extract pathfinding logic from `BotMovement` (2 hours)
  - ValidatePath()
  - HandleBlocking()
  - RecoverFromStuck()

- [ ] **1.1.6** Verify all extracted functions have tests (2 hours)

**Files Changed:**
- `playerbot_attack.cpp`
- `playerbot_idle.cpp`
- `playerbot_curious.cpp`
- `playerbot_investigate.cpp`
- `playerbot_movement.cpp`

---

## Task 1.2: Create Data Structures
**Epic:** Code Quality (`epics/10-code-quality-refactoring.md`)
**Estimate:** 2 days
**Priority:** HIGH

### Subtasks
- [ ] **1.2.1** Create `CombatState` struct (30 min)
  ```cpp
  struct CombatState {
      float recentDamage;
      int damageWindowStart;
      float burstDuration;
      float burstDelay;
      bool requireLowSpread;
      bool ammoLow;
  };
  ```

- [ ] **1.2.2** Create `CoverState` struct (30 min)
  ```cpp
  struct CoverState {
      CoverPoint current;
      CoverStateEnum state;
      int nextPeekTime;
      int peekStartTime;
      float peekDuration;
  };
  ```

- [ ] **1.2.3** Create `MemoryState` struct (1 hour)
  ```cpp
  struct MemoryState {
      EnemyMemory enemyMemory;
      int investigateStartTime;
      int investigateEventTime;
      Vector investigateEventPos;
      int currentEventPriority;
  };
  ```

- [ ] **1.2.4** Create `SquadState` struct (1 hour)
  ```cpp
  struct SquadState {
      SquadInfo squad;
      SquadRole role;
      int lastSquadUpdateTime;
      int roleAssignmentTime;
      Vector flankPosition;
      bool flankPositionValid;
  };
  ```

- [ ] **1.2.5** Refactor `BotController` to use new structs (4 hours)
  - Replace individual members with struct members
  - Update all access points
  - Verify compilation
  - Test in-game

**Files Changed:**
- `playerbot.h`
- `playerbot_core.cpp`
- All `playerbot_*.cpp` files (update member access)

---

## Task 1.3: Set Up Test Infrastructure
**Epic:** Testing (`epics/06-testing-infrastructure.md`)
**Estimate:** 2 days
**Priority:** HIGH

### Subtasks
- [ ] **1.3.1** Integrate GoogleTest framework (3 hours)
  - Add GoogleTest to build system
  - Create `tests/` directory
  - Set up CMake/build configuration
  - Verify test runner works

- [ ] **1.3.2** Create test utilities (2 hours)
  - MockSentient class
  - MockPlayer class
  - TestNavMesh helpers
  - Common test fixtures

- [ ] **1.3.3** Write first 10 unit tests (4 hours)
  - BotMovement::CalculateDir()
  - BotMovement::CanMoveTo()
  - BotController::IsValidEnemy()
  - BotController::SelectBestTarget() (mocked)
  - BotRotation::TurnThink() (angle math)
  - UtilityEvaluator basic scoring (if exists)
  - 4 more tests for critical functions

- [ ] **1.3.4** Set up CI to run tests (2 hours)
  - Add test step to CI pipeline
  - Fail build if tests fail
  - Show test results in CI output

**Files Created:**
- `tests/CMakeLists.txt`
- `tests/test_bot_movement.cpp`
- `tests/test_bot_controller.cpp`
- `tests/test_bot_rotation.cpp`
- `tests/test_utilities.h`

---

## Task 1.4: Improve Debug Visualization
**Epic:** Debug Visualization (`epics/07-debug-visualization.md`)
**Estimate:** 2 days
**Priority:** MEDIUM

### Subtasks
- [ ] **1.4.1** Add console commands (2 hours)
  ```cpp
  void Cmd_BotDebugInfo(int clientNum);     // Print current state
  void Cmd_BotForceState(int clientNum, int state);  // Force state
  void Cmd_BotShowPerception(int clientNum);  // Toggle FOV/enemies
  ```

- [ ] **1.4.2** Improve path visualization (2 hours)
  - Draw waypoints with numbers
  - Draw current goal in different color
  - Draw blocking detection visuals
  - Draw jump detection visuals

- [ ] **1.4.3** Add enemy visualization (2 hours)
  - Draw lines to visible enemies (green)
  - Draw lines to remembered enemies (orange, fading)
  - Draw target lock indicator
  - Draw aim target crosshair

- [ ] **1.4.4** Add state info overlay (2 hours)
  - Show current active states
  - Show state timers
  - Show enemy info (distance, last seen time)
  - Show weapon info (ammo, spread)

**Files Changed:**
- `playerbot_util.cpp` (add debug commands)
- `playerbot_core.cpp` (improve debug drawing)
- `g_cmds.cpp` (register commands)

---

## Task 1.5: Code Cleanup & Style
**Epic:** Code Quality (`epics/10-code-quality-refactoring.md`)
**Estimate:** 2 days
**Priority:** MEDIUM

### Subtasks
- [ ] **1.5.1** Set up clang-format (1 hour)
  - Create `.clang-format` config
  - Format all bot files
  - Add format check to CI

- [ ] **1.5.2** Fix compiler warnings (3 hours)
  - Enable stricter warning levels
  - Fix all warnings in bot code
  - Add `-Werror` for bot files

- [ ] **1.5.3** Add const-correctness (3 hours)
  - Mark getters const
  - Mark parameters const where applicable
  - Use const references for large parameters

- [ ] **1.5.4** Replace magic numbers with named constants (2 hours)
  ```cpp
  constexpr float DEFAULT_VISION_RANGE = 2048.0f;
  constexpr float DEFAULT_FOV = 80.0f;
  constexpr float MELEE_RANGE = 64.0f;
  constexpr int CURIOUS_DURATION_MS = 20000;
  ```

**Files Changed:**
- `.clang-format` (new)
- `playerbot.h` (add constants)
- All `playerbot_*.cpp` files (formatting, const, warnings)

---

## Task 1.6: Documentation
**Epic:** Code Quality (`epics/10-code-quality-refactoring.md`)
**Estimate:** 1 day
**Priority:** LOW

### Subtasks
- [ ] **1.6.1** Update CLAUDE.md with recent changes (1 hour)

- [ ] **1.6.2** Add function documentation (2 hours)
  - Document all public BotController methods
  - Document BotMovement public API
  - Document BotRotation public API

- [ ] **1.6.3** Add TODO comments for known issues (1 hour)
  - Mark grenade state as TODO
  - Mark navigation system as TODO
  - Mark weapon state as TODO

**Files Changed:**
- `CLAUDE.md`
- `playerbot.h` (add doxygen comments)
- `playerbot_movement.cpp` (add comments)

---

## Phase 1 Deliverables

### Code Quality Improvements
- [ ] All functions < 100 lines (target: < 50)
- [ ] Related data grouped into structs
- [ ] Zero compiler warnings
- [ ] Consistent code style (clang-format)
- [ ] Named constants replace magic numbers

### Testing
- [ ] GoogleTest integrated
- [ ] 10-15 unit tests passing
- [ ] CI runs tests automatically
- [ ] Test utilities for future tests

### Debugging
- [ ] Console commands for common debug tasks
- [ ] Improved visualization (paths, enemies, states)
- [ ] Better debug output

### Documentation
- [ ] Updated architecture docs
- [ ] Function documentation
- [ ] TODOs marked

## Success Criteria
- [ ] No functional regressions (AI behaves identically)
- [ ] Code is easier to read and understand
- [ ] Foundation ready for Phase 2
- [ ] Team confident in making changes

## Risks
- **Time estimate too optimistic:** Refactoring always finds surprises
  - Mitigation: Budget extra 2-3 days for unexpected issues

- **Breaking AI behavior:** Refactoring changes behavior
  - Mitigation: Test extensively, compare before/after videos

---

## Total Phase 1 Estimate: 12 days
## Recommended Team Size: 1-2 developers
## Prerequisites: None, can start immediately
