// Added in OPM - Phase 2B Task 2B.2 Review Fixes
// bt_manager.h: Manager for behavior tree lifecycle

#ifndef __BT_MANAGER_H__
#define __BT_MANAGER_H__

#include "behavior_tree.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

/**
 * Manages the lifecycle of behavior trees in the game.
 * Provides loading, storage, retrieval, and unloading of trees.
 *
 * This is a singleton that stores all loaded behavior trees by name,
 * allowing them to be reused across multiple bots without reloading.
 *
 * Thread Safety: NOT thread-safe. All operations should be called from
 * the main game thread.
 */
class BTManager {
public:
    /**
     * Get the singleton instance.
     */
    static BTManager &Instance()
    {
        static BTManager instance;
        return instance;
    }

    /**
     * Load a behavior tree from file and store it.
     * If a tree with the same name already exists, it will be replaced.
     *
     * @param name Unique name for this tree (e.g., "engage_enemy")
     * @param filepath Path to YAML file (e.g., "behaviors/engage_enemy.yaml")
     * @return true if loaded successfully, false otherwise
     */
    bool LoadTree(const std::string &name, const char *filepath);

    /**
     * Reload a behavior tree from its original filepath.
     * If the tree doesn't exist, this will load it for the first time.
     *
     * @param name Name of tree to reload
     * @return true if reloaded successfully, false otherwise
     */
    bool ReloadTree(const std::string &name);

    /**
     * Unload a behavior tree and free its memory.
     * @param name Name of tree to unload
     * @return true if tree existed and was unloaded, false if not found
     */
    bool UnloadTree(const std::string &name);

    /**
     * Get a behavior tree by name.
     * @param name Name of tree to retrieve
     * @return Pointer to tree, or nullptr if not found
     */
    [[nodiscard]] BehaviorTree *GetTree(const std::string &name) const;

    /**
     * Check if a tree is loaded.
     * @param name Name of tree to check
     * @return true if tree is loaded, false otherwise
     */
    [[nodiscard]] bool HasTree(const std::string &name) const;

    /**
     * Get list of all loaded tree names.
     * @return Vector of tree names
     */
    [[nodiscard]] std::vector<std::string> GetLoadedTreeNames() const;

    /**
     * Get number of loaded trees.
     */
    [[nodiscard]] size_t GetTreeCount() const { return trees.size(); }

    /**
     * Unload all trees and clear manager.
     * Useful for map changes or testing.
     */
    void Clear();

private:
    BTManager() = default;

    struct TreeEntry {
        std::unique_ptr<BehaviorTree> tree;
        std::string                   filepath; // Store filepath for reload support

        TreeEntry(std::unique_ptr<BehaviorTree> t, const std::string &path)
            : tree(std::move(t))
            , filepath(path)
        {
        }
    };

    std::unordered_map<std::string, TreeEntry> trees;
};

#endif // __BT_MANAGER_H__
