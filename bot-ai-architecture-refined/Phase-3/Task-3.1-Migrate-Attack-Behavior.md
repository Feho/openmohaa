# Task 3.1: Migrate Attack Behavior to Behavior Trees

**Status:** Ready to Execute  
**Duration:** 2 weeks  
**Priority:** HIGH  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Migrates all combat-related behaviors from the old priority-based state machine to the new Behavior Tree system. This includes enemy engagement, tactical retreat, distance management, reload logic, grenade usage, and weapon switching.

### Why This Matters
- **Combat is Core:** Attack behavior is the most important and complex bot behavior
- **Complete Migration:** This task migrates the entire combat system, not just basic shooting
- **Tactical Depth:** BT structure enables more sophisticated combat tactics
- **Foundation:** Other behaviors (investigation, patrol) build on this

### What's Already Complete

**Phase 2A (Perception):**
- ✅ PerceptionSystem detects enemies with FOV, occlusion, distance
- ✅ EnemyInfo includes: position, velocity, distance, visibilityFactor, angleFromForward
- ✅ ThreatLevel assessment (NONE/LOW/MEDIUM/HIGH)
- ✅ MemorySystem tracks last-known enemy positions with confidence decay
- ✅ AudioSensor detects weapon fire and footsteps

**Phase 2B (Behavior Trees):**
- ✅ BTNode base class with Status (SUCCESS/FAILURE/RUNNING)
- ✅ Composite nodes: BTSelector, BTSequence, BTParallel
- ✅ Leaf nodes: BTCondition, BTAction
- ✅ Blackboard for shared state storage
- ✅ YAML tree loading with action registry
- ✅ BT visualizer showing active nodes
- ✅ Feature flag `g_bot_use_new_ai_system`

**What Bots Can Do Now:**
- Simple patrol behavior via BT
- Detect enemies with realistic perception
- Basic engage behavior (aim + shoot if enemy visible)

**What Bots Need:**
- Complete combat logic (retreat, reload, grenade, weapon switch)
- Tactical distance management
- Emergency behaviors (low health, low ammo)
- Smooth integration with perception data

---

## Current State: Old Attack System

### State Machine Logic
```cpp
// From playerbot.cpp - Current attack state
void BotController::State_Attack() {
    // 200+ lines handling:
    // - Enemy validation
    // - Distance management
    // - Aiming
    // - Firing with burst control
    // - Reload logic
    // - Grenade throwing
    // - Weapon switching
    // - Retreat when low health
}

// Priority in botfuncs array
static botfunc_t botfuncs[MAX_BOT_FUNCTIONS] = {
    {&CheckCondition_Attack, &BeginAttack, &EndAttack, &State_Attack},  // Priority 0
    // ... other states
};

bool BotController::CheckCondition_Attack() {
    return m_pEnemy != NULL && CanSee(m_pEnemy);
}
```

### Key Behaviors to Migrate

1. **Enemy Engagement**
   - Aim at enemy
   - Fire weapon with burst control
   - Track moving targets

2. **Distance Management**
   - Too close: back away
   - Too far: move closer
   - Optimal range: maintain position
   - Consider weapon effective range

3. **Tactical Retreat**
   - Health < 25%: find cover and retreat
   - Outnumbered (3+ enemies): fall back
   - No ammo: retreat to cover and reload

4. **Reload Management**
   - Reload when ammo low (< 20%)
   - Reload behind cover if possible
   - Don't reload under direct fire unless desperate
   - Switch weapon if out of ammo

5. **Grenade Usage**
   - Throw grenade if multiple enemies clustered
   - Throw grenade to flush enemy from cover
   - Don't throw if allies nearby

6. **Weapon Switching**
   - Switch to better weapon for current range
   - Switch if current weapon out of ammo
   - Prefer weapon based on bot profile

---

## Technical Specification

### Combat Tree Architecture

