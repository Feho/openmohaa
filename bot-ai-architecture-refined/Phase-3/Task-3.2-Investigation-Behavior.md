# Task 3.2: Investigation Behavior

**Status:** Ready to Execute  
**Duration:** 1 week  
**Priority:** MEDIUM  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Implements investigation behaviors that allow bots to search for enemies they've lost sight of or investigate suspicious sounds. This creates more believable AI that doesn't immediately forget about threats.

### Why This Matters
- **Realism:** Real soldiers search areas where they heard sounds or saw enemies
- **Unpredictability:** Players can't just hide and wait for bot to give up
- **Tension:** Creates suspenseful moments when bot is actively searching
- **Memory Integration:** Uses the MemorySystem from Phase 2A effectively

### What's Already Complete

**Phase 2A (Perception):**
- ✅ MemorySystem stores enemy last-known positions
- ✅ EnemyMemory includes: lastKnownPosition, predictedPosition, confidence decay
- ✅ AudioSensor detects sounds with 3D position
- ✅ AudioEvent includes: position, direction, loudness, timestamp

**Phase 2B (Behavior Trees):**
- ✅ BT framework with all node types
- ✅ YAML tree loading
- ✅ Blackboard for state storage

**Phase 3.1 (Combat):**
- ✅ Complete combat behaviors
- ✅ Action patterns and structure

**What Bots Can Do Now:**
- Detect and engage visible enemies
- Remember enemy positions briefly
- Hear sounds

**What Bots Need:**
- Behavior to move to last-known enemy position
- Search pattern when reaching search area
- Investigation timeout (give up after time)
- Transition back to patrol/combat

---

## Current State: Old Investigation System

### State Machine Logic
```cpp
// From playerbot.cpp - Current investigate state
void BotController::State_Investigate() {
    if (!m_pEnemy) {
        // Lost enemy, search last known position
        if (PathExists(m_vLastEnemyPos)) {
            MoveTo(m_vLastEnemyPos);
        }
    }
    
    // Look around when reached
    if (DistanceTo(m_vLastEnemyPos) < 128.0f) {
        LookAroundRandomly();
    }
    
    // Timeout after 10 seconds
    if (level.time - m_iInvestigateStartTime > 10.0f) {
        SetState(STATE_IDLE);
    }
}

bool BotController::CheckCondition_Investigate() {
    // Enter investigate if lost enemy recently
    return m_pEnemy == NULL && 
           m_vLastEnemyPos != vec_zero &&
           (level.time - m_iLastEnemyTime) < 5.0f;
}
```

### Key Behaviors to Migrate

1. **Move to Last Known Position**
   - Use predicted position from MemorySystem
   - Navigate to location
   - Handle unreachable positions

2. **Search Area**
   - Spiral search pattern around position
   - Look in multiple directions
   - Check common hiding spots

3. **Sound Investigation**
   - Move toward interesting sounds
   - Check sound origin
   - Determine if threat or false alarm

4. **Investigation Timeout**
   - Give up after reasonable time (10-15 seconds)
   - Lower confidence on failed search
   - Return to previous activity

---

## Technical Specification

### Investigation Tree Architecture

```yaml
# behaviors/investigation.btree
tree:
  type: selector
  name: "Investigation Root"
  children:
    # Priority 1: Investigate high-confidence enemy memory
    - type: sequence
      name: "Investigate Enemy Memory"
      children:
        - type: condition
          check: "HasHighConfidenceMemory"  # Confidence > 0.5
        - type: action
          action: "SetInvestigationTarget"  # Use predicted position
        - type: action
          action: "MoveToInvestigationTarget"
        - type: selector
          name: "Search or Timeout"
          children:
            # Found enemy during search
            - type: condition
              check: "HasVisibleEnemy"
            
            # Reached target, search area
            - type: sequence
              name: "Search Area"
              children:
                - type: condition
                  check: "ReachedInvestigationTarget"
                - type: action
                  action: "SearchArea"  # Spiral/grid pattern
            
            # Timeout
            - type: sequence
              name: "Investigation Timeout"
              children:
                - type: condition
                  check: "InvestigationTimedOut"  # > 15 seconds
                - type: action
                  action: "AbandonInvestigation"

    # Priority 2: Investigate interesting sound
    - type: sequence
      name: "Investigate Sound"
      children:
        - type: condition
          check: "HasInterestingSound"  # Weapon fire, footsteps
        - type: action
          action: "SetSoundInvestigationTarget"
        - type: action
          action: "MoveToSoundLocation"
        - type: selector
          children:
            # Found enemy
            - type: condition
              check: "HasVisibleEnemy"
            
            # Reached sound location
            - type: sequence
              children:
                - type: condition
                  check: "ReachedSoundLocation"
                - type: action
                  action: "LookAround"
                - type: action
                  action: "MarkSoundInvestigated"

    # No investigation needed
    - type: action
      action: "ReturnToIdle"
```

