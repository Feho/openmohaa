# Task 2B.2: YAML Tree Loading

**Status:** Ready to Execute (after Task 2B.1)  
**Duration:** 8 hours  
**Priority:** HIGH  
**Phase:** 2B - Brain & Behavior

---

## Context & Background

### What This Task Achieves
Enables behavior trees to be defined in YAML files instead of C++ code, allowing designers to create and modify bot behaviors without programming.

### Why This Matters
- **Designer Empowerment:** Non-programmers can create bot behaviors
- **Rapid Iteration:** Edit YAML, reload in-game, test immediately
- **Version Control Friendly:** YAML diffs are easy to review
- **Reduced Compilation:** No C++ recompile for behavior changes

### What's Already Complete (from Task 2B.1)
- ✅ Core BT node types (Selector, Sequence, Parallel, Condition, Action)
- ✅ Blackboard system
- ✅ BehaviorTreeBuilder fluent interface
- ✅ CreateEngageEnemyTree() in C++
- ✅ 12 tests passing

### What's Still Needed
- Load trees from YAML files
- Map action/condition names to C++ functions
- Validate YAML structure
- Error handling with helpful messages

---

## Technical Specification

### YAML Tree Format

```yaml
# behaviors/engage_enemy.yaml
tree:
  name: "Engage Enemy"
  description: "Basic combat behavior"
  
  root:
    type: selector
    children:
      # Has enemy? Attack
      - type: sequence
        name: "Combat Sequence"
        children:
          - type: condition
            check: "HasVisibleEnemy"
          
          - type: action
            action: "AimAtEnemy"
          
          - type: action
            action: "ShootEnemy"
      
      # No enemy? Idle
      - type: action
        action: "Idle"
```

### Action/Condition Registry Pattern

Create a registry to map YAML names to C++ functions:

```cpp
// code/fgame/bt_action_registry.h

#ifndef __BT_ACTION_REGISTRY_H__
#define __BT_ACTION_REGISTRY_H__

#include "behavior_tree.h"
#include <unordered_map>
#include <string>

/**
 * Registry for mapping action/condition names to C++ functions.
 * Used by YAML loader to create BTAction and BTCondition nodes.
 */
class BTActionRegistry {
public:
    using ActionFunc = BTAction::ActionFunc;
    using ConditionFunc = BTCondition::ConditionFunc;
    
    static BTActionRegistry& Instance() {
        static BTActionRegistry instance;
        return instance;
    }
    
    /**
     * Register an action that can be used in YAML.
     */
    void RegisterAction(const std::string& name, ActionFunc func) {
        actions[name] = func;
    }
    
    /**
     * Register a condition that can be used in YAML.
     */
    void RegisterCondition(const std::string& name, ConditionFunc func) {
        conditions[name] = func;
    }
    
    /**
     * Get action by name. Returns nullptr if not found.
     */
    ActionFunc GetAction(const std::string& name) const {
        auto it = actions.find(name);
        return (it != actions.end()) ? it->second : nullptr;
    }
    
    /**
     * Get condition by name. Returns nullptr if not found.
     */
    ConditionFunc GetCondition(const std::string& name) const {
        auto it = conditions.find(name);
        return (it != conditions.end()) ? it->second : nullptr;
    }
    
    /**
     * Check if action exists.
     */
    bool HasAction(const std::string& name) const {
        return actions.find(name) != actions.end();
    }
    
    /**
     * Check if condition exists.
     */
    bool HasCondition(const std::string& name) const {
        return conditions.find(name) != conditions.end();
    }
    
private:
    BTActionRegistry() = default;
    
    std::unordered_map<std::string, ActionFunc> actions;
    std::unordered_map<std::string, ConditionFunc> conditions;
};

// Convenience macro for registering actions/conditions
#define REGISTER_BT_ACTION(name, func) \
    BTActionRegistry::Instance().RegisterAction(name, func)

#define REGISTER_BT_CONDITION(name, func) \
    BTActionRegistry::Instance().RegisterCondition(name, func)

#endif // __BT_ACTION_REGISTRY_H__
```

