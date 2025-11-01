# Task 2B.4: Integration & Polish

**Status:** Ready to Execute (after Task 2B.3)  
**Duration:** 1 week (40 hours)  
**Priority:** CRITICAL  
**Phase:** 2B - Brain & Behavior

---

## Context & Background

### What This Task Achieves
Integrates the complete behavior tree system with BotController, adds console commands for runtime control, implements behavior tree visualization, and writes final integration tests. This completes Phase 2B.

### Why This Matters
- **Full Integration:** New AI system works end-to-end
- **Runtime Control:** Switch profiles, reload trees without restart
- **Visual Debugging:** See what bots are thinking in real-time
- **Production Ready:** Feature flag allows safe deployment

### What's Complete (from Tasks 2B.1-2B.3)
- ✅ Behavior Tree framework (7 node types, Blackboard)
- ✅ YAML tree loading with action registry
- ✅ 2 behavior trees (engage_enemy.yaml, patrol.yaml)
- ✅ Complete profile system (5 profiles with all parameters)
- ✅ 4 core actions, 4 core conditions registered

### What's Still Needed
- Integrate BT with BotController
- Load tree specified in profile
- Execute tree in Think() when feature flag enabled
- Console commands (setprofile, listprofiles, blackboard, reload)
- BT visualizer showing active nodes
- 5 integration tests

---

## Technical Specification

### 1. Extend BotController (6 hours)

```cpp
// code/fgame/playerbot.h

class BotController {
public:
    // Existing methods...
    void Think();
    
    // New for Phase 2B
    void LoadProfile(const char* profileName);
    void ReloadProfile();
    BehaviorTree* GetBehaviorTree() { return behaviorTree.get(); }
    Blackboard& GetBlackboard() { return blackboard; }
    BotProfile* GetProfile() { return profile; }

private:
    // Phase 2A (existing)
    PerceptionSystem* perception;
    BotProfile* profile;
    
    // Phase 2B (new)
    std::unique_ptr<BehaviorTree> behaviorTree;
    Blackboard blackboard;
    
    // Helper methods
    void PopulateBlackboard();
    void ExecuteBehaviorTree(float deltaTime);
};
```

```cpp
// code/fgame/playerbot.cpp

BotController::BotController(Player* ent) {
    controlledEnt = ent;
    
    // Phase 2A - Create perception system
    perception = new PerceptionSystem();
    
    // Load profile (default or from config)
    const char* profileName = "balanced";  // TODO: Get from config
    LoadProfile(profileName);
}

void BotController::LoadProfile(const char* profileName) {
    // Load bot profile
    char path[256];
    Com_sprintf(path, sizeof(path), "profiles/%s.yaml", profileName);
    
    profile = BotProfile::LoadFromFile(path);
    if (!profile) {
        gi.Printf("ERROR: Failed to load profile '%s', using default\n", profileName);
        profile = BotProfile::LoadFromFile("profiles/balanced.yaml");
    }
    
    // Load behavior tree specified in profile
    if (profile && g_bot_use_new_ai_system->integer) {
        const char* treeName = profile->GetBehaviorTree();
        char treePath[256];
        Com_sprintf(treePath, sizeof(treePath), "behaviors/%s.yaml", treeName);
        
        behaviorTree = BTYamlLoader::LoadFromFile(treePath);
        if (!behaviorTree) {
            gi.Printf("ERROR: Failed to load behavior tree '%s'\n", treeName);
        } else {
            gi.DPrintf("Bot %d loaded profile '%s' with tree '%s'\n",
                      controlledEnt->entnum, profileName, treeName);
        }
    }
}

void BotController::ReloadProfile() {
    if (profile) {
        LoadProfile(profile->GetName());
    }
}

void BotController::Think() {
    if (!controlledEnt) return;
    
    float deltaTime = level.frametime;
    
    // Phase 2A - Update perception (always runs)
    auto snapshot = perception->Update(controlledEnt, deltaTime);
    
    // Feature flag decides which AI system to use
    if (g_bot_use_new_ai_system->integer) {
        // Phase 2B - New behavior tree system
        PopulateBlackboard();
        ExecuteBehaviorTree(deltaTime);
    } else {
        // Old state machine
        CheckStates();
    }
}

void BotController::PopulateBlackboard() {
    // Get latest perception data
    auto snapshot = perception->GetLastSnapshot();
    
    // Store in blackboard for behavior tree
    blackboard.Set<PerceptionSnapshot*>("perception", &snapshot);
    blackboard.Set<BotController*>("bot", this);
    blackboard.Set<Player*>("entity", controlledEnt);
    blackboard.Set<float>("health", controlledEnt->health);
    blackboard.Set<float>("maxHealth", controlledEnt->max_health);
    
    // Store profile for easy access
    blackboard.Set<BotProfile*>("profile", profile);
}

void BotController::ExecuteBehaviorTree(float deltaTime) {
    if (!behaviorTree) return;
    
    BTNode::Status status = behaviorTree->Execute(blackboard, deltaTime);
    
    // Debug output
    if (g_bot_debug->integer == controlledEnt->entnum) {
        const char* statusStr = (status == BTNode::Status::SUCCESS) ? "SUCCESS" :
                               (status == BTNode::Status::FAILURE) ? "FAILURE" : "RUNNING";
        gi.Printf("Bot %d BT status: %s\n", controlledEnt->entnum, statusStr);
    }
}
```

