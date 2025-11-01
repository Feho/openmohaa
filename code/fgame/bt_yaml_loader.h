// Added in OPM - Phase 2B Task 2B.2
// bt_yaml_loader.h: Loads behavior trees from YAML files

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
 *
 * YAML Format:
 * tree:
 *   name: "Tree Name"
 *   description: "Description"
 *   root:
 *     type: selector|sequence|parallel|condition|action
 *     children: [...]  # For composite nodes
 *     check: "ConditionName"  # For condition nodes
 *     action: "ActionName"  # For action nodes
 *     policy: RequireAll|RequireOne|RequireN  # For parallel nodes
 *     required_count: N  # For parallel RequireN policy
 */
class BTYamlLoader {
public:
    /**
     * Load a behavior tree from YAML file.
     * @param filepath Path to YAML file (e.g., "behaviors/engage_enemy.yaml")
     * @return Loaded tree, or nullptr on error
     */
    static std::unique_ptr<BehaviorTree> LoadFromFile(const char *filepath)
    {
        try {
            // Load YAML file using game filesystem
            void *buffer = nullptr;
            long  length = gi.FS_ReadFile(filepath, &buffer, qfalse);
            
            if (length < 0 || !buffer) {
                gi.Printf("ERROR: Could not read behavior tree file: %s\n", filepath);
                return nullptr;
            }

            // Parse YAML from buffer
            YAML::Node root = YAML::Load(static_cast<const char *>(buffer));
            gi.FS_FreeFile(buffer);

            if (!root["tree"]) {
                gi.Printf("ERROR: YAML file missing 'tree' root node: %s\n", filepath);
                return nullptr;
            }

            YAML::Node treeNode = root["tree"];

            // Optional metadata
            if (treeNode["name"]) {
                gi.DPrintf("Loading behavior tree: %s\n", treeNode["name"].as<std::string>().c_str());
            }

            // Load root node
            if (!treeNode["root"]) {
                gi.Printf("ERROR: Tree missing 'root' node: %s\n", filepath);
                return nullptr;
            }

            auto tree     = std::make_unique<BehaviorTree>();
            auto rootNode = LoadNode(treeNode["root"], filepath);

            if (!rootNode) {
                gi.Printf("ERROR: Failed to load root node: %s\n", filepath);
                return nullptr;
            }

            tree->SetRoot(std::move(rootNode));
            return tree;

        } catch (const YAML::Exception &e) {
            gi.Printf("ERROR: YAML parse error in %s: %s\n", filepath, e.what());
            return nullptr;
        } catch (const std::exception &e) {
            gi.Printf("ERROR: Exception while loading %s: %s\n", filepath, e.what());
            return nullptr;
        }
    }

private:
    /**
     * Recursively load a node from YAML.
     */
    static std::unique_ptr<BTNode> LoadNode(const YAML::Node &node, const char *filepath)
    {
        if (!node["type"]) {
            gi.Printf("ERROR: Node missing 'type' field in %s\n", filepath);
            return nullptr;
        }

        std::string type = node["type"].as<std::string>();

        // Composite nodes
        if (type == "selector") {
            return LoadSelector(node, filepath);
        } else if (type == "sequence") {
            return LoadSequence(node, filepath);
        } else if (type == "parallel") {
            return LoadParallel(node, filepath);
        }
        // Leaf nodes
        else if (type == "condition") {
            return LoadCondition(node, filepath);
        } else if (type == "action") {
            return LoadAction(node, filepath);
        } else {
            gi.Printf("ERROR: Unknown node type '%s' in %s\n", type.c_str(), filepath);
            return nullptr;
        }
    }

    static std::unique_ptr<BTNode> LoadSelector(const YAML::Node &node, const char *filepath)
    {
        auto selector = std::make_unique<BTSelector>();

        if (!node["children"]) {
            gi.Printf("ERROR: Selector missing 'children' in %s\n", filepath);
            return nullptr;
        }

        // Changed in OPM
        //  Fixed unsafe static_cast - BTSelector inherits from BTComposite, so cast is safe
        //  but we make it explicit for clarity and use the composite pointer directly
        BTComposite *composite = selector.get();

        for (const auto &childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            composite->AddChild(std::move(child));
        }

        return selector;
    }

