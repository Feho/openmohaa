# Task 3.3: Idle Behaviors

**Status:** Ready to Execute  
**Duration:** 1 week  
**Priority:** MEDIUM  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Implements all non-combat behaviors: patrol, wander, curious (ambient sound investigation), and idle. These behaviors make bots look alive and purposeful when no immediate threats exist.

### Why This Matters
- **Believability:** Bots don't just stand still waiting for combat
- **Map Coverage:** Patrols ensure bots cover important areas
- **Unpredictability:** Random elements prevent predictable patterns
- **Immersion:** Ambient behaviors create living world feel

### What's Already Complete

**Phase 2A (Perception):**
- ✅ AudioSensor detects ambient sounds
- ✅ PerceptionSnapshot includes recent sounds
- ✅ ThreatLevel assessment (NONE = idle time)

**Phase 2B (Behavior Trees):**
- ✅ BT framework
- ✅ Simple patrol behavior as proof-of-concept

**Phase 3.1 (Combat):**
- ✅ Complete combat behaviors
- ✅ Emergency transitions

**Phase 3.2 (Investigation):**
- ✅ Sound investigation logic
- ✅ Search patterns

**What Bots Can Do Now:**
- Fight and investigate threats
- Basic patrol (proof-of-concept)

**What Bots Need:**
- Complete patrol system with waypoints
- Random wander when no patrol route
- Curious behavior (investigate minor sounds)
- Idle animations and behaviors
- Smooth transitions to combat when threat appears

---

## Current State: Old Idle System

### State Machine Logic
```cpp
// From playerbot.cpp - Current idle behaviors
void BotController::State_Idle() {
    // Simple patrol between waypoints
    if (m_patrolWaypoints.size() > 0) {
        MoveToNextWaypoint();
    } else {
        // Stand and look around
        LookAroundRandomly();
    }
}

void BotController::State_Curious() {
    // Investigate minor sounds (footsteps, doors)
    if (m_vCuriousPos != vec_zero) {
        MoveTo(m_vCuriousPos);
        
        if (DistanceTo(m_vCuriousPos) < 64.0f) {
            LookAround();
            m_vCuriousPos = vec_zero;
        }
    }
}

bool BotController::CheckCondition_Curious() {
    // Minor sound heard (not weapon fire)
    return m_bHeardSound && !m_bWeaponSound;
}
```

### Key Behaviors to Migrate

1. **Patrol**
   - Follow waypoint path
   - Loop or bounce patrol routes
   - Pause at waypoints
   - Look around at interesting points

2. **Random Wander**
   - Move to random nearby positions
   - Avoid getting stuck
   - Stay in playable areas
   - Occasional pauses

3. **Curious**
   - Investigate minor ambient sounds
   - Shorter investigation than enemy search
   - Quick look and return to patrol

4. **Idle**
   - Stand in place
   - Look around occasionally
   - Play idle animations
   - Lowest priority fallback

5. **Attractive Nodes**
   - Move to interesting map locations
   - Sniper positions
   - Defensive positions
   - Cover points

---

## Technical Specification

### Idle Tree Architecture

