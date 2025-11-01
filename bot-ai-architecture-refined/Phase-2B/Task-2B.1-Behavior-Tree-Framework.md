# Task 2B.1: Behavior Tree Framework

**Status:** Ready to Execute  
**Duration:** 1-2 weeks (12 days estimated)  
**Priority:** CRITICAL  
**Phase:** 2B - Brain & Behavior

---

## Context & Background

### What This Task Achieves
Implements the core Behavior Tree (BT) system that will replace the current priority-based state machine. This provides a hierarchical, visual, and modular approach to bot decision-making.

### Why This Matters
- **Maintainability:** Visual tree structure is easier to understand than procedural code
- **Composability:** Subtrees can be reused across different bot types
- **Reactivity:** Trees re-evaluate every frame, allowing dynamic responses
- **Designer-Friendly:** Trees can be defined in YAML (added in Task 2B.2)

### Current State (What's Already Complete)
**Phase 2A delivered:**
- ✅ PerceptionSystem with VisionSensor, AudioSensor, MemorySystem
- ✅ Rich perception data structures (EnemyInfo, AllyInfo, AudioEvent, EnemyMemory)
- ✅ PerceptionSnapshot with helper methods
- ✅ BotProfile system with YAML parsing (perception parameters)
- ✅ Feature flag `g_bot_use_new_ai_system` (default: 0)
- ✅ 26 tests passing (19 perception + 3 profile + 4 integration)

**What Bots Can Now Do:**
- Detect enemies with realistic vision (FOV, occlusion, distance)
- Hear sounds with 3D positional audio
- Remember enemy positions with confidence decay
- Assess threat levels (NONE/LOW/MEDIUM/HIGH)
- Detect nearby allies

**What Bots Still Use:**
- Old priority-based state machine for decision-making
- Hardcoded behavior logic in C++

---

## Technical Specification

### Architecture Overview

Behavior Trees use a hierarchical node structure:

```
Root (Selector)
├─ Emergency Sequence
│  ├─ Condition: Health < 25%
│  └─ Action: Retreat
├─ Combat Sequence
│  ├─ Condition: HasEnemy
│  └─ Parallel
│     ├─ Action: Aim
│     ├─ Action: Fire
│     └─ Action: Move
└─ Action: Patrol (fallback)
```

**Node Execution Flow:**
1. Tree starts at root
2. Each node returns: SUCCESS, FAILURE, or RUNNING
3. Composite nodes (Selector/Sequence/Parallel) control child execution
4. Leaf nodes (Condition/Action) perform actual logic
5. Tree execution pauses on RUNNING, resumes next frame

### Core Components to Implement

#### 1. BTNode Base Class (4 hours)

```cpp
// code/fgame/behavior_tree.h

#ifndef __BEHAVIOR_TREE_H__
#define __BEHAVIOR_TREE_H__

#include "g_local.h"
#include <memory>
#include <vector>
#include <functional>
#include <any>
#include <unordered_map>
#include <string>

// Forward declarations
class Blackboard;

/**
 * Base class for all Behavior Tree nodes.
 * Each node returns a Status when executed.
 */
class BTNode {
public:
    enum class Status {
        SUCCESS,  // Node completed successfully
        FAILURE,  // Node failed
        RUNNING   // Node is still executing (multi-frame)
    };

    virtual ~BTNode() = default;

    /**
     * Execute this node.
     * @param blackboard Shared data storage
     * @param deltaTime Time since last frame
     * @return Status of execution
     */
    virtual Status Execute(Blackboard& blackboard, float deltaTime) = 0;

    /**
     * Reset node to initial state.
     * Called when tree restarts.
     */
    virtual void Reset() = 0;

    /**
     * Get node name for debugging.
     */
    virtual const char* GetName() const = 0;

protected:
    Status lastStatus = Status::FAILURE;
};

/**
 * Base class for composite nodes (have children).
 */
class BTComposite : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child) {
        children.push_back(std::move(child));
    }

    void Reset() override {
        for (auto& child : children) {
            child->Reset();
        }
        currentChildIndex = 0;
    }

protected:
    std::vector<std::unique_ptr<BTNode>> children;
    size_t currentChildIndex = 0;
};

#endif // __BEHAVIOR_TREE_H__
```

#### 2. BTSelector Node (3 hours)

Tries children left-to-right until one succeeds (OR logic).

