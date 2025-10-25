# User Stories: Testing Infrastructure

## Epic: Testing Infrastructure
Related to: `epics/06-testing-infrastructure.md`

---

## Story 1: Unit Test Framework
**As a** developer
**I want** a unit testing framework integrated into the build
**So that** I can test individual algorithms automatically

### Acceptance Criteria
- [ ] GoogleTest or similar framework integrated
- [ ] Tests run as part of build process
- [ ] Clear test output (pass/fail counts)
- [ ] CI runs tests on every commit

### Priority: HIGH
### Estimate: 2 days

---

## Story 2: Algorithm Unit Tests
**As a** developer
**I want** tests for core algorithms (pathfinding, aiming, utility scoring)
**So that** refactoring doesn't break functionality

### Acceptance Criteria
- [ ] Pathfinding tests (finds optimal path, handles obstacles)
- [ ] Aiming tests (calculates correct angles, handles limits)
- [ ] Utility scoring tests (scores actions correctly)
- [ ] Memory system tests (stores, decays, predicts)
- [ ] 50+ unit tests total

### Priority: HIGH
### Estimate: 5 days

---

## Story 3: Integration Test Harness
**As a** developer
**I want** to test AI behavior without launching full game
**So that** testing is fast and repeatable

### Acceptance Criteria
- [ ] BotTestHarness can inject perception data
- [ ] Can set bot state (health, ammo, position)
- [ ] Can fast-forward time
- [ ] Can query bot decisions and actions

### Priority: HIGH
### Estimate: 4 days

### Example
```cpp
TEST(CombatIntegrationTest, BotRetreatsWhenLowHealth) {
    BotTestHarness bot;
    bot.SetHealth(0.2f);
    bot.InjectEnemy(distance: 100.0f);

    bot.Think(1.0f);

    EXPECT_TRUE(bot.IsRetreating());
}
```

---

## Story 4: Behavior Integration Tests
**As a** developer
**I want** tests for complete behaviors (combat, retreat, search)
**So that** I know behaviors work end-to-end

### Acceptance Criteria
- [ ] Combat behavior test (engages enemy correctly)
- [ ] Retreat behavior test (finds cover when low health)
- [ ] Search behavior test (investigates last known position)
- [ ] Cover behavior test (uses cover effectively)
- [ ] 20+ integration tests total

### Priority: HIGH
### Estimate: 6 days

---

## Story 5: Scenario Tests
**As a** developer
**I want** tests that simulate gameplay scenarios
**So that** I verify bots behave correctly in realistic situations

### Acceptance Criteria
- [ ] Objective completion test (bot reaches objective)
- [ ] Ambush scenario test (bot reacts to surprise attack)
- [ ] Multiple enemy test (bot handles 3+ enemies)
- [ ] Complex navigation test (bot navigates multi-level map)
- [ ] 5+ scenario tests

### Priority: MEDIUM
### Estimate: 5 days

---

## Story 6: Performance Benchmarks
**As a** developer
**I want** automated performance benchmarks
**So that** I catch performance regressions

### Acceptance Criteria
- [ ] Benchmark BehaviorTree::Execute()
- [ ] Benchmark PerceptionSystem::Update()
- [ ] Benchmark pathfinding
- [ ] Benchmark with 10, 50, 100 bots
- [ ] Fail build if performance degrades > 20%

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 7: Mock System
**As a** developer
**I want** to mock game systems (entities, events, world)
**So that** tests don't depend on full game engine

### Acceptance Criteria
- [ ] MockWorld (spawn entities, simulate time)
- [ ] MockEntity (configurable properties)
- [ ] MockPerception (inject what bot "sees")
- [ ] MockNavigation (deterministic paths)

### Priority: HIGH
### Estimate: 4 days

---

## Story 8: Regression Test Suite
**As a** developer
**I want** tests for every bug fix
**So that** bugs don't resurface

### Acceptance Criteria
- [ ] When bug fixed, add test reproducing bug
- [ ] Test fails before fix, passes after
- [ ] Document bug ID in test name
- [ ] Build regression test library over time

### Priority: MEDIUM
### Estimate: Ongoing (15 min per bug)

---

## Story 9: Code Coverage Reporting
**As a** developer
**I want** to see which code is tested
**So that** I can identify gaps in test coverage

### Acceptance Criteria
- [ ] Coverage tool integrated (lcov, gcov)
- [ ] HTML coverage reports generated
- [ ] Target: >80% coverage for core AI code
- [ ] CI shows coverage trends

### Priority: LOW
### Estimate: 2 days

---

## Story 10: Test Documentation
**As a** new developer
**I want** clear documentation on running and writing tests
**So that** I can contribute tests confidently

### Acceptance Criteria
- [ ] How to run tests (build commands)
- [ ] How to write new tests (examples)
- [ ] Test organization guidelines
- [ ] Best practices (what to test, how to mock)

### Priority: MEDIUM
### Estimate: 1 day

---

## Total Stories: 10
## Total Estimated Time: 32 days
