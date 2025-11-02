# Task 3.1: Migrate Attack Behavior - Implementation Guide

This directory contains 8 self-contained subtasks for migrating the attack behavior from the old state machine to the Behavior Tree system.

## Implementation Order (Foundation-First Strategy)

The subtasks are designed to be implemented in this specific order to build a solid, testable foundation:

### Phase 1: Core Combat Loop (Days 1-7)
Build the essential combat mechanics first - this gives us a working, testable combat system early.

1. **[Task 3.1.01](Task-3.1.01-Target-Selection-Tracking.md)** - Target Selection & Tracking (2 days)
   - Implements enemy scanning, target stickiness, target switching
   - **Provides**: SelectTarget action, HasValidTarget condition
   - **Requires**: PerceptionSystem, Blackboard
   - **Test independently**: Unit tests for target selection logic

2. **[Task 3.1.02](Task-3.1.02-Aiming-Fire-Control.md)** - Aiming & Fire Control (3 days)
   - Implements smooth aiming, burst/continuous fire, melee attacks
   - **Provides**: AimAtTarget, FireWeapon, MeleeAttack actions
   - **Requires**: Task 3.1.01 (for target)
   - **Test independently**: Unit tests for aim convergence, burst timing

3. **[Task 3.1.03](Task-3.1.03-Combat-Movement.md)** - Combat Movement (2 days)
   - Implements distance-based approach/retreat, range checking
   - **Provides**: MoveToCombatPosition action, InWeaponRange condition
   - **Requires**: Task 3.1.01 (for target), Task 3.1.02 (for weapon state)
   - **Test independently**: Unit tests for movement decisions

### Phase 2: Basic Combat Tree (Day 8-9)
Assemble the core combat behaviors into a functional tree.

4. **[Task 3.1.04](Task-3.1.04-Combat-Tree-Assembly.md)** - Combat Tree Assembly (2 days)
   - Creates YAML tree using actions from 3.1.01-03
   - Registers all actions/conditions
   - **Provides**: Complete basic combat behavior tree
   - **Requires**: Tasks 3.1.01, 3.1.02, 3.1.03
   - **Test end-to-end**: Bot engages enemies, aims, fires, moves

**Milestone**: After 3.1.04, bots have functional combat AI that can be tested in-game.

5. **[Task 3.1.05](Task-3.1.05-Weapon-Switching.md)** - Weapon Switching (1 day) ⚡ **Priority after milestone**
   - Implements automatic weapon switching logic
   - **Provides**: SwitchWeapon action, ShouldSwitchWeapon condition
   - **Requires**: Core combat tree (3.1.04)
   - **Why now**: Combat testing immediately reveals ammo issues; weapon switching enables more robust testing of subsequent tactical behaviors
   - **Test independently**: Unit tests for weapon selection

### Phase 3: Advanced Tactical Systems (Days 10-16)
Add sophisticated behaviors that enhance the core combat loop.

6. **[Task 3.1.06](Task-3.1.06-Cover-System.md)** - Cover System Integration (3 days)
   - Integrates existing cover finding/movement/peeking logic
   - **Provides**: FindCover, MoveToCover, PeekFromCover actions
   - **Requires**: Core combat tree (3.1.04)
   - **Test independently**: Unit tests for cover evaluation

7. **[Task 3.1.07](Task-3.1.07-Tactical-Retreat.md)** - Tactical Combat & Retreat (2 days)
   - Implements retreat logic, suppression fire, reload management
   - **Provides**: ShouldRetreat, TacticalRetreat, SuppressFire actions
   - **Requires**: Core combat tree (3.1.04), Task 3.1.06 (for cover)
   - **Test independently**: Unit tests for retreat triggers

8. **[Task 3.1.08](Task-3.1.08-Grenade-System.md)** - Grenade System (1 day)
   - Implements grenade throwing with cluster detection
   - **Provides**: ThrowGrenade action, ShouldThrowGrenade condition
   - **Requires**: Core combat tree (3.1.04)
   - **Test independently**: Unit tests for cluster detection

## Dependency Graph

