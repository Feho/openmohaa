# Task 3.1f: Combat Behavior Tree Assembly

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 2 days  
**Priority**: HIGH  
**Dependencies**: Tasks 3.1a, 3.1b, 3.1c (Core Combat Actions)

---

## Overview

Assemble all combat actions and conditions from Tasks 3.1a-c into a complete, functional YAML behavior tree. This creates the **first working combat AI** using the new behavior tree system.

### What This Achieves

- **Complete Combat Tree**: YAML structure using all implemented actions
- **Action Registry**: All actions/conditions registered and loadable
- **Tree Loading**: YAML parser integration working
- **Execution Testing**: Full combat behavior functional in-game
- **Profile Integration**: Different profiles produce different behaviors

### Why This Matters

This is the **critical milestone** for Task 3.1. After this subtask:
- Bots can engage enemies with working combat AI
- We can test the complete system end-to-end
- Advanced features (3.1d-h) build on this foundation

---

## Combat Tree Structure

### Complete YAML Tree

Create `behaviors/combat.yaml`:

```yaml
# Combat Behavior Tree
# Uses actions from Tasks 3.1a (target), 3.1b (aim/fire), 3.1c (movement)

tree:
  type: selector
  name: "Combat Root"
  children:
    # Priority 1: Validate target exists
    - type: sequence
      name: "Ensure Target Selected"
      children:
        - type: condition
          check: "HasValidTarget"
          invert: true  # If NOT has valid target...
        - type: action
          action: "SelectTarget"  # ...then select one
    
    # Priority 2: Main combat engagement
    - type: sequence
      name: "Engage Enemy"
      children:
        # Must have valid, visible target
        - type: condition
          check: "HasValidTarget"
        - type: condition
          check: "TargetVisible"
        
        # Combat loop: move + aim + fire in parallel
        - type: parallel
          policy: "RequireAll"
          name: "Combat Actions"
          children:
            # Subtree: Distance management
            - type: selector
              name: "Manage Combat Distance"
              children:
                # Too close: back away
                - type: sequence
                  name: "Retreat From Close Enemy"
                  children:
                    - type: condition
                      check: "EnemyTooClose"
                    - type: action
                      action: "RetreatFromEnemy"
                
                # Too far: move closer
                - type: sequence
                  name: "Approach Distant Enemy"
                  children:
                    - type: condition
                      check: "EnemyTooFar"
                    - type: action
                      action: "ApproachEnemy"
                
                # In range: maintain with strafe
                - type: sequence
                  name: "Maintain Optimal Range"
                  children:
                    - type: condition
                      check: "InOptimalRange"
                    - type: action
                      action: "MaintainDistance"
            
            # Subtree: Aiming
            - type: action
              action: "AimAtTarget"
              name: "Aim At Enemy"
            
            # Subtree: Firing
            - type: selector
              name: "Fire Weapon"
              children:
                # Melee if very close or no ammo
                - type: sequence
                  name: "Melee Attack"
                  children:
                    - type: condition
                      check: "InMeleeRange"
                    - type: action
                      action: "MeleeAttack"
                
                # Normal fire if weapon ready
                - type: sequence
                  name: "Shoot Enemy"
                  children:
                    - type: condition
                      check: "WeaponReady"
                    - type: condition
                      check: "IsAimedAtTarget"
                    - type: action
                      action: "FireWeapon"
    
    # Priority 3: Fallback - lost enemy
    - type: action
      action: "Idle"
      name: "No Enemy Visible"

metadata:
  name: "Basic Combat"
  description: "Core combat behavior: target selection, aiming, firing, movement"
  version: "1.0"
  author: "OpenMoHAA"
  phase: "3.1f"
```

---

## Action Registry Integration

### Register All Actions

In `code/fgame/bt_core_actions.cpp`, add to `RegisterCoreBTActions()`:

