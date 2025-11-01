# Task 3.6: Remove Old State Machine

**Status:** Ready to Execute  
**Duration:** 1 week  
**Priority:** LOW  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Removes the legacy priority-based state machine code after verifying all behaviors have been successfully migrated to the new Behavior Tree system.

### Why This Matters
- **Code Cleanliness:** Remove 1000+ lines of obsolete code
- **Maintainability:** Single source of truth for bot behavior
- **Clarity:** No confusion about which system is active
- **Performance:** Remove dead code paths

### What Must Be Complete First
- ✅ Task 3.1: Combat behaviors migrated
- ✅ Task 3.2: Investigation behaviors migrated
- ✅ Task 3.3: Idle behaviors migrated
- ✅ Task 3.4: Utility AI functional
- ✅ All behaviors tested and working in new system
- ✅ Feature flag `g_bot_use_new_ai_system` defaulted to 1

**CRITICAL:** Do NOT start this task until all behaviors work perfectly in new system!

---

## Migration Verification Checklist

### Before Removing Anything

Run comprehensive tests to ensure new system matches or exceeds old system:

#### Combat Behaviors
- [ ] Bot engages visible enemies
- [ ] Bot aims and fires accurately
- [ ] Bot manages combat distance (backs away when too close)
- [ ] Bot reloads when ammo low
- [ ] Bot retreats when health critical
- [ ] Bot throws grenades appropriately
- [ ] Bot switches weapons when empty
- [ ] Bot handles multiple enemies

#### Investigation Behaviors
- [ ] Bot investigates last known enemy position
- [ ] Bot searches area systematically
- [ ] Bot investigates weapon fire sounds
- [ ] Bot gives up after timeout
- [ ] Bot returns to patrol after investigation

#### Idle Behaviors
- [ ] Bot follows patrol routes
- [ ] Bot wanders when no patrol
- [ ] Bot investigates curious sounds briefly
- [ ] Bot uses attractive nodes (sniper positions)
- [ ] Bot stands idle when nothing to do

#### Transitions
- [ ] Combat → Investigation (enemy lost)
- [ ] Investigation → Combat (enemy found)
- [ ] Investigation → Idle (timeout)
- [ ] Idle → Combat (enemy detected)
- [ ] Idle → Investigation (sound heard)

#### Edge Cases
- [ ] Bot doesn't get stuck in walls
- [ ] Bot handles unreachable paths
- [ ] Bot doesn't spam state changes
- [ ] Bot works on all maps
- [ ] Bot works with all weapons

#### Performance
- [ ] New system performs as well or better than old
- [ ] No frame drops with 50+ bots
- [ ] Memory usage acceptable

---

## Code to Remove

### Primary Files to Delete/Gut

#### 1. `code/fgame/playerbot_state.cpp`
```cpp
// This entire file can be deleted
// Contains:
// - State_Attack()
// - State_Investigate()
// - State_Curious()
// - State_Idle()
// - State_Weapon()
// - State_Grenade()
// - CheckCondition_*() functions
// - BeginState_*() functions
// - EndState_*() functions
```

**Estimated lines removed:** 800-1000

#### 2. `code/fgame/playerbot.h`
```cpp
// Remove state machine members:
class BotController {
    // DELETE THESE:
    int m_iState;
    int m_iPrevState;
    float m_fStateChangeTime;
    
    // State functions (delete declarations)
    void State_Attack();
    void State_Investigate();
    void State_Curious();
    void State_Idle();
    void State_Weapon();
    void State_Grenade();
    
    bool CheckCondition_Attack();
    bool CheckCondition_Investigate();
    bool CheckCondition_Curious();
    bool CheckCondition_Idle();
    // ... etc
    
    void BeginAttack();
    void BeginInvestigate();
    // ... etc
    
    void EndAttack();
    void EndInvestigate();
    // ... etc
    
    // DELETE state-related variables:
    Vector m_vLastEnemyPos;
    float m_iLastEnemyTime;
    Vector m_vCuriousPos;
    bool m_bHeardSound;
    // ... etc
};
```

**Estimated lines removed:** 100-150

#### 3. `code/fgame/playerbot_core.cpp`
```cpp
// Remove state machine initialization and execution

void BotController::Think() {
    // DELETE OLD:
    // CheckStates();
    // ExecuteCurrentState();
    
    // KEEP NEW:
    // Perception update
    // Utility AI evaluation
    // Behavior tree execution
}

// DELETE ENTIRE FUNCTION:
void BotController::CheckStates() {
    // Priority-based state checking
}

// DELETE ENTIRE FUNCTION:
void BotController::ExecuteCurrentState() {
    // State function table lookup and execution
}

// DELETE:
static botfunc_t botfuncs[MAX_BOT_FUNCTIONS] = {
    {&CheckCondition_Attack, &BeginAttack, &EndAttack, &State_Attack},
    {&CheckCondition_Investigate, &BeginInvestigate, &EndInvestigate, &State_Investigate},
    // ... etc
};
```

**Estimated lines removed:** 200-300

### Files to Modify (Not Delete)

#### `code/fgame/playerbot.cpp`
- Remove state machine Think() logic
- Keep perception updates
- Keep BT/utility AI execution
- Remove state transition logging

#### `code/fgame/playerbot_util.cpp`
- Remove old debug visualization for states
- Keep new BT/utility debug viz

---

## Step-by-Step Removal Plan

