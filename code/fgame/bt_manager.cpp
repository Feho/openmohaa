// Added in OPM - Phase 2B Task 2B.2 Review Fixes
// bt_manager.cpp: Implementation of behavior tree manager

#include "bt_manager.h"
#include "bt_yaml_loader.h"
#include "g_local.h"

bool BTManager::LoadTree(const std::string &name, const char *filepath)
{
    if (!filepath || strlen(filepath) == 0) {
        gi.Printf("ERROR: BTManager::LoadTree - invalid filepath\n");
        return false;
    }

    // Load tree from YAML
    auto tree = BTYamlLoader::LoadFromFile(filepath);
    if (!tree) {
        gi.Printf("ERROR: BTManager::LoadTree - failed to load tree from '%s'\n", filepath);
        return false;
    }

    // Store or replace existing tree
    auto it = trees.find(name);
    if (it != trees.end()) {
        gi.DPrintf("BTManager: Replacing existing tree '%s'\n", name.c_str());
        trees.erase(it);
    }

    trees.emplace(name, TreeEntry(std::move(tree), filepath));
    gi.DPrintf("BTManager: Loaded tree '%s' from '%s'\n", name.c_str(), filepath);

    return true;
}

bool BTManager::ReloadTree(const std::string &name)
{
    auto it = trees.find(name);

    if (it == trees.end()) {
        gi.Printf("ERROR: BTManager::ReloadTree - tree '%s' not found\n", name.c_str());
        return false;
    }

    // Get the original filepath
    const std::string filepath = it->second.filepath;

    // Unload and reload
    trees.erase(it);

    gi.Printf("BTManager: Reloading tree '%s' from '%s'\n", name.c_str(), filepath.c_str());
    return LoadTree(name, filepath.c_str());
}

bool BTManager::UnloadTree(const std::string &name)
{
    auto it = trees.find(name);

    if (it == trees.end()) {
        return false;
    }

    gi.DPrintf("BTManager: Unloading tree '%s'\n", name.c_str());
    trees.erase(it);
    return true;
}

BehaviorTree *BTManager::GetTree(const std::string &name) const
{
    auto it = trees.find(name);
    return (it != trees.end()) ? it->second.tree.get() : nullptr;
}

bool BTManager::HasTree(const std::string &name) const
{
    return trees.find(name) != trees.end();
}

std::vector<std::string> BTManager::GetLoadedTreeNames() const
{
    std::vector<std::string> names;
    names.reserve(trees.size());

    for (const auto &pair : trees) {
        names.push_back(pair.first);
    }

    return names;
}

void BTManager::Clear()
{
    gi.DPrintf("BTManager: Clearing all trees (%d loaded)\n", static_cast<int>(trees.size()));
    trees.clear();
}