### 2. Console Commands (4 hours)

```cpp
// code/fgame/g_cmds.cpp

// bot_setprofile <botnum> <profilename>
void Bot_SetProfile_f() {
    if (gi.argc() < 3) {
        gi.Printf("Usage: bot_setprofile <botnum> <profilename>\n");
        gi.Printf("Example: bot_setprofile 0 aggressive\n");
        return;
    }
    
    int botNum = atoi(gi.argv(1));
    const char* profileName = gi.argv(2);
    
    // Find bot
    BotController* bot = GetBotByNum(botNum);
    if (!bot) {
        gi.Printf("ERROR: Bot %d not found\n", botNum);
        return;
    }
    
    // Load profile
    bot->LoadProfile(profileName);
    gi.Printf("Bot %d profile set to '%s'\n", botNum, profileName);
}

// bot_listprofiles
void Bot_ListProfiles_f() {
    gi.Printf("=== Available Bot Profiles ===\n");
    
    // List all .yaml files in profiles/ directory
    const char* profiles[] = {
        "aggressive", "balanced", "defensive", "sniper", "rusher"
    };
    
    for (int i = 0; i < 5; i++) {
        gi.Printf("  %s\n", profiles[i]);
    }
    
    gi.Printf("\nUsage: bot_setprofile <botnum> <profilename>\n");
}

// bot_blackboard <botnum>
void Bot_Blackboard_f() {
    if (gi.argc() < 2) {
        gi.Printf("Usage: bot_blackboard <botnum>\n");
        return;
    }
    
    int botNum = atoi(gi.argv(1));
    BotController* bot = GetBotByNum(botNum);
    
    if (!bot) {
        gi.Printf("ERROR: Bot %d not found\n", botNum);
        return;
    }
    
    gi.Printf("=== Bot %d Blackboard ===\n", botNum);
    
    auto& bb = bot->GetBlackboard();
    
    // Print key values (this is a simplified version)
    if (bb.Has("perception")) {
        auto perc = bb.Get<PerceptionSnapshot*>("perception");
        gi.Printf("  visibleEnemies: %d\n", perc->visibleEnemies.size());
        gi.Printf("  knownEnemies: %d\n", perc->knownEnemies.size());
        gi.Printf("  threatLevel: %d\n", (int)perc->threatLevel);
    }
    
    if (bb.Has("health")) {
        gi.Printf("  health: %.1f / %.1f\n", 
                 bb.Get<float>("health"),
                 bb.Get<float>("maxHealth"));
    }
    
    // Add more as needed...
}

// bot_reload_profiles
void Bot_ReloadProfiles_f() {
    gi.Printf("Reloading all bot profiles...\n");
    
    int count = 0;
    for (int i = 0; i < maxclients->integer; i++) {
        BotController* bot = GetBotByNum(i);
        if (bot) {
            bot->ReloadProfile();
            count++;
        }
    }
    
    gi.Printf("Reloaded profiles for %d bots\n", count);
}

// Register commands
void InitBotCommands() {
    gi.AddCommand("bot_setprofile", Bot_SetProfile_f);
    gi.AddCommand("bot_listprofiles", Bot_ListProfiles_f);
    gi.AddCommand("bot_blackboard", Bot_Blackboard_f);
    gi.AddCommand("bot_reload_profiles", Bot_ReloadProfiles_f);
}
```

