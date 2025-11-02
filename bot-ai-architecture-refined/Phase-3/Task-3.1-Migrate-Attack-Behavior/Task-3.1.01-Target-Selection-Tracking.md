# Task 3.1a: Target Selection & Tracking System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 2 days  
**Priority**: HIGH (Foundation for all combat)  
**Dependencies**: PerceptionSystem (Phase 2A), Blackboard (Phase 2B)

---

## Overview

Migrate the target selection and tracking logic from `CheckCondition_Attack()` to behavior tree actions and conditions. This provides the foundation for all combat behaviors by determining which enemy the bot should engage.

### What This Task Achieves

- **Enemy Scanning**: Evaluates all visible enemies to find best target
- **Target Stickiness**: Prevents rapid target switching (maintains target lock)
- **Target Switching**: Allows switching to better targets after delay
- **Target Validation**: Ensures target is alive, visible, and attackable
- **Memory Integration**: Updates enemy memory system with target data

### Why This Matters

Target selection is the **critical first step** in combat AI. Without reliable target selection:
- Bots won't know who to shoot
- Aiming system has no target
- Movement system can't position relative to enemy
- All other combat behaviors fail

---

## Current Implementation (State Machine)

### File: `code/fgame/playerbot_attack.cpp` (Lines 107-237)

```cpp
bool BotController::CheckCondition_Attack(void)
{
    Container<Sentient *> sents       = SentientList;
    float                 maxDistance = 0;

    bot_origin = controlledEnt->origin;
    sents.Sort(sentients_compare);  // Sort by distance

    maxDistance = Q_min(world->m_fAIVisionDistance, world->farplane_distance * BotConstants::FARPLANE_VISION_FACTOR);

    // Scan for enemies (even if we already have one, we might want to switch)
    float     bestDistanceSq = BotConstants::LARGE_DISTANCE_SQ;
    Sentient *bestEnemy      = SelectBestTarget(maxDistance, bestDistanceSq);

    // If we found a visible enemy, target it
    if (bestEnemy) {
        bool shouldSwitch = false;

        if (m_pEnemy != bestEnemy) {
            // Trying to switch to a different target - check time lock
            if (!m_pEnemy) {
                // No current target, always allow
                shouldSwitch = true;
            } else if (!IsValidEnemy(m_pEnemy)) {
                // Current target became invalid (dead, hidden, etc), always allow switch
                shouldSwitch = true;
            } else {
                // Check if enough time has passed since target lock
                float timeSinceLock = (level.svsTime - m_iTargetLockTime);
                float minLockTime   = g_bot_target_lock_time->value;

                if (timeSinceLock >= minLockTime) {
                    shouldSwitch = true;
                } else {
                    // Time lock still active, stick with current target if still visible
                    shouldSwitch = false;

                    // Keep current target if it's still in the visible list
                    for (int i = 1; i <= SentientList.NumObjects(); i++) {
                        Sentient *sent = SentientList.ObjectAt(i);
                        if (sent == m_pEnemy
                            && controlledEnt->CanSee(sent, BotConstants::DEFAULT_FOV_DEGREES, maxDistance, false)) {
                            bestEnemy      = m_pEnemy;
                            bestDistanceSq = (m_pEnemy->origin - controlledEnt->origin).lengthSquared();
                            break;
                        }
                    }
                }
            }

            if (shouldSwitch) {
                m_iEnemyEyesTag = -1;
                // Debug output for target switching...
            }
        } else {
            // Same target, no switch needed
            shouldSwitch = false;
        }

        // Acquire new target or refresh lock time
        if (!m_pEnemy || shouldSwitch) {
            if (!m_pEnemy) {
                m_iLastUnseenTime = level.inttime;
                // Debug output for initial target acquisition...
            }

            m_pEnemy          = bestEnemy;
            m_iTargetLockTime = level.svsTime;
        }

        m_vLastEnemyPos = m_pEnemy->origin;

        // Update enemy memory
        memoryState.enemyMemory.enemy             = bestEnemy;
        memoryState.enemyMemory.lastKnownPosition = bestEnemy->origin;
        memoryState.enemyMemory.lastKnownVelocity = bestEnemy->velocity;
        memoryState.enemyMemory.lastSeenTime      = level.svsTime;
        memoryState.enemyMemory.confidenceLevel   = 1.0f;

        m_iAttackTime = level.inttime + BotConstants::ATTACK_REACQUIRE_DELAY;
        return true;
    }

    // No visible enemies found
    if (level.inttime > m_iAttackTime) {
        if (m_iAttackTime) {
            movement.ClearMove();
            m_iAttackTime = 0;
        }

        return false;
    }

    return true;
}

// Helper: Select best target from visible enemies
Sentient *BotController::SelectBestTarget(float maxDistance, float& outDistanceSq)
{
    Sentient *bestEnemy      = NULL;
    float     bestDistanceSq = 999999.0f;

    for (int i = 1; i <= SentientList.NumObjects(); i++) {
        Sentient *sent = SentientList.ObjectAt(i);

        if (!IsValidEnemy(sent)) {
            continue;
        }

        // Increased FOV from 80 to 100 degrees for better peripheral vision
        if (controlledEnt->CanSee(sent, 100, maxDistance, false)) {
            float distSq = (sent->origin - controlledEnt->origin).lengthSquared();

            // Target stickiness: prefer current target unless new target is significantly closer
            if (!bestEnemy) {
                bestEnemy      = sent;
                bestDistanceSq = distSq;
            } else {
                // Check if new enemy is significantly closer
                float switchThreshold   = g_bot_target_switch_threshold->value;
                float switchThresholdSq = switchThreshold * switchThreshold;
                float distAdvantage = bestDistanceSq - distSq;

                if (distAdvantage > switchThresholdSq || !m_pEnemy) {
                    bestEnemy      = sent;
                    bestDistanceSq = distSq;
                }
            }
        }
    }

    outDistanceSq = bestDistanceSq;
    return bestEnemy;
}

// Validation: Check if enemy is attackable
bool BotController::IsValidEnemy(Sentient *sent) const
{
    if (sent == controlledEnt) {
        return false;
    }

    if (sent->hidden() || (sent->flags & FL_NOTARGET)) {
        return false;  // Ignore hidden / non-target enemies
    }

    if (sent->IsDead()) {
        return false;  // Ignore dead enemies
    }

    if (sent->getSolidType() == SOLID_NOT) {
        return false;  // Ignore non-solid, like spectators
    }

    if (sent->IsSubclassOfPlayer()) {
        Player *player = static_cast<Player *>(sent);
        if (g_gametype->integer >= GT_TEAM && player->GetTeam() == controlledEnt->GetTeam()) {
            return false;  // Same team
        }
    } else {
        if (sent->m_Team == controlledEnt->m_Team) {
            return false;  // Same team
        }
    }

    return true;
}
```

