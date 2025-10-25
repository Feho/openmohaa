# Phase 3: Advanced Features - AI Capabilities & Migration

**Duration:** 1-2 months
**Risk:** MEDIUM
**Value:** VERY HIGH
**Goal:** Migrate all behaviors to new system, add advanced AI features

**Prerequisites:** Phase 2 complete, new systems proven

---

## Task 3.1: Migrate Attack Behavior to BT
**Epic:** Behavior Tree System
**Estimate:** 2 weeks
**Priority:** HIGH

### Subtasks

#### Week 1: Combat Tree Design & Implementation
- [ ] **3.1.1** Design combat behavior tree (4 hours)
  - Map current attack logic to tree structure
  - Identify reusable subtrees
  - Create YAML specification

- [ ] **3.1.2** Implement combat actions (12 hours)
  - Action: AimAtEnemy
  - Action: FireWeapon (burst/continuous logic)
  - Action: MaintainOptimalRange
  - Action: BackAwayFromEnemy
  - Action: MoveTowardEnemy
  - Each action ~2 hours

- [ ] **3.1.3** Implement combat conditions (4 hours)
  - Condition: HasVisibleEnemy
  - Condition: EnemyInRange
  - Condition: WeaponReady
  - Condition: HasAmmo

- [ ] **3.1.4** Create combat subtrees (6 hours)
  - Subtree: ManageDistance (too close? back up : too far? move closer)
  - Subtree: EngageFire (aim + fire with burst control)
  - Subtree: CheckWeapon (valid weapon, has ammo)

#### Week 2: Integration & Testing
- [ ] **3.1.5** Assemble complete combat tree (4 hours)
  ```yaml
  combat_tree:
    type: selector
    children:
      - type: sequence
        name: "Engage Enemy"
        children:
          - type: condition
            check: "HasVisibleEnemy()"
          - include: "check_weapon"
          - type: parallel
            policy: "RequireAll"
            children:
              - include: "manage_distance"
              - include: "engage_fire"
  ```

- [ ] **3.1.6** Test combat tree extensively (8 hours)
  - Unit tests for actions/conditions
  - Integration test: bot engages enemy
  - Integration test: bot manages range
  - Integration test: bot handles ammo depletion
  - Compare vs. old attack state

- [ ] **3.1.7** Tune parameters (6 hours)
  - Reaction time delays
  - Burst timing
  - Range thresholds
  - Fire discipline

- [ ] **3.1.8** Performance validation (2 hours)
  - Profile combat tree execution
  - Ensure < 0.5ms average
  - Test with 50+ bots in combat

**Files Created:**
- `behaviors/combat.btree`
- `behaviors/subtrees/manage_distance.btree`
- `behaviors/subtrees/engage_fire.btree`
- `code/fgame/bt_actions_combat.cpp`
- `tests/test_combat_behavior.cpp`

---

## Task 3.2: Migrate Investigation Behavior to BT
**Epic:** Behavior Tree System
**Estimate:** 1 week
**Priority:** MEDIUM

### Subtasks
- [ ] **3.2.1** Design investigation tree (2 hours)
- [ ] **3.2.2** Implement investigation actions (6 hours)
  - Action: MoveToLastKnownPosition
  - Action: SearchArea (spiral search pattern)
  - Action: LookAround
- [ ] **3.2.3** Implement investigation conditions (2 hours)
  - Condition: HasEnemyMemory
  - Condition: HighConfidenceMemory
  - Condition: SearchTimedOut
- [ ] **3.2.4** Create investigation tree YAML (2 hours)
- [ ] **3.2.5** Test investigation behavior (6 hours)
- [ ] **3.2.6** Compare vs. old investigate state (2 hours)

**Files Created:**
- `behaviors/investigation.btree`
- `code/fgame/bt_actions_investigation.cpp`
- `tests/test_investigation_behavior.cpp`

---

## Task 3.3: Migrate Curious/Idle Behaviors to BT
**Epic:** Behavior Tree System
**Estimate:** 1 week
**Priority:** MEDIUM