```cpp
void RegisterCoreBTActions()
{
    // ... existing registrations ...
    
    // ========== Task 3.1a: Target Selection ==========
    REGISTER_BT_ACTION("SelectTarget", Action_SelectTarget);
    
    REGISTER_BT_CONDITION("HasValidTarget", Condition_HasValidTarget);
    REGISTER_BT_CONDITION("TargetVisible", Condition_TargetVisible);
    
    // ========== Task 3.1b: Aiming & Fire Control ==========
    REGISTER_BT_ACTION("AimAtTarget", Action_AimAtTarget);
    REGISTER_BT_ACTION("FireWeapon", Action_FireWeapon);
    REGISTER_BT_ACTION("MeleeAttack", Action_MeleeAttack);
    
    REGISTER_BT_CONDITION("WeaponReady", Condition_WeaponReady);
    REGISTER_BT_CONDITION("IsAimedAtTarget", Condition_IsAimedAtTarget);
    REGISTER_BT_CONDITION("InMeleeRange", Condition_InMeleeRange);
    
    // ========== Task 3.1c: Combat Movement ==========
    REGISTER_BT_ACTION("ApproachEnemy", Action_ApproachEnemy);
    REGISTER_BT_ACTION("RetreatFromEnemy", Action_RetreatFromEnemy);
    REGISTER_BT_ACTION("MaintainDistance", Action_MaintainDistance);
    
    REGISTER_BT_CONDITION("EnemyTooClose", Condition_EnemyTooClose);
    REGISTER_BT_CONDITION("EnemyTooFar", Condition_EnemyTooFar);
    REGISTER_BT_CONDITION("InOptimalRange", Condition_InOptimalRange);
}
```

---

## BotController Integration

### Load Combat Tree

In `code/fgame/playerbot.cpp`:

```cpp
void BotController::InitializeBehaviorTree()
{
    if (g_bot_use_new_ai_system->integer) {
        // Load profile
        const char* profilePath = "profiles/balanced.yaml";  // TODO: per-bot selection
        profile = BotProfile::LoadFromFile(profilePath);
        
        if (!profile) {
            gi.DPrintf("Failed to load bot profile: %s\n", profilePath);
            return;
        }
        
        // Load behavior tree specified in profile
        const char* treePath = profile->GetBehaviorTree().c_str();
        if (!treePath || !treePath[0]) {
            treePath = "behaviors/combat.yaml";  // Default
        }
        
        behaviorTree = BTYAMLLoader::LoadFromFile(treePath);
        
        if (!behaviorTree) {
            gi.DPrintf("Failed to load behavior tree: %s\n", treePath);
            return;
        }
        
        gi.DPrintf("[BOT] %s: Loaded profile '%s' and tree '%s'\n",
            controlledEnt->client->pers.netname,
            profile->GetName().c_str(),
            treePath
        );
    }
}

void BotController::ExecuteBehaviorTree(float deltaTime)
{
    if (!behaviorTree || !profile) {
        return;
    }
    
    // Populate blackboard with current state
    PopulateBlackboard();
    
    // Execute tree
    BTNode::Status status = behaviorTree->Execute(blackboard, deltaTime);
    
    // Debug output
    if (g_bot_debug->integer >= 2) {
        gi.DPrintf("[BT] %s: Tree status = %s\n",
            controlledEnt->client->pers.netname,
            StatusToString(status)
        );
    }
}

void BotController::PopulateBlackboard()
{
    // Core references
    blackboard.Set(BlackboardKeys::BOT, this);
    blackboard.Set(BlackboardKeys::PLAYER, static_cast<Player*>(controlledEnt.Pointer()));
    blackboard.Set(BlackboardKeys::PROFILE, profile.get());
    
    // Perception snapshot
    PerceptionSnapshot* snapshot = &perceptionSystem.GetSnapshot();
    blackboard.Set(BlackboardKeys::PERCEPTION, snapshot);
    
    // Health/ammo state
    blackboard.Set("health", controlledEnt->health);
    blackboard.Set("maxHealth", controlledEnt->max_health);
    
    Weapon* weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
    if (weapon) {
        blackboard.Set("hasAmmo", weapon->HasAmmo(FIRE_PRIMARY) != qfalse);
        blackboard.Set("ammoPercent", weapon->GetAmmoPercent());
    }
}
```

---

## Implementation Steps

### Day 1: Tree Creation & Registration (8 hours)

