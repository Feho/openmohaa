# Task 3.1g: Grenade System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 1 day  
**Priority**: LOW  
**Dependencies**: Task 3.1f (Core Combat)

---

## Overview

Implement grenade throwing logic with cluster detection, cooldown management, and ally safety checks. Adds tactical grenade usage to flush enemies from cover or attack clusters.

### What This Achieves

- **Cluster Detection**: Identifies when multiple enemies are grouped
- **Grenade Throwing**: Calculates trajectory and throws grenade
- **Cooldown Management**: Prevents grenade spam (10 second cooldown)
- **Ally Safety**: Ensures no friendlies in blast radius
- **Profile Integration**: Grenade frequency based on personality

---

## Design Specifications

### Grenade Usage Rules

1. **Has Grenades**: Bot must have grenade inventory
2. **Multiple Enemies**: At least 2 enemies visible
3. **Clustered**: Enemies within 256 units of each other
4. **No Allies Nearby**: No friendlies within 384 units of blast center
5. **Cooldown Expired**: At least 10 seconds since last grenade
6. **Profile Check**: Random chance based on `grenade_frequency` parameter

### Cluster Detection Algorithm

```cpp
bool AreEnemiesClustered(const std::vector<EnemyInfo>& enemies, float maxRadius)
{
    if (enemies.size() < 2) return false;
    
    // Find center point of all enemies
    Vector centerPos = Vector::Zero;
    for (const auto& enemy : enemies) {
        centerPos += enemy.position;
    }
    centerPos /= enemies.size();
    
    // Check if all enemies within maxRadius of center
    for (const auto& enemy : enemies) {
        float distance = (enemy.position - centerPos).length();
        if (distance > maxRadius) {
            return false;  // One enemy too far
        }
    }
    
    return true;
}

Vector CalculateClusterCenter(const std::vector<EnemyInfo>& enemies)
{
    Vector center = Vector::Zero;
    for (const auto& enemy : enemies) {
        center += enemy.position;
    }
    return center / enemies.size();
}
```

### Ally Safety Check

```cpp
bool HasAlliesNearPosition(Vector position, float safetyRadius, PerceptionSnapshot* perception)
{
    for (const auto& ally : perception->visibleAllies) {
        float distance = (ally.position - position).length();
        if (distance < safetyRadius) {
            return true;  // Ally too close
        }
    }
    return false;
}
```

---

## Behavior Tree Design

### Actions

#### `ThrowGrenade`
- **Purpose**: Throws grenade at calculated position
- **Returns**: SUCCESS when grenade thrown, FAILURE if can't throw
- **Blackboard**: Reads `GRENADE_TARGET_POSITION`, writes `LAST_GRENADE_TIME`

#### `CalculateGrenadeTarget`
- **Purpose**: Determines optimal grenade throw position
- **Returns**: SUCCESS if target calculated, FAILURE if none
- **Blackboard**: Reads `PERCEPTION`, writes `GRENADE_TARGET_POSITION`

### Conditions

#### `HasGrenades`
- **Returns**: true if bot has grenade inventory

#### `ShouldThrowGrenade`
- **Returns**: true if all criteria met (cluster, cooldown, allies safe, profile)

#### `EnemiesClustered`
- **Returns**: true if 2+ enemies within 256 units

---

## Implementation Steps

### Morning (4 hours)
- [ ] Implement `AreEnemiesClustered()` helper
- [ ] Implement `CalculateClusterCenter()` helper
- [ ] Implement `HasAlliesNearPosition()` helper
- [ ] Implement `Condition_HasGrenades`
- [ ] Implement `Condition_EnemiesClustered`

### Afternoon (4 hours)
- [ ] Implement `Condition_ShouldThrowGrenade` (combines all checks)
- [ ] Implement `Action_CalculateGrenadeTarget`
- [ ] Implement `Action_ThrowGrenade`
- [ ] Add player methods: `HasGrenades()`, `ThrowGrenade(Vector target)`
- [ ] Write 4 unit tests
- [ ] Add profile parameter: `grenade_frequency`

---

## Testing Strategy

### Unit Tests (4 tests)

1. **AreEnemiesClustered_TwoClose**: TRUE for 2 enemies at 200 units
2. **AreEnemiesClustered_TwoFar**: FALSE for 2 enemies at 300 units
3. **HasAlliesNearPosition**: TRUE when ally within safety radius
4. **ShouldThrowGrenade_Cooldown**: FALSE when last grenade < 10 seconds ago

---

## Files to Create/Modify

```
code/fgame/bt_actions_grenade.cpp
code/fgame/bt_actions_grenade.h
code/fgame/bt_conditions_grenade.cpp
code/fgame/bt_conditions_grenade.h
code/fgame/bt_combat_helpers.cpp      # Add cluster detection helpers
tests/test_grenade_system.cpp
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    constexpr const char* GRENADE_TARGET_POSITION = "grenadeTargetPosition"; // Vector
    constexpr const char* LAST_GRENADE_TIME = "lastGrenadeTime";             // float
}
```

---

## Profile Parameters

```yaml
tactics:
  grenade_frequency: 0.4       # 0.0 (never) - 1.0 (always when possible)
```

---

## Constants

```cpp
namespace BotConstants
{
    constexpr float GRENADE_CLUSTER_RADIUS = 256.0f;  // Max distance for cluster
    constexpr float GRENADE_ALLY_SAFETY = 384.0f;     // Min ally distance
    constexpr float GRENADE_COOLDOWN = 10.0f;         // Seconds between grenades
}
```

---

## Integration with Combat Tree

Add to `behaviors/combat.yaml` before main engagement:

```yaml
# Consider grenade throw
- type: selector
  children:
    - type: sequence
      name: "Throw Grenade At Cluster"
      children:
        - type: condition
          check: "HasGrenades"
        - type: condition
          check: "ShouldThrowGrenade"
        - type: action
          action: "CalculateGrenadeTarget"
        - type: action
          action: "ThrowGrenade"
    - type: action
      action: "DoNothing"  # Skip grenade
```

---

## Player Interface

Add to `code/fgame/player.h`:

```cpp
class Player : public Sentient
{
    // ...
    
    // Grenade interface
    bool HasGrenades() const;
    void ThrowGrenade(const Vector& target);
    int GetGrenadeCount() const;
};
```
