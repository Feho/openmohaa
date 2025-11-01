# Task 3.7: Performance Tuning

**Status:** Ready to Execute  
**Duration:** 1 week  
**Priority:** MEDIUM  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Profiles and optimizes the AI systems to achieve the performance target of <1ms per bot per frame, ensuring the game can handle 50+ bots at 60 FPS without performance degradation.

### Why This Matters
- **Scalability:** Support more bots simultaneously
- **Frame Rate:** Maintain 60 FPS target
- **Player Experience:** Smooth gameplay without hitches
- **Future-Proofing:** Leave performance headroom for Phase 4 features

### What's Already Complete
- ✅ Complete AI system (perception, BT, utility, behaviors)
- ✅ All behaviors functional
- ✅ Debug visualization tools
- ✅ Comprehensive test suite

**Current Performance:** Unknown (needs profiling)
**Target Performance:** <1ms per bot per frame average

---

## Performance Budget

### Per-Bot Budget (1ms total @ 60 FPS)
```
Perception System:    0.30ms (30%)
Behavior Tree:        0.40ms (40%)
Utility AI:           0.15ms (15%)
Pathfinding:          0.10ms (10%)
Other (misc):         0.05ms (5%)
TOTAL:                1.00ms (100%)
```

### Acceptable Ranges
- **Excellent:** <0.5ms per bot
- **Good:** 0.5-1.0ms per bot
- **Acceptable:** 1.0-2.0ms per bot
- **Poor:** >2.0ms per bot (needs optimization)

---

## Profiling Tools & Setup

### Tools to Use

#### 1. Built-in Profiling
```cpp
// code/fgame/bot_profiler.h

class BotProfiler {
public:
    struct ProfileData {
        float perceptionTime;
        float behaviorTreeTime;
        float utilityAITime;
        float pathfindingTime;
        float totalTime;
        int sampleCount;
    };
    
    void StartFrame();
    void EndFrame();
    
    void StartSection(const char* name);
    void EndSection(const char* name);
    
    ProfileData GetAverageData(int numFrames = 60);
    void PrintReport();
};

// Usage:
void BotController::Think(float dt) {
    botProfiler.StartFrame();
    
    botProfiler.StartSection("perception");
    auto perception = perceptionSystem.Update(bot, dt);
    botProfiler.EndSection("perception");
    
    botProfiler.StartSection("utility");
    auto action = utilityEvaluator.SelectBestAction(perception, bot, profile);
    botProfiler.EndSection("utility");
    
    botProfiler.StartSection("behavior_tree");
    currentTree->Execute(blackboard, dt);
    botProfiler.EndSection("behavior_tree");
    
    botProfiler.EndFrame();
}
```

#### 2. Console Commands
```cpp
// Profiling commands
bot_profile <botnum>              // Profile specific bot
bot_profile_all                   // Profile all bots
bot_profile_report                // Print profiling report
bot_profile_reset                 // Reset profiling data
```

#### 3. External Tools
- **perf** (Linux)
- **Instruments** (macOS)
- **Visual Studio Profiler** (Windows)
- **Valgrind** (memory profiling)

---

## Optimization Strategies

### 1. Perception System Optimization

#### Current Bottlenecks (Expected)
- Vision cone calculations (trigonometry)
- Occlusion testing (raycasts)
- Enemy iteration (all entities)
- Memory confidence updates

#### Optimizations

**A. Spatial Indexing for Enemy Queries**
```cpp
// code/fgame/spatial_index.h

class SpatialIndex {
public:
    void Insert(Entity* entity);
    void Remove(Entity* entity);
    std::vector<Entity*> QueryRadius(Vector center, float radius);
    
private:
    struct GridCell {
        std::vector<Entity*> entities;
    };
    
    std::map<GridCoord, GridCell> grid;
    float cellSize = 512.0f;
};

// Usage in perception:
std::vector<Entity*> nearbyEntities = spatialIndex.QueryRadius(bot->origin, visionRange);
// Instead of iterating ALL entities
```

**B. Cache Vision Cone Calculations**
```cpp
// Cache sin/cos values for FOV
struct VisionCache {
    float cosHalfFOV;
    float cosHalfPeripheralFOV;
    Vector forwardVector;
    int cacheFrame;
};

// Only recalculate when bot turns or profile changes
```

**C. Reduce Occlusion Test Frequency**
```cpp
// Don't test every frame for every enemy
if (enemy->lastOcclusionTest + 0.1f < level.time) {
    bool visible = TestOcclusion(bot, enemy);
    enemy->lastOcclusionTest = level.time;
    enemy->cachedVisible = visible;
}
return enemy->cachedVisible;
```

**D. Early-Out Distance Checks**
```cpp
// Check distance before expensive vision cone
float distSq = (enemy->origin - bot->origin).lengthSquared();
if (distSq > visionRangeSq) continue;  // Skip this enemy
```