```
        ┌─────────────────────────────────┐
        │   PerceptionSystem (Phase 2A)   │
        │   BehaviorTree Framework (2B)   │
        │   Bot Profile System (2B)       │
        └────────────────┬────────────────┘
                         │
                         ▼
        ┌────────────────────────────────┐
        │   3.1.01: Target Selection     │ (2 days)
        └────────────┬───────────────────┘
                     │
                     ▼
        ┌────────────────────────────────┐
        │ 3.1.02: Aiming & Fire Control  │ (3 days)
        └────────────┬───────────────────┘
                     │
                     ▼
        ┌────────────────────────────────┐
        │   3.1.03: Combat Movement      │ (2 days)
        └────────────┬───────────────────┘
                     │
                     ▼
        ┌────────────────────────────────┐
        │3.1.04: Combat Tree Assembly    │ (2 days) ◄─── MILESTONE
        └────────────┬───────────────────┘
                     │
                     ▼
        ┌────────────────────────────────┐
        │  3.1.05: Weapon Switching      │ (1 day) ⚡ Priority
        └────┬───────┬───────┬───────────┘
             │       │       │
     ┌───────┘       │       └──────┐
     ▼               ▼              ▼
┌─────────┐   ┌────────────┐  ┌──────────┐
│ 3.1.06: │   │  3.1.07:   │  │ 3.1.08:  │
│ Cover   │   │ Tactical   │  │ Grenade  │
│ System  │   │ Retreat    │  │ System   │
│(3 days) │   │ (2 days)   │  │ (1 day)  │
└─────────┘   └────────────┘  └──────────┘
```

## Testing Strategy

### Unit Tests (Per Subtask)
Each subtask includes 2-6 unit tests for its specific functionality:
- 3.1a: 4 tests (target selection, stickiness, switching, validation)
- 3.1b: 6 tests (aim convergence, burst timing, melee, fire modes)
- 3.1c: 3 tests (approach, retreat, strafe)
- 3.1d: 4 tests (cover finding, quality evaluation, movement, peeking)
- 3.1e: 3 tests (retreat triggers, suppression, reload timing)
- 3.1f: 6 tests (tree loading, execution flow, profile integration)
- 3.1g: 2 tests (cluster detection, grenade cooldown)
- 3.1h: 2 tests (weapon selection, auto-switch)

**Total**: ~30 unit tests

### Integration Tests (After Each Phase)
- **After 3.1f**: Test complete combat engagement (aim, fire, move)
- **After 3.1h**: Test all systems together (cover, retreat, grenades, weapon switch)

### Manual Testing Checkpoints
1. **After 3.1c**: Basic combat works (bot shoots at enemies)
2. **After 3.1f**: Combat feels responsive (bot engages effectively)
3. **After 3.1h**: Advanced tactics visible (bot uses cover, retreats, throws grenades)

## File Organization

```
Task-3.1-Migrate-Attack-Behavior/
├── README.md                                   # This file
├── Task-3.1.01-Target-Selection-Tracking.md   # Execute first
├── Task-3.1.02-Aiming-Fire-Control.md         # Execute second
├── Task-3.1.03-Combat-Movement.md             # Execute third
├── Task-3.1.04-Combat-Tree-Assembly.md        # Execute fourth (MILESTONE)
├── Task-3.1.05-Weapon-Switching.md            # Execute fifth
├── Task-3.1.06-Cover-System.md                # Execute sixth
├── Task-3.1.07-Tactical-Retreat.md            # Execute seventh
└── Task-3.1.08-Grenade-System.md              # Execute eighth
```

## Success Criteria (Overall)

After completing all 8 subtasks:

### Functionality
- [ ] Bots engage visible enemies with smooth aiming
- [ ] Bots manage combat distance based on weapon range
- [ ] Bots use cover when available
- [ ] Bots retreat when health is critical
- [ ] Bots throw grenades at clustered enemies
- [ ] Bots switch weapons when appropriate
- [ ] Different profiles show distinct playstyles

### Quality
- [ ] ~30 unit tests pass
- [ ] 6+ integration tests pass
- [ ] Zero regressions with `g_bot_use_new_ai_system=0`
- [ ] Code follows OpenMoHAA standards
- [ ] All functions < 100 lines

### Performance
- [ ] Combat tree execution < 0.5ms per bot
- [ ] Supports 50+ bots without FPS drop

## Notes

- **Feature Flag**: All new behavior tree code runs only when `g_bot_use_new_ai_system=1`
- **Old System**: Original state machine (`playerbot_attack.cpp`) remains untouched until Phase 3 Task 3.6
- **Incremental Testing**: Each subtask can be tested independently before moving to the next
- **Blackboard Keys**: Use consistent key names defined in `bt_blackboard_keys.h`
- **Shared Utilities**: During implementation, refactor common checks (e.g., `IsValidEnemy()`, line-of-sight, distance calculations) into `BotUtility` namespace to avoid duplication and technical debt

## Next Phase

After Task 3.1 is complete, proceed to:
- **Task 3.2**: Migrate Investigation Behavior (search patterns for lost enemies)
- **Task 3.3**: Migrate Idle Behaviors (patrol, waypoints)
