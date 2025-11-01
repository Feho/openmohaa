# Phase 2: Core Systems - Implementation Plan

**Status:** Planning Complete, Ready to Execute
**Duration:** 5-6 weeks (optimized from original 6-8 weeks)
**Approach:** Vertical Slice (build → integrate → test incrementally)
**Risk Level:** MEDIUM → LOW (reduced via phased approach)

---

## Strategic Approach (Based on Gemini Consultation)

### Key Changes from Original Plan

**✅ Vertical Slice Strategy**
- Build systems to 100%, integrate immediately, test continuously
- NOT building all systems to 80% then integrating (avoids "integration hell")

**✅ Split into Two Sub-Phases**
- **Phase 2A:** Senses & Memory (Perception + Basic Config) - 1.5-2 weeks
- **Phase 2B:** Brain & Behavior (Behavior Trees + Full Config) - 3-4 weeks

**✅ Single Master Feature Flag**
- Use `g_bot_use_new_ai_system` instead of multiple independent flags
- Prevents combinatorial explosion of test states
- Either full legacy or full new system (no mix-and-match initially)

**✅ Testing & Debug as You Go**
- Integration tests written during development (not at end)
- Debug visualization added alongside features
- Tasks 2.4 and 2.5 are ongoing activities, not separate phases

**✅ Old System Untouched**
- Leave legacy state machine completely alone
- Feature flag is the safety net
- Deprecation warnings come later after new system proven

---

## Task 2.0: Foundation - Integrate yaml-cpp (2-3 days)

**Priority:** CRITICAL - Must complete before Phase 2A starts

### Why First?
- Foundational project-level change
- Prevents build issues from derailing feature work
- Needed for both Profiles (2.2) and Behavior Trees (2.3)

### Subtasks

#### 2.0.1: Research & Plan (2 hours)
- [ ] Examine existing library integrations in `cmake/libraries/`
- [ ] Study `sdl.cmake` and `openal.cmake` as reference
- [ ] Determine yaml-cpp version (recommend: 0.7.0+)
- [ ] Check if system yaml-cpp available or need embedded

#### 2.0.2: CMake Integration (4 hours)
- [ ] Create `cmake/libraries/yaml-cpp.cmake`
- [ ] Add FetchContent or find_package logic
- [ ] Set include directories and link targets
- [ ] Add to main CMakeLists.txt

#### 2.0.3: Test Integration (2 hours)
- [ ] Create simple test program in `tests/test_yaml_cpp.cpp`
- [ ] Parse a basic YAML file
- [ ] Verify it builds and links correctly
- [ ] Add to CTest suite

#### 2.0.4: Documentation (1 hour)
- [ ] Update CLAUDE.md with yaml-cpp dependency
- [ ] Document build requirements

**Files Created:**
- `cmake/libraries/yaml-cpp.cmake`
- `tests/test_yaml_cpp.cpp`

**Success Criteria:**
- [ ] Project builds with yaml-cpp linked
- [ ] Simple YAML parsing test passes
- [ ] No breaking changes to existing build

---

## Phase 2A: Senses & Memory (2-2.5 weeks)

**Goal:** Bot can perceive the world and remember what it sees/hears

**Updated Duration:** Extended from 1.5-2 weeks to include richer perception data structures from Epic 02.

### Task 2A.1: Perception System Core (Week 1)

#### 2A.1.1: Core Structure (4 hours)
- [ ] Create `code/fgame/perception.h` and `perception.cpp`
- [ ] Define `PerceptionSystem` class interface
- [ ] Create `PerceptionSnapshot` struct (holds perception data)
- [ ] Implement basic `Update()` method stub

```cpp
class PerceptionSystem {
public:
    PerceptionSnapshot Update(Player* bot, float deltaTime);

private:
    VisionSensor visionSensor;
    AudioSensor audioSensor;
    MemorySystem memory;
};
```