### Key Configuration (CVars)

```cpp
// From game code
extern cvar_t *g_bot_target_switch_threshold;  // Default: 128.0 units (distance advantage needed to switch)
extern cvar_t *g_bot_target_lock_time;         // Default: 2000 ms (time to lock onto target)
```

---

## Behavior Tree Design

### Actions

#### `SelectTarget`
**Purpose**: Scans visible enemies and selects best target based on distance and stickiness  
**Returns**: 
- `SUCCESS` if target selected
- `FAILURE` if no valid enemies visible

**Blackboard Writes**:
- `SELECTED_TARGET` (Sentient*) - The chosen target
- `TARGET_DISTANCE` (float) - Distance to target
- `TARGET_LOCK_TIME` (float) - When target was locked
- `TARGET_SWITCHED` (bool) - Whether we switched from previous target

**Pseudocode**:
```
1. Get perception snapshot from blackboard
2. If no visible enemies → FAILURE
3. Get current target from blackboard (if any)
4. Evaluate all visible enemies:
   - Validate (IsValidEnemy check)
   - Calculate distance
   - Apply target stickiness (prefer closer OR current target)
5. Check target switching rules:
   - No current target → always allow
   - Current target invalid → always allow
   - Lock time expired AND new target significantly closer → allow
   - Otherwise keep current target
6. Update blackboard with selected target
7. Update memory system
8. Return SUCCESS
```

### Conditions

#### `HasValidTarget`
**Purpose**: Checks if current target still exists and is attackable  
**Returns**: 
- `true` if `SELECTED_TARGET` exists, is alive, and passes IsValidEnemy checks
- `false` otherwise

**Blackboard Reads**: `SELECTED_TARGET`

