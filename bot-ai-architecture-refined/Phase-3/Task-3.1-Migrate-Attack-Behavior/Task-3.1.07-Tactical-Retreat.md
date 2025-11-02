# Task 3.1e: Tactical Combat & Retreat System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 2 days  
**Priority**: MEDIUM  
**Dependencies**: Task 3.1f (Core Combat), Task 3.1d (Cover System)

---

## Overview

Migrate tactical combat decision-making and retreat logic. This adds intelligent self-preservation behaviors based on health, damage, and tactical situation.

### What This Achieves

- **Retreat Logic**: Bot retreats when health critical or outnumbered
- **Safe Reload**: Reloads behind cover or when not under fire
- **Suppression Fire**: Fires at last-known position to suppress enemies
- **Ammo Conservation**: Adjusts fire rate based on ammo remaining
- **Combat Profiles**: AGGRESSIVE, CAUTIOUS, DEFENSIVE, RETREATING modes

---

## Current Implementation

### From `playerbot_tactics.cpp` and `playerbot_attack.cpp`

#### Combat Profile Determination

```cpp
enum CombatProfile {
    AGGRESSIVE,   // Push forward, continuous fire
    CAUTIOUS,     // Balanced approach
    DEFENSIVE,    // Maintain distance, burst fire
    RETREATING    // Fall back, suppress fire
};

CombatProfile DetermineCombatProfile() {
    float healthPercent = controlledEnt->health / controlledEnt->max_health;
    float recentDamage = combatState.recentDamage;  // Damage in last 2 seconds
    int enemyCount = perception->GetEnemyCount();
    
    // Retreat if hurt badly
    if (healthPercent < 0.25f || recentDamage > 30.0f) {
        return RETREATING;
    }
    
    // Defensive if outnumbered or low health
    if (enemyCount >= 3 || (enemyCount >= 2 && healthPercent < 0.5f)) {
        return DEFENSIVE;
    }
    
    // Aggressive if healthy and profile supports it
    if (healthPercent > 0.7f && profile->GetAggression() > 0.6f) {
        return AGGRESSIVE;
    }
    
    return CAUTIOUS;
}
```

#### Recent Damage Tracking

```cpp
struct CombatState {
    float recentDamage;        // Damage taken in last 2 seconds
    float lastDamageTime;      // When last took damage
    float burstDuration;       // Current burst length
    float burstDelay;          // Current burst pause
    bool requireLowSpread;     // Needs accurate fire
    bool ammoLow;              // Less than 30% ammo
};

void TrackDamage(float damage) {
    float currentTime = level.svsTime / 1000.0f;
    
    // Clear old damage outside 2-second window
    if (currentTime - combatState.lastDamageTime > 2.0f) {
        combatState.recentDamage = 0.0f;
    }
    
    combatState.recentDamage += damage;
    combatState.lastDamageTime = currentTime;
}
```

#### Reload Logic

```cpp
bool ShouldReload() {
    Weapon* weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) return false;
    
    float ammoPercent = (float)weapon->GetAmmo() / weapon->GetMaxAmmo();
    
    // Always reload if empty
    if (ammoPercent == 0.0f) return true;
    
    // Reload if low and not under fire
    if (ammoPercent < 0.2f) {
        float timeSinceDamage = level.svsTime - combatState.lastDamageTime;
        if (timeSinceDamage > 2000.0f) {  // Not under fire for 2 seconds
            return true;
        }
    }
    
    // Profile-based: conservative reloads earlier
    if (ammoPercent < profile->GetAmmoConservation() * 0.3f) {
        if (coverState.state == COVER_IN_COVER) {  // Safe in cover
            return true;
        }
    }
    
    return false;
}
```

---

## Behavior Tree Design

### Actions

#### `TacticalRetreat`
- **Purpose**: Finds safe position away from enemies and retreats
- **Returns**: RUNNING (retreating), SUCCESS (reached safety)
- **Blackboard**: Writes `RETREAT_POSITION`, `RETREAT_START_TIME`

#### `SafeReload`
- **Purpose**: Reloads weapon, preferably behind cover
- **Returns**: RUNNING (reloading), SUCCESS (complete)
- **Blackboard**: Reads `COVER_STATE`, writes `RELOAD_START_TIME`

