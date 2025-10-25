# Epic 9: Modularity & Plugin System

## Overview
Create plugin architecture allowing game modes, mods, and third-party developers to extend bot AI without modifying core code.

## Business Value
- **Extensibility:** New behaviors without touching core
- **Modularity:** Features can be enabled/disabled
- **Community:** Third-party mods can add behaviors
- **Experimentation:** Try new ideas without risk

## Current State
All AI logic in core files. Adding game-mode-specific behavior requires modifying core.

**Problems:**
- Core code bloats with mode-specific features
- Hard to maintain (changes affect everything)
- No community extensibility

## Target State
```cpp
class IBehaviorPlugin {
    virtual void Initialize() = 0;
    virtual void RegisterBehaviors(BehaviorTreeFactory& factory) = 0;
    virtual void RegisterUtilityActions(UtilityEvaluator& eval) = 0;
    virtual void OnEvent(const AIEvent& event) = 0;
};

// Example plugin
class VehicleBehaviorPlugin : public IBehaviorPlugin {
    void RegisterBehaviors(BehaviorTreeFactory& factory) override {
        factory.RegisterAction("EnterVehicle", &EnterVehicle);
        factory.RegisterAction("DriveVehicle", &DriveVehicle);
        factory.RegisterAction("GunnerPosition", &UseGunnerPosition);
    }
};

// Loaded via config
plugins:
  - core/combat
  - core/movement
  - gamemodes/objective
  - optional/vehicles
```

## Acceptance Criteria
- [ ] Plugin interface defined
- [ ] Core behaviors as plugins (prove system works)
- [ ] Example third-party plugin
- [ ] Plugins loaded from config
- [ ] Runtime enable/disable plugins
- [ ] Plugin hot-reload for development

## Technical Components

### Plugin Interface
```cpp
class IBehaviorPlugin {
    virtual const char* GetName() = 0;
    virtual const char* GetVersion() = 0;
    virtual void Initialize(PluginContext& ctx) = 0;
    virtual void Shutdown() = 0;
    virtual void RegisterBehaviors(BehaviorTreeFactory&) = 0;
    virtual void RegisterUtilityActions(UtilityEvaluator&) = 0;
    virtual void OnEvent(const AIEvent&) = 0;
};
```

### Plugin Manager
```cpp
class PluginManager {
    void LoadPlugin(const char* path);
    void UnloadPlugin(const char* name);
    void ReloadPlugin(const char* name);
    void RegisterPlugin(IBehaviorPlugin* plugin);
};
```

### Example Plugins
- **CoreCombatPlugin:** Basic combat behaviors
- **ObjectiveGameModePlugin:** CTF, bomb planting
- **VehicleBehaviorPlugin:** Vehicle usage
- **TauntPlugin:** Voice lines, gestures

## Dependencies
- Dynamic library loading (DLLs/SOs)
- Plugin configuration system

## Related Epics
- Epic 1 (plugins register BT nodes)
- Epic 3 (plugins register utility actions)

## References
- [Game Plugin Architectures](https://gameprogrammingpatterns.com/subclass-sandbox.html)