### Actions to Implement

```cpp
// code/fgame/bt_actions_investigation.cpp

// Action: SetInvestigationTarget
// Sets the target position for investigation from enemy memory
BTNode::Status Action_SetInvestigationTarget(Blackboard& bb, float dt) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    // Find highest confidence enemy memory
    EnemyMemory* bestMemory = nullptr;
    float highestConfidence = 0.0f;
    
    for (auto& memory : perception->knownEnemies) {
        if (memory.confidenceLevel > highestConfidence) {
            highestConfidence = memory.confidenceLevel;
            bestMemory = &memory;
        }
    }
    
    if (!bestMemory) return BTNode::Status::FAILURE;
    
    // Use predicted position (accounts for last known velocity)
    bb.Set("investigationTarget", bestMemory->predictedPosition);
    bb.Set("investigationStartTime", level.time);
    bb.Set("investigationRadius", 256.0f);  // Search radius
    bb.Set("investigatingMemory", bestMemory);
    
    return BTNode::Status::SUCCESS;
}

// Action: MoveToInvestigationTarget
// Moves bot toward investigation target
BTNode::Status Action_MoveToInvestigationTarget(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Vector target = bb.Get<Vector>("investigationTarget");
    
    // Move to target
    bot->MoveTo(target);
    
    // Check if reached (within 128 units)
    float distance = (target - bot->origin).length();
    if (distance < 128.0f) {
        bb.Set("reachedInvestigationTarget", true);
        return BTNode::Status::SUCCESS;
    }
    
    // Check if path blocked
    if (!bot->PathExists(target)) {
        // Try alternative nearby position
        Vector alternative = FindNearbyReachablePosition(bot->origin, target);
        if (alternative != Vector::Zero) {
            bb.Set("investigationTarget", alternative);
        } else {
            return BTNode::Status::FAILURE;  // Can't reach
        }
    }
    
    return BTNode::Status::RUNNING;
}

// Action: SearchArea
// Performs systematic search of area around target
BTNode::Status Action_SearchArea(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Vector centerPos = bb.Get<Vector>("investigationTarget");
    float searchRadius = bb.Get<float>("investigationRadius", 256.0f);
    
    // Get or initialize search state
    int searchPhase = bb.Get<int>("searchPhase", 0);
    float searchAngle = bb.Get<float>("searchAngle", 0.0f);
    float searchTimer = bb.Get<float>("searchTimer", 0.0f);
    
    searchTimer += dt;
    
    // Phase 0: Look around from center (4 cardinal directions)
    if (searchPhase == 0) {
        if (searchTimer > 1.0f) {  // Look for 1 second
            searchAngle += 90.0f;  // Next cardinal direction
            searchTimer = 0.0f;
            
            if (searchAngle >= 360.0f) {
                searchPhase = 1;  // Move to spiral search
                searchAngle = 0.0f;
            }
        }
        
        // Look in current direction
        Vector lookDir = AngleVectors(Vector(0, searchAngle, 0));
        bot->SetViewAngles(VectorToAngles(lookDir));
    }
    // Phase 1: Spiral search pattern
    else if (searchPhase == 1) {
        // Calculate spiral position
        float spiralRadius = (searchAngle / 360.0f) * searchRadius;
        Vector spiralOffset = Vector(
            cos(DEG2RAD(searchAngle)) * spiralRadius,
            sin(DEG2RAD(searchAngle)) * spiralRadius,
            0
        );
        Vector spiralPos = centerPos + spiralOffset;
        
        // Move to spiral position
        bot->MoveTo(spiralPos);
        
        // Increment spiral
        searchAngle += 30.0f;  // 30 degrees per step
        
        if (searchAngle >= 720.0f) {  // Two full circles
            searchPhase = 2;  // Search complete
        }
    }
    // Phase 2: Search complete
    else {
        bb.Set("searchComplete", true);
        return BTNode::Status::SUCCESS;
    }
    
    // Update blackboard
    bb.Set("searchPhase", searchPhase);
    bb.Set("searchAngle", searchAngle);
    bb.Set("searchTimer", searchTimer);
    
    return BTNode::Status::RUNNING;
}

// Action: SetSoundInvestigationTarget
// Sets target to investigate sound
BTNode::Status Action_SetSoundInvestigationTarget(Blackboard& bb, float dt) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    if (!perception->loudestSound) {
        return BTNode::Status::FAILURE;
    }
    
    AudioEvent* sound = perception->loudestSound;
    
    bb.Set("investigationTarget", sound->position);
    bb.Set("investigationStartTime", level.time);
    bb.Set("investigatingSound", true);
    
    return BTNode::Status::SUCCESS;
}

// Action: MoveToSoundLocation
// Moves to sound origin
BTNode::Status Action_MoveToSoundLocation(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Vector target = bb.Get<Vector>("investigationTarget");
    
    bot->MoveTo(target);
    
    float distance = (target - bot->origin).length();
    if (distance < 128.0f) {
        bb.Set("reachedSoundLocation", true);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: LookAround
// Looks in multiple directions from current position
BTNode::Status Action_LookAround(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    float lookTimer = bb.Get<float>("lookTimer", 0.0f);
    int lookCount = bb.Get<int>("lookCount", 0);
    
    lookTimer += dt;
    
    // Look for 1 second in each direction
    if (lookTimer > 1.0f) {
        lookCount++;
        lookTimer = 0.0f;
        
        if (lookCount >= 4) {  // Looked in 4 directions
            bb.Set("lookComplete", true);
            bb.Set("lookCount", 0);
            return BTNode::Status::SUCCESS;
        }
        
        // Choose new random direction
        float randomAngle = random() * 360.0f;
        Vector lookDir = AngleVectors(Vector(0, randomAngle, 0));
        bot->SetViewAngles(VectorToAngles(lookDir));
    }
    
    bb.Set("lookTimer", lookTimer);
    bb.Set("lookCount", lookCount);
    
    return BTNode::Status::RUNNING;
}

// Action: MarkSoundInvestigated
// Marks sound as investigated to avoid re-investigating
BTNode::Status Action_MarkSoundInvestigated(Blackboard& bb, float dt) {
    // Clear sound investigation state
    bb.Set("investigatingSound", false);
    bb.Set("reachedSoundLocation", false);
    bb.Set("lookComplete", false);
    
    return BTNode::Status::SUCCESS;
}

// Action: AbandonInvestigation
// Gives up investigation and cleans up state
BTNode::Status Action_AbandonInvestigation(Blackboard& bb, float dt) {
    EnemyMemory* memory = bb.Get<EnemyMemory*>("investigatingMemory", nullptr);
    
    // Lower confidence in memory
    if (memory) {
        memory->confidenceLevel *= 0.5f;  // Reduce by half
    }
    
    // Clear investigation state
    bb.Set("investigationTarget", Vector::Zero);
    bb.Set("investigatingMemory", nullptr);
    bb.Set("reachedInvestigationTarget", false);
    bb.Set("searchPhase", 0);
    bb.Set("searchAngle", 0.0f);
    bb.Set("searchComplete", false);
    
    return BTNode::Status::SUCCESS;
}
```