```cpp
// In behavior_tree.h

/**
 * Selector node: Returns SUCCESS on first child success.
 * Tries children left-to-right until one succeeds or all fail.
 * Usage: Pick first viable option (emergency OR combat OR patrol)
 */
class BTSelector : public BTComposite {
public:
    Status Execute(Blackboard& blackboard, float deltaTime) override {
        while (currentChildIndex < children.size()) {
            Status status = children[currentChildIndex]->Execute(blackboard, deltaTime);
            
            if (status == Status::SUCCESS) {
                Reset();  // Success, reset for next run
                return Status::SUCCESS;
            }
            else if (status == Status::RUNNING) {
                return Status::RUNNING;  // Pause here
            }
            // FAILURE: try next child
            currentChildIndex++;
        }
        
        // All children failed
        Reset();
        return Status::FAILURE;
    }

    const char* GetName() const override { return "Selector"; }
};
```

#### 3. BTSequence Node (3 hours)

Executes children left-to-right until one fails (AND logic).

```cpp
// In behavior_tree.h

/**
 * Sequence node: Returns FAILURE on first child failure.
 * Executes children left-to-right until one fails or all succeed.
 * Usage: Do multiple steps in order (find cover AND move to cover AND heal)
 */
class BTSequence : public BTComposite {
public:
    Status Execute(Blackboard& blackboard, float deltaTime) override {
        while (currentChildIndex < children.size()) {
            Status status = children[currentChildIndex]->Execute(blackboard, deltaTime);
            
            if (status == Status::FAILURE) {
                Reset();  // Failure, reset for next run
                return Status::FAILURE;
            }
            else if (status == Status::RUNNING) {
                return Status::RUNNING;  // Pause here
            }
            // SUCCESS: move to next child
            currentChildIndex++;
        }
        
        // All children succeeded
        Reset();
        return Status::SUCCESS;
    }

    const char* GetName() const override { return "Sequence"; }
};
```

#### 4. BTParallel Node (4 hours)

Executes all children simultaneously.

```cpp
// In behavior_tree.h

/**
 * Parallel node: Executes all children simultaneously.
 * Policy determines when to return SUCCESS/FAILURE.
 */
class BTParallel : public BTComposite {
public:
    enum class Policy {
        RequireAll,   // All children must succeed
        RequireOne,   // At least one must succeed
        RequireN      // N children must succeed
    };

    BTParallel(Policy policy = Policy::RequireAll, int requiredCount = 0)
        : policy(policy), requiredSuccessCount(requiredCount) {}

    Status Execute(Blackboard& blackboard, float deltaTime) override {
        int successCount = 0;
        int failureCount = 0;
        int runningCount = 0;

        // Execute all children
        for (auto& child : children) {
            Status status = child->Execute(blackboard, deltaTime);
            
            switch (status) {
                case Status::SUCCESS: successCount++; break;
                case Status::FAILURE: failureCount++; break;
                case Status::RUNNING: runningCount++; break;
            }
        }

        // Check policy
        switch (policy) {
            case Policy::RequireAll:
                if (failureCount > 0) return Status::FAILURE;
                if (runningCount > 0) return Status::RUNNING;
                return Status::SUCCESS;

            case Policy::RequireOne:
                if (successCount > 0) return Status::SUCCESS;
                if (runningCount > 0) return Status::RUNNING;
                return Status::FAILURE;

            case Policy::RequireN:
                if (successCount >= requiredSuccessCount) return Status::SUCCESS;
                if (failureCount > static_cast<int>(children.size()) - requiredSuccessCount) {
                    return Status::FAILURE;
                }
                return Status::RUNNING;
        }

        return Status::FAILURE;
    }

    const char* GetName() const override { return "Parallel"; }

private:
    Policy policy;
    int requiredSuccessCount;
};
```

#### 5. BTCondition Node (3 hours)

Lambda-based condition checks.

```cpp
// In behavior_tree.h

/**
 * Condition node: Boolean check that returns SUCCESS/FAILURE.
 * Does not modify state, only reads blackboard.
 */
class BTCondition : public BTNode {
public:
    using ConditionFunc = std::function<bool(Blackboard&)>;

    BTCondition(const char* name, ConditionFunc func)
        : name(name), conditionFunc(func) {}

    Status Execute(Blackboard& blackboard, float deltaTime) override {
        bool result = conditionFunc(blackboard);
        lastStatus = result ? Status::SUCCESS : Status::FAILURE;
        return lastStatus;
    }

    void Reset() override {
        // Conditions are stateless
    }

    const char* GetName() const override { return name; }

private:
    const char* name;
    ConditionFunc conditionFunc;
};
```