#### Morning (4 hours)
- [ ] Create `behaviors/combat.yaml` with complete tree structure
- [ ] Validate YAML syntax (test with parser)
- [ ] Register all 13 actions/conditions in `bt_core_actions.cpp`:
  - 1 action (SelectTarget)
  - 2 conditions (HasValidTarget, TargetVisible)
  - 3 actions (AimAtTarget, FireWeapon, MeleeAttack)
  - 3 conditions (WeaponReady, IsAimedAtTarget, InMeleeRange)
  - 3 actions (ApproachEnemy, RetreatFromEnemy, MaintainDistance)
  - 3 conditions (EnemyTooClose, EnemyTooFar, InOptimalRange)
- [ ] Update `bt_action_registry.h` if needed

#### Afternoon (4 hours)
- [ ] Implement `InitializeBehaviorTree()` in BotController
- [ ] Implement `ExecuteBehaviorTree()` in BotController
- [ ] Implement `PopulateBlackboard()` helper
- [ ] Add tree execution to bot Think loop
- [ ] Test tree loading (check logs for errors)

### Day 2: Integration Testing & Tuning (8 hours)

#### Morning (4 hours)
- [ ] Create test map with bots and enemies
- [ ] Test basic combat engagement:
  - Bot selects target when enemy appears
  - Bot aims at target
  - Bot fires weapon
  - Bot moves to correct range
- [ ] Fix any integration bugs

#### Afternoon (4 hours)
- [ ] Test with different profiles (aggressive, defensive, balanced)
- [ ] Verify profile parameters affect behavior:
  - Aggressive: faster approach, longer bursts
  - Defensive: maintain distance, shorter bursts
  - Balanced: adaptive behavior
- [ ] Write 6 integration tests
- [ ] Performance profiling (tree execution time)
- [ ] Update profile YAMLs with combat parameters

---

## Testing Strategy

### Integration Tests (6 tests)

These test the complete combat system end-to-end:

```cpp
TEST(CombatIntegration, BotEngagesVisibleEnemy)
{
    // Setup: Bot and enemy in line of sight
    // Expect: Bot selects target, aims, fires within 2 seconds
}

TEST(CombatIntegration, BotApproachesDistantEnemy)
{
    // Setup: Enemy at 1000 units (rifle range = 2048)
    // Expect: Bot moves toward enemy while aiming
}

TEST(CombatIntegration, BotRetreatsFromCloseEnemy)
{
    // Setup: Enemy at 50 units (rifle min range = 128)
    // Expect: Bot backs away while maintaining aim
}

TEST(CombatIntegration, BotStrafesAtOptimalRange)
{
    // Setup: Enemy at 512 units (optimal range)
    // Expect: Bot strafes perpendicular, alternates direction
}

TEST(CombatIntegration, BotSwitchesToMeleeWhenClose)
{
    // Setup: Enemy at 60 units, weapon has melee secondary
    // Expect: Bot uses melee attack (secondary fire)
}

TEST(CombatIntegration, ProfileAffectsBehavior)
{
    // Setup: Aggressive vs Defensive profile
    // Expect: Aggressive pushes forward, Defensive maintains distance
}
```

### Behavioral Tests (High-Level Decision Making)

These test BT Selector node prioritization and decision logic:

```cpp
TEST(BehaviorTree, Bot_WithValidTarget_SelectsEngageBranch)
{
    // Setup: Blackboard with valid target, good health
    // Expect: Tree selects "Engage Enemy" branch
    // Verifies: Selector prioritizes combat when target available
}

TEST(BehaviorTree, Bot_WithNoTarget_SelectsIdleBranch)
{
    // Setup: Blackboard with no visible enemies
    // Expect: Tree falls through to "Idle" action
    // Verifies: Selector handles no-target gracefully
}

TEST(BehaviorTree, Bot_WithInvalidTarget_ReacquiresTarget)
{
    // Setup: Blackboard with dead/invalid target
    // Expect: Tree executes "SelectTarget" action first
    // Verifies: Target validation works before engagement
}

TEST(BehaviorTree, CombatParallel_ExecutesAllChildren)
{
    // Setup: Valid target, weapon ready
    // Expect: Parallel node runs AimAtTarget, MoveToCombatPosition, FireWeapon concurrently
    // Verifies: Parallel combat actions execute simultaneously
}
```