#### 2A.1.2: Vision Sensor (8 hours)
- [ ] Extract existing `CanSee()` logic from bot code
- [ ] Implement FOV calculation (cone check)
- [ ] Add occlusion testing (line-of-sight)
- [ ] Add distance attenuation
- [ ] Add peripheral vision detection
- [ ] **Write unit tests** (5 tests)

#### 2A.1.3: Perception Data Structures (4 hours)
- [ ] Define `EnemyInfo` struct in perception.h
  - Include: entity, position, velocity, distance, visibility factor, angle, isInPeripheral
- [ ] Define `AllyInfo` struct for nearby allies
  - Include: entity, position, distance, canSeeMe
- [ ] Define `ThreatLevel` enum (NONE, LOW, MEDIUM, HIGH)
- [ ] Add helper methods to EnemyInfo (IsVisible, IsInPeripheral)
- [ ] **Write unit tests** (2 tests)

```cpp
struct EnemyInfo {
    Sentient* entity;
    Vector position;
    Vector velocity;
    float distance;
    float visibilityFactor;  // 0.0 (barely visible) - 1.0 (clear view)
    float angleFromForward;  // Degrees off center
    bool isInPeripheral;

    bool IsVisible() const { return visibilityFactor > 0.1f; }
    bool IsInPeripheral() const { return isInPeripheral; }
};

struct AllyInfo {
    Player* entity;
    Vector position;
    float distance;
    bool canSeeMe;
};

enum ThreatLevel {
    THREAT_NONE,
    THREAT_LOW,     // 1 enemy, distant
    THREAT_MEDIUM,  // 1-2 enemies, medium range
    THREAT_HIGH     // 3+ enemies or very close
};
```

#### 2A.1.4: Audio Sensor (6 hours)
- [ ] Refactor `NoticeEvent()` into `AudioSensor`
- [ ] Implement 3D positional audio calculations
- [ ] Add priority filtering (important vs. background sounds)
- [ ] Add direction estimation
- [ ] Define `AudioEvent` struct (type, position, direction, loudness, priority, timestamp, confidence)
- [ ] **Write unit tests** (3 tests)

```cpp
struct AudioEvent {
    AIEventType type;            // WEAPON_FIRE, FOOTSTEP, etc.
    Vector position;
    Vector estimatedDirection;   // Where bot thinks it came from
    float loudness;              // 0.0 - 1.0
    float priority;              // 0.0 - 1.0
    float timestamp;
    float confidence;            // How sure about direction
};
```

#### 2A.1.5: Memory System (8 hours)
- [ ] Implement `MemorySystem` class
- [ ] Store enemy last-known positions with timestamp
- [ ] Implement confidence decay over time
- [ ] Add velocity extrapolation for predicted positions
- [ ] Add cleanup for old memories (> 30 seconds)
- [ ] Define `EnemyMemory` struct
- [ ] **Write unit tests** (4 tests)

```cpp
struct EnemyMemory {
    Sentient* enemy;
    Vector lastKnownPosition;
    Vector lastKnownVelocity;
    Vector predictedPosition;      // Based on velocity + time
    float lastSeenTime;
    float confidenceLevel;         // Decays over time (1.0 to 0.0)
    int timesSpotted;
    bool investigationStarted;
};
```

#### 2A.1.6: Enhanced PerceptionSnapshot (4 hours)
- [ ] Add convenience pointers to PerceptionSnapshot
  - `EnemyInfo* closestEnemy`
  - `EnemyInfo* mostDangerousEnemy`
  - `AudioEvent* loudestSound`
- [ ] Implement helper methods
  - `HasVisibleEnemy()`, `HasKnownEnemy()`, `GetEnemyCount()`
- [ ] Implement threat level calculation based on enemy count/distance
- [ ] **Write unit tests** (3 tests)

