# Task 3.1b: Aiming & Fire Control System

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 3 days  
**Priority**: HIGH  
**Dependencies**: Task 3.1a (Target Selection)

---

## Overview

Migrate aiming and weapon firing logic to behavior trees. This includes smooth aim tracking, burst fire control, semi/full-auto handling, zoom management, and melee attacks.

### What This Achieves

- **Smooth Aiming**: Interpolates aim toward target with reaction time
- **Burst Control**: Profile-driven burst fire (burst duration + pause)
- **Fire Modes**: Handles semi-auto (tap fire) and full-auto weapons
- **Accuracy Management**: Checks weapon spread, stops moving if needed
- **Zoom Control**: Activates scope for long-range weapons
- **Melee Integration**: Uses secondary fire for melee when appropriate

---

## Current Implementation

### File: `code/fgame/playerbot_attack.cpp`

#### AimAtTarget() - Lines 305-346

```cpp
void BotController::AimAtTarget(bool canSee)
{
    if (canSee || level.inttime < m_iAttackStopAimTime) {
        Vector        vTarget;
        orientation_t eyes_or;

        if (m_iEnemyEyesTag == -1) {
            m_iEnemyEyesTag = gi.Tag_NumForName(m_pEnemy->edict->tiki, "eyes bone");
        }

        if (m_iEnemyEyesTag != -1) {
            m_pEnemy->GetTag(m_iEnemyEyesTag, &eyes_or);
            vTarget = eyes_or.origin;
        } else {
            vTarget = m_pEnemy->origin;
        }

        // Update aim offset every 100ms for human-like inaccuracy
        if (level.inttime >= m_iLastAimTime + BotConstants::AIM_UPDATE_INTERVAL) {
            if (m_iEnemyEyesTag != -1) {
                m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5);
                m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5);
                m_vAimOffset[2] = -G_Random(m_pEnemy->maxs.z * 0.5);
            } else {
                m_vAimOffset[0] = G_CRandom((m_pEnemy->maxs.x - m_pEnemy->mins.x) * 0.5);
                m_vAimOffset[1] = G_CRandom((m_pEnemy->maxs.y - m_pEnemy->mins.y) * 0.5);
                m_vAimOffset[2] = 16 + G_Random(m_pEnemy->viewheight - 16);
            }
            m_iLastAimTime = level.inttime;
        }

        rotation.AimAt(vTarget + m_vAimOffset * g_bot_attack_spreadmult->value);
    } else {
        AimAtAimNode();  // Fallback: aim at general search direction
    }
}
```

#### HandleWeaponFiring() - Lines 414-486

```cpp
void BotController::HandleWeaponFiring(
    bool canSee, float distanceSq, float primaryRangeSq, 
    float secondaryRangeSq, Weapon *weapon,
    bool& outNoMove, bool& outFiring, bool& outMelee)
{
    float fSpreadFactor = weapon->GetSpreadFactor(FIRE_PRIMARY);

    // Check max fire movement - stop if weapon requires accuracy
    if (weapon->GetMaxFireMovement() < 1 && weapon->HasAmmoInClip(FIRE_PRIMARY)) {
        float length = controlledEnt->velocity.length();
        if ((length / sv_runspeed->value) > weapon->GetMaxFireMovementMult()) {
            outNoMove = true;
            movement.ClearMove();
        }
    }

    if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0 
        && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
        // No ammo - stop firing
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        controlledEnt->ZoomOff();
    } else if (distanceSq > primaryRangeSq) {
        // Out of range - stop firing
        m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
        controlledEnt->ZoomOff();
    } else {
        // In range - attack
        if (weapon->IsSemiAuto()) {
            // Semi-auto: tap fire only when idle and accurate
            if (controlledEnt->client->ps.iViewModelAnim != VM_ANIM_IDLE
                && (controlledEnt->client->ps.iViewModelAnim < VM_ANIM_IDLE_0
                    || controlledEnt->client->ps.iViewModelAnim > VM_ANIM_IDLE_2)) {
                m_botCmd.buttons &= ~(BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT);
                controlledEnt->ZoomOff();
            } else if (fSpreadFactor < BotConstants::WEAPON_SPREAD_THRESHOLD) {
                outFiring = true;
                m_botCmd.buttons ^= BUTTON_ATTACKLEFT;  // Toggle for tap
                if (weapon->GetZoom()) {
                    if (!controlledEnt->IsZoomed()) {
                        m_botCmd.buttons |= BUTTON_ATTACKRIGHT;
                    }
                }
            } else {
                outNoMove = true;
                movement.ClearMove();
            }
        } else {
            // Full-auto: check spread requirement
            if (combatState.requireLowSpread 
                && fSpreadFactor >= BotConstants::WEAPON_SPREAD_THRESHOLD) {
                outNoMove = true;
                movement.ClearMove();
                m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
            } else {
                outFiring = true;
                m_botCmd.buttons |= BUTTON_ATTACKLEFT;
            }
        }
    }
}
```