### 2. Behavior Tree Optimization

#### Current Bottlenecks (Expected)
- Node traversal overhead
- Blackboard lookups
- Condition evaluations
- Action updates

#### Optimizations

**A. Node Result Caching**
```cpp
class BTNode {
    Status Execute(Blackboard& bb, float dt) {
        // Cache result for fast-changing conditions
        if (CanCache() && cacheValid) {
            return cachedStatus;
        }
        
        Status result = ExecuteImpl(bb, dt);
        
        if (CanCache()) {
            cachedStatus = result;
            cacheValid = true;
            cacheTime = level.time;
        }
        
        return result;
    }
};
```

**B. Blackboard Access Optimization**
```cpp
// Pre-fetch common values at frame start
struct BlackboardCache {
    Player* bot;
    PerceptionSnapshot* perception;
    BotProfile* profile;
    Sentient* currentTarget;
};

// Pass cache to nodes instead of blackboard lookups
```

**C. Selector/Sequence Short-Circuiting**
```cpp
// Already implemented, but ensure:
// Selector: return on first SUCCESS
// Sequence: return on first FAILURE
// Don't evaluate unnecessary nodes
```

**D. Reduce Tree Depth**
```yaml
# Bad (deep nesting):
selector:
  - sequence:
      - selector:
          - sequence: ...

# Good (flatter structure):
selector:
  - condition_and_action_1
  - condition_and_action_2
  - condition_and_action_3
```

### 3. Utility AI Optimization

#### Current Bottlenecks (Expected)
- Scoring all actions every evaluation
- Consideration curve calculations
- Blackboard population

#### Optimizations

**A. Reduce Evaluation Frequency**
```cpp
// Current: Evaluate every 0.5 seconds
// Optimized: Evaluate based on context

float evalInterval = 0.5f;

// Reduce frequency when situation stable
if (threatLevel == THREAT_NONE) {
    evalInterval = 1.0f;  // Evaluate less often when idle
} else if (threatLevel == THREAT_HIGH) {
    evalInterval = 0.25f;  // Evaluate more often in combat
}

if (level.time - lastEvalTime > evalInterval) {
    EvaluateUtility();
}
```

**B. Cache Consideration Values**
```cpp
struct ConsiderationCache {
    float healthFactor;
    float ammoFactor;
    int enemyCount;
    // ... etc
    int cacheFrame;
};

// Populate cache once per frame, reuse for all actions
```

**C. Early-Out for Dominant Actions**
```cpp
// If one action scores >0.9, don't evaluate rest
float maxScore = 0.0f;
ScoredAction best;

for (auto& action : actions) {
    float score = ScoreAction(action);
    if (score > maxScore) {
        maxScore = score;
        best = {action.name, score, action.tree};
    }
    
    if (score > 0.9f) {
        return best;  // Good enough, skip remaining
    }
}
```

**D. Simplify Curves**
```cpp
// Use lookup tables for expensive curves
static float exponentialCurve[101];  // Pre-computed 0.00-1.00

float ApplyCurve(float input) {
    int index = (int)(input * 100.0f);
    return exponentialCurve[index];
}
```

### 4. Pathfinding Optimization

#### Optimizations

**A. Path Request Throttling**
```cpp
// Don't request new path every frame
if (level.time - lastPathRequest < 1.0f) {
    return;  // Use existing path
}

// Only request if target moved significantly
float targetMovement = (currentTarget - lastPathTarget).length();
if (targetMovement < 128.0f) {
    return;  // Target hasn't moved much
}
```

**B. Path Simplification**
```cpp
// Reduce waypoint count using Ramer-Douglas-Peucker
std::vector<Vector> SimplifyPath(const std::vector<Vector>& path, float epsilon) {
    // Remove waypoints that don't significantly change direction
}
```

**C. Async Pathfinding** (Phase 4 feature, but note for now)
```cpp
// Note: Consider deferring to Phase 4 (Task 4.3)
// Pathfinding on separate thread
```

---

## Implementation Steps

### Day 1: Profiling Setup (6 hours)
- [ ] Implement `BotProfiler` class
- [ ] Add profiling sections to bot Think()
- [ ] Add console commands for profiling
- [ ] Create test scenario with 50 bots
- [ ] Run baseline profiling
- [ ] Document baseline performance

### Day 2: Profile & Identify Bottlenecks (8 hours)
- [ ] Profile with 10, 25, 50, 100 bots
- [ ] Identify slowest systems
- [ ] Identify slowest functions
- [ ] Create performance report
- [ ] Prioritize optimizations