```cpp
struct PerceptionSnapshot {
    // Enemies
    std::vector<EnemyInfo> visibleEnemies;
    std::vector<EnemyMemory> knownEnemies;
    EnemyInfo* closestEnemy = nullptr;
    EnemyInfo* mostDangerousEnemy = nullptr;  // Based on distance, weapon, health

    // Allies
    std::vector<AllyInfo> nearbyAllies;

    // Audio
    std::vector<AudioEvent> recentSounds;
    AudioEvent* loudestSound = nullptr;

    // Threat Assessment
    ThreatLevel threatLevel = THREAT_NONE;

    // Helper methods
    bool HasVisibleEnemy() const { return !visibleEnemies.empty(); }
    bool HasKnownEnemy() const { return !knownEnemies.empty(); }
    int GetEnemyCount() const { return visibleEnemies.size(); }
    int GetTotalKnownEnemies() const { return visibleEnemies.size() + knownEnemies.size(); }
};
```

#### 2A.1.7: Ally Detection (3 hours)
- [ ] Implement ally detection in VisionSensor
- [ ] Populate `nearbyAllies` vector
- [ ] Calculate if ally can see bot (`canSeeMe` field)
- [ ] Filter by distance (e.g., within 1024 units)
- [ ] **Write unit tests** (2 tests)

**Files Created:**
- `code/fgame/perception.h`
- `code/fgame/perception.cpp`
- `tests/test_perception.cpp`

**Tests Written:** 19 unit tests (was 12, added 7 for new features)

---

### Task 2A.2: Basic Bot Profiles (Week 1-2)

#### 2A.2.1: Profile Structure (4 hours)
- [ ] Create `code/fgame/bot_profile.h` and `bot_profile.cpp`
- [ ] Define `BotProfile` class
- [ ] Implement perception-related getters (vision range, FOV, audio sensitivity)
- [ ] Add validation methods

#### 2A.2.2: YAML Parsing - Perception Only (4 hours)
- [ ] Implement `LoadFromFile()` static method
- [ ] Parse `perception:` section from YAML
  - `vision_range`
  - `field_of_view`
  - `peripheral_fov`
  - `audio_sensitivity`
- [ ] Add error handling and validation

#### 2A.2.3: Create Test Profiles (2 hours)
- [ ] Create `profiles/test_perception.yaml` with perception settings
- [ ] Create `profiles/sniper.yaml` (high vision, narrow FOV)
- [ ] Create `profiles/rusher.yaml` (low vision, wide FOV)

#### 2A.2.4: Profile Tests (2 hours)
- [ ] **Write unit tests** for profile loading (3 tests)
- [ ] Test error handling (invalid files)
- [ ] Test value validation

**Files Created:**
- `code/fgame/bot_profile.h`
- `code/fgame/bot_profile.cpp`
- `profiles/test_perception.yaml`
- `profiles/sniper.yaml`
- `profiles/rusher.yaml`
- `tests/test_bot_profile.cpp`

**Tests Written:** 3 unit tests

---

### Task 2A.3: Integration & Testing (Week 2)

#### 2A.3.1: Integrate with BotController (6 hours)
- [ ] Add `PerceptionSystem* perception` member to `BotController`
- [ ] Add `BotProfile* profile` member to `BotController`
- [ ] Load profile in constructor (from bot name or random)
- [ ] Call `perception->Update()` in `Think()` method
- [ ] Add feature flag: `g_bot_use_new_ai_system` (default: 0)
- [ ] When flag=0, use old perception code (unchanged)
- [ ] When flag=1, use new `PerceptionSystem`

#### 2A.3.2: Debug Visualization (4 hours)
- [ ] Add perception visualization to `DrawDebugVisualization()`
- [ ] Draw FOV cone (green for visible, gray for peripheral)
- [ ] Draw visible enemies (green lines)
- [ ] Draw remembered enemy positions (orange markers fading over time)
- [ ] Draw audio events (blue pulses)
- [ ] Show current profile name above bot

#### 2A.3.3: Integration Tests (4 hours)
- [ ] Create `tests/bot_test_harness.h` (simple mock world)
- [ ] Write integration test: bot detects visible enemy
- [ ] Write integration test: bot remembers lost enemy
- [ ] Write integration test: bot hears sound event
- [ ] Write integration test: different profiles behave differently