### 3. Behavior Tree Visualizer (8 hours)

```cpp
// code/fgame/playerbot_util.cpp

void BotController::DrawDebugVisualization() {
    if (!g_bot_debug->integer) return;
    if (g_bot_debug->integer != controlledEnt->entnum) return;
    
    // Phase 2A - Perception visualization (existing)
    DrawPerceptionDebug();
    
    // Phase 2B - Behavior tree visualization (new)
    if (g_bot_use_new_ai_system->integer && behaviorTree) {
        DrawBehaviorTreeDebug();
    }
}

void BotController::DrawBehaviorTreeDebug() {
    // Draw above bot's head
    Vector screenPos = controlledEnt->origin + Vector(0, 0, 100);
    
    // Draw profile name
    if (profile) {
        G_DebugString(screenPos, 1.0f, 1.0f, 1.0f, 1.0f, profile->GetName());
        screenPos.z += 20;
    }
    
    // Draw tree structure
    BTNode* root = behaviorTree->GetRoot();
    if (root) {
        DrawTreeNode(root, screenPos, 0);
    }
}

void BotController::DrawTreeNode(BTNode* node, Vector pos, int depth) {
    if (!node) return;
    
    // Get node status
    BTNode::Status status = node->GetLastStatus();
    
    // Color by status
    Vector color;
    if (status == BTNode::Status::SUCCESS) {
        color = Vector(0, 1, 0);  // Green
    } else if (status == BTNode::Status::FAILURE) {
        color = Vector(1, 0, 0);  // Red
    } else {
        color = Vector(1, 1, 0);  // Yellow (RUNNING)
    }
    
    // Draw node name
    const char* name = node->GetName();
    G_DebugString(pos, color.x, color.y, color.z, 1.0f, name);
    
    // Draw children (if composite)
    BTComposite* composite = dynamic_cast<BTComposite*>(node);
    if (composite) {
        Vector childPos = pos;
        childPos.z -= 15;
        childPos.x += 20 * (depth + 1);
        
        for (int i = 0; i < composite->GetChildCount(); i++) {
            DrawTreeNode(composite->GetChild(i), childPos, depth + 1);
            childPos.z -= 15;
        }
    }
}
```

### 4. Integration Tests (6 hours)

