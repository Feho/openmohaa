#ifndef __BEHAVIOR_TREE_BUILDER_H__
#define __BEHAVIOR_TREE_BUILDER_H__

#include "behavior_tree.h"
#include <stack>

/**
 * Fluent interface for building behavior trees.
 *
 * LIFETIME GUARANTEES:
 * - The builder uses raw pointers internally to track the build stack
 * - These pointers remain valid because:
 *   1. Composite nodes are moved into unique_ptrs (either root or parent's children vector)
 *   2. Those unique_ptrs are never destroyed until Build() completes
 *   3. Vectors never reallocate during the build process (no capacity changes)
 * - WARNING: Do not add methods that could trigger early cleanup or reallocation
 * - The builder is single-use: call Build() once, then discard
 *
 * USAGE RULES:
 * - Every Selector/Sequence/Parallel must have matching End()
 * - Leaf nodes (Condition/Action) cannot be the root
 * - Call Build() exactly once after construction is complete
 * - Do not reuse builder after Build()
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
    BehaviorTreeBuilder() { nodeStack.push(nullptr); // Root placeholder
    }

    BehaviorTreeBuilder &Selector()
    {
        auto node = std::make_unique<BTSelector>();
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder &Sequence()
    {
        auto node = std::make_unique<BTSequence>();
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder &Parallel(BTParallel::Policy policy = BTParallel::Policy::RequireAll, int n = 0)
    {
        auto node = std::make_unique<BTParallel>(policy, n);
        PushComposite(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder &Condition(const char *name, BTCondition::ConditionFunc func)
    {
        auto node = std::make_unique<BTCondition>(name, func);
        AddLeaf(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder &Action(const char *name, BTAction::ActionFunc func)
    {
        auto node = std::make_unique<BTAction>(name, func);
        AddLeaf(std::move(node));
        return *this;
    }

    BehaviorTreeBuilder &End()
    {
        if (nodeStack.size() <= 1) {
            gi.Error(ERR_DROP, "BehaviorTreeBuilder: End() called without matching composite");
        }
        nodeStack.pop();
        return *this;
    }

    std::unique_ptr<BehaviorTree> Build()
    {
        if (nodeStack.size() != 1) {
            gi.Error(ERR_DROP, "BehaviorTreeBuilder: Mismatched Begin/End calls");
        }

        auto tree = std::make_unique<BehaviorTree>();
        tree->SetRoot(std::move(rootNode));
        return tree;
    }

private:
    void PushComposite(std::unique_ptr<BTComposite> node)
    {
        BTComposite *rawPtr = node.get();

        if (nodeStack.top() == nullptr) {
            // This is the root
            rootNode = std::move(node);
        } else {
            // Add as child to current composite
            nodeStack.top()->AddChild(std::move(node));
        }

        nodeStack.push(rawPtr);
    }

    void AddLeaf(std::unique_ptr<BTNode> node)
    {
        if (nodeStack.top() == nullptr) {
            gi.Error(ERR_DROP, "BehaviorTreeBuilder: Leaf node cannot be root");
        }
        nodeStack.top()->AddChild(std::move(node));
    }

    std::stack<BTComposite *>   nodeStack;
    std::unique_ptr<BTNode>     rootNode;
};

#endif // __BEHAVIOR_TREE_BUILDER_H__