    static std::unique_ptr<BTNode> LoadSequence(const YAML::Node &node, const char *filepath)
    {
        auto sequence = std::make_unique<BTSequence>();

        if (!node["children"]) {
            gi.Printf("ERROR: Sequence missing 'children' in %s\n", filepath);
            return nullptr;
        }

        // Changed in OPM
        //  Fixed unsafe static_cast - BTSequence inherits from BTComposite, so cast is safe
        //  but we make it explicit for clarity and use the composite pointer directly
        BTComposite *composite = sequence.get();

        for (const auto &childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            composite->AddChild(std::move(child));
        }

        return sequence;
    }

    static std::unique_ptr<BTNode> LoadParallel(const YAML::Node &node, const char *filepath)
    {
        BTParallel::Policy policy = BTParallel::Policy::RequireAll;

        if (node["policy"]) {
            std::string policyStr = node["policy"].as<std::string>();
            if (policyStr == "RequireOne") {
                policy = BTParallel::Policy::RequireOne;
            } else if (policyStr == "RequireN") {
                policy = BTParallel::Policy::RequireN;
            } else if (policyStr != "RequireAll") {
                gi.Printf("WARNING: Unknown parallel policy '%s' in %s, using RequireAll\n", policyStr.c_str(), filepath);
            }
        }

        // Changed in OPM
        //  Added validation for parallel node configuration
        int requiredCount = node["required_count"] ? node["required_count"].as<int>() : 0;

        // Validate requiredCount
        if (requiredCount < 0) {
            gi.Printf("ERROR: Parallel node has negative required_count (%d) in %s\n", requiredCount, filepath);
            return nullptr;
        }

        if (policy == BTParallel::Policy::RequireN && requiredCount == 0) {
            gi.Printf("ERROR: Parallel node with RequireN policy must have required_count > 0 in %s\n", filepath);
            return nullptr;
        }

        auto parallel = std::make_unique<BTParallel>(policy, requiredCount);

        if (!node["children"]) {
            gi.Printf("ERROR: Parallel missing 'children' in %s\n", filepath);
            return nullptr;
        }

        // Changed in OPM
        //  Fixed unsafe static_cast - BTParallel inherits from BTComposite, so cast is safe
        //  but we make it explicit for clarity and use the composite pointer directly
        BTComposite *composite = parallel.get();

        // Load children and count them
        int childCount = 0;
        for (const auto &childNode : node["children"]) {
            auto child = LoadNode(childNode, filepath);
            if (!child) {
                return nullptr;
            }
            composite->AddChild(std::move(child));
            ++childCount;
        }

        // Changed in OPM
        //  Validate that requiredCount doesn't exceed child count
        if (policy == BTParallel::Policy::RequireN && requiredCount > childCount) {
            gi.Printf(
                "ERROR: Parallel node requires %d children but only has %d in %s\n", requiredCount, childCount, filepath
            );
            return nullptr;
        }

        return parallel;
    }

    static std::unique_ptr<BTNode> LoadCondition(const YAML::Node &node, const char *filepath)
    {
        if (!node["check"]) {
            gi.Printf("ERROR: Condition missing 'check' field in %s\n", filepath);
            return nullptr;
        }

        std::string checkName = node["check"].as<std::string>();

        auto func = BTActionRegistry::Instance().GetCondition(checkName);
        if (!func) {
            gi.Printf("ERROR: Unknown condition '%s' in %s\n", checkName.c_str(), filepath);
            PrintAvailableConditions();
            return nullptr;
        }

        return std::make_unique<BTCondition>(checkName.c_str(), func);
    }

    static std::unique_ptr<BTNode> LoadAction(const YAML::Node &node, const char *filepath)
    {
        if (!node["action"]) {
            gi.Printf("ERROR: Action missing 'action' field in %s\n", filepath);
            return nullptr;
        }

        std::string actionName = node["action"].as<std::string>();

        auto func = BTActionRegistry::Instance().GetAction(actionName);
        if (!func) {
            gi.Printf("ERROR: Unknown action '%s' in %s\n", actionName.c_str(), filepath);
            PrintAvailableActions();
            return nullptr;
        }

        return std::make_unique<BTAction>(actionName.c_str(), func);
    }

    static void PrintAvailableConditions()
    {
        auto conditions = BTActionRegistry::Instance().GetConditionNames();
        gi.Printf("       Available conditions:\n");
        for (const auto &name : conditions) {
            gi.Printf("         - %s\n", name.c_str());
        }
    }

    static void PrintAvailableActions()
    {
        auto actions = BTActionRegistry::Instance().GetActionNames();
        gi.Printf("       Available actions:\n");
        for (const auto &name : actions) {
            gi.Printf("         - %s\n", name.c_str());
        }
    }
};

#endif // __BT_YAML_LOADER_H__