**Files Created:**
- `tests/bot_test_harness.h`
- `tests/integration_test_perception.cpp`

**Tests Written:** 4 integration tests

---

### Phase 2A Deliverables

**Systems:**
- ✅ PerceptionSystem with VisionSensor, AudioSensor, MemorySystem
- ✅ Rich perception data structures:
  - EnemyInfo with visibility factors and peripheral vision
  - AllyInfo for nearby allies detection
  - AudioEvent for 3D positional sound
  - EnemyMemory with confidence decay and prediction
  - ThreatLevel assessment
- ✅ Enhanced PerceptionSnapshot with:
  - Convenience pointers (closestEnemy, mostDangerousEnemy, loudestSound)
  - Helper methods (HasVisibleEnemy, GetEnemyCount, etc.)
- ✅ BotProfile system with YAML parsing (perception parameters only)
- ✅ 3 test profiles (sniper, rusher, test)

**Testing:**
- ✅ 26 total tests (19 perception unit + 3 profile unit + 4 integration)
- ✅ Test harness for future integration tests

**Integration:**
- ✅ Perception integrated with BotController
- ✅ Feature flag functional (old system still works)
- ✅ Debug visualization for perception (FOV, allies, threat level)

**Success Criteria:**
- [ ] All 26 tests pass
- [ ] Bot can switch between old and new perception via cvar
- [ ] No regressions when flag disabled
- [ ] Visible improvement in bot awareness when flag enabled
- [ ] Threat assessment shows correct levels based on enemy count/distance
- [ ] Ally detection functional for squad coordination

---

## Phase 2B: Brain & Behavior (3-4 weeks)

**Goal:** Bot makes decisions using Behavior Trees driven by perception data

### Task 2B.1: Behavior Tree Framework (Week 1-2)

#### 2B.1.1: Core Node Types (Day 1-2: 12 hours)
- [ ] Create `code/fgame/behavior_tree.h` and `behavior_tree.cpp`
- [ ] Implement `BTNode` base class with Status enum
- [ ] Implement `BTSelector` (tries children until SUCCESS)
- [ ] Implement `BTSequence` (executes children until FAILURE)
- [ ] Implement `BTParallel` (executes all children)
- [ ] Implement `BTCondition` (lambda-based condition checks)
- [ ] Implement `BTAction` (lambda-based actions, supports RUNNING)
- [ ] **Write unit tests for each node type** (7 tests)

#### 2B.1.2: Blackboard System (Day 3: 4 hours)
- [ ] Implement `Blackboard` class (type-safe key-value store)
- [ ] Use `std::any` or `std::variant` for values
- [ ] Add Get/Set/Has/Remove methods
- [ ] Add type safety with templates
- [ ] **Write unit tests** (3 tests)

#### 2B.1.3: Tree Builder (Day 4: 4 hours)
- [ ] Create `behavior_tree_builder.h`
- [ ] Implement `BehaviorTreeBuilder` with fluent interface
- [ ] Add validation (detect malformed trees)
- [ ] Add examples in comments

#### 2B.1.4: Simple Test Behavior (Day 5: 6 hours)
- [ ] Create `EngageEnemyTree` in code (not YAML yet)
- [ ] Tree structure:
  ```
  Selector
    Sequence (Has Enemy)
      Condition: Enemy visible in perception
      Action: Aim at enemy
      Action: Shoot if in range
    Action: Idle (no enemy)
  ```
- [ ] Integrate with perception data
- [ ] **Write integration test** (1 test)

**Files Created:**
- `code/fgame/behavior_tree.h`
- `code/fgame/behavior_tree.cpp`
- `code/fgame/behavior_tree_builder.h`
- `tests/test_behavior_tree.cpp`

**Tests Written:** 11 unit tests + 1 integration test

---

### Task 2B.2: YAML Tree Loading (Week 2-3: 8 hours)

#### 2B.2.1: Tree Format Design (2 hours)
- [ ] Design YAML structure for behavior trees
- [ ] Document node types and parameters
- [ ] Create example: `behaviors/engage_enemy.yaml`