#### HandleBurstControl() - Lines 388-412

```cpp
void BotController::HandleBurstControl(bool firing, int fireDelay, 
                                        int maxContinuousFireTime, int maxBurstTime)
{
    if (m_iLastBurstTime) {
        // In pause between bursts
        if (level.inttime > m_iLastBurstTime + maxBurstTime) {
            m_iLastBurstTime      = 0;
            m_iContinuousFireTime = 0;
        } else {
            m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
        }
    } else {
        // Tracking continuous fire time
        if (firing) {
            m_iContinuousFireTime += level.intframetime;
        } else {
            m_iContinuousFireTime = 0;
        }

        // Exceeded max continuous time - start pause
        if (!m_iLastBurstTime && m_iContinuousFireTime > maxContinuousFireTime) {
            m_iLastBurstTime      = level.inttime;
            m_iContinuousFireTime = 0;
        }
    }
}
```

#### HandleMeleeAttack() - Lines 351-386

```cpp
void BotController::HandleMeleeAttack(
    bool canSee, float distanceSq, float secondaryRangeSq, 
    Weapon *weapon, bool& outMelee)
{
    if (!weapon) return;

    if (weapon->GetFireType(FIRE_SECONDARY) == FT_MELEE) {
        // Use melee if out of ammo OR very close
        if (controlledEnt->client->ps.stats[STAT_AMMO] <= 0 
            && controlledEnt->client->ps.stats[STAT_CLIPAMMO] <= 0) {
            outMelee = true;
        } else if (distanceSq <= secondaryRangeSq) {
            outMelee = true;
        }
    }

    if (outMelee) {
        m_botCmd.buttons &= ~BUTTON_ATTACKLEFT;
        if (distanceSq <= secondaryRangeSq) {
            m_botCmd.buttons ^= BUTTON_ATTACKRIGHT;  // Toggle melee
        } else {
            m_botCmd.buttons &= ~BUTTON_ATTACKRIGHT;
        }
    }
}
```

---

## Behavior Tree Design

### Actions

#### `AimAtTarget`
- **Purpose**: Smoothly aims at current target with profile-based inaccuracy
- **Returns**: RUNNING (multi-frame), SUCCESS when aimed within tolerance
- **Blackboard**: Reads `SELECTED_TARGET`, writes `IS_AIMED_AT_TARGET`, `AIM_OFFSET`

#### `FireWeapon`
- **Purpose**: Fires weapon with burst control
- **Returns**: RUNNING (burst in progress), SUCCESS (burst complete)
- **Blackboard**: Reads `SELECTED_TARGET`, `IS_AIMED_AT_TARGET`, writes `BURST_STATE`, `LAST_FIRE_TIME`

#### `MeleeAttack`
- **Purpose**: Executes melee attack (secondary fire)
- **Returns**: SUCCESS when melee performed
- **Blackboard**: Reads `SELECTED_TARGET`, `TARGET_DISTANCE`

### Conditions

#### `WeaponReady`
- **Returns**: true if weapon has ammo and spread acceptable

#### `IsAimedAtTarget`
- **Returns**: true if aim within 5° of target

#### `InMeleeRange`
- **Returns**: true if target within weapon secondary range

---

## Implementation Steps

### Day 1: Aiming System (8 hours)
- [ ] Implement `Action_AimAtTarget` with smooth interpolation
- [ ] Add aim offset calculation (updated every 100ms)
- [ ] Implement `Condition_IsAimedAtTarget` (5° tolerance)
- [ ] Add profile parameters: reaction_time, spread_multiplier, headshot_bias
- [ ] Write 3 unit tests

