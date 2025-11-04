#ifndef __BEHAVIOR_TREE_H__
#define __BEHAVIOR_TREE_H__

#ifndef BEHAVIOR_TREE_TESTING
#include "g_local.h"
#else
#include "test_game_stubs.h"
#endif

#include <memory>
#include <vector>
#include <functional>
#include <any>
#include <optional>
#include <unordered_map>
#include <string>
#include <string_view>

// Forward declarations
class Blackboard;
class BehaviorTree;

/**
 * Base class for all Behavior Tree nodes.
 * Each node returns a Status when executed.
 */
class BTNode {
public:
    enum class Status {
        SUCCESS, // Node completed successfully
        FAILURE, // Node failed
        RUNNING  // Node is still executing (multi-frame)
    };

    virtual ~BTNode() = default;

    /**
     * Execute this node.
     * @param blackboard Shared data storage
     * @param deltaTime Time since last frame
     * @return Status of execution
     */
    virtual Status Execute(Blackboard &blackboard, float deltaTime) = 0;

    /**
     * Reset node to initial state.
     * Called when tree restarts.
     */
    virtual void Reset() = 0;

    /**
     * Get node name for debugging.
     */
    virtual const char *GetName() const = 0;

protected:
    Status lastStatus = Status::FAILURE;
};

/**
 * Base class for composite nodes (have children).
 */
class BTComposite : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child)
    {
        if (!child) {
            gi.Error(ERR_DROP, "BTComposite::AddChild: Cannot add null child node");
        }
        children.push_back(std::move(child));
    }

    void Reset() override
    {
        for (auto &child : children) {
            child->Reset();
        }
        currentChildIndex = 0;
    }

protected:
    std::vector<std::unique_ptr<BTNode>> children;
    size_t                               currentChildIndex = 0;
};

/**
 * Selector node: Returns SUCCESS on first child success.
 * Tries children left-to-right until one succeeds or all fail.
 * Usage: Pick first viable option (emergency OR combat OR patrol)
 */
class BTSelector : public BTComposite {
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        while (currentChildIndex < children.size()) {
            Status status = children[currentChildIndex]->Execute(blackboard, deltaTime);

            if (status == Status::SUCCESS) {
                Reset(); // Success, reset for next run
                return Status::SUCCESS;
            } else if (status == Status::RUNNING) {
                return Status::RUNNING; // Pause here
            }
            // FAILURE: try next child
            currentChildIndex++;
        }

        // All children failed
        Reset();
        return Status::FAILURE;
    }

    const char *GetName() const override { return "Selector"; }
};

/**
 * Sequence node: Returns FAILURE on first child failure.
 * Executes children left-to-right until one fails or all succeed.
 * Usage: Do multiple steps in order (find cover AND move to cover AND heal)
 */
class BTSequence : public BTComposite {
public:
    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        while (currentChildIndex < children.size()) {
            Status status = children[currentChildIndex]->Execute(blackboard, deltaTime);

            if (status == Status::FAILURE) {
                Reset(); // Failure, reset for next run
                return Status::FAILURE;
            } else if (status == Status::RUNNING) {
                return Status::RUNNING; // Pause here
            }
            // SUCCESS: move to next child
            currentChildIndex++;
        }

        // All children succeeded
        Reset();
        return Status::SUCCESS;
    }

    const char *GetName() const override { return "Sequence"; }
};

/**
 * Parallel node: Executes all children simultaneously.
 * Policy determines when to return SUCCESS/FAILURE.
 *
 * Special cases:
 * - Empty parallel node returns FAILURE
 * - RequireN with N > children count returns FAILURE immediately
 */
class BTParallel : public BTComposite {
public:
    enum class Policy {
        RequireAll, // All children must succeed
        RequireOne, // At least one must succeed
        RequireN    // N children must succeed
    };

    BTParallel(Policy policyType = Policy::RequireAll, int requiredCount = 0)
        : policy(policyType), requiredSuccessCount(requiredCount)
    {
        // Validate parameters
        if (policy == Policy::RequireN && requiredCount < 0) {
            gi.Error(ERR_DROP, "BTParallel: requiredCount cannot be negative (got %d)", requiredCount);
        }
    }

    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        // Empty parallel node is a failure
        if (children.empty()) {
            return Status::FAILURE;
        }