```yaml
# behaviors/combat.btree
tree:
  type: selector
  name: "Combat Root"
  children:
    # Priority 1: Emergency behaviors
    - type: sequence
      name: "Emergency Retreat"
      children:
        - type: condition
          check: "IsEmergency"  # Health < 25% OR (outnumbered AND health < 50%)
        - type: action
          action: "FindNearestCover"
        - type: action
          action: "RetreatToCover"
        - type: action
          action: "HealOrReload"  # Heal if available, else reload

    # Priority 2: Reload if needed (not in combat)
    - type: sequence
      name: "Safe Reload"
      children:
        - type: condition
          check: "NeedsReload"  # Ammo < 20%
        - type: condition
          check: "!UnderDirectFire"
        - type: action
          action: "ReloadWeapon"

    # Priority 3: Main combat engagement
    - type: sequence
      name: "Engage Enemy"
      children:
        - type: condition
          check: "HasVisibleEnemy"
        
        # Consider grenade throw
        - type: selector
          children:
            - type: sequence
              name: "Throw Grenade"
              children:
                - type: condition
                  check: "ShouldThrowGrenade"  # Multiple enemies, no allies nearby
                - type: action
                  action: "ThrowGrenadeAtEnemies"
            - type: action
              action: "DoNothing"  # Skip grenade

        # Main combat parallel: movement + aiming + firing
        - type: parallel
          policy: "RequireAll"
          children:
            # Subtree: Manage combat distance
            - type: selector
              name: "Distance Management"
              children:
                - type: sequence
                  name: "Back Away"
                  children:
                    - type: condition
                      check: "EnemyTooClose"  # Distance < weapon.minRange
                    - type: action
                      action: "BackAwayFromEnemy"
                
                - type: sequence
                  name: "Move Closer"
                  children:
                    - type: condition
                      check: "EnemyTooFar"  # Distance > weapon.maxRange
                    - type: action
                      action: "MoveTowardEnemy"
                
                - type: action
                  action: "MaintainDistance"  # Strafe, keep optimal range

            # Subtree: Aiming
            - type: sequence
              name: "Aim at Target"
              children:
                - type: action
                  action: "SelectBestTarget"  # Closest or most dangerous
                - type: action
                  action: "AimAtEnemy"  # Smooth aim with reaction time

            # Subtree: Firing
            - type: sequence
              name: "Fire Weapon"
              children:
                - type: condition
                  check: "WeaponReady"  # Has ammo, not reloading
                - type: condition
                  check: "EnemyInRange"
                - type: selector
                  children:
                    - type: sequence
                      name: "Burst Fire"
                      children:
                        - type: condition
                          check: "UseBurstFire"  # Based on profile
                        - type: action
                          action: "FireBurst"
                    - type: action
                      action: "FireContinuous"

    # Priority 4: Handle weapon issues
    - type: selector
      name: "Weapon Management"
      children:
        - type: sequence
          name: "Switch Weapon"
          children:
            - type: condition
              check: "CurrentWeaponEmpty"
            - type: action
              action: "SwitchToLoadedWeapon"
        
        - type: sequence
          name: "Reload Under Fire"
          children:
            - type: condition
              check: "NeedsReload"
            - type: action
              action: "ReloadWeapon"  # Desperate reload

    # Fallback: Lost enemy
    - type: action
      action: "ReturnToIdle"
```

### Actions to Implement

#### Core Combat Actions