### Subtasks
- [ ] **3.3.1** Design curious/idle tree (3 hours)
- [ ] **3.3.2** Implement curious/idle actions (8 hours)
  - Action: InvestigateSound
  - Action: PatrolArea (already done in Phase 2)
  - Action: UseAttractiveNode
  - Action: RandomWander
- [ ] **3.3.3** Create idle tree YAML (2 hours)
- [ ] **3.3.4** Test idle behaviors (5 hours)
- [ ] **3.3.5** Compare vs. old states (2 hours)

**Files Created:**
- `behaviors/idle.btree`
- `code/fgame/bt_actions_idle.cpp`
- `tests/test_idle_behavior.cpp`

---

## Task 3.4: Implement Utility AI
**Epic:** Utility AI
**Estimate:** 2 weeks
**Priority:** MEDIUM

### Subtasks

#### Week 1: Core Utility System
- [ ] **3.4.1** Design utility evaluator (4 hours)
  - Identify actions to score
  - Design consideration curves
  - Define scoring algorithm

- [ ] **3.4.2** Implement `UtilityEvaluator` (8 hours)
  ```cpp
  class UtilityEvaluator {
  public:
      ScoredAction SelectBestAction(const Context& ctx);

  private:
      float ScoreAction(const std::string& action, const Context& ctx);
      std::vector<Consideration> considerations;
  };
  ```

- [ ] **3.4.3** Implement consideration curves (8 hours)
  - Linear curve
  - Exponential curve
  - Inverse linear curve
  - Threshold curve
  - Custom curves via lambdas

#### Week 2: Integration
- [ ] **3.4.4** Define actions to score (4 hours)
  - Aggress
  - Defend
  - Retreat
  - Investigate
  - Support
  - Flank

- [ ] **3.4.5** Implement scoring logic for each action (12 hours)
  - Health considerations
  - Ammo considerations
  - Enemy count/proximity
  - Ally positions
  - Cover availability

- [ ] **3.4.6** YAML configuration for curves (4 hours)
  ```yaml
  utility:
    aggress:
      considerations:
        - name: health_factor
          curve: linear
          weight: 0.4
        - name: ammo_factor
          curve: exponential
          exponent: 2.0
          weight: 0.3
  ```

- [ ] **3.4.7** Integrate with behavior trees (4 hours)
  - Utility AI selects high-level strategy
  - BT executes chosen strategy
  - Smooth transitions between strategies

- [ ] **3.4.8** Test utility system (6 hours)
  - Unit tests for scoring
  - Integration tests for action selection
  - Compare bot variety vs. old system

**Files Created:**
- `code/fgame/utility_evaluator.h`
- `code/fgame/utility_evaluator.cpp`
- `utility/considerations.yaml`
- `tests/test_utility_ai.cpp`

---

## Task 3.5: Advanced Debug Visualization
**Epic:** Debug Visualization
**Estimate:** 1.5 weeks
**Priority:** MEDIUM

### Subtasks

#### Week 1: Visual Tools
- [ ] **3.5.1** Enhanced BT visualization (8 hours)
  - Hierarchical tree layout
  - Node execution history
  - Timing information per node
  - Blackboard inspection

- [ ] **3.5.2** Utility score visualization (6 hours)
  - Bar chart of action scores
  - Highlight selected action
  - Show consideration breakdown
  - Update in real-time

- [ ] **3.5.3** Tactical overlay (6 hours)
  - Cover points (colored by quality)
  - Danger zones (heat map)
  - Squad lines (coordination indicators)
  - Objective markers

#### Week 2: Recording & Playback
- [ ] **3.5.4** Implement recording system (8 hours)
  - Record bot decisions over time
  - Record perception snapshots
  - Record actions taken
  - Serialize to file

- [ ] **3.5.5** Implement playback system (8 hours)
  - Load recorded session
  - Scrub through timeline
  - Step frame-by-frame
  - Compare multiple recordings

- [ ] **3.5.6** Console commands (2 hours)
  - `bot_record_start <num>`
  - `bot_record_stop`
  - `bot_playback <file>`
  - `bot_playback_step`