        // For RequireN policy, check if goal is achievable
        if (policy == Policy::RequireN && requiredSuccessCount > static_cast<int>(children.size())) {
            return Status::FAILURE; // Impossible to satisfy
        }

        int successCount = 0;
        int failureCount = 0;
        int runningCount = 0;

        // Execute all children
        for (auto &child : children) {
            Status status = child->Execute(blackboard, deltaTime);

            switch (status) {
            case Status::SUCCESS:
                successCount++;
                break;
            case Status::FAILURE:
                failureCount++;
                break;
            case Status::RUNNING:
                runningCount++;
                break;
            }
        }

        // Check policy
        switch (policy) {
        case Policy::RequireAll:
            if (failureCount > 0)
                return Status::FAILURE;
            if (runningCount > 0)
                return Status::RUNNING;
            return Status::SUCCESS;

        case Policy::RequireOne:
            if (successCount > 0)
                return Status::SUCCESS;
            if (runningCount > 0)
                return Status::RUNNING;
            return Status::FAILURE;

        case Policy::RequireN:
            if (successCount >= requiredSuccessCount)
                return Status::SUCCESS;
            if (failureCount > static_cast<int>(children.size()) - requiredSuccessCount) {
                return Status::FAILURE;
            }
            return Status::RUNNING;
        }

        return Status::FAILURE;
    }

    void Reset() override
    {
        BTComposite::Reset(); // Reset children and currentChildIndex
        // No parallel-specific state to reset currently
    }

    const char *GetName() const override { return "Parallel"; }

private:
    Policy policy;
    int    requiredSuccessCount;
};

/**
 * Condition node: Boolean check that returns SUCCESS/FAILURE.
 * Does not modify state, only reads blackboard.
 */
class BTCondition : public BTNode {
public:
    using ConditionFunc = std::function<bool(Blackboard &)>;

    BTCondition(const char *nodeName, ConditionFunc func) : name(nodeName), conditionFunc(func) {}

    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        try {
            bool result = conditionFunc(blackboard);
            lastStatus  = result ? Status::SUCCESS : Status::FAILURE;
            return lastStatus;
        } catch (const std::exception &e) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("BTCondition '%s' threw exception: %s\n", name, e.what());
#endif
            return Status::FAILURE;
        } catch (...) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("BTCondition '%s' threw unknown exception\n", name);
#endif
            return Status::FAILURE;
        }
    }

    void Reset() override
    {
        // Conditions are stateless
    }

    const char *GetName() const override { return name; }

private:
    const char   *name;
    ConditionFunc conditionFunc;
};

/**
 * Subtree wrapper: Executes a loaded behavior tree's root node.
 * Allows tree composition and reuse.
 * Added in OPM - Phase 3 Task 3.2 (Subtree Support)
 */
class BTSubtreeWrapper : public BTNode {
public:
    BTSubtreeWrapper(const char *nodeName, std::unique_ptr<BTNode> rootNode) 
        : name(nodeName), subtreeRoot(std::move(rootNode)) {}

    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        if (!subtreeRoot) {
            return Status::FAILURE;
        }

        return subtreeRoot->Execute(blackboard, deltaTime);
    }

    void Reset() override
    {
        if (subtreeRoot) {
            subtreeRoot->Reset();
        }
    }

    const char *GetName() const override { return name; }

private:
    const char                  *name;
    std::unique_ptr<BTNode>      subtreeRoot;
};

/**
 * Action node: Performs actual behavior.
 * Can return RUNNING to indicate multi-frame execution.
 */
class BTAction : public BTNode {
public:
    using ActionFunc = std::function<Status(Blackboard &, float)>;

    BTAction(const char *nodeName, ActionFunc func) : name(nodeName), actionFunc(func) {}