#### 2B.2.2: YAML Parser (6 hours)
- [ ] Implement tree loading from YAML
- [ ] Map action/condition names to C++ lambdas (registry pattern)
- [ ] Add validation and error reporting
- [ ] Load the `EngageEnemyTree` from YAML

**Files Created:**
- `behaviors/engage_enemy.yaml`
- `behaviors/patrol.yaml`

---

### Task 2B.3: Complete Profile System (Week 3: 8 hours)

#### 2B.3.1: Extend Profile Parameters (4 hours)
- [ ] Add `combat:` section (preferred_range, fire_discipline, burst_length)
- [ ] Add `movement:` section (aggression, caution)
- [ ] Add `personality:` section (teamwork, creativity)
- [ ] Add `behavior_tree:` reference (which tree to use)

#### 2B.3.2: Create Full Profiles (4 hours)
- [ ] Create `profiles/aggressive.yaml` (close range, high aggression)
- [ ] Create `profiles/balanced.yaml` (medium all-around)
- [ ] Create `profiles/defensive.yaml` (cautious, cover-focused)
- [ ] Update `profiles/sniper.yaml` (long range, patient)
- [ ] Update `profiles/rusher.yaml` (fast, aggressive)

**Files Created:**
- `profiles/aggressive.yaml`
- `profiles/balanced.yaml`
- `profiles/defensive.yaml`
- Updated: `profiles/sniper.yaml`, `profiles/rusher.yaml`

---

### Task 2B.4: Integration & Polish (Week 4)

#### 2B.4.1: Full Integration (6 hours)
- [ ] Add `BehaviorTree* behaviorTree` to `BotController`
- [ ] Load tree from profile in constructor
- [ ] Execute tree in `Think()` when `g_bot_use_new_ai_system=1`
- [ ] Populate blackboard with perception data each frame
- [ ] Fallback to old state machine when flag=0

#### 2B.4.2: Console Commands (4 hours)
- [ ] `bot_setprofile <botIndex> <profileName>` - switch profiles
- [ ] `bot_listprofiles` - list available profiles
- [ ] `bot_blackboard <botIndex>` - print blackboard contents
- [ ] `bot_reload_profiles` - hot-reload all profiles

#### 2B.4.3: Behavior Tree Visualizer (8 hours)
- [ ] Draw tree structure above bot
- [ ] Highlight active nodes (green)
- [ ] Show running nodes (yellow)
- [ ] Show failed nodes (red)
- [ ] Color-code by status
- [ ] Show key blackboard values

#### 2B.4.4: Integration Tests (6 hours)
- [ ] Write test: aggressive profile attacks more
- [ ] Write test: defensive profile uses cover
- [ ] Write test: tree switches based on perception
- [ ] Write test: blackboard state persists across frames
- [ ] Write test: profile hot-reload works

**Files Modified:**
- `code/fgame/playerbot.h` (add BT member)
- `code/fgame/playerbot.cpp` (integrate BT)
- `code/fgame/playerbot_util.cpp` (debug viz)
- `code/fgame/gamecmds.cpp` (console commands)

**Tests Written:** 5 integration tests

---

### Phase 2B Deliverables

**Systems:**
- ✅ Behavior Tree framework (7 node types, Blackboard, Builder)
- ✅ YAML tree loading
- ✅ 2 working behaviors (engage enemy, patrol)
- ✅ Complete profile system (5 profiles)

**Testing:**
- ✅ 17 new tests (11 BT unit + 1 BT integration + 5 full integration)
- ✅ Total Phase 2 tests: 36 (19 from 2A + 17 from 2B)

**Integration:**
- ✅ BT integrated with BotController
- ✅ BT uses perception data via blackboard
- ✅ Profiles control both perception and behavior
- ✅ Feature flag enables/disables entire new system

**Debug Tools:**
- ✅ BT visualizer with status colors
- ✅ 4 new console commands (setprofile, listprofiles, blackboard, reload_profiles)