### Conditions to Implement

```cpp
// code/fgame/bt_conditions_investigation.cpp

// Condition: HasHighConfidenceMemory
bool Condition_HasHighConfidenceMemory(Blackboard& bb) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    for (const auto& memory : perception->knownEnemies) {
        if (memory.confidenceLevel > 0.5f) {
            return true;
        }
    }
    
    return false;
}

// Condition: HasInterestingSound
bool Condition_HasInterestingSound(Blackboard& bb) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    if (!perception->loudestSound) return false;
    
    AudioEvent* sound = perception->loudestSound;
    
    // Only investigate important sounds
    return (sound->type == AI_EVENT_WEAPON_FIRE ||
            sound->type == AI_EVENT_FOOTSTEP ||
            sound->type == AI_EVENT_GRENADE) &&
           sound->priority > 0.5f;
}

// Condition: ReachedInvestigationTarget
bool Condition_ReachedInvestigationTarget(Blackboard& bb) {
    return bb.Get<bool>("reachedInvestigationTarget", false);
}

// Condition: InvestigationTimedOut
bool Condition_InvestigationTimedOut(Blackboard& bb) {
    float startTime = bb.Get<float>("investigationStartTime", 0.0f);
    float elapsed = level.time - startTime;
    
    return elapsed > 15.0f;  // 15 second timeout
}

// Condition: ReachedSoundLocation
bool Condition_ReachedSoundLocation(Blackboard& bb) {
    return bb.Get<bool>("reachedSoundLocation", false);
}
```

