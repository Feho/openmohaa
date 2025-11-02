# Task 3.1c: Combat Movement System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 2 days  
**Priority**: HIGH  
**Dependencies**: Task 3.1a (Target Selection), Task 3.1b (Weapon State)

---

## Overview

Migrate distance-based combat movement to behavior trees. This includes approaching enemies when too far, retreating when too close, and maintaining optimal combat distance with strafing.

### What This Achieves

- **Range Management**: Automatically positions bot at optimal weapon range
- **Approach Logic**: Moves toward distant enemies
- **Retreat Logic**: Backs away from close enemies
- **Strafe Maintenance**: Strafes to maintain position and be harder to hit
- **Weapon Integration**: Uses weapon min/max range for decisions

---

## Current Implementation

### File: `code/fgame/playerbot_attack.cpp` - Lines 591-625

```cpp
void BotController::UpdateAttackMovement(bool noMove, bool melee, bool canSee, float minDistanceSq)
{
    float fEnemyDistanceSquared = (controlledEnt->origin - m_vLastEnemyPos).lengthSquared();

    if ((!movement.MoveToBestAttractivePoint(5) && !movement.IsMoving())
        || (m_vOldEnemyPos != m_vLastEnemyPos && !movement.MoveDone()) 
        || fEnemyDistanceSquared < minDistanceSq) {
        
        if (!melee || !canSee) {
            if (fEnemyDistanceSquared < minDistanceSq) {
                // Too close - back away
                Vector vDir = controlledEnt->origin - m_vLastEnemyPos;
                VectorNormalizeFast(vDir);

                float fMinDistance = sqrt(minDistanceSq);
                movement.AvoidPath(m_vLastEnemyPos, fMinDistance, 
                                  Vector(controlledEnt->orientation[1]) * 512);
            } else {
                // Too far - move closer
                movement.MoveTo(m_vLastEnemyPos);
            }

            if (!canSee && movement.MoveDone()) {
                // Lost track of the enemy
                ClearEnemy();
                return;
            }
        } else {
            // Melee mode - always approach
            movement.MoveTo(m_vLastEnemyPos);
        }
    }

    if (movement.IsMoving()) {
        m_iAttackTime = level.inttime + BotConstants::ATTACK_REACQUIRE_DELAY;
    }
}
```

### Weapon Range Calculation - Lines 488-590

```cpp
// From ExecuteFiring()
float fPrimaryBulletRange = weapon->GetBulletRange(FIRE_PRIMARY) / ATTACK_RANGE_DIVISOR;
float fPrimaryBulletRangeSquared = fPrimaryBulletRange * fPrimaryBulletRange;

// Minimum attack distance based on weapon range
float fMinDistance = fPrimaryBulletRange;
if (fMinDistance > MAX_MIN_ATTACK_DISTANCE) {
    fMinDistance = MAX_MIN_ATTACK_DISTANCE;
}
```

---

## Behavior Tree Design

### Actions

#### `ApproachEnemy`
- **Purpose**: Moves toward target to get in weapon range
- **Returns**: RUNNING (moving), SUCCESS (in range)
- **Blackboard**: Reads `SELECTED_TARGET`, `TARGET_DISTANCE`, writes `MOVING_TO_POSITION`

#### `RetreatFromEnemy`
- **Purpose**: Backs away from target (too close for weapon)
- **Returns**: RUNNING (retreating), SUCCESS (at safe distance)
- **Blackboard**: Reads `SELECTED_TARGET`, `TARGET_DISTANCE`

#### `MaintainDistance`
- **Purpose**: Strafes perpendicular to enemy to maintain range
- **Returns**: SUCCESS (continuous action)
- **Blackboard**: Reads `SELECTED_TARGET`, writes `STRAFE_DIRECTION`, `STRAFE_TIMER`

### Conditions

#### `EnemyTooClose`
- **Returns**: true if `TARGET_DISTANCE < weapon->GetMinRange()`

#### `EnemyTooFar`
- **Returns**: true if `TARGET_DISTANCE > weapon->GetMaxRange()`

#### `InOptimalRange`
- **Returns**: true if distance between min and max range

---

## Implementation Steps

### Day 1: Range Actions (8 hours)