**Success Criteria:**
- [ ] All 36 tests pass
- [ ] Bots with different profiles behave noticeably differently
- [ ] No regressions when flag disabled
- [ ] Performance acceptable (< 10% overhead measured with profiling)

---

## Phase 2 Final Checklist

### Code Quality
- [ ] All code follows OpenMoHAA standards
- [ ] Zero compiler warnings (strict checking enabled)
- [ ] Const-correctness enforced
- [ ] Formatted with clang-format
- [ ] All public APIs documented with Doxygen

### Testing
- [ ] 36+ tests passing (19 from 2A + 17 from 2B)
- [ ] Test harness created for future tests
- [ ] Integration tests validate end-to-end functionality
- [ ] Performance profiled (no significant regressions)

### Integration
- [ ] `g_bot_use_new_ai_system` cvar functional
- [ ] Old system works when flag=0 (zero regressions)
- [ ] New system works when flag=1 (perception + BT)
- [ ] Smooth transition between modes

### Documentation
- [ ] CLAUDE.md updated with new systems
- [ ] Profile YAML format documented
- [ ] Behavior tree YAML format documented
- [ ] Console commands documented
- [ ] Architecture diagrams updated

### Deliverables
- [ ] PerceptionSystem functional and tested
- [ ] BotProfile system with 5 profiles
- [ ] Behavior Tree framework working
- [ ] At least 2 behaviors (engage enemy, patrol)
- [ ] Debug visualizations for perception and BT
- [ ] 36+ tests passing

---

## Risk Management

### Risk 1: yaml-cpp Integration Issues
**Mitigation:** Task 2.0 completes this first, before feature work starts
**Fallback:** Use JSON (simpler parser) if yaml-cpp proves problematic

### Risk 2: Performance Overhead
**Mitigation:** Profile continuously during development, optimize hot paths
**Target:** < 10% CPU overhead vs. old system
**Fallback:** Add performance tuning phase if needed

### Risk 3: Scope Creep
**Mitigation:** Strict adherence to vertical slice approach - ship 2A before starting 2B
**Focus:** Deliver working, tested, integrated functionality at each step

### Risk 4: Integration Complexity
**Mitigation:** Feature flag ensures old system always works
**Strategy:** Build one system to 100%, integrate, test, then move to next
**Fallback:** Can disable new system anytime via cvar

---

## Timeline

**Total Duration:** 5.5-7 weeks (updated with enhanced perception features)

| Phase | Duration | Tests | Details |
|-------|----------|-------|---------|
| **Task 2.0: yaml-cpp** | 2-3 days | 1 test | Foundation - integrate library |
| **Phase 2A: Senses** | 2-2.5 weeks | 26 tests | Perception + Allies + Threat + Basic Profiles |
| **Phase 2B: Brain** | 3-4 weeks | 17 tests | Behavior Trees + Full Profiles |
| **TOTAL** | **5.5-7 weeks** | **44 tests** | Complete new AI system |

**What Changed:**
- Phase 2A extended by ~1 day to include:
  - AllyInfo struct and detection
  - ThreatLevel assessment
  - Enhanced PerceptionSnapshot with convenience pointers
  - Additional helper methods
  - 7 more unit tests (26 total vs 19 originally)

**Milestones:**
- End of Week 1: Perception system core functional (vision, audio, memory)
- End of Week 2: Allies and threat assessment working
- End of Week 2.5: Phase 2A complete (perception fully integrated)
- End of Week 4.5: BT framework complete
- End of Week 6: Phase 2B complete (full integration)
- Week 7: Polish, performance tuning, documentation

---

## Next Steps

1. **Review this plan** with stakeholders
2. **Commit to starting** Task 2.0 (yaml-cpp integration)
3. **Set up tracking** for Phase 2A subtasks
4. **Begin implementation** following vertical slice approach

---

## Notes

- This plan is based on Gemini AI consultation (2025-10-29)
- Uses vertical slice approach for lower risk
- Testing and debug tools are ongoing activities (not separate phases)
- Feature flag provides safety net for all changes
- Old system remains completely untouched