### Day 1: Backup & Preparation (4 hours)
- [ ] Create git branch: `remove-old-state-machine`
- [ ] Run full test suite (all tests must pass)
- [ ] Take performance measurements (baseline)
- [ ] Document any custom state machine behaviors not yet migrated
- [ ] Review all uses of state-related variables with grep

### Day 2: Remove State Functions (6 hours)
- [ ] Comment out (don't delete yet) all State_*() functions
- [ ] Comment out CheckCondition_*() functions
- [ ] Comment out BeginState_*() and EndState_*() functions
- [ ] Build and test
- [ ] Fix any compilation errors
- [ ] Run tests (all must pass)

### Day 3: Remove State Machine Core (6 hours)
- [ ] Remove botfuncs array
- [ ] Remove CheckStates() function
- [ ] Remove ExecuteCurrentState() function
- [ ] Remove state-related Think() logic
- [ ] Build and test
- [ ] Run full test suite
- [ ] Manual testing on multiple maps

### Day 4: Remove State Members (4 hours)
- [ ] Remove m_iState, m_iPrevState, etc. from BotController
- [ ] Remove state-related member variables
- [ ] Find all references with grep (should be none left)
- [ ] Build and test
- [ ] Run full test suite

### Day 5: Delete Files (2 hours)
- [ ] Delete `playerbot_state.cpp` entirely
- [ ] Update CMakeLists.txt to remove file
- [ ] Build and test
- [ ] Run full test suite

### Day 6: Cleanup & Verification (6 hours)
- [ ] Remove old debug visualization code
- [ ] Remove state-related console commands
- [ ] Remove cvars related to old system
- [ ] Remove comments referencing old states
- [ ] Update code comments
- [ ] Build and test
- [ ] Run full test suite
- [ ] Performance testing

### Day 7: Documentation & Testing (4 hours)
- [ ] Update CLAUDE.md (remove state machine documentation)
- [ ] Update architecture documents
- [ ] Final comprehensive testing
- [ ] Performance comparison (before/after)
- [ ] Create pull request
- [ ] Code review

---

## Safety Measures

### Rollback Plan
If issues discovered after removal:

1. **Immediate:** Revert git commit
2. **Short-term:** Cherry-pick specific fixes to new system
3. **Long-term:** Re-evaluate migration quality

### Feature Flag Transition
```cpp
// Current (Phase 3.1-3.5):
if (g_bot_use_new_ai_system->integer) {
    // New BT system
} else {
    // Old state machine
}

// After removal (Phase 3.6):
// Old code deleted, only new system remains
// Remove feature flag entirely
```

### Testing Requirements
- [ ] All 50+ tests pass (unit + integration)
- [ ] Manual testing on 5+ different maps
- [ ] Performance matches or beats old system
- [ ] No memory leaks
- [ ] No crashes after 1 hour of gameplay

---

## Files to Create/Modify

### New Files
```
docs/REMOVED_STATE_MACHINE.md        # Document what was removed and why
tests/test_removal_verification.cpp  # Comprehensive behavior verification
```

### Modified Files
```
code/fgame/playerbot.h               # Remove state declarations
code/fgame/playerbot.cpp             # Remove state execution
code/fgame/playerbot_core.cpp        # Remove CheckStates(), ExecuteCurrentState()
code/fgame/playerbot_util.cpp        # Remove old debug viz
code/fgame/CMakeLists.txt            # Remove playerbot_state.cpp
CLAUDE.md                            # Update documentation
```

### Deleted Files
```
code/fgame/playerbot_state.cpp       # Entire file deleted
```

---

## Acceptance Criteria

### Code Removal
- [ ] playerbot_state.cpp deleted
- [ ] All State_*() functions removed
- [ ] All CheckCondition_*() functions removed
- [ ] botfuncs array removed
- [ ] State machine core logic removed
- [ ] State-related member variables removed
- [ ] Old debug visualization removed

### Quality
- [ ] All tests pass (100% pass rate)
- [ ] Zero compilation warnings
- [ ] Zero new bugs introduced
- [ ] Performance equal or better
- [ ] Code compiles cleanly
- [ ] No dead code references

### Documentation
- [ ] CLAUDE.md updated
- [ ] REMOVED_STATE_MACHINE.md created
- [ ] Code comments updated
- [ ] Architecture docs updated

---

## Success Metrics

### Code Quality
- **Lines Removed:** 1000-1500 lines
- **Files Deleted:** 1 file
- **Warnings:** 0
- **Complexity:** Reduced significantly

### Maintainability
- **Single System:** Only BT system remains
- **Clarity:** No confusion about active system
- **Future Development:** Only one system to maintain

---

## Troubleshooting

### Build Errors After Removal
- Check for lingering references with grep
- Ensure all state-related includes removed
- Verify CMakeLists.txt updated

### Behavioral Regressions
- Compare with pre-removal video recordings
- Check test coverage
- May need to refine new behaviors

### Performance Issues
- Profile before and after
- Check for introduced bottlenecks
- May need optimization pass

---

## Notes & Considerations

### Why Wait Until End of Phase 3?
- All behaviors must be migrated first
- Need extensive testing period
- Must prove new system superior
- Rollback easier if old code intact

### What If New System Has Gaps?
- Document gaps before removal
- Implement missing behaviors
- Don't remove until feature parity

### Post-Removal Benefits
- Cleaner codebase
- Easier maintenance
- No confusion about which system to use
- Forced commitment to new architecture

---

**Next Task:** Task 3.7 - Performance Tuning
