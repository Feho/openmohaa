#ifndef __BEHAVIOR_TREE_FACTORY_H__
#define __BEHAVIOR_TREE_FACTORY_H__

#include "behavior_tree.h"
#include <unordered_map>
#include <string>

/**
 * Factory for creating behavior tree nodes from string names.
 * Essential for YAML-based tree loading (Task 2B.3).
 *
 * Usage:
 *   BTNodeFactory factory;
 *   factory.RegisterCondition("HasEnemy", [](Blackboard& bb) {
 *       return bb.TryGet<PerceptionSnapshot*>("perception")
 *           .value_or(nullptr)->HasVisibleEnemy();
 *   });
 *   factory.RegisterAction("Attack", [](Blackboard& bb, float dt) {
 *       // Attack logic
 *       return BTNode::Status::SUCCESS;
 *   });
 *
 *   auto condition = factory.CreateCondition("HasEnemy");
 *   auto action = factory.CreateAction("Attack");
 */
class BTNodeFactory {
public:
    using ConditionCreator = std::function<bool(Blackboard &)>;
    using ActionCreator    = std::function<BTNode::Status(Blackboard &, float)>;

    /**
     * Register a condition creator function by name.
     * @param name The condition name (used in YAML)
     * @param creator Lambda that implements the condition logic
     */
    void RegisterCondition(const std::string &name, ConditionCreator creator)
    {
        conditions[name] = creator;
    }

    /**
     * Register an action creator function by name.
     * @param name The action name (used in YAML)
     * @param creator Lambda that implements the action logic
     */
    void RegisterAction(const std::string &name, ActionCreator creator) { actions[name] = creator; }

    /**
     * Create a condition node by name.
     * @param name The registered condition name
     * @return Unique pointer to the condition node
     * @throws Error if name not found
     */
    [[nodiscard]] std::unique_ptr<BTNode> CreateCondition(const std::string &name) const
    {
        auto it = conditions.find(name);
        if (it == conditions.end()) {
            gi.Error(ERR_DROP, "BTNodeFactory: Unknown condition '%s'", name.c_str());
        }
        return std::make_unique<BTCondition>(name.c_str(), it->second);
    }

    /**
     * Create an action node by name.
     * @param name The registered action name
     * @return Unique pointer to the action node
     * @throws Error if name not found
     */
    [[nodiscard]] std::unique_ptr<BTNode> CreateAction(const std::string &name) const
    {
        auto it = actions.find(name);
        if (it == actions.end()) {
            gi.Error(ERR_DROP, "BTNodeFactory: Unknown action '%s'", name.c_str());
        }
        return std::make_unique<BTAction>(name.c_str(), it->second);
    }

    /**
     * Check if a condition is registered.
     */
    [[nodiscard]] bool HasCondition(const std::string &name) const noexcept
    {
        return conditions.find(name) != conditions.end();
    }

    /**
     * Check if an action is registered.
     */
    [[nodiscard]] bool HasAction(const std::string &name) const noexcept
    {
        return actions.find(name) != actions.end();
    }

    /**
     * Clear all registered nodes.
     */
    void Clear() noexcept
    {
        conditions.clear();
        actions.clear();
    }

private:
    std::unordered_map<std::string, ConditionCreator> conditions;
    std::unordered_map<std::string, ActionCreator>    actions;
};

#endif // __BEHAVIOR_TREE_FACTORY_H__