### Day 2: Fire Control (8 hours)
- [ ] Implement `Action_FireWeapon` with semi/full-auto logic
- [ ] Implement burst control (profile-driven timing)
- [ ] Add spread factor checking
- [ ] Implement `Condition_WeaponReady`
- [ ] Write 4 unit tests

### Day 3: Melee & Integration (8 hours)
- [ ] Implement `Action_MeleeAttack`
- [ ] Implement `Condition_InMeleeRange`
- [ ] Add zoom control logic
- [ ] Write 3 unit tests
- [ ] Integration testing with Task 3.1a

---

## Testing Strategy

### Unit Tests (10 tests)

1. **AimConvergence**: Aim approaches target over time
2. **AimInaccuracy**: Aim offset applied correctly
3. **BurstTiming**: Burst fires for correct duration
4. **BurstPause**: Pause between bursts observed
5. **SemiAutoTap**: Semi-auto toggles fire button
6. **FullAutoHold**: Full-auto holds fire button
7. **SpreadCheck**: Stops firing when spread too high
8. **MeleeRange**: Melee activates when close
9. **MeleeNoAmmo**: Melee activates when out of ammo
10. **ZoomActivation**: Zoom enabled for scoped weapons

---

## Acceptance Criteria

### Functionality
- [ ] Bot smoothly aims at moving targets
- [ ] Burst fire timing matches profile
- [ ] Semi-auto weapons tap fire correctly
- [ ] Melee activates when appropriate
- [ ] Spread factor prevents inaccurate fire
- [ ] Zoom activates for scoped weapons

### Code Quality
- [ ] 10 unit tests pass
- [ ] Functions < 50 lines
- [ ] Proper error handling

### Performance
- [ ] Aiming < 0.05ms per bot
- [ ] Fire control < 0.05ms per bot

---

## Files to Create/Modify

### New Files
```
code/fgame/bt_actions_aim.cpp
code/fgame/bt_actions_aim.h
code/fgame/bt_actions_fire.cpp
code/fgame/bt_actions_fire.h
code/fgame/bt_conditions_combat.cpp
code/fgame/bt_conditions_combat.h
tests/test_aim_fire.cpp
```

### Modified Files
```
code/fgame/bt_blackboard_keys.h      # Add AIM_OFFSET, BURST_STATE, IS_AIMED_AT_TARGET
code/fgame/bot_profile.h             # Add reaction_time, burst params
profiles/*.yaml                      # Add aim/fire parameters
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    // Aiming (Task 3.1b)
    constexpr const char* AIM_OFFSET = "aimOffset";              // Vector - Current aim inaccuracy
    constexpr const char* AIM_UPDATE_TIME = "aimUpdateTime";     // float - Last offset update time
    constexpr const char* IS_AIMED_AT_TARGET = "isAimedAtTarget"; // bool - Aim within tolerance
    constexpr const char* ENEMY_EYES_TAG = "enemyEyesTag";       // int - Cached eye bone tag
    
    // Firing (Task 3.1b)
    constexpr const char* BURST_STATE = "burstState";            // int - 0=not firing, 1=burst, 2=pause
    constexpr const char* BURST_START_TIME = "burstStartTime";   // float - When burst started
    constexpr const char* CONTINUOUS_FIRE_TIME = "continuousFireTime"; // float - Total fire time
    constexpr const char* LAST_FIRE_TIME = "lastFireTime";       // float - Last shot time
}
```

---

## Profile Parameters

```yaml
aim:
  reaction_time: [0.2, 0.5]      # [min, max] seconds to aim at target
  tracking_smoothness: 0.7       # 0.0 (jerky) - 1.0 (smooth)
  spread_multiplier: 1.0         # Aim offset scale (0.5 = more accurate)
  headshot_bias: 0.3             # Probability of aiming at head vs center mass

combat:
  burst_length: [0.3, 0.8]       # [min, max] seconds of continuous fire
  burst_delay: [0.2, 0.5]        # [min, max] seconds between bursts
  fire_discipline: 0.5           # 0.0 (spray) - 1.0 (controlled)
  ammo_conservation: 0.5         # Affects burst length
```
