# Task 3.1h: Weapon Switching System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 1 day  
**Priority**: LOW  
**Dependencies**: Task 3.1f (Core Combat)

---

## Overview

Implement automatic weapon switching logic. Bots intelligently switch weapons based on ammo availability, range requirements, and profile preferences.

### What This Achieves

- **Auto-Switch on Empty**: Switches to loaded weapon when current is empty
- **Range-Based Selection**: Chooses best weapon for current combat distance
- **Profile Preferences**: Respects weapon preferences from bot profile
- **Seamless Integration**: Works with existing fire control system

---

## Design Specifications

### Weapon Selection Criteria

1. **Ammo Availability**: Weapon must have ammo
2. **Range Suitability**: Weapon effective at current distance
3. **Profile Preference**: Higher weight for preferred weapons
4. **Current Weapon**: Bonus for already equipped (avoid thrashing)

### Weapon Scoring Algorithm

```cpp
float CalculateWeaponScore(Weapon* weapon, float targetDistance, BotProfile* profile)
{
    float score = 0.0f;
    
    // 1. Ammo check (eliminate if empty)
    if (!weapon->HasAmmo(FIRE_PRIMARY)) {
        return -1.0f;  // Invalid
    }
    
    // 2. Range suitability (0.0 - 1.0)
    float minRange = weapon->GetMinRange();
    float maxRange = weapon->GetMaxRange();
    
    if (targetDistance < minRange || targetDistance > maxRange) {
        score += 0.0f;  // Out of range
    } else {
        // In range: score based on optimal distance
        float optimalRange = (minRange + maxRange) / 2.0f;
        float distanceFromOptimal = fabs(targetDistance - optimalRange);
        float rangeScore = 1.0f - (distanceFromOptimal / maxRange);
        score += rangeScore * 0.5f;
    }
    
    // 3. Profile preference (0.0 - 0.3)
    float preference = profile->GetWeaponPreference(weapon->GetWeaponClass());
    score += preference * 0.3f;
    
    // 4. Current weapon bonus (0.2 to avoid rapid switching)
    if (weapon == currentWeapon) {
        score += 0.2f;
    }
    
    return score;
}
```

### Weapon Classes

```cpp
enum WeaponClass {
    WC_PISTOL,
    WC_RIFLE,
    WC_SHOTGUN,
    WC_SNIPER,
    WC_SMG,
    WC_MG
};
```

---

## Behavior Tree Design

### Actions

#### `SelectBestWeapon`
- **Purpose**: Evaluates all weapons and selects best for situation
- **Returns**: SUCCESS if weapon selected, FAILURE if none available
- **Blackboard**: Reads `TARGET_DISTANCE`, `PROFILE`, writes `SELECTED_WEAPON`

#### `SwitchWeapon`
- **Purpose**: Switches to selected weapon
- **Returns**: RUNNING (switching animation), SUCCESS (complete)
- **Blackboard**: Reads `SELECTED_WEAPON`

### Conditions

#### `CurrentWeaponEmpty`
- **Returns**: true if current weapon has no ammo

#### `BetterWeaponAvailable`
- **Returns**: true if another weapon scores significantly higher (> 0.3 difference)

#### `WeaponSwitchReady`
- **Returns**: true if not currently switching or reloading

---

## Implementation Steps

### Morning (4 hours)
- [ ] Implement `CalculateWeaponScore()` helper
- [ ] Implement `Action_SelectBestWeapon`
- [ ] Implement `Action_SwitchWeapon`
- [ ] Add `Weapon::GetWeaponClass()` method
- [ ] Add `Player::SwitchToWeapon(Weapon*)` method

### Afternoon (4 hours)
- [ ] Implement conditions (CurrentWeaponEmpty, BetterWeaponAvailable, WeaponSwitchReady)
- [ ] Add profile weapon preferences
- [ ] Write 4 unit tests
- [ ] Integration test with fire control

---

## Testing Strategy

### Unit Tests (4 tests)

1. **SelectBestWeapon_EmptyCurrent**: Switches from empty weapon
2. **SelectBestWeapon_RangeBased**: Prefers weapon suited for current range
3. **SelectBestWeapon_ProfilePreference**: Weighs profile preferences
4. **BetterWeaponAvailable**: TRUE when significantly better weapon exists

---

## Files to Create/Modify

```
code/fgame/bt_actions_weapon.cpp
code/fgame/bt_actions_weapon.h
code/fgame/bt_conditions_weapon.cpp
code/fgame/bt_conditions_weapon.h
code/fgame/weapon.h                  # Add GetWeaponClass()
code/fgame/player.h                  # Add SwitchToWeapon()
tests/test_weapon_switching.cpp
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    constexpr const char* SELECTED_WEAPON = "selectedWeapon";        // Weapon*
    constexpr const char* WEAPON_SWITCH_TIME = "weaponSwitchTime";   // float
}
```

---

## Profile Parameters

```yaml
combat:
  weapon_preferences:
    pistol: 0.3
    rifle: 0.8
    shotgun: 0.5
    sniper: 0.6
    smg: 0.7
    mg: 0.5
```

Add to `bot_profile.h`:

```cpp
float GetWeaponPreference(WeaponClass weaponClass) const;
```

---

## Integration with Combat Tree

Add to `behaviors/combat.yaml` in weapon management section:

```yaml
# Priority 4: Handle weapon issues
- type: selector
  name: "Weapon Management"
  children:
    # Switch if current weapon empty
    - type: sequence
      name: "Switch From Empty Weapon"
      children:
        - type: condition
          check: "CurrentWeaponEmpty"
        - type: condition
          check: "WeaponSwitchReady"
        - type: action
          action: "SelectBestWeapon"
        - type: action
          action: "SwitchWeapon"
    
    # Switch if better weapon for range
    - type: sequence
      name: "Switch To Better Weapon"
      children:
        - type: condition
          check: "BetterWeaponAvailable"
        - type: condition
          check: "WeaponSwitchReady"
        - type: action
          action: "SelectBestWeapon"
        - type: action
          action: "SwitchWeapon"
```

---

## Weapon Class Definitions

Add to `code/fgame/weapon.cpp`:

```cpp
WeaponClass Weapon::GetWeaponClass() const
{
    // Determine from weapon name or stats
    if (getName().contains("pistol") || getName().contains("colt")) {
        return WC_PISTOL;
    }
    if (getName().contains("rifle") || getName().contains("kar98")) {
        return WC_RIFLE;
    }
    if (getName().contains("shotgun")) {
        return WC_SHOTGUN;
    }
    if (getName().contains("sniper") || getName().contains("springfield")) {
        return WC_SNIPER;
    }
    if (getName().contains("thompson") || getName().contains("mp40")) {
        return WC_SMG;
    }
    if (getName().contains("bar") || getName().contains("mg42")) {
        return WC_MG;
    }
    
    return WC_RIFLE;  // Default
}
```

---

## Constants

```cpp
namespace BotConstants
{
    constexpr float WEAPON_SWITCH_SCORE_THRESHOLD = 0.3f;  // Score advantage needed to switch
}
```