#### `TargetVisible`
**Purpose**: Checks if current target is currently visible (in perception snapshot)  
**Returns**:
- `true` if target is in `perception->visibleEnemies`
- `false` otherwise

**Blackboard Reads**: `SELECTED_TARGET`, `PERCEPTION`

---

## Implementation Details

### New Blackboard Keys

Add to `code/fgame/bt_blackboard_keys.h`:

```cpp
namespace BlackboardKeys
{
    // ... existing keys ...
    
    // Target selection (Task 3.1a)
    constexpr const char* SELECTED_TARGET = "selectedTarget";      // Sentient* - Current attack target
    constexpr const char* TARGET_DISTANCE = "targetDistance";      // float - Distance to target
    constexpr const char* TARGET_LOCK_TIME = "targetLockTime";     // float - When target was locked (level.svsTime)
    constexpr const char* TARGET_SWITCHED = "targetSwitched";      // bool - Whether we switched targets this frame
    constexpr const char* PREVIOUS_TARGET = "previousTarget";      // Sentient* - Previous target (for comparison)
}
```

### Profile Parameters

Add to profile YAML (`profiles/*.yaml`):

```yaml
combat:
  # ... existing combat params ...
  target_lock_time: 2.0        # seconds to lock onto target before considering switch
  target_switch_threshold: 128.0  # distance advantage (units) needed to switch targets
```

Add accessors to `code/fgame/bot_profile.h`:

```cpp
float GetTargetLockTime() const { return combat.targetLockTime; }
float GetTargetSwitchThreshold() const { return combat.targetSwitchThreshold; }
```

### Helper Functions

Create `code/fgame/bt_combat_helpers.h`:

```cpp
namespace BT::Combat
{
    // Validate if sentient is attackable enemy
    bool IsValidEnemy(const Player* bot, Sentient* enemy);
    
    // Calculate target stickiness score (higher = better target)
    // Factors: distance, whether it's current target, lock time
    float CalculateTargetScore(
        Sentient* enemy,
        Sentient* currentTarget,
        float distance,
        float lockTime,
        float lockDuration,
        float switchThreshold
    );
    
    // Find closest visible enemy from perception
    const EnemyInfo* FindClosestVisibleEnemy(const PerceptionSnapshot* perception);
}
```

---

## Files to Create/Modify

### New Files

```
code/fgame/bt_actions_target.cpp       # SelectTarget action implementation
code/fgame/bt_actions_target.h         # Action declarations
code/fgame/bt_conditions_target.cpp    # HasValidTarget, TargetVisible conditions
code/fgame/bt_conditions_target.h      # Condition declarations
code/fgame/bt_combat_helpers.cpp       # IsValidEnemy, CalculateTargetScore helpers
code/fgame/bt_combat_helpers.h         # Helper declarations
tests/test_target_selection.cpp        # Unit tests
```

### Modified Files

```
code/fgame/bt_blackboard_keys.h        # Add target-related keys
code/fgame/bot_profile.h               # Add target selection parameters
profiles/*.yaml                        # Add target_lock_time, target_switch_threshold
code/fgame/CMakeLists.txt              # Add new source files
tests/CMakeLists.txt                   # Add test file
```

---

## Implementation Steps

### Day 1: Core Target Selection (8 hours)

#### Morning (4 hours)
- [ ] Create `bt_combat_helpers.h` and `bt_combat_helpers.cpp`
- [ ] Implement `IsValidEnemy()` helper (migrate from playerbot_attack.cpp)
- [ ] Implement `CalculateTargetScore()` for target stickiness
- [ ] Implement `FindClosestVisibleEnemy()` helper
- [ ] Add blackboard keys to `bt_blackboard_keys.h`
- [ ] Add profile parameters to `bot_profile.h`

#### Afternoon (4 hours)
- [ ] Create `bt_actions_target.cpp` and `bt_actions_target.h`
- [ ] Implement `Action_SelectTarget`:
  - Get perception snapshot
  - Iterate visible enemies
  - Validate each with IsValidEnemy
  - Calculate scores with CalculateTargetScore
  - Apply target switching rules (lock time, threshold)
  - Update blackboard
  - Return SUCCESS/FAILURE
- [ ] Add debug logging for target selection

### Day 2: Conditions & Testing (8 hours)