### Register Core Actions/Conditions

```cpp
// code/fgame/bt_core_actions.cpp (new file)

#include "bt_action_registry.h"
#include "perception.h"
#include "playerbot.h"

/**
 * Register all core actions and conditions.
 * Called on game initialization.
 */
void RegisterCoreBTActions() {
    // === CONDITIONS ===
    
    REGISTER_BT_CONDITION("HasVisibleEnemy", [](Blackboard& bb) {
        auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
        return snapshot->HasVisibleEnemy();
    });
    
    REGISTER_BT_CONDITION("HasKnownEnemy", [](Blackboard& bb) {
        auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
        return snapshot->HasKnownEnemy();
    });
    
    REGISTER_BT_CONDITION("LowHealth", [](Blackboard& bb) {
        auto bot = bb.Get<BotController*>("bot");
        float healthPercent = bot->GetHealth() / bot->GetMaxHealth();
        return healthPercent < 0.25f;
    });
    
    REGISTER_BT_CONDITION("HasAmmo", [](Blackboard& bb) {
        auto bot = bb.Get<BotController*>("bot");
        return bot->HasAmmo();
    });
    
    // === ACTIONS ===
    
    REGISTER_BT_ACTION("AimAtEnemy", [](Blackboard& bb, float dt) {
        auto bot = bb.Get<BotController*>("bot");
        auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
        
        if (snapshot->closestEnemy) {
            bot->AimAt(snapshot->closestEnemy->position);
            return BTNode::Status::SUCCESS;
        }
        return BTNode::Status::FAILURE;
    });
    
    REGISTER_BT_ACTION("ShootEnemy", [](Blackboard& bb, float dt) {
        auto bot = bb.Get<BotController*>("bot");
        auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
        
        if (snapshot->closestEnemy && 
            snapshot->closestEnemy->distance < 1024.0f) {
            bot->Fire();
            return BTNode::Status::SUCCESS;
        }
        return BTNode::Status::FAILURE;
    });
    
    REGISTER_BT_ACTION("Idle", [](Blackboard& bb, float dt) {
        // Stand still
        return BTNode::Status::SUCCESS;
    });
    
    REGISTER_BT_ACTION("Retreat", [](Blackboard& bb, float dt) {
        auto bot = bb.Get<BotController*>("bot");
        // Move away from closest enemy
        auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
        if (snapshot->closestEnemy) {
            Vector awayDir = bot->GetOrigin() - snapshot->closestEnemy->position;
            awayDir.normalize();
            bot->MoveTo(bot->GetOrigin() + awayDir * 512.0f);
            return BTNode::Status::RUNNING;  // Multi-frame
        }
        return BTNode::Status::FAILURE;
    });
}
```

### YAML Loader