```cpp
// code/fgame/bt_actions_combat.cpp

// Action: AimAtEnemy
// Smoothly aims at current target with reaction time and spread
BTNode::Status Action_AimAtEnemy(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    if (!target) return BTNode::Status::FAILURE;
    
    // Get target position with lead prediction
    Vector targetPos = PredictTargetPosition(target, profile->GetAimLeadFactor());
    
    // Add human-like inaccuracy
    Vector spread = CalculateSpread(profile->GetSpreadMultiplier());
    targetPos += spread;
    
    // Smooth aim toward target
    Vector currentAim = bot->GetViewAngles();
    Vector desiredAim = VectorToAngles(targetPos - bot->origin);
    
    float reactionTime = profile->GetReactionTime();
    float aimSpeed = 1.0f / reactionTime;
    
    Vector newAim = LerpAngles(currentAim, desiredAim, aimSpeed * dt);
    bot->SetViewAngles(newAim);
    
    // Check if aimed at target (within tolerance)
    float angleToTarget = AngleBetween(newAim, desiredAim);
    if (angleToTarget < 5.0f) {  // 5 degree tolerance
        bb.Set("isAimedAtTarget", true);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;  // Still aiming
}

// Action: FireBurst
// Fires weapon in controlled bursts
BTNode::Status Action_FireBurst(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    // Get burst state
    float burstTimer = bb.Get<float>("burstTimer", 0.0f);
    bool inBurst = bb.Get<bool>("inBurst", false);
    
    if (!inBurst) {
        // Start new burst
        float burstLength = profile->GetBurstLength();  // 0.3 - 1.5 seconds
        bb.Set("burstLength", burstLength);
        bb.Set("inBurst", true);
        burstTimer = 0.0f;
    }
    
    burstTimer += dt;
    bb.Set("burstTimer", burstTimer);
    
    float burstLength = bb.Get<float>("burstLength");
    
    if (burstTimer < burstLength) {
        // Fire during burst
        if (bb.Get<bool>("isAimedAtTarget", false)) {
            bot->FireWeapon();
        }
        return BTNode::Status::RUNNING;
    } else {
        // Burst complete, pause
        float pauseTime = profile->GetBurstPause();  // 0.2 - 0.5 seconds
        
        if (burstTimer < burstLength + pauseTime) {
            return BTNode::Status::RUNNING;  // Pausing between bursts
        } else {
            // Reset for next burst
            bb.Set("inBurst", false);
            bb.Set("burstTimer", 0.0f);
            return BTNode::Status::SUCCESS;
        }
    }
}

// Action: FireContinuous
// Fires weapon continuously (e.g., machine gun)
BTNode::Status Action_FireContinuous(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    if (bb.Get<bool>("isAimedAtTarget", false)) {
        bot->FireWeapon();
    }
    
    return BTNode::Status::SUCCESS;
}

// Action: BackAwayFromEnemy
// Moves away from enemy while maintaining aim
BTNode::Status Action_BackAwayFromEnemy(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    
    if (!target) return BTNode::Status::FAILURE;
    
    // Calculate direction away from enemy
    Vector toEnemy = target->origin - bot->origin;
    toEnemy.z = 0;  // Keep on ground plane
    toEnemy.normalize();
    
    Vector backDir = -toEnemy;
    
    // Move backward
    bot->SetMoveDirection(backDir);
    bot->SetMoveSpeed(1.0f);  // Full speed
    
    // Check if at safe distance
    float distance = (target->origin - bot->origin).length();
    float minRange = bot->GetCurrentWeapon()->GetMinRange();
    
    if (distance > minRange * 1.2f) {  // 20% buffer
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: MoveTowardEnemy
// Moves toward enemy to get in effective range
BTNode::Status Action_MoveTowardEnemy(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    if (!target) return BTNode::Status::FAILURE;
    
    // Calculate direction to enemy
    Vector toEnemy = target->origin - bot->origin;
    toEnemy.z = 0;
    toEnemy.normalize();
    
    // Add some path deviation for unpredictability
    float deviation = profile->GetPathDeviation();  // 0.0 - 1.0
    Vector deviatedDir = AddPathDeviation(toEnemy, deviation);
    
    bot->SetMoveDirection(deviatedDir);
    bot->SetMoveSpeed(profile->GetSpeedPreference());
    
    // Check if in range
    float distance = (target->origin - bot->origin).length();
    float maxRange = bot->GetCurrentWeapon()->GetMaxRange();
    
    if (distance < maxRange * 0.8f) {  // Within 80% of max range
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: MaintainDistance
// Strafes to maintain optimal combat distance
BTNode::Status Action_MaintainDistance(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    if (!target) return BTNode::Status::FAILURE;
    
    // Strafe perpendicular to enemy
    Vector toEnemy = target->origin - bot->origin;
    toEnemy.z = 0;
    toEnemy.normalize();
    
    // Perpendicular vector (strafe direction)
    Vector strafeDir = Vector(-toEnemy.y, toEnemy.x, 0);
    
    // Alternate strafe direction periodically
    float strafeTimer = bb.Get<float>("strafeTimer", 0.0f);
    strafeTimer += dt;
    
    if (strafeTimer > 2.0f) {  // Change direction every 2 seconds
        strafeDir = -strafeDir;
        strafeTimer = 0.0f;
    }
    
    bb.Set("strafeTimer", strafeTimer);
    
    bot->SetMoveDirection(strafeDir);
    bot->SetMoveSpeed(0.5f);  // Half speed for strafing
    
    return BTNode::Status::SUCCESS;
}

// Action: ReloadWeapon
// Reloads current weapon
BTNode::Status Action_ReloadWeapon(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    if (!bot->IsReloading()) {
        bot->StartReload();
        bb.Set("reloadStartTime", level.time);
    }
    
    if (bot->IsReloading()) {
        return BTNode::Status::RUNNING;
    }
    
    return BTNode::Status::SUCCESS;
}

// Action: ThrowGrenadeAtEnemies
// Throws grenade at clustered enemies
BTNode::Status Action_ThrowGrenadeAtEnemies(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    if (!bot->HasGrenades()) {
        return BTNode::Status::FAILURE;
    }
    
    // Calculate grenade target (center of enemy cluster)
    Vector targetPos = CalculateClusterCenter(perception->visibleEnemies);
    
    // Throw grenade
    bot->ThrowGrenade(targetPos);
    
    // Set cooldown
    bb.Set("lastGrenadeTime", level.time);
    
    return BTNode::Status::SUCCESS;
}

// Action: SwitchToLoadedWeapon
// Switches to a weapon with ammo
BTNode::Status Action_SwitchToLoadedWeapon(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    
    Weapon* bestWeapon = bot->FindBestLoadedWeapon();
    
    if (bestWeapon && bestWeapon != bot->GetCurrentWeapon()) {
        bot->SwitchToWeapon(bestWeapon);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::FAILURE;
}

// Action: FindNearestCover
// Identifies nearest cover point
BTNode::Status Action_FindNearestCover(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* threat = bb.Get<Sentient*>("currentTarget");
    
    CoverPoint* cover = FindNearestCover(bot->origin, threat ? threat->origin : Vector::Zero);
    
    if (cover) {
        bb.Set("targetCover", cover);
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::FAILURE;
}

// Action: RetreatToCover
// Moves to previously found cover point
BTNode::Status Action_RetreatToCover(Blackboard& bb, float dt) {
    Player* bot = bb.Get<Player*>("bot");
    CoverPoint* cover = bb.Get<CoverPoint*>("targetCover");
    
    if (!cover) return BTNode::Status::FAILURE;
    
    // Move to cover
    bot->MoveTo(cover->position);
    
    // Check if reached
    float distance = (cover->position - bot->origin).length();
    if (distance < 64.0f) {  // Close enough
        return BTNode::Status::SUCCESS;
    }
    
    return BTNode::Status::RUNNING;
}

// Action: SelectBestTarget
// Chooses best enemy to attack
BTNode::Status Action_SelectBestTarget(Blackboard& bb, float dt) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    if (perception->visibleEnemies.empty()) {
        return BTNode::Status::FAILURE;
    }
    
    // Use closest enemy (could be most dangerous)
    Sentient* target = perception->closestEnemy->entity;
    bb.Set("currentTarget", target);
    
    return BTNode::Status::SUCCESS;
}
```