```yaml
# behaviors/idle.btree
tree:
  type: selector
  name: "Idle Root"
  children:
    # Priority 1: Investigate curious sounds (minor ambient sounds)
    - type: sequence
      name: "Curious Investigation"
      children:
        - type: condition
          check: "HasCuriousSound"  # Footsteps, doors, not weapon fire
        - type: action
          action: "SetCuriousTarget"
        - type: action
          action: "MoveToCuriousLocation"
        - type: sequence
          name: "Quick Look"
          children:
            - type: condition
              check: "ReachedCuriousLocation"
            - type: action
              action: "QuickLookAround"  # Shorter than investigation
            - type: action
              action: "ClearCuriousState"

    # Priority 2: Use attractive node if available
    - type: sequence
      name: "Use Attractive Node"
      children:
        - type: condition
          check: "HasNearbyAttractiveNode"
        - type: action
          action: "MoveToAttractiveNode"
        - type: sequence
          children:
            - type: condition
              check: "ReachedAttractiveNode"
            - type: action
              action: "UseAttractiveNode"  # Snipe, guard, etc.

    # Priority 3: Follow patrol route
    - type: sequence
      name: "Patrol"
      children:
        - type: condition
          check: "HasPatrolRoute"
        - type: action
          action: "MoveToNextPatrolWaypoint"
        - type: selector
          children:
            # Reached waypoint
            - type: sequence
              children:
                - type: condition
                  check: "ReachedPatrolWaypoint"
                - type: action
                  action: "PauseAtWaypoint"  # Brief pause
                - type: action
                  action: "AdvanceToNextWaypoint"
            
            # Still moving
            - type: action
              action: "ContinuePatrol"

    # Priority 4: Random wander
    - type: sequence
      name: "Wander"
      children:
        - type: condition
          check: "ShouldWander"  # No patrol, not recently moved
        - type: action
          action: "PickRandomWanderTarget"
        - type: action
          action: "MoveToWanderTarget"
        - type: sequence
          children:
            - type: condition
              check: "ReachedWanderTarget"
            - type: action
              action: "PauseAfterWander"
            - type: action
              action: "ClearWanderTarget"

    # Priority 5: Stand idle
    - type: sequence
      name: "Stand Idle"
      children:
        - type: action
          action: "StandInPlace"
        - type: action
          action: "OccasionalLookAround"
```

### Actions to Implement