```cpp
// code/fgame/bt_yaml_loader.h

#ifndef __BT_YAML_LOADER_H__
#define __BT_YAML_LOADER_H__

#include "behavior_tree.h"
#include "behavior_tree_builder.h"
#include "bt_action_registry.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <memory>

/**
 * Loads behavior trees from YAML files.
 */
class BTYamlLoader {
public:
    /**
     * Load a behavior tree from YAML file.
     * @param filepath Path to YAML file (e.g., "behaviors/engage_enemy.yaml")
     * @return Loaded tree, or nullptr on error
     */
    static std::unique_ptr<BehaviorTree> LoadFromFile(const char* filepath) {
        try {
            YAML::Node root = YAML::LoadFile(filepath);
            
            if (!root["tree"]) {
                gi.Printf("ERROR: YAML file missing 'tree' root node: %s\n", filepath);
                return nullptr;
            }
            
            YAML::Node treeNode = root["tree"];
            
            // Optional metadata
            if (treeNode["name"]) {
                gi.DPrintf("Loading behavior tree: %s\n", 
                          treeNode["name"].as<std::string>().c_str());
            }
            
            // Load root node
            if (!treeNode["root"]) {
                gi.Printf("ERROR: Tree missing 'root' node: %s\n", filepath);
                return nullptr;
            }
            
            auto tree = std::make_unique<BehaviorTree>();
            auto rootNode = LoadNode(treeNode["root"], filepath);
            
            if (!rootNode) {
                gi.Printf("ERROR: Failed to load root node: %s\n", filepath);
                return nullptr;
            }
            
            tree->SetRoot(std::move(rootNode));
            return tree;
            
        } catch (const YAML::Exception& e) {
            gi.Printf("ERROR: YAML parse error in %s: %s\n", filepath, e.what());
            return nullptr;
        }
    }

private:
    /**
     * Recursively load a node from YAML.
     */
    static std::unique_ptr<BTNode> LoadNode(const YAML::Node& node, const char* filepath) {
        if (!node["type"]) {
            gi.Printf("ERROR: Node missing 'type' field in %s\n", filepath);
            return nullptr;
        }
        
        std::string type = node["type"].as<std::string>();
        
        // Composite nodes
        if (type == "selector") {
            return LoadSelector(node, filepath);
        }
        else if (type == "sequence") {
            return LoadSequence(node, filepath);
        }
        else if (type == "parallel") {
            return LoadParallel(node, filepath);
        }
        // Leaf nodes
        else if (type == "condition") {
            return LoadCondition(node, filepath);
        }
        else if (type == "action") {
            return LoadAction(node, filepath);
        }
        else {
            gi.Printf("ERROR: Unknown node type '%s' in %s\n", type.c_str(), filepath);
            return nullptr;
        }
    }
    
    static std::unique_ptr<BTNode> LoadSelector(const YAML::Node& node, const char* filepath) {
        auto selector = std::make_unique<BTSelector>();
        
        if (!node["children"]) {
            gi.Printf("ERROR: Selector missing 'children' in %s\n", filepath);
            return nullptr;
        }
        
        for (const auto& childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            static_cast<BTComposite*>(selector.get())->AddChild(std::move(child));
        }
        
        return selector;
    }
    
    static std::unique_ptr<BTNode> LoadSequence(const YAML::Node& node, const char* filepath) {
        auto sequence = std::make_unique<BTSequence>();
        
        if (!node["children"]) {
            gi.Printf("ERROR: Sequence missing 'children' in %s\n", filepath);
            return nullptr;
        }
        
        for (const auto& childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            static_cast<BTComposite*>(sequence.get())->AddChild(std::move(child));
        }
        
        return sequence;
    }
    
    static std::unique_ptr<BTNode> LoadParallel(const YAML::Node& node, const char* filepath) {
        BTParallel::Policy policy = BTParallel::Policy::RequireAll;
        
        if (node["policy"]) {
            std::string policyStr = node["policy"].as<std::string>();
            if (policyStr == "RequireOne") {
                policy = BTParallel::Policy::RequireOne;
            }
            else if (policyStr == "RequireN") {
                policy = BTParallel::Policy::RequireN;
            }
        }
        
        int requiredCount = node["required_count"] ? node["required_count"].as<int>() : 0;
        
        auto parallel = std::make_unique<BTParallel>(policy, requiredCount);
        
        if (!node["children"]) {
            gi.Printf("ERROR: Parallel missing 'children' in %s\n", filepath);
            return nullptr;
        }
        
        for (const auto& childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            static_cast<BTComposite*>(parallel.get())->AddChild(std::move(child));
        }
        
        return parallel;
    }
    
    static std::unique_ptr<BTNode> LoadCondition(const YAML::Node& node, const char* filepath) {
        if (!node["check"]) {
            gi.Printf("ERROR: Condition missing 'check' field in %s\n", filepath);
            return nullptr;
        }
        
        std::string checkName = node["check"].as<std::string>();
        
        auto func = BTActionRegistry::Instance().GetCondition(checkName);
        if (!func) {
            gi.Printf("ERROR: Unknown condition '%s' in %s\n", checkName.c_str(), filepath);
            gi.Printf("       Available conditions:\n");
            // TODO: List registered conditions
            return nullptr;
        }
        
        return std::make_unique<BTCondition>(checkName.c_str(), func);
    }
    
    static std::unique_ptr<BTNode> LoadAction(const YAML::Node& node, const char* filepath) {
        if (!node["action"]) {
            gi.Printf("ERROR: Action missing 'action' field in %s\n", filepath);
            return nullptr;
        }
        
        std::string actionName = node["action"].as<std::string>();
        
        auto func = BTActionRegistry::Instance().GetAction(actionName);
        if (!func) {
            gi.Printf("ERROR: Unknown action '%s' in %s\n", actionName.c_str(), filepath);
            gi.Printf("       Available actions:\n");
            // TODO: List registered actions
            return nullptr;
        }
        
        return std::make_unique<BTAction>(actionName.c_str(), func);
    }
};

#endif // __BT_YAML_LOADER_H__
```