#### Conditions to Implement

```cpp
// code/fgame/bt_conditions_combat.cpp

// Condition: IsEmergency
bool Condition_IsEmergency(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    
    float healthPercent = bot->health / bot->max_health;
    int enemyCount = perception->GetEnemyCount();
    
    // Emergency if:
    // - Health critical (< 25%)
    // - Outnumbered AND hurt (3+ enemies AND health < 50%)
    return (healthPercent < 0.25f) || 
           (enemyCount >= 3 && healthPercent < 0.5f);
}

// Condition: HasVisibleEnemy
bool Condition_HasVisibleEnemy(Blackboard& bb) {
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    return perception->HasVisibleEnemy();
}

// Condition: NeedsReload
bool Condition_NeedsReload(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Weapon* weapon = bot->GetCurrentWeapon();
    
    if (!weapon) return false;
    
    float ammoPercent = (float)weapon->GetAmmo() / weapon->GetMaxAmmo();
    return ammoPercent < 0.2f;  // Less than 20% ammo
}

// Condition: UnderDirectFire
bool Condition_UnderDirectFire(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    
    // Check if took damage recently (last 2 seconds)
    float lastDamageTime = bb.Get<float>("lastDamageTime", 0.0f);
    return (level.time - lastDamageTime) < 2.0f;
}

// Condition: EnemyTooClose
bool Condition_EnemyTooClose(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    
    if (!target) return false;
    
    float distance = (target->origin - bot->origin).length();
    float minRange = bot->GetCurrentWeapon()->GetMinRange();
    
    return distance < minRange;
}

// Condition: EnemyTooFar
bool Condition_EnemyTooFar(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    
    if (!target) return false;
    
    float distance = (target->origin - bot->origin).length();
    float maxRange = bot->GetCurrentWeapon()->GetMaxRange();
    
    return distance > maxRange;
}

// Condition: WeaponReady
bool Condition_WeaponReady(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Weapon* weapon = bot->GetCurrentWeapon();
    
    return weapon && weapon->GetAmmo() > 0 && !bot->IsReloading();
}

// Condition: EnemyInRange
bool Condition_EnemyInRange(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Sentient* target = bb.Get<Sentient*>("currentTarget");
    
    if (!target) return false;
    
    float distance = (target->origin - bot->origin).length();
    Weapon* weapon = bot->GetCurrentWeapon();
    
    return distance >= weapon->GetMinRange() && 
           distance <= weapon->GetMaxRange();
}

// Condition: UseBurstFire
bool Condition_UseBurstFire(Blackboard& bb) {
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    Weapon* weapon = bb.Get<Player*>("bot")->GetCurrentWeapon();
    
    // Use burst fire for rifles, not for machine guns
    return weapon->SupportsBurstFire() && profile->GetFireDiscipline() > 0.3f;
}

// Condition: ShouldThrowGrenade
bool Condition_ShouldThrowGrenade(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    PerceptionSnapshot* perception = bb.Get<PerceptionSnapshot*>("perception");
    BotProfile* profile = bb.Get<BotProfile*>("profile");
    
    // Don't throw if no grenades
    if (!bot->HasGrenades()) return false;
    
    // Check cooldown (don't spam grenades)
    float lastGrenadeTime = bb.Get<float>("lastGrenadeTime", 0.0f);
    if (level.time - lastGrenadeTime < 10.0f) return false;
    
    // Throw if multiple enemies clustered
    if (perception->visibleEnemies.size() < 2) return false;
    
    // Check if enemies are clustered (within 256 units)
    if (!AreEnemiesClustered(perception->visibleEnemies, 256.0f)) return false;
    
    // Check no allies nearby
    if (HasAlliesNearEnemies(perception, 384.0f)) return false;
    
    // Random chance based on profile
    return random() < profile->GetGrenadeFrequency();
}

// Condition: CurrentWeaponEmpty
bool Condition_CurrentWeaponEmpty(Blackboard& bb) {
    Player* bot = bb.Get<Player*>("bot");
    Weapon* weapon = bot->GetCurrentWeapon();
    
    return weapon && weapon->GetAmmo() == 0;
}
```

