# Epic 8: Performance Optimization

## Overview
Implement Level-of-Detail (LOD) AI, spatial indexing, and async operations to support 100+ bots at 60 FPS.

## Business Value
- **Scalability:** Support massive battles (100+ bots)
- **Smoothness:** No frame hitches from AI
- **Smart Resources:** CPU spent where it matters
- **Gameplay:** Large-scale battles now possible

## Current State
Every bot runs full AI every frame, regardless of relevance to player.

**Problems:**
- 30-40 bots max before FPS drop
- Pathfinding can cause frame hitches
- All bots use same CPU regardless of importance

## Target State
```cpp
class AIScheduler {
    void Update(float dt) {
        for (auto& bot : bots) {
            AILOD lod = DetermineLOD(bot);

            switch (lod) {
                case HIGH:    bot->Think(dt); break;              // 60 FPS
                case MEDIUM:  if (frame % 3 == 0) bot->Think(dt*3); break;  // 20 FPS
                case LOW:     if (frame % 6 == 0) bot->Think(dt*6); break;  // 10 FPS
                case SLEEPING: if (frame % 60 == 0) bot->Think(dt*60); break; // 1 FPS
            }
        }
    }
};

AILOD DetermineLOD(Bot* bot) {
    if (bot->IsInCombat()) return HIGH;
    if (bot->IsNearPlayer(1000)) return MEDIUM;
    if (bot->IsVisibleToPlayer()) return LOW;
    return SLEEPING;
}
```

## Acceptance Criteria
- [ ] LOD system: bots update at different rates
- [ ] Spatial indexing for fast neighbor queries
- [ ] Async pathfinding (no frame hitches)
- [ ] Performance: 100+ bots at 60 FPS (vs. current ~30)
- [ ] Bot Think() time < 1ms average
- [ ] Pathfinding doesn't block main thread

## Technical Components

### 1. LOD System
Adjust AI update frequency based on importance:
- **HIGH (60 FPS):** In combat, near player
- **MEDIUM (20 FPS):** Visible to player, near combat
- **LOW (10 FPS):** Far from player, not in combat
- **SLEEPING (1 FPS):** Very far, just exists

### 2. Spatial Indexing
```cpp
class BotSpatialIndex {
    // Grid-based hash for O(1) neighbor queries
    std::vector<Bot*> FindBotsInRadius(Vector pos, float radius);
    Bot* FindNearestBot(Vector pos, Team team);
    Bot* FindNearestEnemy(Vector pos, Team team);
};
```

### 3. Async Pathfinding
```cpp
class AsyncPathfinder {
    std::future<PathResult> RequestPath(PathRequest req) {
        return std::async(std::launch::async, [=]() {
            return ComputePath(req);  // Worker thread
        });
    }
};

// Bot continues current path while waiting
```

### 4. Query Caching
Cache expensive queries (enemy detection, cover search) and reuse for several frames.

### 5. Profiling Integration
```cpp
class AIProfiler {
    void BeginSample(const char* name);
    void EndSample();
    void GenerateReport();
};

// Usage
PROFILE_SCOPE("BehaviorTree::Execute");
```

## Success Metrics
- **Scalability:** 100+ bots at 60 FPS (vs. current ~30 bots)
- **Latency:** Bot Think() < 1ms average (< 0.5ms for HIGH LOD)
- **Smoothness:** No frame hitches from pathfinding
- **Memory:** +20% max memory (acceptable for performance gain)

## Dependencies
- Profiling tools
- Spatial data structure
- Thread pool for async operations

## Related Epics
- Epic 5 (ECS enables better cache locality)

## References
- `references/performance-optimization.md`
- [Game Programming Patterns: Data Locality](https://gameprogrammingpatterns.com/data-locality.html)