---

## Implementation Steps

### Step 1: Create Action Registry (2 hours)
1. Create `bt_action_registry.h`
2. Implement singleton registry
3. Add RegisterAction/RegisterCondition methods
4. Add convenience macros

### Step 2: Register Core Actions (2 hours)
1. Create `bt_core_actions.cpp`
2. Implement RegisterCoreBTActions()
3. Register conditions: HasVisibleEnemy, HasKnownEnemy, LowHealth, HasAmmo
4. Register actions: AimAtEnemy, ShootEnemy, Idle, Retreat
5. Call RegisterCoreBTActions() in game initialization

### Step 3: Create YAML Loader (3 hours)
1. Create `bt_yaml_loader.h`
2. Implement LoadFromFile()
3. Implement LoadNode() (recursive)
4. Implement loaders for each node type
5. Add error handling with helpful messages

### Step 4: Create YAML Behavior Files (1 hour)
1. Create `behaviors/engage_enemy.yaml` (replaces C++ version)
2. Create `behaviors/patrol.yaml` (simple patrol behavior)
3. Validate YAML syntax

---

## YAML Behavior Examples

### engage_enemy.yaml

```yaml
# behaviors/engage_enemy.yaml
tree:
  name: "Engage Enemy"
  description: "Basic combat behavior using perception data"
  
  root:
    type: selector
    children:
      # Low health? Retreat
      - type: sequence
        name: "Emergency Retreat"
        children:
          - type: condition
            check: "LowHealth"
          
          - type: action
            action: "Retreat"
      
      # Has visible enemy? Attack
      - type: sequence
        name: "Combat"
        children:
          - type: condition
            check: "HasVisibleEnemy"
          
          - type: condition
            check: "HasAmmo"
          
          - type: action
            action: "AimAtEnemy"
          
          - type: action
            action: "ShootEnemy"
      
      # No enemy? Idle
      - type: action
        action: "Idle"
```

### patrol.yaml

```yaml
# behaviors/patrol.yaml
tree:
  name: "Patrol"
  description: "Simple patrol behavior"
  
  root:
    type: selector
    children:
      # Heard sound? Investigate
      - type: sequence
        name: "Investigate Sound"
        children:
          - type: condition
            check: "HeardRecentSound"
          
          - type: action
            action: "MoveToSound"
      
      # Default: patrol waypoints
      - type: action
        action: "PatrolWaypoints"
```

---

## Testing

### Manual Testing Steps
1. Create `behaviors/engage_enemy.yaml`
2. Load tree in-game: `bt_load engage_enemy`
3. Spawn bot with tree
4. Verify bot engages enemies
5. Modify YAML (e.g., change condition)
6. Reload: `bt_reload engage_enemy`
7. Verify behavior changed