---

## Implementation Steps

### Week 1: Core Combat Actions

#### Day 1-2: Aiming and Firing (12 hours)
- [ ] Implement `Action_AimAtEnemy` with smooth tracking
- [ ] Implement `Action_SelectBestTarget`
- [ ] Implement `Action_FireBurst` with profile-based timing
- [ ] Implement `Action_FireContinuous`
- [ ] Add helper functions: `PredictTargetPosition`, `CalculateSpread`, `LerpAngles`
- [ ] **Write unit tests** (4 tests: aim convergence, burst timing, target selection, fire modes)

#### Day 3-4: Movement Actions (12 hours)
- [ ] Implement `Action_BackAwayFromEnemy`
- [ ] Implement `Action_MoveTowardEnemy`
- [ ] Implement `Action_MaintainDistance` with strafing
- [ ] Add helper function: `AddPathDeviation`
- [ ] **Write unit tests** (3 tests: back away success, move closer success, strafe alternation)

#### Day 4-5: Combat Conditions (8 hours)
- [ ] Implement all 12 combat conditions
- [ ] Add helper functions: `AreEnemiesClustered`, `HasAlliesNearEnemies`
- [ ] **Write unit tests** (6 tests: emergency detection, range checks, weapon state, grenade logic)

### Week 2: Advanced Combat & Integration