```cpp
// code/fgame/bt_actions_idle.cpp

// Action: SetCuriousTarget
// Sets target for curious investigation
BTNode::Status Action_SetCuriousTarget(Blackboard& bb, float dt) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    // Find curious sound (footsteps, doors, ambient)
    AudioEvent* curiousSound = nullptr;
    for (auto& sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP ||
            sound.type == AI_EVENT_DOOR ||
            sound.type == AI_EVENT_AMBIENT) {
            curiousSound = &sound;
            break;
        }
    }
    
    if (!curiousSound) return BTNode::Status::FAILURE;
    
    bb.Set("curiousTarget", curiousSound->position);
    bb.Set("curiousStartTime", level.time);
    
    return BTNode::Status::SUCCESS;
}

// Action: MoveToCuriousLocation
// Moves to curious sound location
BTNode::Status Action_MoveToCuriousLocation(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Vector target = bb.Get<Vector>("curiousTarget");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    // Move at walk speed (curious, not urgent)
    bot->MoveTo(target);
    bot->SetMoveSpeed(0.6f * profile->GetSpeedPreference());
    
    float distance = (target - bot->origin).length();
    if (distance < 64.0f) {
        bb.Set("reachedCuriousLocation", true);
        return BTNode::Status::SUCCESS;
    }
    
    // Timeout after 5 seconds (shorter than investigation)
    float startTime = bb.Get<float>("curiousStartTime");
    if (level.time - startTime > 5.0f) {
        return BTNode::Status::FAILURE;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: QuickLookAround
// Briefly looks around (shorter than investigation)
BTNode::Status Action_QuickLookAround(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    float lookTimer = bb.Get<float>("curiousLookTimer", 0.0f);
    int lookCount = bb.Get<int>("curiousLookCount", 0);
    
    lookTimer += dt;
    
    if (lookTimer > 0.5f) {  // 0.5 seconds per direction (faster than investigation)
        lookCount++;
        lookTimer = 0.0f;
        
        if (lookCount >= 2) {  // Only look left and right
            bb.Set("curiousLookCount", 0);
            return BTNode::Status::SUCCESS;
        }
        
        // Alternate left/right
        float angle = bot->GetYaw() + (lookCount % 2 == 0 ? 90.0f : -90.0f);
        Vector lookDir = AngleVectors(Vector(0, angle, 0));
        bot->SetViewAngles(VectorToAngles(lookDir));
    }
    
    bb.Set("curiousLookTimer", lookTimer);
    bb.Set("curiousLookCount", lookCount);
    
    return BTNode::Status::RUNNING;
}

// Action: ClearCuriousState
// Cleans up curious investigation state
BTNode::Status Action_ClearCuriousState(Blackboard& bb, float dt) {
    bb.Set("curiousTarget", Vector::Zero);
    bb.Set("reachedCuriousLocation", false);
    bb.Set("curiousLookTimer", 0.0f);
    bb.Set("curiousLookCount", 0);
    
    return BTNode::Status::SUCCESS;
}

// Action: MoveToAttractiveNode
// Moves to attractive map node
BTNode::Status Action_MoveToAttractiveNode(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    PathNode* attractiveNode = bb.Get<PathNode*>("attractiveNode");
    
    if (!attractiveNode) {
        // Find new attractive node
        attractiveNode = FindNearbyAttractiveNode(bot->origin);
        if (!attractiveNode) return BTNode::Status::FAILURE;
        bb.Set("attractiveNode", attractiveNode);
    }
    
    bot->MoveTo(attractiveNode->origin);
    
    float distance = (attractiveNode->origin - bot->origin).length();
    if (distance < 64.0f) {
        bb.Set("reachedAttractiveNode", true);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: UseAttractiveNode
// Uses attractive node behavior (snipe, guard, etc.)
BTNode::Status Action_UseAttractiveNode(Blackboard& bb, float dt) {
    PathNode* node = bb.Get<PathNode*>("attractiveNode");
    Player* bot = bb.Get<Player*>("bot");
    
    // Use node for configured duration
    float useTimer = bb.Get<float>("attractiveNodeTimer", 0.0f);
    useTimer += dt;
    
    // Look in node's preferred direction
    if (node->targetName != "") {
        Vector lookTarget = FindTargetPosition(node->targetName);
        bot->SetViewAngles(VectorToAngles(lookTarget - bot->origin));
    } else {
        // Look around
        float lookAngle = (useTimer / 2.0f) * 360.0f;  // Full rotation every 2 seconds
        Vector lookDir = AngleVectors(Vector(0, lookAngle, 0));
        bot->SetViewAngles(VectorToAngles(lookDir));
    }
    
    // Use node for 10-15 seconds
    float useDuration = 10.0f + random() * 5.0f;
    
    if (useTimer > useDuration) {
        bb.Set("attractiveNode", nullptr);
        bb.Set("reachedAttractiveNode", false);
        bb.Set("attractiveNodeTimer", 0.0f);
        return BTNode::Status::SUCCESS;
    }
    
    bb.Set("attractiveNodeTimer", useTimer);
    return BTNode::Status::RUNNING;
}

// Action: MoveToNextPatrolWaypoint
// Moves to next waypoint in patrol route
BTNode::Status Action_MoveToNextPatrolWaypoint(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    std::vector<PathNode*>* patrolRoute = bb.Get<std::vector<PathNode*>*>("patrolRoute");
    int waypointIndex = bb.Get<int>("patrolWaypointIndex", 0);
    
    if (!patrolRoute || patrolRoute->empty()) {
        return BTNode::Status::FAILURE;
    }
    
    PathNode* waypoint = (*patrolRoute)[waypointIndex];
    
    bot->MoveTo(waypoint->origin);
    bot->SetMoveSpeed(0.5f);  // Patrol at half speed
    
    float distance = (waypoint->origin - bot->origin).length();
    if (distance < 32.0f) {
        bb.Set("reachedPatrolWaypoint", true);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: PauseAtWaypoint
// Pauses briefly at patrol waypoint
BTNode::Status Action_PauseAtWaypoint(Blackboard& bb, float dt) {
    float pauseTimer = bb.Get<float>("waypointPauseTimer", 0.0f);
    pauseTimer += dt;
    
    // Pause for 1-3 seconds
    float pauseDuration = 1.0f + random() * 2.0f;
    
    if (pauseTimer > pauseDuration) {
        bb.Set("waypointPauseTimer", 0.0f);
        return BTNode::Status::SUCCESS;
    }
    
    bb.Set("waypointPauseTimer", pauseTimer);
    return BTNode::Status::RUNNING;
}

// Action: AdvanceToNextWaypoint
// Advances to next waypoint in patrol route
BTNode::Status Action_AdvanceToNextWaypoint(Blackboard& bb, float dt) {
    std::vector<PathNode*>* patrolRoute = bb.Get<std::vector<PathNode*>*>("patrolRoute");
    int waypointIndex = bb.Get<int>("patrolWaypointIndex", 0);
    bool patrolReverse = bb.Get<bool>("patrolReverse", false);
    
    if (patrolReverse) {
        waypointIndex--;
        if (waypointIndex < 0) {
            waypointIndex = 1;
            patrolReverse = false;  // Reached start, go forward
        }
    } else {
        waypointIndex++;
        if (waypointIndex >= patrolRoute->size()) {
            // Check patrol mode
            std::string patrolMode = bb.Get<std::string>("patrolMode", "loop");
            
            if (patrolMode == "loop") {
                waypointIndex = 0;  // Loop back to start
            } else if (patrolMode == "bounce") {
                waypointIndex = patrolRoute->size() - 2;
                patrolReverse = true;  // Reverse direction
            } else {
                waypointIndex = patrolRoute->size() - 1;  // Stay at end
            }
        }
    }
    
    bb.Set("patrolWaypointIndex", waypointIndex);
    bb.Set("patrolReverse", patrolReverse);
    bb.Set("reachedPatrolWaypoint", false);
    
    return BTNode::Status::SUCCESS;
}

// Action: ContinuePatrol
// Continues moving (placeholder for status)
BTNode::Status Action_ContinuePatrol(Blackboard& bb, float dt) {
    return BTNode::Status::SUCCESS;
}

// Action: PickRandomWanderTarget
// Selects random nearby position to wander to
BTNode::Status Action_PickRandomWanderTarget(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    // Pick random direction and distance
    float angle = random() * 360.0f;
    float distance = 256.0f + random() * 512.0f;  // 256-768 units
    
    Vector direction = Vector(
        cos(DEG2RAD(angle)),
        sin(DEG2RAD(angle)),
        0
    );
    
    Vector wanderTarget = bot->origin + (direction * distance);
    
    // Check if reachable
    if (!bot->PathExists(wanderTarget)) {
        // Try closer position
        wanderTarget = bot->origin + (direction * (distance * 0.5f));
        
        if (!bot->PathExists(wanderTarget)) {
            return BTNode::Status::FAILURE;
        }
    }
    
    bb.Set("wanderTarget", wanderTarget);
    bb.Set("wanderStartTime", level.time);
    
    return BTNode::Status::SUCCESS;
}

// Action: MoveToWanderTarget
// Moves to wander target
BTNode::Status Action_MoveToWanderTarget(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Vector target = bb.Get<Vector>("wanderTarget");
    
    bot->MoveTo(target);
    bot->SetMoveSpeed(0.4f);  // Slow wander
    
    float distance = (target - bot->origin).length();
    if (distance < 64.0f) {
        bb.Set("reachedWanderTarget", true);
        return BTNode::Status::SUCCESS;
    }
    
    // Timeout after 10 seconds
    float startTime = bb.Get<float>("wanderStartTime");
    if (level.time - startTime > 10.0f) {
        return BTNode::Status::FAILURE;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: PauseAfterWander
// Pauses after reaching wander target
BTNode::Status Action_PauseAfterWander(Blackboard& bb, float dt) {
    float pauseTimer = bb.Get<float>("wanderPauseTimer", 0.0f);
    pauseTimer += dt;
    
    // Pause for 2-5 seconds
    float pauseDuration = 2.0f + random() * 3.0f;
    
    if (pauseTimer > pauseDuration) {
        bb.Set("wanderPauseTimer", 0.0f);
        return BTNode::Status::SUCCESS;
    }
    
    bb.Set("wanderPauseTimer", pauseTimer);
    return BTNode::Status::RUNNING;
}

// Action: ClearWanderTarget
// Clears wander state
BTNode::Status Action_ClearWanderTarget(Blackboard& bb, float dt) {
    bb.Set("wanderTarget", Vector::Zero);
    bb.Set("reachedWanderTarget", false);
    
    return BTNode::Status::SUCCESS;
}

// Action: StandInPlace
// Bot stands still
BTNode::Status Action_StandInPlace(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    bot->StopMoving();
    
    return BTNode::Status::SUCCESS;
}

// Action: OccasionalLookAround
// Occasionally looks in random directions
BTNode::Status Action_OccasionalLookAround(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    float lookTimer = bb.Get<float>("idleLookTimer", 0.0f);
    lookTimer += dt;
    
    // Look around every 3-6 seconds
    float lookInterval = 3.0f + random() * 3.0f;
    
    if (lookTimer > lookInterval) {
        // Pick random look direction
        float lookAngle = random() * 360.0f;
        Vector lookDir = AngleVectors(Vector(0, lookAngle, 0));
        bot->SetViewAngles(VectorToAngles(lookDir));
        
        lookTimer = 0.0f;
    }
    
    bb.Set("idleLookTimer", lookTimer);
    
    return BTNode::Status::SUCCESS;
}
```