#### Morning (3 hours)
- [ ] Create `bt_conditions_target.cpp` and `bt_conditions_target.h`
- [ ] Implement `Condition_HasValidTarget`:
  - Read SELECTED_TARGET from blackboard
  - Validate with IsValidEnemy
  - Check entity still in use (edict->inuse)
  - Return true/false
- [ ] Implement `Condition_TargetVisible`:
  - Read SELECTED_TARGET and PERCEPTION
  - Check if target in visibleEnemies list
  - Return true/false

#### Afternoon (5 hours)
- [ ] Create `tests/test_target_selection.cpp`
- [ ] Write unit tests:
  - **Test 1**: `SelectTarget_NoEnemies_ReturnsFailure`
  - **Test 2**: `SelectTarget_OneEnemy_SelectsIt`
  - **Test 3**: `SelectTarget_MultipleEnemies_SelectsClosest`
  - **Test 4**: `SelectTarget_TargetStickiness_KeepsCurrentTarget`
  - **Test 5**: `SelectTarget_LockTimeExpired_AllowsSwitch`
  - **Test 6**: `SelectTarget_NewTargetMuchCloser_Switches`
  - **Test 7**: `IsValidEnemy_DeadEnemy_ReturnsFalse`
  - **Test 8**: `IsValidEnemy_SameTeam_ReturnsFalse`
- [ ] Run tests: `ctest -R test_target_selection`
- [ ] Fix any test failures
- [ ] Update profile YAMLs with target parameters

---

## Testing Strategy

### Unit Tests (8 tests)

Use GoogleTest framework with mock entities:

```cpp
TEST(TargetSelectionTest, SelectTarget_NoEnemies_ReturnsFailure)
{
    // Setup
    Blackboard bb;
    MockPerceptionSnapshot perception;
    perception.visibleEnemies.clear();  // No enemies
    bb.Set(BlackboardKeys::PERCEPTION, &perception);
    
    // Execute
    BTNode::Status result = Action_SelectTarget(bb, 0.1f);
    
    // Verify
    EXPECT_EQ(result, BTNode::Status::FAILURE);
    EXPECT_FALSE(bb.Has(BlackboardKeys::SELECTED_TARGET));
}

TEST(TargetSelectionTest, SelectTarget_TargetStickiness_KeepsCurrentTarget)
{
    // Setup
    Blackboard bb;
    MockPerceptionSnapshot perception;
    
    // Current target at 500 units
    MockSentient currentEnemy(Vector(500, 0, 0));
    bb.Set(BlackboardKeys::SELECTED_TARGET, &currentEnemy);
    bb.Set(BlackboardKeys::TARGET_LOCK_TIME, level.svsTime);
    
    // New enemy at 400 units (closer but not by threshold)
    MockSentient newEnemy(Vector(400, 0, 0));
    perception.visibleEnemies.push_back({&newEnemy, newEnemy.origin, 400.0f, ...});
    perception.visibleEnemies.push_back({&currentEnemy, currentEnemy.origin, 500.0f, ...});
    bb.Set(BlackboardKeys::PERCEPTION, &perception);
    
    // Execute
    BTNode::Status result = Action_SelectTarget(bb, 0.1f);
    
    // Verify - should keep current target (100 unit advantage < 128 threshold)
    EXPECT_EQ(result, BTNode::Status::SUCCESS);
    Sentient* selected = bb.Get<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    EXPECT_EQ(selected, &currentEnemy);
    EXPECT_FALSE(bb.Get<bool>(BlackboardKeys::TARGET_SWITCHED));
}

TEST(TargetSelectionTest, SelectTarget_LockTimeExpired_AllowsSwitch)
{
    // Setup
    Blackboard bb;
    MockPerceptionSnapshot perception;
    MockProfile profile;
    profile.combat.targetLockTime = 2.0f;  // 2 second lock
    bb.Set(BlackboardKeys::PROFILE, &profile);
    
    // Current target at 500 units, locked 3 seconds ago
    MockSentient currentEnemy(Vector(500, 0, 0));
    bb.Set(BlackboardKeys::SELECTED_TARGET, &currentEnemy);
    bb.Set(BlackboardKeys::TARGET_LOCK_TIME, level.svsTime - 3000);
    
    // New enemy at 450 units (closer)
    MockSentient newEnemy(Vector(450, 0, 0));
    perception.visibleEnemies.push_back({&newEnemy, newEnemy.origin, 450.0f, ...});
    bb.Set(BlackboardKeys::PERCEPTION, &perception);
    
    // Execute
    BTNode::Status result = Action_SelectTarget(bb, 0.1f);
    
    // Verify - should switch because lock time expired
    EXPECT_EQ(result, BTNode::Status::SUCCESS);
    Sentient* selected = bb.Get<Sentient*>(BlackboardKeys::SELECTED_TARGET);
    EXPECT_EQ(selected, &newEnemy);
    EXPECT_TRUE(bb.Get<bool>(BlackboardKeys::TARGET_SWITCHED));
}
```