**Files Created:**
- `code/fgame/bot_debugger.h`
- `code/fgame/bot_debugger.cpp`
- `code/fgame/bot_recorder.h`
- `code/fgame/bot_recorder.cpp`

---

## Task 3.6: Remove Old State Machine
**Epic:** Code Quality
**Estimate:** 1 week
**Priority:** LOW

### Subtasks
- [ ] **3.6.1** Verify all behaviors migrated (2 hours)
  - Attack → combat.btree
  - Investigate → investigation.btree
  - Curious → idle.btree (sound investigation)
  - Idle → idle.btree
  - Weapon → integrated into combat tree

- [ ] **3.6.2** Remove old state functions (4 hours)
  - Remove `State_Attack()`
  - Remove `State_Investigate()`
  - Remove `State_Curious()`
  - Remove `State_Idle()`
  - Remove `CheckCondition_*()` functions

- [ ] **3.6.3** Remove botfuncs array (1 hour)
  - Remove `botfunc_t botfuncs[MAX_BOT_FUNCTIONS]`
  - Remove state initialization functions

- [ ] **3.6.4** Clean up state management (4 hours)
  - Remove old state flags
  - Remove state entry time tracking (if BT handles it)
  - Simplify `Think()` method

- [ ] **3.6.5** Update documentation (2 hours)
  - Update CLAUDE.md
  - Remove references to old state machine
  - Document new BT-based system

- [ ] **3.6.6** Thorough testing (6 hours)
  - All behaviors still work
  - No regressions
  - Performance unchanged or better

**Files Changed:**
- `playerbot.h` (remove old state declarations)
- `playerbot_core.cpp` (remove old state logic)
- `playerbot_state.cpp` (DELETE or heavily refactor)
- `CLAUDE.md`

---

## Task 3.7: Performance Tuning
**Epic:** Performance Optimization
**Estimate:** 1 week
**Priority:** MEDIUM

### Subtasks
- [ ] **3.7.1** Profile AI systems (6 hours)
  - Perception update time
  - BT execution time
  - Utility evaluation time
  - Identify bottlenecks

- [ ] **3.7.2** Optimize perception (6 hours)
  - Cache enemy queries
  - Spatial indexing for nearby entities
  - Reduce occlusion test frequency

- [ ] **3.7.3** Optimize BT execution (6 hours)
  - Cache node results
  - Early-out optimizations
  - Reduce allocations

- [ ] **3.7.4** Optimize utility evaluation (4 hours)
  - Pre-compute common values
  - Cache consideration results
  - Reduce branching

- [ ] **3.7.5** Measure improvements (4 hours)
  - Before/after benchmarks
  - Document optimizations
  - Set performance budgets

**Deliverable:** AI Think() time < 1ms average

---

## Phase 3 Deliverables

### Migrated Behaviors
- [ ] All behaviors in behavior trees
- [ ] Old state machine removed
- [ ] Zero functional regressions

### Advanced AI
- [ ] Utility AI for dynamic decision making
- [ ] Measurably more varied bot behaviors
- [ ] Emergent tactics observed

### Tools
- [ ] Full BT visualization
- [ ] Utility score visualization
- [ ] Recording/playback system
- [ ] Tactical overlay

### Performance
- [ ] AI Think() < 1ms average
- [ ] Supports 100+ bots at 60 FPS
- [ ] Profiled and optimized

## Success Criteria
- [ ] AI quality measurably better (player feedback, metrics)
- [ ] Behavior diversity evident (different playstyles)
- [ ] Performance meets targets
- [ ] Old code removed (cleaner codebase)
- [ ] Debug tools dramatically speed up AI development

## Risks
- **AI Quality:** New system might not match old
  - Mitigation: Extensive testing, A/B comparison, tuning time

- **Performance:** New systems slower than expected
  - Mitigation: Profile early, optimize hot paths, LOD (Phase 4)

- **Complexity:** Too many moving parts
  - Mitigation: Thorough documentation, good debug tools

---

## Total Phase 3 Estimate: 9-10 weeks
## Recommended Team Size: 2-3 developers
## Prerequisites: Phase 2 complete, systems proven