```cpp
// tests/integration_test_behaviortree.cpp

TEST(BehaviorTreeIntegrationTest, AggressiveProfileAttacksMore) {
    // Create two bots with different profiles
    BotController aggressiveBot;
    aggressiveBot.LoadProfile("aggressive");
    
    BotController defensiveBot;
    defensiveBot.LoadProfile("defensive");
    
    // Set up identical scenarios
    PerceptionSnapshot snapshot;
    EnemyInfo enemy;
    enemy.position = Vector(500, 0, 0);
    enemy.distance = 500.0f;
    snapshot.visibleEnemies.push_back(enemy);
    snapshot.closestEnemy = &snapshot.visibleEnemies[0];
    
    // Run for 100 frames
    int aggressiveAttacks = 0;
    int defensiveAttacks = 0;
    
    for (int i = 0; i < 100; i++) {
        // ... count attacks
    }
    
    // Aggressive should attack more
    EXPECT_GT(aggressiveAttacks, defensiveAttacks);
}

TEST(BehaviorTreeIntegrationTest, DefensiveProfileUsesCover) {
    // Test that defensive profile seeks cover when threatened
    // ...
}

TEST(BehaviorTreeIntegrationTest, TreeSwitchesBasedOnPerception) {
    // Test tree reacts to perception changes
    // ...
}

TEST(BehaviorTreeIntegrationTest, BlackboardStatePersists) {
    // Test blackboard maintains state across frames
    // ...
}

TEST(BehaviorTreeIntegrationTest, ProfileHotReloadWorks) {
    // Test hot-reloading profiles
    // ...
}
```

---

## Implementation Steps

### Day 1-2: BotController Integration (12 hours)
1. Add behaviorTree and blackboard members to BotController
2. Implement LoadProfile() with tree loading
3. Modify Think() to use feature flag
4. Implement PopulateBlackboard()
5. Implement ExecuteBehaviorTree()
6. Test with simple scenarios

### Day 3: Console Commands (8 hours)
1. Implement bot_setprofile
2. Implement bot_listprofiles
3. Implement bot_blackboard
4. Implement bot_reload_profiles
5. Register all commands
6. Test each command

### Day 4: BT Visualizer (8 hours)
1. Implement DrawBehaviorTreeDebug()
2. Implement DrawTreeNode() recursively
3. Color-code by status
4. Show profile name
5. Test visualization in-game

### Day 5: Integration Tests & Polish (12 hours)
1. Write 5 integration tests
2. Fix any bugs found
3. Performance testing
4. Documentation updates
5. Final testing with all profiles

---

## Files to Create/Modify

### Modified Files
- `code/fgame/playerbot.h` - Add BT members
- `code/fgame/playerbot.cpp` - Integrate BT execution
- `code/fgame/playerbot_util.cpp` - Add BT visualization
- `code/fgame/g_cmds.cpp` - Add console commands

### New Files
- `tests/integration_test_behaviortree.cpp` - Integration tests

---

## Acceptance Criteria

- [ ] BotController loads profile and behavior tree on spawn
- [ ] Think() executes BT when g_bot_use_new_ai_system=1
- [ ] Think() uses old state machine when flag=0
- [ ] Blackboard populated with perception data each frame
- [ ] bot_setprofile command works
- [ ] bot_listprofiles lists all 5 profiles
- [ ] bot_blackboard shows current state
- [ ] bot_reload_profiles hot-reloads all bots
- [ ] BT visualizer shows tree structure with status colors
- [ ] 5 integration tests pass
- [ ] No regressions when flag=0
- [ ] Performance < 10% overhead when flag=1

---

## Testing Checklist

Manual Testing:
- [ ] Load game with g_bot_use_new_ai_system 0 → old AI works
- [ ] Load game with g_bot_use_new_ai_system 1 → new AI works
- [ ] Switch profiles mid-game → behavior changes
- [ ] Reload profiles → changes take effect
- [ ] BT visualizer shows active nodes
- [ ] Different profiles behave differently
- [ ] Bots engage enemies appropriately
- [ ] Bots retreat when low health

Automated Testing:
- [ ] All 43 tests pass (26 from 2A + 17 from 2B)
- [ ] Performance profiled, < 10% overhead
- [ ] Memory leaks checked (valgrind)

---

## Phase 2B Complete! 🎉

After this task:
- ✅ Complete behavior tree system
- ✅ YAML-driven behaviors and profiles
- ✅ 43 tests passing
- ✅ Debug visualization
- ✅ Console commands
- ✅ Feature flag for safe deployment

**Next:** Phase 3 will migrate all remaining behaviors to behavior trees and add Utility AI.