### Integration with Existing Systems

**Perception System**: SelectTarget reads from `PerceptionSnapshot->visibleEnemies`

**Memory System**: SelectTarget updates `memoryState.enemyMemory` when target selected

**Profile System**: Uses `profile->GetTargetLockTime()` and `profile->GetTargetSwitchThreshold()`

---

## Acceptance Criteria

### Functionality
- [ ] SelectTarget action implemented and working
- [ ] Target stickiness prevents rapid switching
- [ ] Target switching works after lock time expires
- [ ] Target switching works when new target significantly closer
- [ ] Invalid enemies (dead, same team, hidden) are filtered
- [ ] HasValidTarget condition correctly validates targets
- [ ] TargetVisible condition checks perception snapshot
- [ ] Memory system updated with target information

### Code Quality
- [ ] All 8 unit tests pass
- [ ] Code follows OpenMoHAA standards (naming, formatting)
- [ ] Functions < 50 lines each
- [ ] Proper error handling (null checks, validation)
- [ ] Debug logging for target selection events

### Performance
- [ ] Target selection < 0.1ms per bot
- [ ] No redundant calculations (cache distance squared)

---

## Debug & Troubleshooting

### Debug Commands

Add to `code/fgame/playerbot.cpp`:

```cpp
// Print current target info
void BotController::DebugPrintTargetInfo() const
{
    if (blackboard.Has(BlackboardKeys::SELECTED_TARGET)) {
        Sentient* target = blackboard.Get<Sentient*>(BlackboardKeys::SELECTED_TARGET);
        float distance = blackboard.Get<float>(BlackboardKeys::TARGET_DISTANCE);
        float lockTime = blackboard.Get<float>(BlackboardKeys::TARGET_LOCK_TIME);
        
        gi.Printf("[BOT] %s target: %s at %.0f units (locked %.1fs ago)\n",
            controlledEnt->client->pers.netname,
            target->targetname.c_str(),
            distance,
            (level.svsTime - lockTime) / 1000.0f
        );
    } else {
        gi.Printf("[BOT] %s has no target\n", controlledEnt->client->pers.netname);
    }
}
```

### Common Issues

**Issue**: Bot switches targets too frequently  
**Fix**: Increase `target_lock_time` in profile or increase `target_switch_threshold`

**Issue**: Bot never switches targets  
**Fix**: Decrease `target_lock_time` or decrease `target_switch_threshold`

**Issue**: Bot targets dead enemies  
**Fix**: Check IsValidEnemy includes `IsDead()` check

**Issue**: Bot targets teammates  
**Fix**: Check IsValidEnemy includes team check

---

## Dependencies for Next Tasks

**Task 3.1b** (Aiming & Fire Control) requires:
- `SELECTED_TARGET` blackboard key populated
- `HasValidTarget` condition working
- `TargetVisible` condition working

**Task 3.1c** (Combat Movement) requires:
- `SELECTED_TARGET` for distance calculations
- `TARGET_DISTANCE` for range checks

---

## References

### Existing Code
- `code/fgame/playerbot_attack.cpp` (lines 107-303) - Original target selection
- `code/fgame/perception.h` - PerceptionSnapshot structure
- `code/fgame/sentient.h` - Sentient base class

### Related Systems
- **PerceptionSystem** (Phase 2A) - Provides visible enemy list
- **MemorySystem** (Phase 2A) - Stores last-known enemy positions
- **BotProfile** (Phase 2B) - Provides target selection parameters

### Constants
```cpp
// From BotConstants namespace
DEFAULT_FOV_DEGREES = 100.0f      // FOV for visibility checks
LARGE_DISTANCE_SQ = 999999.0f     // Initial "worst case" distance
```