### Conditions to Implement

```cpp
// code/fgame/bt_conditions_idle.cpp

// Condition: HasCuriousSound
bool Condition_HasCuriousSound(Blackboard& bb) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    for (const auto& sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP ||
            sound.type == AI_EVENT_DOOR ||
            sound.type == AI_EVENT_AMBIENT) {
            return true;
        }
    }
    
    return false;
}

// Condition: HasNearbyAttractiveNode
bool Condition_HasNearbyAttractiveNode(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    PathNode* node = FindNearbyAttractiveNode(bot->origin);
    
    if (node) {
        bb.Set("attractiveNode", node);
        return true;
    }
    
    return false;
}

// Condition: HasPatrolRoute
bool Condition_HasPatrolRoute(Blackboard& bb) {
    std::vector<PathNode*>* patrolRoute = bb.Get<std::vector<PathNode*>*>("patrolRoute", nullptr);
    return patrolRoute && !patrolRoute->empty();
}

// Condition: ReachedCuriousLocation
bool Condition_ReachedCuriousLocation(Blackboard& bb) {
    return bb.Get<bool>("reachedCuriousLocation", false);
}

// Condition: ReachedAttractiveNode
bool Condition_ReachedAttractiveNode(Blackboard& bb) {
    return bb.Get<bool>("reachedAttractiveNode", false);
}

// Condition: ReachedPatrolWaypoint
bool Condition_ReachedPatrolWaypoint(Blackboard& bb) {
    return bb.Get<bool>("reachedPatrolWaypoint", false);
}

// Condition: ShouldWander
bool Condition_ShouldWander(Blackboard& bb) {
    // Wander if no patrol route and not recently wandered
    float lastWanderTime = bb.Get<float>("lastWanderTime", 0.0f);
    return (level.time - lastWanderTime) > 5.0f;
}

// Condition: ReachedWanderTarget
bool Condition_ReachedWanderTarget(Blackboard& bb) {
    return bb.Get<bool>("reachedWanderTarget", false);
}
```