#### 6. BTAction Node (3 hours)

Lambda-based actions that can run over multiple frames.

```cpp
// In behavior_tree.h

/**
 * Action node: Performs actual behavior.
 * Can return RUNNING to indicate multi-frame execution.
 */
class BTAction : public BTNode {
public:
    using ActionFunc = std::function<Status(Blackboard&, float)>;

    BTAction(const char* name, ActionFunc func)
        : name(name), actionFunc(func) {}

    Status Execute(Blackboard& blackboard, float deltaTime) override {
        lastStatus = actionFunc(blackboard, deltaTime);
        return lastStatus;
    }

    void Reset() override {
        lastStatus = Status::FAILURE;
    }

    const char* GetName() const override { return name; }

private:
    const char* name;
    ActionFunc actionFunc;
};
```

#### 7. Blackboard System (4 hours)

Type-safe shared data storage.

```cpp
// In behavior_tree.h

/**
 * Blackboard: Type-safe key-value storage for behavior tree data.
 * Allows nodes to share information without tight coupling.
 */
class Blackboard {
public:
    /**
     * Set a value in the blackboard.
     */
    template<typename T>
    void Set(const std::string& key, const T& value) {
        data[key] = value;
    }

    /**
     * Get a value from the blackboard.
     * Throws if key doesn't exist or type mismatch.
     */
    template<typename T>
    T Get(const std::string& key) const {
        auto it = data.find(key);
        if (it == data.end()) {
            gi.Error("Blackboard key not found: %s", key.c_str());
        }
        
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            gi.Error("Blackboard type mismatch for key: %s", key.c_str());
        }
        
        return T{};  // Never reached
    }

    /**
     * Get a value with default if not found.
     */
    template<typename T>
    T GetOrDefault(const std::string& key, const T& defaultValue) const {
        auto it = data.find(key);
        if (it == data.end()) {
            return defaultValue;
        }
        
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return defaultValue;
        }
    }

    /**
     * Check if key exists.
     */
    bool Has(const std::string& key) const {
        return data.find(key) != data.end();
    }

    /**
     * Remove a key.
     */
    void Remove(const std::string& key) {
        data.erase(key);
    }

    /**
     * Clear all data.
     */
    void Clear() {
        data.clear();
    }

private:
    std::unordered_map<std::string, std::any> data;
};
```

#### 8. BehaviorTree Container (2 hours)

```cpp
// In behavior_tree.h

/**
 * BehaviorTree: Container for tree execution.
 */
class BehaviorTree {
public:
    BehaviorTree() = default;

    void SetRoot(std::unique_ptr<BTNode> root) {
        rootNode = std::move(root);
    }

    BTNode::Status Execute(Blackboard& blackboard, float deltaTime) {
        if (!rootNode) {
            return BTNode::Status::FAILURE;
        }
        return rootNode->Execute(blackboard, deltaTime);
    }

    void Reset() {
        if (rootNode) {
            rootNode->Reset();
        }
    }

    BTNode* GetRoot() const {
        return rootNode.get();
    }

private:
    std::unique_ptr<BTNode> rootNode;
};
```

#### 9. BehaviorTreeBuilder (4 hours)

Fluent interface for tree construction.