### Manual Testing Checklist

In-game testing with `devmap` and bots:

- [ ] **Target Selection**: Bot acquires new enemies
- [ ] **Target Stickiness**: Bot doesn't rapidly switch targets
- [ ] **Aiming**: Bot smoothly aims at target
- [ ] **Burst Fire**: Bot fires in controlled bursts (not continuous spray)
- [ ] **Range Management**: Bot approaches far enemies, retreats from close
- [ ] **Strafing**: Bot strafes at optimal range
- [ ] **Melee**: Bot uses melee when very close
- [ ] **Profile Variation**: Aggressive bot behaves differently than Defensive
- [ ] **Performance**: No FPS drops with 10+ bots fighting

---

## Acceptance Criteria

### Functionality
- [ ] Combat tree loads from YAML without errors
- [ ] All 13 actions/conditions registered and callable
- [ ] Bot engages visible enemies (selects, aims, fires)
- [ ] Bot manages combat distance (approach/retreat/strafe)
- [ ] Bot uses melee when appropriate
- [ ] Different profiles show distinct behaviors
- [ ] Tree executes without crashes or errors

### Code Quality
- [ ] 6 integration tests pass
- [ ] YAML syntax valid (no parser errors)
- [ ] Debug logging shows tree execution flow
- [ ] Code follows OpenMoHAA standards

### Performance
- [ ] Tree execution < 0.3ms per bot (target selection + aim + fire + movement)
- [ ] Supports 20+ bots in combat without FPS drop
- [ ] No memory leaks (tree properly cleaned up)

---

## Files to Create/Modify

### New Files
```
behaviors/combat.yaml              # Complete combat behavior tree
tests/test_combat_integration.cpp  # 6 integration tests
```

### Modified Files
```
code/fgame/bt_core_actions.cpp     # Register all 13 actions/conditions
code/fgame/playerbot.cpp           # Add InitializeBehaviorTree(), ExecuteBehaviorTree()
code/fgame/playerbot.h             # Add blackboard, behaviorTree, profile members
profiles/*.yaml                    # Set behavior_tree: "combat.yaml"
```

---

## Debug & Troubleshooting

### Enable Debug Output

```cpp
// In console
set g_bot_debug 2
set g_bot_use_new_ai_system 1

// Start map with bots
devmap dm/mohdm6
addbot
```

### Common Issues

**Issue**: Tree fails to load  
**Fix**: Check YAML syntax, ensure file exists in `behaviors/` directory

**Issue**: Action not found  
**Fix**: Verify action registered in `RegisterCoreBTActions()`, check spelling in YAML

**Issue**: Bot doesn't fire  
**Fix**: Check `WeaponReady` condition, verify `IsAimedAtTarget` returns true

**Issue**: Bot doesn't move  
**Fix**: Check movement conditions (EnemyTooClose/TooFar), verify `movement.MoveTo()` called

**Issue**: Tree always returns FAILURE  
**Fix**: Check condition logic, ensure at least one branch succeeds

### Visualizing Tree Execution

Add to `BotController::ExecuteBehaviorTree()`:

```cpp
if (g_bot_debug->integer >= 3) {
    // Print active nodes
    const char* activePath = behaviorTree->GetActiveNodePath();
    gi.DPrintf("[BT] %s: Active: %s\n", 
        controlledEnt->client->pers.netname, 
        activePath
    );
}
```

---

## Next Steps

After Task 3.1f is complete, you have a **working combat AI**. Next tasks add advanced features:

- **Task 3.1d** (Cover System): Bots find and use cover
- **Task 3.1e** (Tactical Retreat): Bots retreat when hurt
- **Task 3.1g** (Grenades): Bots throw grenades at clusters
- **Task 3.1h** (Weapon Switching): Bots switch weapons intelligently

These integrate into the combat tree as additional branches or decorators.

---

## References

- **BT Framework**: `code/fgame/behavior_tree.h`
- **YAML Loader**: `code/fgame/bt_yaml_loader.h`
- **Action Registry**: `code/fgame/bt_action_registry.h`
- **Existing Trees**: `behaviors/engage_enemy.yaml`, `behaviors/patrol.yaml`