### Helper Functions

```cpp
// code/fgame/idle_helpers.cpp

// Find nearby attractive node
PathNode* FindNearbyAttractiveNode(Vector origin) {
    const float searchRadius = 1024.0f;
    PathNode* bestNode = nullptr;
    float bestScore = 0.0f;
    
    for (auto* node : g_pathNodes) {
        if (node->nodeflags & AI_SNIPER ||
            node->nodeflags & AI_CORNER ||
            node->nodeflags & AI_COVER) {
            
            float distance = (node->origin - origin).length();
            if (distance < searchRadius) {
                // Score based on distance and node type
                float score = 1.0f - (distance / searchRadius);
                
                // Bonus for sniper nodes
                if (node->nodeflags & AI_SNIPER) {
                    score *= 1.5f;
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestNode = node;
                }
            }
        }
    }
    
    return bestNode;
}

// Find target position by name
Vector FindTargetPosition(const std::string& targetName) {
    Entity* target = G_FindTarget(targetName.c_str());
    if (target) {
        return target->origin;
    }
    return Vector::Zero;
}
```

---

## Implementation Steps

### Day 1: Curious Behavior (6 hours)
- [ ] Implement `Action_SetCuriousTarget`
- [ ] Implement `Action_MoveToCuriousLocation`
- [ ] Implement `Action_QuickLookAround`
- [ ] Implement `Action_ClearCuriousState`
- [ ] Implement `Condition_HasCuriousSound`
- [ ] Implement `Condition_ReachedCuriousLocation`
- [ ] **Write unit tests** (3 tests: curious targeting, quick look, sound filtering)