#### `SuppressFire`
- **Purpose**: Fires at last-known enemy position to suppress
- **Returns**: RUNNING (suppressing), SUCCESS after duration
- **Blackboard**: Reads `LAST_KNOWN_ENEMY_POSITION`, writes `SUPPRESS_START_TIME`

### Conditions

#### `ShouldRetreat`
- **Returns**: true if health < 25% OR recent damage > 30 OR outnumbered (3+ enemies)

#### `UnderHeavyFire`
- **Returns**: true if took damage in last 2 seconds

#### `AmmoLow`
- **Returns**: true if ammo < 20%

#### `SafeToReload`
- **Returns**: true if in cover OR not under fire for 2+ seconds

---

## Implementation Steps

### Day 1: Retreat & Damage Tracking (8 hours)

#### Morning (4 hours)
- [ ] Add damage tracking to BotController
- [ ] Implement `Condition_ShouldRetreat`
- [ ] Implement `Condition_UnderHeavyFire`
- [ ] Implement `Action_TacticalRetreat`
- [ ] Add profile parameters: `retreat_threshold`, `aggression`

#### Afternoon (4 hours)
- [ ] Implement combat profile determination
- [ ] Add `CombatProfile` enum and blackboard key
- [ ] Write 3 unit tests (retreat conditions, damage tracking)

### Day 2: Reload & Suppression (8 hours)

#### Morning (4 hours)
- [ ] Implement `Condition_AmmoLow`
- [ ] Implement `Condition_SafeToReload`
- [ ] Implement `Action_SafeReload`
- [ ] Add profile parameter: `ammo_conservation`

#### Afternoon (4 hours)
- [ ] Implement `Action_SuppressFire`
- [ ] Integrate into combat tree
- [ ] Write 3 unit tests (reload logic, suppression)
- [ ] Integration testing with cover system

---

## Testing Strategy

### Unit Tests (6 tests)

1. **ShouldRetreat_LowHealth**: TRUE when health < 25%
2. **ShouldRetreat_HeavyDamage**: TRUE when recent damage > 30
3. **ShouldRetreat_Outnumbered**: TRUE when 3+ enemies
4. **UnderHeavyFire**: TRUE when damaged in last 2 seconds
5. **SafeToReload_InCover**: TRUE when in cover with low ammo
6. **SafeToReload_NotUnderFire**: TRUE when not damaged for 2+ seconds

---

## Files to Create/Modify

```
code/fgame/bt_actions_tactical.cpp
code/fgame/bt_actions_tactical.h
code/fgame/bt_conditions_tactical.cpp
code/fgame/bt_conditions_tactical.h
tests/test_tactical_combat.cpp
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    constexpr const char* RECENT_DAMAGE = "recentDamage";           // float
    constexpr const char* LAST_DAMAGE_TIME = "lastDamageTime";      // float
    constexpr const char* COMBAT_PROFILE = "combatProfile";         // int (CombatProfile enum)
    constexpr const char* RETREAT_POSITION = "retreatPosition";     // Vector
    constexpr const char* SUPPRESS_START_TIME = "suppressStartTime"; // float
    constexpr const char* RELOAD_START_TIME = "reloadStartTime";    // float
}
```

---

## Profile Parameters

```yaml
personality:
  aggression: 0.7              # Affects combat profile selection
  caution: 0.5                 # Affects retreat threshold

combat:
  ammo_conservation: 0.5       # 0.0 (spray) - 1.0 (conservative)

tactics:
  retreat_threshold: 0.25      # Health percent to trigger retreat
  damage_threshold: 30.0       # Damage amount to trigger retreat
```

---

## Integration with Combat Tree

Add retreat priority to `behaviors/combat.yaml`:

```yaml
tree:
  type: selector
  children:
    # Priority 1: Emergency retreat
    - type: sequence
      name: "Emergency Retreat"
      children:
        - type: condition
          check: "ShouldRetreat"
        - type: action
          action: "FindCover"  # From 3.1d
        - type: action
          action: "TacticalRetreat"
        - type: sequence
          name: "Heal Or Reload"
          children:
            - type: condition
              check: "AmmoLow"
            - type: action
              action: "SafeReload"
    
    # Priority 2: Safe reload
    - type: sequence
      name: "Reload When Safe"
      children:
        - type: condition
          check: "AmmoLow"
        - type: condition
          check: "SafeToReload"
        - type: action
          action: "SafeReload"
    
    # ... existing combat engagement ...
```