### Day 3-4: Implement Optimizations (16 hours)
Based on profiling results, implement highest-impact optimizations:
- [ ] Spatial indexing for perception (if needed)
- [ ] Vision cone caching (if needed)
- [ ] Occlusion test throttling (if needed)
- [ ] BT node caching (if needed)
- [ ] Blackboard access optimization (if needed)
- [ ] Utility evaluation throttling (if needed)
- [ ] Consideration caching (if needed)
- [ ] Path request throttling (if needed)

### Day 5: Measure Improvements (6 hours)
- [ ] Re-run profiling tests
- [ ] Compare before/after
- [ ] Document improvements
- [ ] Identify remaining bottlenecks
- [ ] Prioritize next round of optimizations

### Day 6: Additional Optimizations (8 hours)
- [ ] Implement second-priority optimizations
- [ ] Test edge cases
- [ ] Ensure no behavioral regressions
- [ ] Verify stability

### Day 7: Final Testing & Documentation (8 hours)
- [ ] Final performance testing
- [ ] Test with different bot counts (10-100)
- [ ] Test on different maps
- [ ] Test with different configurations
- [ ] Document all optimizations
- [ ] Create performance report
- [ ] Update architecture docs

---

## Testing Strategy

### Performance Benchmarks

```cpp
// tests/benchmark_ai.cpp

void BenchmarkAI() {
    // Test 1: Single bot performance
    Measure(1, "single_bot");
    
    // Test 2: 10 bots
    Measure(10, "ten_bots");
    
    // Test 3: 25 bots
    Measure(25, "twentyfive_bots");
    
    // Test 4: 50 bots (target)
    Measure(50, "fifty_bots");
    
    // Test 5: 100 bots (stress test)
    Measure(100, "hundred_bots");
}

void Measure(int botCount, const char* label) {
    // Spawn bots
    // Run for 60 seconds
    // Record avg/min/max frame times
    // Record avg/min/max bot Think() times
    // Save report
}
```

### Regression Testing
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Behavior is identical to pre-optimization
- [ ] No new bugs introduced

---

## Files to Create/Modify

### New Files
```
code/fgame/bot_profiler.h            # BotProfiler class
code/fgame/bot_profiler.cpp          # Implementation
code/fgame/spatial_index.h           # Spatial indexing (if needed)
code/fgame/spatial_index.cpp         # Implementation
tests/benchmark_ai.cpp               # Performance benchmarks
docs/PERFORMANCE_REPORT.md           # Before/after analysis
```

### Modified Files
```
code/fgame/perception.cpp            # Perception optimizations
code/fgame/behavior_tree.cpp         # BT optimizations
code/fgame/utility_evaluator.cpp     # Utility optimizations
code/fgame/playerbot.cpp             # Integration of optimizations
```

---

## Acceptance Criteria

### Performance
- [ ] Average bot Think() time <1ms
- [ ] 50 bots @ 60 FPS stable
- [ ] No frame drops during normal gameplay
- [ ] Performance documented with before/after metrics

### Quality
- [ ] Zero behavioral regressions
- [ ] All tests pass
- [ ] No new bugs introduced
- [ ] Code quality maintained

### Documentation
- [ ] PERFORMANCE_REPORT.md created
- [ ] Optimizations documented
- [ ] Profiling methodology documented
- [ ] Future optimization opportunities noted

---

## Success Metrics

### Performance Targets
- **Per-Bot:** <1ms average
- **50 Bots:** 60 FPS stable
- **100 Bots:** 30 FPS acceptable
- **Improvement:** 2x-5x faster than baseline

### Code Quality
- **Maintainability:** Optimizations don't obscure code
- **Clarity:** Performance-critical sections documented
- **Flexibility:** Easy to add features without breaking optimization

---

## Troubleshooting

### Performance Still Poor After Optimization
- Profile again to find new bottlenecks
- Consider algorithmic changes (not just micro-optimizations)
- May need to defer some work to Phase 4 (LOD, async)

### Optimizations Break Behavior
- Revert specific optimization
- Add regression tests
- Re-verify optimization correctness

### Can't Hit Performance Target
- Document why
- Propose alternative targets
- Consider Phase 4 optimizations (LOD, async, multi-threading)

---

## Notes & Considerations

### Optimization vs. Clarity
- Prefer clear code over micro-optimizations
- Only optimize hot paths (80/20 rule)
- Document performance-critical sections

### Future Performance Improvements (Phase 4)
- LOD system (update bots at different rates)
- Async pathfinding
- Multi-threaded perception
- Job system for bot updates

### Performance Monitoring Post-Phase 3
- Add telemetry to track performance in production
- Monitor performance across different maps
- Watch for regressions in future changes

---

**Phase 3 Complete!** 🎉

All behaviors migrated, AI enhanced with utility system, old code removed, performance optimized.

**Next:** Phase 4 - Optimization & Polish (LOD, plugins, testing, documentation)