### Day 2: Patrol System (8 hours)
- [ ] Implement `Action_MoveToNextPatrolWaypoint`
- [ ] Implement `Action_PauseAtWaypoint`
- [ ] Implement `Action_AdvanceToNextWaypoint` with loop/bounce modes
- [ ] Implement `Action_ContinuePatrol`
- [ ] Implement `Condition_HasPatrolRoute`
- [ ] Implement `Condition_ReachedPatrolWaypoint`
- [ ] Add patrol route setup in BotController
- [ ] **Write unit tests** (4 tests: waypoint advancement, loop mode, bounce mode, pause timing)

### Day 3: Wander System (6 hours)
- [ ] Implement `Action_PickRandomWanderTarget`
- [ ] Implement `Action_MoveToWanderTarget`
- [ ] Implement `Action_PauseAfterWander`
- [ ] Implement `Action_ClearWanderTarget`
- [ ] Implement `Condition_ShouldWander`
- [ ] Implement `Condition_ReachedWanderTarget`
- [ ] **Write unit tests** (3 tests: target picking, reachability check, pause)

### Day 4: Attractive Nodes & Idle (6 hours)
- [ ] Implement `Action_MoveToAttractiveNode`
- [ ] Implement `Action_UseAttractiveNode`
- [ ] Implement `Action_StandInPlace`
- [ ] Implement `Action_OccasionalLookAround`
- [ ] Implement `Condition_HasNearbyAttractiveNode`
- [ ] Implement `Condition_ReachedAttractiveNode`
- [ ] Add helper: `FindNearbyAttractiveNode`
- [ ] Add helper: `FindTargetPosition`
- [ ] **Write unit tests** (3 tests: node finding, node usage, idle look timing)

### Day 5: YAML Tree & Integration (6 hours)
- [ ] Create `behaviors/idle.btree` with complete tree
- [ ] Register all actions and conditions
- [ ] Create master behavior selector that chooses between combat/investigation/idle
- [ ] Test transitions between behaviors
- [ ] Ensure smooth priority handling

### Day 6-7: Testing & Tuning (10 hours)
- [ ] **Write integration tests** (5 tests):
  - Bot follows patrol route
  - Bot wanders when no patrol
  - Bot investigates curious sound briefly
  - Bot uses attractive nodes
  - Bot transitions from idle to combat smoothly
- [ ] Test on different maps
- [ ] Tune timing (patrol speed, pause duration, wander distance)
- [ ] Test with different profiles
- [ ] Verify transitions to/from combat
- [ ] Fix bugs found during testing

---

## Files to Create/Modify

### New Files
```
code/fgame/bt_actions_idle.cpp           # 16 idle actions
code/fgame/bt_actions_idle.h             # Action declarations
code/fgame/bt_conditions_idle.cpp        # 8 idle conditions
code/fgame/bt_conditions_idle.h          # Condition declarations
code/fgame/idle_helpers.cpp              # Helper functions
code/fgame/idle_helpers.h                # FindNearbyAttractiveNode, etc.
behaviors/idle.btree                     # Idle behavior tree YAML
behaviors/master.btree                   # Master selector (combat/investigate/idle)
tests/test_idle_actions.cpp              # Unit tests
tests/test_idle_conditions.cpp           # Unit tests
tests/integration_test_idle.cpp          # Integration tests
```