#### Morning (4 hours)
- [ ] Add weapon range accessors: `GetMinRange()`, `GetMaxRange()` to `weapon.h`
- [ ] Implement `Action_ApproachEnemy`:
  - Calculate direction to target
  - Add path deviation for unpredictability (profile param)
  - Use `movement.MoveTo()`
  - Check if in range
- [ ] Implement `Action_RetreatFromEnemy`:
  - Calculate direction away from target
  - Use `movement.AvoidPath()` with min distance
  - Check if at safe distance

#### Afternoon (4 hours)
- [ ] Implement `Action_MaintainDistance`:
  - Calculate perpendicular strafe vector
  - Alternate direction every 2 seconds
  - Use partial movement speed (profile param)
- [ ] Add profile parameters: `path_deviation`, `strafe_usage`, `speed_preference`
- [ ] Write 3 unit tests

### Day 2: Conditions & Integration (8 hours)

#### Morning (3 hours)
- [ ] Implement `Condition_EnemyTooClose`
- [ ] Implement `Condition_EnemyTooFar`
- [ ] Implement `Condition_InOptimalRange`
- [ ] Add blackboard keys

#### Afternoon (5 hours)
- [ ] Write 3 more unit tests (conditions)
- [ ] Integration test with 3.1a and 3.1b
- [ ] Test with different weapon types (pistol, rifle, shotgun)
- [ ] Profile tuning (test aggressive vs defensive movement)

---

## Testing Strategy

### Unit Tests (6 tests)

1. **ApproachEnemy**: Bot moves toward distant enemy
2. **ApproachEnemy_InRange**: SUCCESS when in range
3. **RetreatFromEnemy**: Bot backs away from close enemy
4. **RetreatFromEnemy_SafeDistance**: SUCCESS at safe distance
5. **MaintainDistance**: Bot strafes perpendicular
6. **MaintainDistance_AlternateDirection**: Changes direction periodically

---

## Acceptance Criteria

### Functionality
- [ ] Bot approaches enemies outside weapon range
- [ ] Bot retreats from enemies inside minimum range
- [ ] Bot strafes at optimal range
- [ ] Movement speed matches profile
- [ ] Different weapons use appropriate ranges

### Code Quality
- [ ] 6 unit tests pass
- [ ] Functions < 50 lines
- [ ] Proper integration with movement system

### Performance
- [ ] Movement decisions < 0.05ms per bot

---

## Files to Create/Modify

### New Files
```
code/fgame/bt_actions_movement.cpp
code/fgame/bt_actions_movement.h
code/fgame/bt_conditions_range.cpp
code/fgame/bt_conditions_range.h
tests/test_combat_movement.cpp
```

### Modified Files
```
code/fgame/weapon.h                  # Add GetMinRange(), GetMaxRange()
code/fgame/bt_blackboard_keys.h     # Add movement keys
code/fgame/bot_profile.h            # Add movement parameters
profiles/*.yaml                     # Add movement params
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    // Combat Movement (Task 3.1c)
    constexpr const char* MOVING_TO_POSITION = "movingToPosition";   // Vector - Target position
    constexpr const char* STRAFE_DIRECTION = "strafeDirection";      // int - 1=right, -1=left
    constexpr const char* STRAFE_TIMER = "strafeTimer";              // float - Time in current direction
    constexpr const char* OPTIMAL_RANGE = "optimalRange";            // float - Weapon's optimal range
}
```

---

## Profile Parameters

```yaml
movement:
  speed_preference: 1.0          # 0.5 (cautious) - 1.0 (aggressive)
  path_deviation: 0.3            # 0.0 (straight) - 1.0 (unpredictable)
  strafe_usage: 0.7              # 0.0 (never) - 1.0 (always)

tactics:
  preferred_range_factor: 0.7    # 0.7 = stay at 70% of max weapon range
```

---

## Weapon Range Configuration

Different weapons need appropriate ranges:

```cpp
// In weapon definitions
Weapon::Pistol:
  minRange = 64.0f
  maxRange = 768.0f
  
Weapon::Rifle:
  minRange = 128.0f
  maxRange = 2048.0f
  
Weapon::Shotgun:
  minRange = 32.0f
  maxRange = 384.0f
  
Weapon::Sniper:
  minRange = 256.0f
  maxRange = 4096.0f
```
