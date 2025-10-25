# Epic 2: Perception System

## Overview
Extract sensing logic (vision, hearing, memory) into a dedicated Perception System that provides clean separation between "what the bot knows" and "what the bot does."

## Business Value
- **Realism:** Bots can only act on what they can actually sense
- **Tactical Depth:** Sound, vision limits create gameplay opportunities
- **Modularity:** Perception separate from decision logic
- **Testing:** Easy to mock perception for AI testing

## Current State
```cpp
// Vision, hearing, and memory mixed throughout BotController
void BotController::State_Attack() {
    // Inline enemy detection
    for (Sentient* sent : allSentients) {
        if (CanSee(sent, 80, viewDistance)) {
            m_pEnemy = sent;
            // ...
        }
    }
}

void BotController::NoticeEvent(Vector pos, int type, ...) {
    // Event handling mixed with state logic
    if (type == AI_EVENT_WEAPON_FIRE) {
        m_iCuriousTime = level.inttime + 20000;
        m_vNewCuriousPos = pos;
    }
}
```

**Problems:**
- Perception logic scattered across files
- No clear interface between sensing and decision-making
- Hard to test (can't mock what bot sees/hears)
- No memory persistence (bot forgets instantly)
- Vision/audio not realistic (omniscient within range)

## Target State
```cpp
// Clean perception interface
class PerceptionSystem {
public:
    PerceptionSnapshot Update(float dt);

    VisionSensor& GetVision() { return vision; }
    AudioSensor& GetHearing() { return hearing; }
    MemorySystem& GetMemory() { return memory; }

private:
    VisionSensor vision;
    AudioSensor hearing;
    MemorySystem memory;
    WorldKnowledgeBase knowledge;
};

// Rich perception data
struct PerceptionSnapshot {
    std::vector<EnemyInfo> visibleEnemies;     // Currently seen
    std::vector<EnemyInfo> knownEnemies;       // From memory
    std::vector<AudioEvent> recentSounds;      // Last 5 seconds
    std::vector<AllyInfo> nearbyAllies;
    std::vector<TacticalPoint> interestPoints; // Cover, objectives
    ThreatLevel currentThreat;
    float visibility;                           // 0.0 (dark) - 1.0 (bright)
};

// Usage in BotController
void BotController::Think() {
    auto snapshot = perception.Update(dt);

    // Decision layer uses clean snapshot
    blackboard.Set("visibleEnemies", snapshot.visibleEnemies);
    blackboard.Set("threatLevel", snapshot.currentThreat);

    behaviorTree.Execute(blackboard, dt);
}
```

**Benefits:**
- Single source of truth for what bot knows
- Easy to test (mock PerceptionSnapshot)
- Realistic sensing (FOV, occlusion, distance, lighting)
- Memory creates interesting search behaviors

## Acceptance Criteria
- [ ] `PerceptionSystem` class separates sensing from decision logic
- [ ] Vision system with FOV, occlusion, distance attenuation
- [ ] Audio system with 3D positional sound, priority filtering
- [ ] Memory system stores enemy positions with confidence decay
- [ ] Perception snapshot provides clean interface to decision layer
- [ ] All AI states use perception snapshot (not direct queries)
- [ ] Performance: Perception update < 0.3ms per bot
- [ ] Configurable per bot profile (vision range, hearing sensitivity)

## Technical Components

### 1. Vision Sensor
```cpp
class VisionSensor {
public:
    struct VisionParams {
        float fov = 80.0f;           // Field of view in degrees
        float range = 2048.0f;       // Max vision distance
        float peripheralRange = 0.7f; // Peripheral vision multiplier
        bool requiresLight = false;   // If false, ignores lighting
    };

    std::vector<EnemyInfo> UpdateVision(
        const Player* bot,
        const VisionParams& params,
        float dt
    );

private:
    bool CanSee(Vector from, Vector to, float fov, float range);
    bool HasLineOfSight(Vector from, Vector to);
    float CalculateVisibility(Vector from, Vector to, float range);
};

struct EnemyInfo {
    Sentient* entity;
    Vector position;
    Vector velocity;
    float distance;
    float visibilityFactor;  // 0.0 (barely visible) - 1.0 (clear view)
    float angleFromForward;  // Degrees off center
    bool isInPeripheral;
};
```

### 2. Audio Sensor
```cpp
class AudioSensor {
public:
    struct AudioParams {
        float hearingRange = 1024.0f;
        float priorityThreshold = 0.5f;  // Ignore low-priority sounds
        int maxTrackedSounds = 10;
    };

    std::vector<AudioEvent> UpdateHearing(
        const Player* bot,
        const AudioParams& params,
        float dt
    );

    void OnEvent(const AIEvent& event);

private:
    std::deque<AudioEvent> recentEvents;
};

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

### 3. Memory System
```cpp
class MemorySystem {
public:
    void StoreEnemyPosition(Sentient* enemy, Vector pos, Vector vel, float confidence);
    void UpdateMemories(float dt);
    std::vector<EnemyMemory> GetKnownEnemies() const;
    EnemyMemory* GetMemory(Sentient* enemy);

private:
    std::map<EntityID, EnemyMemory> memories;
};

struct EnemyMemory {
    Sentient* enemy;
    Vector lastKnownPosition;
    Vector lastKnownVelocity;
    Vector predictedPosition;      // Based on velocity + time
    float lastSeenTime;
    float confidenceLevel;         // Decays over time
    int timesSpotted;
    bool investigationStarted;
};
```

### 4. World Knowledge Base
```cpp
class WorldKnowledgeBase {
public:
    std::vector<TacticalPoint> GetNearbyPoints(Vector pos, float radius);
    void LearnCoverPoint(Vector pos, float quality);
    void LearnDangerZone(Vector pos, float radius);

private:
    std::vector<CoverPoint> knownCover;
    std::vector<DangerZone> dangerZones;
    std::vector<ItemLocation> itemLocations;
};
```

### 5. Perception Snapshot
```cpp
struct PerceptionSnapshot {
    // Enemies
    std::vector<EnemyInfo> visibleEnemies;
    std::vector<EnemyMemory> knownEnemies;
    EnemyInfo* closestEnemy = nullptr;
    EnemyInfo* mostDangerousEnemy = nullptr;

    // Audio
    std::vector<AudioEvent> recentSounds;
    AudioEvent* loudestSound = nullptr;

    // Allies
    std::vector<AllyInfo> nearbyAllies;

    // Environment
    std::vector<TacticalPoint> nearbyTacticalPoints;
    ThreatLevel threatLevel = THREAT_NONE;
    float environmentalVisibility = 1.0f;  // Lighting, weather

    // Helpers
    bool HasVisibleEnemy() const { return !visibleEnemies.empty(); }
    bool HasKnownEnemy() const { return !knownEnemies.empty(); }
    int GetEnemyCount() const { return visibleEnemies.size(); }
};
```

## Dependencies
- Existing `CanSee()` function (refactor into VisionSensor)
- Existing `NoticeEvent()` function (refactor into AudioSensor)
- Debug rendering for visualization

## Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Performance cost | HIGH | MEDIUM | Profile early, spatial indexing for queries, LOD system |
| Breaking existing AI | HIGH | LOW | Gradual migration, keep old code parallel, thorough testing |
| Over-complicated | MEDIUM | MEDIUM | Start simple, add features incrementally |
| Hard to tune | MEDIUM | HIGH | Expose parameters in bot profiles, debug visualization |

## Implementation Phases

### Phase 1: Core Structure (Week 1)
- Create `PerceptionSystem` class
- Create `PerceptionSnapshot` struct
- Extract vision logic into `VisionSensor`
- Unit tests

### Phase 2: Vision Refinement (Week 2)
- Implement FOV checks
- Implement occlusion testing
- Distance attenuation
- Peripheral vision
- Integration tests

### Phase 3: Audio System (Week 3)
- Create `AudioSensor`
- Refactor `NoticeEvent()` into sensor
- 3D positional audio
- Priority filtering

### Phase 4: Memory System (Week 4)
- Create `MemorySystem`
- Store enemy positions
- Confidence decay over time
- Predicted positions

### Phase 5: Integration (Week 5)
- Update `BotController` to use perception
- Migrate all states to use snapshot
- Remove old perception code
- Performance tuning

## Success Metrics
- **Code Quality:** All perception logic in PerceptionSystem
- **Performance:** < 0.3ms per bot update
- **Realism:** Bots can't "see through walls" or detect silent enemies
- **AI Quality:** Bots search believably when they lose sight of enemy

## Related Epics
- **Epic 1:** Behavior Tree System (consumes perception data)
- **Epic 3:** Utility AI (scores actions based on perception)
- **Epic 7:** Debug Visualization (visualize FOV, sounds, memories)

## References
- `references/perception-systems.md`
- [AI Perception in Unreal Engine](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/ArtificialIntelligence/AIPerception/)

## Open Questions
- [ ] Should we simulate realistic hearing (footsteps louder on metal, muffled through walls)?
- [ ] Do we need smell/touch sensors for completeness?
- [ ] Should memory be per-bot or shared across squad?
- [ ] How to handle stealth mechanics (crouch = quieter, darker areas = harder to see)?

## Example Use Cases

### Use Case 1: Combat with Memory
Bot sees enemy, enemy goes behind cover, bot remembers last position.

```cpp
void BotController::Think() {
    auto perc = perception.Update(dt);

    if (perc.HasVisibleEnemy()) {
        // Store in memory
        perception.GetMemory().StoreEnemyPosition(
            perc.closestEnemy->entity,
            perc.closestEnemy->position,
            perc.closestEnemy->velocity,
            1.0f
        );

        // Attack
        blackboard.Set("target", perc.closestEnemy);
    } else if (perc.HasKnownEnemy()) {
        // No visible enemy, but we remember where they were
        auto memory = perc.knownEnemies[0];
        blackboard.Set("lastKnownPos", memory.predictedPosition);
        blackboard.Set("confidence", memory.confidenceLevel);

        // Investigate if confidence high enough
        if (memory.confidenceLevel > 0.5f) {
            investigateTree.Execute(blackboard, dt);
        }
    }
}
```

### Use Case 2: Audio Investigation
Bot hears gunfire, investigates source.

```cpp
void BotController::Think() {
    auto perc = perception.Update(dt);

    if (perc.loudestSound && perc.loudestSound->priority > 0.7f) {
        // High-priority sound (gunfire, explosion)
        Vector soundDir = perc.loudestSound->estimatedDirection;
        float confidence = perc.loudestSound->confidence;

        blackboard.Set("investigatePos", perc.loudestSound->position);
        blackboard.Set("investigateConfidence", confidence);

        investigateTree.Execute(blackboard, dt);
    }
}
```

### Use Case 3: Peripheral Vision
Enemy spotted in peripheral vision, bot reacts slower.

```cpp
void VisionSensor::UpdateVision(...) {
    for (auto& enemy : potentialEnemies) {
        float angle = AngleToTarget(bot->angles, enemy->origin);

        if (angle < params.fov / 2.0f) {
            // Central vision - full detail
            enemyInfo.visibilityFactor = 1.0f;
        } else if (angle < params.fov) {
            // Peripheral vision - reduced detail
            enemyInfo.visibilityFactor = 0.5f;
            enemyInfo.isInPeripheral = true;
        }
    }
}

// In combat behavior
if (enemy->isInPeripheral) {
    reactionTime *= 2.0f;  // Slower reaction to peripheral targets
}
```

## Configuration Example

```yaml
# Bot profile perception config
perception:
  vision:
    fov: 80.0                 # Degrees
    range: 2048.0             # Units
    peripheral_range: 0.7     # Multiplier for peripheral vision
    requires_light: false     # If true, can't see in dark

  hearing:
    range: 1024.0
    priority_threshold: 0.5   # Ignore quiet sounds
    max_tracked_sounds: 10

  memory:
    decay_rate: 0.1           # Confidence loss per second
    min_confidence: 0.2       # Below this, forget enemy
    predict_movement: true    # Extrapolate enemy position
```

## Debug Visualization

```cpp
void DebugDrawPerception(BotController* bot) {
    auto perc = bot->GetPerception().GetLastSnapshot();

    // Draw FOV cone
    DrawFOVCone(bot->origin, bot->angles, 80.0f, 2048.0f, COLOR_YELLOW);

    // Draw visible enemies (green)
    for (auto& enemy : perc.visibleEnemies) {
        DrawLine(bot->origin, enemy.position, COLOR_GREEN);
        DrawSphere(enemy.position, 16.0f, COLOR_GREEN);
    }

    // Draw remembered enemies (orange, with confidence)
    for (auto& memory : perc.knownEnemies) {
        Color c = COLOR_ORANGE * memory.confidenceLevel;
        DrawLine(bot->origin, memory.predictedPosition, c);
        DrawSphere(memory.predictedPosition, 16.0f, c);
    }

    // Draw audio events (blue)
    for (auto& sound : perc.recentSounds) {
        DrawSphere(sound.position, 32.0f * sound.loudness, COLOR_BLUE);
    }
}
```

## Notes
- Start with simple vision (FOV + range)
- Add complexity incrementally
- Profile performance continuously
- Perception is foundation for good AI - invest time here