### Helper Functions

```cpp
// code/fgame/investigation_helpers.cpp

// Find reachable position near target
Vector FindNearbyReachablePosition(Vector origin, Vector target) {
    const int numAttempts = 8;
    const float searchRadius = 128.0f;
    
    for (int i = 0; i < numAttempts; i++) {
        float angle = (i * 360.0f) / numAttempts;
        Vector offset = Vector(
            cos(DEG2RAD(angle)) * searchRadius,
            sin(DEG2RAD(angle)) * searchRadius,
            0
        );
        
        Vector testPos = target + offset;
        
        if (PathExists(origin, testPos)) {
            return testPos;
        }
    }
    
    return Vector::Zero;  // None found
}
```

---

## Implementation Steps

### Day 1: Enemy Memory Investigation (6 hours)
- [ ] Implement `Action_SetInvestigationTarget`
- [ ] Implement `Action_MoveToInvestigationTarget`
- [ ] Implement `Condition_HasHighConfidenceMemory`
- [ ] Implement `Condition_ReachedInvestigationTarget`
- [ ] Add helper function `FindNearbyReachablePosition`
- [ ] **Write unit tests** (3 tests: target setting, movement, unreachable handling)

### Day 2: Search Pattern (6 hours)
- [ ] Implement `Action_SearchArea` with spiral pattern
- [ ] Implement look-around phase (cardinal directions)
- [ ] Implement spiral movement phase
- [ ] Test search pattern visually
- [ ] **Write unit tests** (2 tests: spiral coverage, phase transitions)

### Day 3: Sound Investigation (6 hours)
- [ ] Implement `Action_SetSoundInvestigationTarget`
- [ ] Implement `Action_MoveToSoundLocation`
- [ ] Implement `Action_LookAround`
- [ ] Implement `Action_MarkSoundInvestigated`
- [ ] Implement `Condition_HasInterestingSound`
- [ ] Implement `Condition_ReachedSoundLocation`
- [ ] **Write unit tests** (3 tests: sound targeting, look around, sound filtering)

### Day 4: Timeout & Cleanup (4 hours)
- [ ] Implement `Action_AbandonInvestigation`
- [ ] Implement `Condition_InvestigationTimedOut`
- [ ] Add confidence reduction on failed search
- [ ] Add state cleanup
- [ ] **Write unit tests** (2 tests: timeout triggers, state cleanup)

### Day 5: YAML Tree & Integration (6 hours)
- [ ] Create `behaviors/investigation.btree`
- [ ] Register all actions and conditions
- [ ] Integrate with BotController
- [ ] Add investigation tree to bot behavior selector
- [ ] Test transitions from combat to investigation
- [ ] Test transitions from investigation back to patrol/combat

### Day 6-7: Testing & Tuning (10 hours)
- [ ] **Write integration tests** (4 tests):
  - Bot investigates last known enemy position
  - Bot searches area systematically
  - Bot investigates weapon fire sound
  - Bot gives up after timeout
- [ ] Test with different map layouts
- [ ] Tune search radius (256 units?)
- [ ] Tune timeout duration (15 seconds?)
- [ ] Tune spiral search parameters
- [ ] Test memory confidence decay
- [ ] Fix bugs found during testing

---

## Files to Create/Modify

### New Files
```
code/fgame/bt_actions_investigation.cpp      # 8 investigation actions
code/fgame/bt_actions_investigation.h        # Action declarations
code/fgame/bt_conditions_investigation.cpp   # 5 investigation conditions
code/fgame/bt_conditions_investigation.h     # Condition declarations
code/fgame/investigation_helpers.cpp         # Helper functions
code/fgame/investigation_helpers.h           # FindNearbyReachablePosition, etc.
behaviors/investigation.btree                # Investigation behavior tree YAML
tests/test_investigation_actions.cpp         # Unit tests for actions
tests/test_investigation_conditions.cpp      # Unit tests for conditions
tests/integration_test_investigation.cpp     # Integration tests
```