    Status Execute(Blackboard &blackboard, float deltaTime) override
    {
        try {
            lastStatus = actionFunc(blackboard, deltaTime);
            return lastStatus;
        } catch (const std::exception &e) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("BTAction '%s' threw exception: %s\n", name, e.what());
#endif
            return Status::FAILURE;
        } catch (...) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("BTAction '%s' threw unknown exception\n", name);
#endif
            return Status::FAILURE;
        }
    }

    void Reset() override { lastStatus = Status::FAILURE; }

    const char *GetName() const override { return name; }

private:
    const char *name;
    ActionFunc  actionFunc;
};

/**
 * Blackboard: Type-safe key-value storage for behavior tree data.
 * Allows nodes to share information without tight coupling.
 *
 * Thread Safety: NOT thread-safe. Each bot must have its own Blackboard instance.
 */
class Blackboard {
public:
    /**
     * Set a value in the blackboard (copy version).
     */
    template<typename T>
    void Set(const std::string &key, const T &value)
    {
        data[key] = value;
    }

    /**
     * Set a value in the blackboard (move version).
     * Prefer this for large objects to avoid copying.
     */
    template<typename T>
    void Set(const std::string &key, T &&value)
    {
        data[key] = std::forward<T>(value);
    }

    /**
     * Try to get a value from the blackboard.
     * @return std::optional<T> containing value if found and type matches, std::nullopt otherwise
     */
    template<typename T>
    [[nodiscard]] std::optional<T> TryGet(const std::string &key) const
    {
        auto it = data.find(key);
        if (it == data.end()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast &) {
            return std::nullopt;
        }
    }

    /**
     * Get a value from the blackboard.
     * FATAL ERROR if key doesn't exist or type mismatch (for critical keys only).
     * Prefer TryGet() for non-critical or YAML-loaded keys.
     *
     * @param key The blackboard key
     * @return The value (program terminates if not found)
     */
    template<typename T>
    [[nodiscard]] T Get(const std::string &key) const
    {
        auto result = TryGet<T>(key);
        if (!result) {
            gi.Error(ERR_DROP, "Blackboard: Required key '%s' not found or type mismatch", key.c_str());
        }
        return *result;
    }

    /**
     * Get a value with default if not found.
     */
    template<typename T>
    [[nodiscard]] T GetOrDefault(const std::string &key, const T &defaultValue) const
    {
        return TryGet<T>(key).value_or(defaultValue);
    }

    /**
     * Check if key exists.
     */
    [[nodiscard]] bool Has(const std::string &key) const noexcept { return data.find(key) != data.end(); }

    /**
     * Remove a key.
     */
    void Remove(const std::string &key) { data.erase(key); }

    /**
     * Clear all data.
     */
    void Clear() noexcept { data.clear(); }

private:
    std::unordered_map<std::string, std::any> data;
};

/**
 * BehaviorTree: Container for tree execution.
 *
 * Thread Safety: NOT thread-safe. Each bot must have its own BehaviorTree instance.
 * Trees may share structure in the future via shared_ptr<const BTNode> optimization,
 * but execution state must remain per-bot.
 */
class BehaviorTree {
public:
    BehaviorTree() = default;

    void SetRoot(std::unique_ptr<BTNode> root) { rootNode = std::move(root); }

    /**
     * Execute the behavior tree for one frame.
     * @param blackboard Shared data storage for this execution
     * @param deltaTime Time since last frame (seconds)
     * @return Status of root node execution
     */
    [[nodiscard]] BTNode::Status Execute(Blackboard &blackboard, float deltaTime)
    {
        if (!rootNode) {
            return BTNode::Status::FAILURE;
        }
        return rootNode->Execute(blackboard, deltaTime);
    }

    /**
     * Reset tree to initial state.
     * Call this when restarting a behavior or switching trees.
     */
    void Reset()
    {
        if (rootNode) {
            rootNode->Reset();
        }
    }

    [[nodiscard]] BTNode *GetRoot() const noexcept { return rootNode.get(); }

private:
    std::unique_ptr<BTNode> rootNode;
};

#endif // __BEHAVIOR_TREE_H__