```cpp
// code/fgame/behavior_tree_builder.h

#ifndef __BEHAVIOR_TREE_BUILDER_H__
#define __BEHAVIOR_TREE_BUILDER_H__

#include "behavior_tree.h"
#include <stack>

/**
 * Fluent interface for building behavior trees.
 * 
 * Example:
 *   auto tree = BehaviorTreeBuilder()
 *       .Selector()
 *           .Sequence()
 *               .Condition("HasEnemy", [](auto& bb) { return bb.Has("enemy"); })
 *               .Action("Attack", [](auto& bb, float dt) { return Attack(); })
 *           .End()
 *           .Action("Patrol", [](auto& bb, float dt) { return Patrol(); })
 *       .End()
 *       .Build();
 */
class BehaviorTreeBuilder {
public:
    BehaviorTreeBuilder() {
        nodeStack.push(nullptr);  // Root placeholder
    }

    BehaviorTreeBuilder& Selector() {
        auto node = std::make_unique<BTSelector>();
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder& Sequence() {
        auto node = std::make_unique<BTSequence>();
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder& Parallel(BTParallel::Policy policy = BTParallel::Policy::RequireAll, int n = 0) {
        auto node = std::make_unique<BTParallel>(policy, n);
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder& Condition(const char* name, BTCondition::ConditionFunc func) {
        auto node = std::make_unique<BTCondition>(name, func);
        AddLeaf(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder& Action(const char* name, BTAction::ActionFunc func) {
        auto node = std::make_unique<BTAction>(name, func);
        AddLeaf(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder& End() {
        if (nodeStack.size() <= 1) {
            gi.Error("BehaviorTreeBuilder: End() called without matching composite");
        }
        nodeStack.pop();
        return *this;
    }

    std::unique_ptr<BehaviorTree> Build() {
        if (nodeStack.size() != 1) {
            gi.Error("BehaviorTreeBuilder: Mismatched Begin/End calls");
        }

        auto tree = std::make_unique<BehaviorTree>();
        tree->SetRoot(std::move(rootNode));
        return tree;
    }

private:
    void PushComposite(std::unique_ptr<BTComposite> node) {
        BTComposite* rawPtr = node.get();
        
        if (nodeStack.top() == nullptr) {
            // This is the root
            rootNode = std::move(node);
        } else {
            // Add as child to current composite
            nodeStack.top()->AddChild(std::move(node));
        }
        
        nodeStack.push(rawPtr);
    }

    void AddLeaf(std::unique_ptr<BTNode> node) {
        if (nodeStack.top() == nullptr) {
            gi.Error("BehaviorTreeBuilder: Leaf node cannot be root");
        }
        nodeStack.top()->AddChild(std::move(node));
    }

    std::stack<BTComposite*> nodeStack;
    std::unique_ptr<BTNode> rootNode;
};

#endif // __BEHAVIOR_TREE_BUILDER_H__
```

#### 10. Simple Test Behavior (6 hours)

Create a simple combat tree that uses perception data.

```cpp
// code/fgame/bot_behaviors.cpp (new file)

#include "behavior_tree.h"
#include "behavior_tree_builder.h"
#include "perception.h"
#include "playerbot.h"

/**
 * Create a simple engage enemy behavior tree.
 * This is a proof-of-concept before YAML loading.
 */
std::unique_ptr<BehaviorTree> CreateEngageEnemyTree() {
    return BehaviorTreeBuilder()
        .Selector()
            // Has enemy? Attack
            .Sequence()
                .Condition("HasVisibleEnemy", [](Blackboard& bb) {
                    auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
                    return snapshot->HasVisibleEnemy();
                })
                .Action("AimAtEnemy", [](Blackboard& bb, float dt) {
                    auto bot = bb.Get<BotController*>("bot");
                    auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
                    
                    if (snapshot->closestEnemy) {
                        bot->AimAt(snapshot->closestEnemy->position);
                        return BTNode::Status::SUCCESS;
                    }
                    return BTNode::Status::FAILURE;
                })
                .Action("ShootEnemy", [](Blackboard& bb, float dt) {
                    auto bot = bb.Get<BotController*>("bot");
                    auto snapshot = bb.Get<PerceptionSnapshot*>("perception");
                    
                    if (snapshot->closestEnemy && 
                        snapshot->closestEnemy->distance < 1024.0f) {
                        bot->Fire();
                        return BTNode::Status::SUCCESS;
                    }
                    return BTNode::Status::FAILURE;
                })
            .End()
            
            // No enemy? Idle
            .Action("Idle", [](Blackboard& bb, float dt) {
                // Just stand still for now
                return BTNode::Status::SUCCESS;
            })
        .End()
        .Build();
}
```

---

## Implementation Steps

### Day 1-2: Core Node Types (12 hours)
1. Create `behavior_tree.h` with BTNode base class
2. Implement BTSelector
3. Implement BTSequence
4. Implement BTParallel
5. Implement BTCondition
6. Implement BTAction
7. Write unit tests for each node type (7 tests)

### Day 3: Blackboard System (4 hours)
1. Implement Blackboard class in behavior_tree.h
2. Add Set/Get/Has/Remove methods
3. Add type safety with templates
4. Write unit tests (3 tests)

### Day 4: Tree Builder (4 hours)
1. Create `behavior_tree_builder.h`
2. Implement fluent interface
3. Add validation
4. Add usage examples in comments

