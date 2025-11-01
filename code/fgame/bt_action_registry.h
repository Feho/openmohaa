// Added in OPM - Phase 2B Task 2B.2
// bt_action_registry.h: Registry for mapping action/condition names to C++ functions

#ifndef __BT_ACTION_REGISTRY_H__
#define __BT_ACTION_REGISTRY_H__

#include "behavior_tree.h"
#include <unordered_map>
#include <string>
#include <vector>

/**
 * Registry for mapping action/condition names to C++ functions.
 * Used by YAML loader to create BTAction and BTCondition nodes.
 *
 * Thread Safety: NOT thread-safe. Registration should happen during
 * game initialization before any bots are spawned.
 */
class BTActionRegistry {
public:
    using ActionFunc    = BTAction::ActionFunc;
    using ConditionFunc = BTCondition::ConditionFunc;

    static BTActionRegistry &Instance()
    {
        static BTActionRegistry instance;
        return instance;
    }

    /**
     * Register an action that can be used in YAML.
     * @param name Action name as it appears in YAML (e.g., "AimAtEnemy")
     * @param func Function that implements the action
     */
    void RegisterAction(const std::string &name, ActionFunc func)
    {
        if (actions.find(name) != actions.end()) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("WARNING: Overwriting existing action '%s'\n", name.c_str());
#endif
        }
        actions[name] = func;
    }

    /**
     * Register a condition that can be used in YAML.
     * @param name Condition name as it appears in YAML (e.g., "HasVisibleEnemy")
     * @param func Function that implements the condition check
     */
    void RegisterCondition(const std::string &name, ConditionFunc func)
    {
        if (conditions.find(name) != conditions.end()) {
#ifndef BEHAVIOR_TREE_TESTING
            gi.DPrintf("WARNING: Overwriting existing condition '%s'\n", name.c_str());
#endif
        }
        conditions[name] = func;
    }

    /**
     * Get action by name. Returns nullptr if not found.
     */
    [[nodiscard]] ActionFunc GetAction(const std::string &name) const
    {
        auto it = actions.find(name);
        return (it != actions.end()) ? it->second : nullptr;
    }

    /**
     * Get condition by name. Returns nullptr if not found.
     */
    [[nodiscard]] ConditionFunc GetCondition(const std::string &name) const
    {
        auto it = conditions.find(name);
        return (it != conditions.end()) ? it->second : nullptr;
    }

    /**
     * Check if action exists.
     */
    [[nodiscard]] bool HasAction(const std::string &name) const { return actions.find(name) != actions.end(); }

    /**
     * Check if condition exists.
     */
    [[nodiscard]] bool HasCondition(const std::string &name) const
    {
        return conditions.find(name) != conditions.end();
    }

    /**
     * Get list of all registered action names (for debugging/help).
     */
    [[nodiscard]] std::vector<std::string> GetActionNames() const
    {
        std::vector<std::string> names;
        names.reserve(actions.size());
        for (const auto &pair : actions) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * Get list of all registered condition names (for debugging/help).
     */
    [[nodiscard]] std::vector<std::string> GetConditionNames() const
    {
        std::vector<std::string> names;
        names.reserve(conditions.size());
        for (const auto &pair : conditions) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * Clear all registrations (for testing).
     */
    void Clear()
    {
        actions.clear();
        conditions.clear();
    }

private:
    BTActionRegistry() = default;

    std::unordered_map<std::string, ActionFunc>    actions;
    std::unordered_map<std::string, ConditionFunc> conditions;
};

// Convenience macros for registering actions/conditions
#define REGISTER_BT_ACTION(name, func) BTActionRegistry::Instance().RegisterAction(name, func)

#define REGISTER_BT_CONDITION(name, func) BTActionRegistry::Instance().RegisterCondition(name, func)

#endif // __BT_ACTION_REGISTRY_H__