### Modified Files
```
code/fgame/playerbot.cpp              # Add master behavior tree
code/fgame/playerbot.h                # Add patrol route member
```

---

## Testing Strategy

### Unit Tests (13 tests)
- 3 curious behavior tests
- 4 patrol tests
- 3 wander tests
- 3 attractive node/idle tests

### Integration Tests (5 tests)
- Full patrol route traversal
- Wander behavior scenario
- Curious investigation scenario
- Attractive node usage
- Idle to combat transition

### Manual Testing
- [ ] Bot follows patrol routes smoothly
- [ ] Bot wanders believably when no patrol
- [ ] Bot investigates minor sounds briefly
- [ ] Bot uses sniper/cover positions
- [ ] Bot transitions to combat immediately when enemy seen
- [ ] Different profiles show different idle behavior

---

## Acceptance Criteria

### Functionality
- [ ] All 16 idle actions implemented and tested
- [ ] All 8 idle conditions implemented and tested
- [ ] Idle tree loads from YAML
- [ ] Patrol system works with loop and bounce modes
- [ ] Wander system creates believable movement
- [ ] Curious investigation works for ambient sounds
- [ ] Attractive node system functional
- [ ] Master behavior tree switches between combat/investigate/idle

### Quality
- [ ] 18 total tests pass (13 unit + 5 integration)
- [ ] Smooth transitions between behaviors
- [ ] No stuck states or repetitive patterns
- [ ] Believable idle behaviors
- [ ] Code follows OpenMoHAA standards

### Performance
- [ ] Idle logic < 0.1ms per bot
- [ ] No performance impact during idle

---

## Success Metrics

### AI Quality
- **Believability:** Bot looks purposeful, not robotic
- **Coverage:** Patrol ensures map coverage
- **Variety:** Wander and attractive nodes add unpredictability
- **Responsiveness:** Immediate transition to combat when needed

### Player Experience
- **Immersion:** Bots feel alive
- **Challenge:** Can't predict bot locations
- **Fairness:** Bot patrols are visible/audible

---

## Notes & Considerations

### Patrol Route Setup
Patrol routes can be defined:
- Map entity chains (pathnodes with target)
- Script commands
- AI waypoint system
- Profile-specific routes

### Wander vs. Patrol
- **Patrol:** Predictable, covers objectives
- **Wander:** Unpredictable, fills empty time
- Use patrol when defined, fall back to wander

### Curious vs. Investigation
- **Curious:** Minor sounds, brief check (5 seconds)
- **Investigation:** Weapon fire, thorough search (15 seconds)
- Curious doesn't interrupt patrol as much

### Attractive Node Priority
Attractive nodes should:
- Not trap bots indefinitely
- Timeout after 10-15 seconds
- Be abandoned if threats appear
- Favor tactical positions (sniper, cover)

### Master Behavior Selector
```yaml
# behaviors/master.btree
tree:
  type: selector
  children:
    - include: "combat.btree"        # Priority 1
    - include: "investigation.btree" # Priority 2
    - include: "idle.btree"          # Priority 3 (fallback)
```

---

## Troubleshooting

### Bot Gets Stuck in Patrol
- Check waypoint reachability
- Verify path exists between waypoints
- Add timeout and skip unreachable waypoints

### Wander Doesn't Look Natural
- Increase pause duration between wanders
- Add more randomness to distance/direction
- Vary movement speed

### Bot Ignores Combat
- Check selector priority (combat should be first)
- Verify HasVisibleEnemy condition works
- Test transition triggers

### Curious Behavior Too Frequent
- Filter sound types more strictly
- Add cooldown between curious investigations
- Check sound priority values

---

## Dependencies

### Requires (from Previous Phases)
- Perception system with audio detection
- BT framework
- Combat and investigation trees
- Pathfinding system

### Provides (for Future Tasks)
- Complete bot behavior coverage
- Master behavior selector pattern
- Idle action library

---

**Next Task:** Task 3.4 - Utility AI (action scoring with consideration curves)