### Day 5: Test Behavior (6 hours)
1. Create `bot_behaviors.cpp`
2. Implement CreateEngageEnemyTree()
3. Integrate with perception data from Phase 2A
4. Write integration test

---

## Testing Requirements

### Unit Tests (tests/test_behavior_tree.cpp)

```cpp
#include "behavior_tree.h"
#include "behavior_tree_builder.h"
#include <gtest/gtest.h>

// Test 1: Selector returns first success
TEST(BehaviorTreeTest, SelectorReturnsFirstSuccess) {
    auto tree = BehaviorTreeBuilder()
        .Selector()
            .Action("Fail1", [](auto& bb, float dt) { return BTNode::Status::FAILURE; })
            .Action("Success", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
            .Action("NotReached", [](auto& bb, float dt) { 
                FAIL() << "Should not reach this";
                return BTNode::Status::SUCCESS;
            })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 2: Selector returns failure if all fail
TEST(BehaviorTreeTest, SelectorReturnsFailureIfAllFail) {
    auto tree = BehaviorTreeBuilder()
        .Selector()
            .Action("Fail1", [](auto& bb, float dt) { return BTNode::Status::FAILURE; })
            .Action("Fail2", [](auto& bb, float dt) { return BTNode::Status::FAILURE; })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);
}

// Test 3: Sequence returns first failure
TEST(BehaviorTreeTest, SequenceReturnsFirstFailure) {
    auto tree = BehaviorTreeBuilder()
        .Sequence()
            .Action("Success1", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
            .Action("Failure", [](auto& bb, float dt) { return BTNode::Status::FAILURE; })
            .Action("NotReached", [](auto& bb, float dt) { 
                FAIL() << "Should not reach this";
                return BTNode::Status::SUCCESS;
            })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);
}

// Test 4: Sequence returns success if all succeed
TEST(BehaviorTreeTest, SequenceReturnsSuccessIfAllSucceed) {
    auto tree = BehaviorTreeBuilder()
        .Sequence()
            .Action("Success1", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
            .Action("Success2", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 5: Parallel RequireAll policy
TEST(BehaviorTreeTest, ParallelRequireAllPolicy) {
    auto tree = BehaviorTreeBuilder()
        .Parallel(BTParallel::Policy::RequireAll)
            .Action("Success1", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
            .Action("Success2", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 6: Parallel RequireOne policy
TEST(BehaviorTreeTest, ParallelRequireOnePolicy) {
    auto tree = BehaviorTreeBuilder()
        .Parallel(BTParallel::Policy::RequireOne)
            .Action("Failure", [](auto& bb, float dt) { return BTNode::Status::FAILURE; })
            .Action("Success", [](auto& bb, float dt) { return BTNode::Status::SUCCESS; })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 7: Condition evaluates correctly
TEST(BehaviorTreeTest, ConditionEvaluates) {
    auto tree = BehaviorTreeBuilder()
        .Condition("TestCondition", [](Blackboard& bb) {
            return bb.GetOrDefault<int>("value", 0) > 10;
        })
        .Build();

    Blackboard bb;
    bb.Set<int>("value", 5);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);

    bb.Set<int>("value", 15);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 8: Blackboard Get/Set
TEST(BehaviorTreeTest, BlackboardGetSet) {
    Blackboard bb;
    
    bb.Set<int>("health", 100);
    bb.Set<float>("speed", 1.5f);
    bb.Set<std::string>("name", "Bot1");

    EXPECT_EQ(bb.Get<int>("health"), 100);
    EXPECT_FLOAT_EQ(bb.Get<float>("speed"), 1.5f);
    EXPECT_EQ(bb.Get<std::string>("name"), "Bot1");
}

// Test 9: Blackboard Has
TEST(BehaviorTreeTest, BlackboardHas) {
    Blackboard bb;
    
    bb.Set<int>("value", 42);
    
    EXPECT_TRUE(bb.Has("value"));
    EXPECT_FALSE(bb.Has("nonexistent"));
}

// Test 10: Blackboard Remove
TEST(BehaviorTreeTest, BlackboardRemove) {
    Blackboard bb;
    
    bb.Set<int>("value", 42);
    EXPECT_TRUE(bb.Has("value"));
    
    bb.Remove("value");
    EXPECT_FALSE(bb.Has("value"));
}

// Test 11: RUNNING status pauses execution
TEST(BehaviorTreeTest, RunningStatusPausesExecution) {
    int executionCount = 0;
    
    auto tree = BehaviorTreeBuilder()
        .Sequence()
            .Action("RunningAction", [&executionCount](auto& bb, float dt) {
                executionCount++;
                return BTNode::Status::RUNNING;
            })
            .Action("NeverReached", [](auto& bb, float dt) {
                FAIL() << "Should not reach after RUNNING";
                return BTNode::Status::SUCCESS;
            })
        .End()
        .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(executionCount, 2);
}
```