#### Day 6-7: Weapon Management (12 hours)
- [ ] Implement `Action_ReloadWeapon`
- [ ] Implement `Action_SwitchToLoadedWeapon`
- [ ] Add `Weapon::GetMinRange()`, `GetMaxRange()`, `SupportsBurstFire()`
- [ ] Add `Player::FindBestLoadedWeapon()`
- [ ] **Write unit tests** (3 tests: reload completes, weapon switching, weapon selection)

#### Day 8: Grenade System (6 hours)
- [ ] Implement `Action_ThrowGrenadeAtEnemies`
- [ ] Add helper function: `CalculateClusterCenter`
- [ ] Add `Player::HasGrenades()`, `ThrowGrenade()`
- [ ] **Write unit tests** (2 tests: grenade targeting, cooldown)

#### Day 9: Retreat System (6 hours)
- [ ] Implement `Action_FindNearestCover`
- [ ] Implement `Action_RetreatToCover`
- [ ] Add cover finding logic: `FindNearestCover()`
- [ ] Define `CoverPoint` struct if not exists
- [ ] **Write unit tests** (2 tests: cover finding, retreat completion)

#### Day 10: YAML Tree Creation (4 hours)
- [ ] Create `behaviors/combat.btree` with complete tree structure
- [ ] Register all actions in action registry
- [ ] Register all conditions in condition registry
- [ ] Load tree in BotController when `g_bot_use_new_ai_system=1`

#### Day 11-12: Integration & Testing (12 hours)
- [ ] Integrate combat tree with BotController
- [ ] Populate blackboard with perception data each frame
- [ ] Update blackboard on damage events (`lastDamageTime`)
- [ ] Add profile parameters: `burst_length`, `burst_pause`, `fire_discipline`, `grenade_frequency`
- [ ] **Write integration tests** (6 tests):
  - Bot engages visible enemy
  - Bot backs away when too close
  - Bot reloads when ammo low
  - Bot retreats when health critical
  - Bot throws grenade at clustered enemies
  - Bot switches weapon when empty

#### Day 13-14: Tuning & Polish (8 hours)
- [ ] Test with different bot profiles (aggressive, defensive, balanced)
- [ ] Tune timing parameters (burst length, reload timing, strafe frequency)
- [ ] Adjust weapon ranges for balance
- [ ] Compare AI quality vs. old state machine
- [ ] Fix bugs found during testing
- [ ] Performance profiling (ensure < 0.5ms per bot)

---

## Files to Create/Modify

### New Files
```
code/fgame/bt_actions_combat.cpp      # 15 combat actions
code/fgame/bt_actions_combat.h        # Action declarations
code/fgame/bt_conditions_combat.cpp   # 12 combat conditions
code/fgame/bt_conditions_combat.h     # Condition declarations
code/fgame/combat_helpers.cpp         # Helper functions
code/fgame/combat_helpers.h           # PredictTarget, CalculateSpread, etc.
behaviors/combat.btree                # Complete combat behavior tree YAML
tests/test_combat_actions.cpp         # Unit tests for actions
tests/test_combat_conditions.cpp      # Unit tests for conditions
tests/integration_test_combat.cpp     # Integration tests
```

### Modified Files
```
code/fgame/playerbot.h                # Add combat-related blackboard keys
code/fgame/playerbot.cpp              # Integrate combat tree
code/fgame/player.h                   # Add weapon/grenade methods if needed
code/fgame/weapons.h                  # Add range getters
profiles/*.yaml                       # Add combat parameters
```

---

## Testing Strategy

### Unit Tests (20 tests)
- 4 aiming/firing tests
- 3 movement tests
- 6 condition tests
- 3 weapon management tests
- 2 grenade tests
- 2 retreat tests

### Integration Tests (6 tests)
- Full combat engagement scenario
- Distance management scenario
- Emergency retreat scenario
- Grenade usage scenario
- Weapon switching scenario
- Profile variation scenario

