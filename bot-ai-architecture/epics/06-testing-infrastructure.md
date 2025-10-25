# Epic 6: Testing Infrastructure

## Overview
Implement comprehensive automated testing at unit, integration, and scenario levels to catch regressions and enable confident refactoring.

## Business Value
- **Quality:** Catch bugs before they reach players
- **Confidence:** Refactor without fear of breaking things
- **Documentation:** Tests show how code is meant to be used
- **Speed:** Automated tests faster than manual testing

## Current State
Manual testing only - play game, observe bots, hope nothing broke.

**Problems:**
- Regressions slip through
- Testing is time-consuming
- Edge cases rarely tested
- No performance benchmarks

## Target State
```cpp
// Unit tests
TEST(PathfindingTest, FindsOptimalPath) {
    NavMesh mesh = CreateTestMesh();
    PathResult result = FindPath(mesh, start, end);
    EXPECT_EQ(result.waypoints.size(), 3);
}

// Integration tests
TEST(CombatTest, BotRetreatsWhenLowHealth) {
    Bot bot = CreateBot({health: 0.2f});
    bot.Think(1.0f);
    EXPECT_TRUE(bot.IsRetreating());
}

// Scenario tests
TEST(ScenarioTest, BotCompletesObjective) {
    World world = LoadMap("test_map");
    Bot bot = world.SpawnBot();
    SimulateUntil([&]() { return bot.ReachedObjective(); }, 60.0f);
    EXPECT_TRUE(bot.CompletedObjective());
}
```

## Acceptance Criteria
- [ ] Test framework integrated (GoogleTest)
- [ ] 50+ unit tests for core algorithms
- [ ] 20+ integration tests for AI behaviors
- [ ] 5+ scenario tests for gameplay situations
- [ ] CI runs tests on every commit
- [ ] Test coverage > 80%
- [ ] Performance benchmarks for critical paths

## Technical Components

### Test Levels
1. **Unit Tests:** Individual functions (pathfinding, aiming, scoring)
2. **Integration Tests:** System interactions (perception → decision → action)
3. **Scenario Tests:** Gameplay situations (ambush, objective, combat)
4. **Performance Tests:** Benchmark AI performance

### Test Utilities
```cpp
class BotTestHarness {
    void SetHealth(float h);
    void SetEnemy(MockEnemy enemy);
    void InjectPerception(PerceptionSnapshot snap);
    DecisionHistory GetDecisions();
    void FastForward(float seconds);
};

class MockWorld {
    Bot SpawnBot(Vector pos);
    Enemy SpawnEnemy(Vector pos);
    void Update(float dt);
    void SimulateUntil(std::function<bool()> condition, float maxTime);
};
```

## Dependencies
- GoogleTest framework
- CI system (GitHub Actions, etc.)
- Test maps/scenarios

## Related Epics
- All epics benefit from testing

## References
- [GoogleTest Documentation](https://google.github.io/googletest/)
- [Test-Driven Development](https://martinfowler.com/bliki/TestDrivenDevelopment.html)