### Error Testing
1. Test missing 'type' field → helpful error
2. Test unknown action name → list available actions
3. Test invalid YAML syntax → parse error with line number
4. Test missing 'children' on composite → clear error

---

## Files to Create

### New Files
- `code/fgame/bt_action_registry.h` - Action/condition registry
- `code/fgame/bt_core_actions.cpp` - Core action registrations
- `code/fgame/bt_yaml_loader.h` - YAML loading
- `behaviors/engage_enemy.yaml` - Combat tree
- `behaviors/patrol.yaml` - Patrol tree

### Modified Files
- `code/fgame/g_main.cpp` - Call RegisterCoreBTActions() on init
- `code/fgame/CMakeLists.txt` - Add new source files

---

## Acceptance Criteria

- [ ] BTActionRegistry implemented and functional
- [ ] 4 conditions registered (HasVisibleEnemy, HasKnownEnemy, LowHealth, HasAmmo)
- [ ] 4 actions registered (AimAtEnemy, ShootEnemy, Idle, Retreat)
- [ ] BTYamlLoader successfully loads engage_enemy.yaml
- [ ] BTYamlLoader successfully loads patrol.yaml
- [ ] Loaded trees execute identically to C++ version
- [ ] Error messages are clear and helpful
- [ ] Unknown action/condition names produce good error messages
- [ ] Invalid YAML syntax produces clear parse errors

---

## Console Commands (for testing)

```cpp
// Add these console commands for testing

// bt_load <filename>
// Loads a behavior tree from YAML
void BT_Load_f() {
    if (gi.argc() < 2) {
        gi.Printf("Usage: bt_load <filename>\n");
        return;
    }
    
    const char* filename = gi.argv(1);
    char path[256];
    Com_sprintf(path, sizeof(path), "behaviors/%s.yaml", filename);
    
    auto tree = BTYamlLoader::LoadFromFile(path);
    if (tree) {
        gi.Printf("Successfully loaded: %s\n", filename);
        // Store tree somewhere for testing
    } else {
        gi.Printf("Failed to load: %s\n", filename);
    }
}

// bt_reload <filename>
// Reloads a behavior tree from YAML
void BT_Reload_f() {
    BT_Load_f();  // Same as load for now
}

// bt_list_actions
// Lists all registered actions
void BT_ListActions_f() {
    gi.Printf("=== Registered BT Actions ===\n");
    // TODO: Add method to BTActionRegistry to list actions
    gi.Printf("  AimAtEnemy\n");
    gi.Printf("  ShootEnemy\n");
    gi.Printf("  Idle\n");
    gi.Printf("  Retreat\n");
}

// bt_list_conditions
// Lists all registered conditions
void BT_ListConditions_f() {
    gi.Printf("=== Registered BT Conditions ===\n");
    gi.Printf("  HasVisibleEnemy\n");
    gi.Printf("  HasKnownEnemy\n");
    gi.Printf("  LowHealth\n");
    gi.Printf("  HasAmmo\n");
}
```

---

## Performance Considerations

- YAML parsing only happens once at load time
- Runtime performance identical to C++ trees
- Keep YAML files small (< 1000 lines)
- Use `#include` or `subtree:` references for large trees (future enhancement)

---

## Common Pitfalls to Avoid

1. **Missing Error Handling:** Every YAML access must check for existence
2. **Unclear Errors:** Print helpful messages, not just "failed"
3. **Case Sensitivity:** "Selector" vs "selector" - be consistent
4. **Memory Leaks:** Use unique_ptr throughout
5. **Registry Not Initialized:** Call RegisterCoreBTActions() early in startup

---

## Next Steps (Task 2B.3)

After this task, you'll have:
- YAML-driven behavior trees
- 2 working behaviors (engage, patrol)
- Easy to add new actions/conditions

Task 2B.3 will extend bot profiles to include combat/movement/personality parameters and reference which behavior tree to use.