### Modified Files
```
code/fgame/playerbot.cpp              # Add investigation tree to behavior selector
code/fgame/perception.cpp             # Ensure loudestSound is populated
```

---

## Testing Strategy

### Unit Tests (10 tests)
- 3 enemy memory investigation tests
- 2 search pattern tests
- 3 sound investigation tests
- 2 timeout/cleanup tests

### Integration Tests (4 tests)
- End-to-end enemy memory investigation
- Sound investigation scenario
- Timeout and return to idle
- Unreachable position handling

### Manual Testing
- [ ] Bot investigates after losing sight of enemy
- [ ] Bot searches systematically (visible pattern)
- [ ] Bot investigates weapon fire sounds
- [ ] Bot gives up after reasonable time
- [ ] Bot returns to patrol after investigation
- [ ] Bot doesn't get stuck in investigation loop

---

## Acceptance Criteria

### Functionality
- [ ] All 8 investigation actions implemented and tested
- [ ] All 5 investigation conditions implemented and tested
- [ ] Investigation tree loads from YAML
- [ ] Bot moves to last known enemy position
- [ ] Bot performs spiral search pattern
- [ ] Bot investigates interesting sounds
- [ ] Bot times out and returns to other behaviors
- [ ] Investigation doesn't block combat (if enemy seen during search)

### Quality
- [ ] 14 total tests pass (10 unit + 4 integration)
- [ ] Search pattern is visibly systematic
- [ ] No stuck states or infinite loops
- [ ] Smooth transitions to/from other behaviors
- [ ] Code follows OpenMoHAA standards

### Performance
- [ ] Investigation logic < 0.2ms per bot
- [ ] No performance regression during search

---

## Success Metrics

### AI Quality
- **Persistence:** Bot doesn't immediately forget about enemies
- **Believability:** Search pattern looks deliberate, not random
- **Responsiveness:** Bot reacts to new threats during investigation
- **Intelligence:** Bot investigates important sounds, ignores unimportant ones

### Player Experience
- **Tension:** Creates suspense when player is hiding
- **Challenge:** Can't just hide and wait
- **Fairness:** Bot searches systematically, not omniscient

---

## Notes & Considerations

### Search Pattern Options
- **Spiral:** Systematic, covers area evenly (chosen)
- **Grid:** More rigid, easier to implement
- **Random:** Unpredictable but less believable
- **Waypoint:** Pre-defined search points

### Investigation Priority
Investigation should yield to combat:
- If enemy spotted during investigation, immediately switch to combat
- Don't finish search if new threat detected

### Memory Confidence
Use confidence to decide investigation priority:
- High confidence (>0.7): Strong investigation
- Medium confidence (0.3-0.7): Brief search
- Low confidence (<0.3): Ignore

### Sound Investigation Tuning
Not all sounds should trigger investigation:
- Weapon fire: High priority
- Footsteps: Medium priority
- Ambient sounds: Ignore
- Distance matters: Ignore very distant sounds

### Integration with Other Systems
- **Combat:** Investigation interrupts for visible enemies
- **Patrol:** Investigation returns to patrol on timeout
- **Utility AI (Task 3.4):** Utility can choose when to investigate vs. other actions

---

## Troubleshooting

### Bot Spins in Place During Search
- Check spiral calculation for NaN values
- Verify angle wrapping (0-360)
- Test movement commands

### Bot Never Reaches Investigation Target
- Check pathfinding returns valid path
- Verify distance threshold (128 units)
- Test unreachable position fallback

### Bot Investigates Every Sound
- Check sound priority filtering
- Verify interesting sound conditions
- Add cooldown between investigations

### Search Pattern Too Small/Large
- Adjust `investigationRadius` (default 256)
- Test on different map sizes
- Consider weapon range for radius

---

## Dependencies

### Requires (from Previous Phases)
- MemorySystem with confidence decay
- AudioSensor with positioned sounds
- BT framework with all node types
- Pathfinding system

### Provides (for Future Tasks)
- Search pattern logic (reusable)
- Investigation tree (can be included in other trees)
- Sound response patterns

---

**Next Task:** Task 3.3 - Idle Behaviors (patrol, wander, curious states)