### Integration Test (1 test)

```cpp
// Test 12: Engage enemy tree with perception
TEST(BehaviorTreeIntegrationTest, EngageEnemyWithPerception) {
    // Create mock perception snapshot
    PerceptionSnapshot snapshot;
    EnemyInfo enemy;
    enemy.position = Vector(100, 0, 0);
    enemy.distance = 500.0f;
    snapshot.visibleEnemies.push_back(enemy);
    snapshot.closestEnemy = &snapshot.visibleEnemies[0];

    // Create mock bot
    BotController bot;
    
    // Create tree
    auto tree = CreateEngageEnemyTree();
    
    // Set up blackboard
    Blackboard bb;
    bb.Set<PerceptionSnapshot*>("perception", &snapshot);
    bb.Set<BotController*>("bot", &bot);
    
    // Execute tree
    auto status = tree->Execute(bb, 0.1f);
    
    // Should successfully aim and shoot
    EXPECT_EQ(status, BTNode::Status::SUCCESS);
}
```

---

## Files to Create/Modify

### New Files
- `code/fgame/behavior_tree.h` - Core BT classes
- `code/fgame/behavior_tree.cpp` - Implementation (if needed)
- `code/fgame/behavior_tree_builder.h` - Builder pattern
- `code/fgame/bot_behaviors.cpp` - Behavior tree definitions
- `tests/test_behavior_tree.cpp` - Unit tests

### Modified Files
- `code/fgame/CMakeLists.txt` or equivalent - Add new source files
- `tests/CMakeLists.txt` - Add test file

---

## Acceptance Criteria

- [ ] All 7 core node types implemented (Selector, Sequence, Parallel, Condition, Action + base classes)
- [ ] Blackboard system functional with type safety
- [ ] BehaviorTreeBuilder provides fluent interface
- [ ] CreateEngageEnemyTree() successfully uses perception data from Phase 2A
- [ ] 11 unit tests pass (7 node tests + 3 blackboard tests + 1 RUNNING test)
- [ ] 1 integration test passes (engage enemy with perception)
- [ ] Zero compiler warnings
- [ ] Code follows OpenMoHAA style
- [ ] All public APIs have comments

---

## Performance Target

- BT execution: < 0.5ms per bot per frame
- Blackboard operations: O(1) average (hash map)
- Memory usage: < 1KB per tree instance

---

## Dependencies

**Requires from Phase 2A:**
- PerceptionSnapshot struct
- EnemyInfo struct
- BotController class with AimAt() and Fire() methods

**Provides for Task 2B.2:**
- Complete BT framework ready for YAML loading
- Working engage enemy tree as example

---

## Common Pitfalls to Avoid

1. **Memory Leaks:** Use std::unique_ptr for node ownership
2. **Type Safety:** Blackboard must validate types on Get()
3. **RUNNING State:** Ensure sequences/selectors pause correctly on RUNNING
4. **Reset Logic:** Reset must clear state for next tree execution
5. **Builder Validation:** Check for mismatched Begin/End calls

---

## Example Usage After Completion

```cpp
// In BotController::Think() (added in Task 2B.4)
void BotController::Think() {
    // Update perception (Phase 2A)
    auto snapshot = perception->Update(controlledEnt, dt);
    
    // Populate blackboard with perception data
    blackboard.Set("perception", &snapshot);
    blackboard.Set("bot", this);
    
    // Execute behavior tree (Phase 2B)
    if (g_bot_use_new_ai_system->integer) {
        behaviorTree->Execute(blackboard, dt);
    } else {
        // Old state machine
        CheckStates();
    }
}
```

---

## Notes

- This task lays the foundation for all future bot behaviors
- Keep implementation simple and focused - YAML loading comes in Task 2B.2
- Test thoroughly - this is critical infrastructure
- Performance matters - profile if needed, but don't over-optimize early