### Manual Testing
- [ ] Test aggressive profile: pushes forward, uses grenades
- [ ] Test defensive profile: maintains distance, retreats early
- [ ] Test balanced profile: adapts to situation
- [ ] Test vs. player: feels challenging but fair
- [ ] Test vs. multiple enemies: handles outnumbered situations
- [ ] Test with different weapons: adjusts tactics per weapon

---

## Acceptance Criteria

### Functionality
- [ ] All 15 combat actions implemented and tested
- [ ] All 12 combat conditions implemented and tested
- [ ] Combat behavior tree loads from YAML
- [ ] Bot engages enemies with appropriate tactics
- [ ] Bot manages distance based on weapon range
- [ ] Bot reloads when necessary
- [ ] Bot retreats when health critical
- [ ] Bot throws grenades at clusters
- [ ] Bot switches weapons when empty
- [ ] Different profiles show distinct combat styles

### Quality
- [ ] 26 total tests pass (20 unit + 6 integration)
- [ ] Zero regressions when `g_bot_use_new_ai_system=0`
- [ ] Combat feels as good or better than old system
- [ ] No obvious AI bugs (stuck, spinning, etc.)
- [ ] Code follows OpenMoHAA standards
- [ ] All functions < 100 lines

### Performance
- [ ] Combat tree execution < 0.5ms per bot
- [ ] No frame drops during heavy combat (10+ bots)
- [ ] Profiling shows no bottlenecks

---

## Success Metrics

### AI Quality
- **Engagement:** Bot consistently engages visible enemies
- **Tactics:** Bot uses cover, manages distance, retreats when appropriate
- **Variety:** Different profiles show measurably different playstyles
- **Competence:** Bot presents reasonable challenge to player

### Code Quality
- **Maintainability:** Each action/condition is single-purpose and < 50 lines
- **Testability:** High test coverage (>80% of combat code)
- **Clarity:** Behavior visible in YAML tree, not hidden in C++

### Performance
- **Speed:** < 0.5ms per bot per frame
- **Scalability:** Supports 50+ bots in combat without FPS drop

---

## Notes & Considerations

### Weapon Range Tuning
Different weapons need appropriate range values:
- Pistol: 128-768 units
- Rifle: 256-2048 units
- Sniper: 512-4096 units
- Shotgun: 64-512 units
- Machine Gun: 256-1536 units

### Profile Impact on Combat
Each profile should feel distinct:
- **Aggressive:** Short bursts, pushes forward, frequent grenades
- **Defensive:** Long bursts, maintains distance, retreats early
- **Balanced:** Adapts to situation, medium everything

### Integration with Perception
Combat tree relies on perception data:
- `perception->visibleEnemies`: For target selection
- `perception->closestEnemy`: For distance checks
- `perception->threatLevel`: For retreat decisions
- `perception->nearbyAllies`: For grenade safety

### Future Enhancements (Not in This Task)
- Cover peeking behavior
- Suppressive fire coordination
- Squad tactics (flanking, covering fire)
- Dynamic weapon preference based on situation
- Advanced grenade trajectories
- Melee combat

---

## Troubleshooting

### Bot Doesn't Fire
- Check `Condition_WeaponReady` returns true
- Check `Condition_EnemyInRange` returns true
- Check `isAimedAtTarget` blackboard value
- Verify perception detects enemy

### Bot Stuck in One Behavior
- Check condition logic (might always return true)
- Check action returns proper status (SUCCESS/RUNNING)
- Verify selector/sequence logic
- Use BT visualizer to see active nodes

### Performance Issues
- Profile with `g_profile_bots 1`
- Check for redundant calculations in actions
- Cache expensive lookups in blackboard
- Reduce action update frequency if needed

---

## Dependencies

### Requires (from Phase 2)
- PerceptionSystem with enemy/ally detection
- Blackboard with type-safe get/set
- BTNode base classes (Selector/Sequence/Parallel/Condition/Action)
- YAML tree loader with action registry
- BotProfile system with combat parameters

### Provides (for Phase 3 tasks)
- Complete combat action library
- Combat behavior patterns (reusable subtrees)
- Example of complex BT integration
- Testing patterns for other behaviors

---

**Next Task:** Task 3.2 - Investigation Behavior (search patterns for lost enemies)
