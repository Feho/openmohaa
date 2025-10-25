# Entity Component System (ECS) - Reference & Theory

## Overview
ECS is a data-oriented architecture pattern that separates data (Components) from logic (Systems) and identity (Entities). It's widely used in modern game engines for performance and flexibility.

## Core Concepts

### Entity
A unique identifier (usually just an integer ID).

```cpp
using Entity = uint32_t;

Entity bot1 = 42;
Entity bot2 = 43;
```

**Entities have NO DATA**. They're just IDs.

### Component
Pure data, no logic.

```cpp
struct HealthComponent {
    float current;
    float max;
    float regenRate;
};

struct PositionComponent {
    Vector position;
    Vector velocity;
};
```

Components are tagged to entities:
```cpp
Entity bot = CreateEntity();
AddComponent<HealthComponent>(bot, {100.0f, 100.0f, 0.5f});
AddComponent<PositionComponent>(bot, {{0,0,0}, {0,0,0}});
```

### System
Logic that operates on components.

```cpp
class HealthSystem {
    void Update(Registry& registry, float dt) {
        // Iterate all entities with HealthComponent
        for (auto [entity, health] : registry.view<HealthComponent>()) {
            // Apply health regen
            health.current = std::min(health.current + health.regenRate * dt, health.max);
        }
    }
};
```

**Systems have NO STATE** (ideally). They're pure functions over components.

## Traditional OOP vs ECS

### Traditional (Inheritance)

```cpp
class Entity {
    Vector position;
    virtual void Update() = 0;
};

class Character : public Entity {
    float health;
    virtual void Update() override { /* ... */ }
};

class Bot : public Character {
    AI ai;
    virtual void Update() override { /* ... */ }
};
```

**Problems:**
- Deep inheritance hierarchies (fragile)
- Diamond problem
- Hard to add capabilities (must modify class)
- Poor cache locality (vtables, scattered data)

### ECS

```cpp
// Entities are just IDs
Entity bot = CreateEntity();

// Components are data
AddComponent<PositionComponent>(bot, ...);
AddComponent<HealthComponent>(bot, ...);
AddComponent<AIComponent>(bot, ...);

// Systems are logic
PositionSystem::Update(registry, dt);  // Updates all positions
HealthSystem::Update(registry, dt);    // Updates all health
AISystem::Update(registry, dt);        // Updates all AI
```

**Benefits:**
- No inheritance (composition!)
- Easy to add/remove capabilities
- Data-oriented (cache-friendly)
- Systems can run in parallel

## Data-Oriented Design

### Cache Locality

**Bad (OOP):**
```
[Bot1: pos, health, AI, weapon, ...]  ← Cache miss
[Bot2: pos, health, AI, weapon, ...]  ← Cache miss
[Bot3: pos, health, AI, weapon, ...]  ← Cache miss
```

Updating positions touches every Bot object (scattered in memory).

**Good (ECS):**
```
PositionComponents: [pos1, pos2, pos3, ...]  ← Sequential!
HealthComponents:   [hp1, hp2, hp3, ...]
AIComponents:       [ai1, ai2, ai3, ...]
```

Updating positions iterates tightly-packed array (cache-friendly!).

### SoA vs AoS

**Array of Structures (AoS)** - OOP style:
```cpp
struct Bot {
    Vector position;
    float health;
    Weapon weapon;
};

std::vector<Bot> bots;
```

**Structure of Arrays (SoA)** - ECS style:
```cpp
std::vector<Vector> positions;
std::vector<float> healths;
std::vector<Weapon> weapons;
```

SoA is faster for iterating a single field (positions) across many entities.

## ECS Libraries

### EnTT (Recommended for C++)
```cpp
#include <entt/entt.hpp>

entt::registry registry;

// Create entity
auto entity = registry.create();

// Add components
registry.emplace<PositionComponent>(entity, Vector{0,0,0});
registry.emplace<HealthComponent>(entity, 100.0f, 100.0f);

// Iterate
auto view = registry.view<PositionComponent, HealthComponent>();
for (auto [entity, pos, health] : view.each()) {
    // Process
}
```

**Pros:**
- Header-only
- Very fast
- Mature, well-tested
- Great documentation

### EntityX
Older, simpler alternative to EnTT.

### Unity's DOTS (Data-Oriented Technology Stack)
C# ECS for Unity engine.

### Custom Implementation
For learning or specific needs.

```cpp
class Registry {
    std::map<EntityID, std::unordered_map<ComponentType, Component*>> components;

    template<typename T>
    T* GetComponent(EntityID entity);

    template<typename T>
    void AddComponent(EntityID entity, T component);
};
```

## Common Patterns

### Pattern 1: Component Queries

Get all entities with specific components:

```cpp
// All entities with Position AND Velocity
auto view = registry.view<Position, Velocity>();

// All entities with Health but NOT Dead
auto view = registry.view<Health>(entt::exclude<Dead>);
```

### Pattern 2: Tags

Components with no data (boolean flags):

```cpp
struct InCombatTag {};
struct DeadTag {};

// Add tag
registry.emplace<InCombatTag>(entity);

// Check tag
bool inCombat = registry.any_of<InCombatTag>(entity);

// Remove tag
registry.remove<InCombatTag>(entity);
```

### Pattern 3: Singleton Components

Global state as components:

```cpp
struct GameStateComponent {
    int score;
    float timeRemaining;
};

// Attach to special entity
Entity globalEntity = registry.create();
registry.emplace<GameStateComponent>(globalEntity, 0, 300.0f);

// Access
auto& gameState = registry.get<GameStateComponent>(globalEntity);
```

### Pattern 4: Events

Components as events:

```cpp
struct DamageEvent {
    Entity source;
    Entity target;
    float amount;
};

// Create event entity
Entity event = registry.create();
registry.emplace<DamageEvent>(event, attacker, victim, 50.0f);

// Process events
for (auto [entity, dmg] : registry.view<DamageEvent>().each()) {
    ApplyDamage(dmg.target, dmg.amount);
    registry.destroy(entity);  // Remove event
}
```

## Systems Design

### Single Responsibility

Each system does ONE thing:

```cpp
class MovementSystem {
    void Update(Registry& registry, float dt) {
        auto view = registry.view<Position, Velocity>();
        for (auto [entity, pos, vel] : view.each()) {
            pos.position += vel.velocity * dt;
        }
    }
};

class GravitySystem {
    void Update(Registry& registry, float dt) {
        auto view = registry.view<Velocity, GravityComponent>();
        for (auto [entity, vel, grav] : view.each()) {
            vel.velocity.z -= grav.force * dt;
        }
    }
};
```

### System Ordering

Some systems depend on others:

```cpp
void Update(float dt) {
    // Order matters!
    inputSystem.Update(registry, dt);     // Read input first
    aiSystem.Update(registry, dt);         // AI decides actions
    physicsSystem.Update(registry, dt);    // Apply physics
    animationSystem.Update(registry, dt);  // Animate based on state
    renderSystem.Update(registry, dt);     // Render last
}
```

### Parallel Systems

Independent systems can run concurrently:

```cpp
// These don't interfere, can run in parallel
std::thread t1([&]() { healthRegenSystem.Update(registry, dt); });
std::thread t2([&]() { ammoRegenSystem.Update(registry, dt); });
std::thread t3([&]() { cooldownSystem.Update(registry, dt); });

t1.join();
t2.join();
t3.join();
```

## Advantages of ECS

### 1. Flexibility

Easy to add/remove capabilities:

```cpp
// Make entity invisible
registry.emplace<InvisibleComponent>(entity);

// Remove flight capability
registry.remove<FlyingComponent>(entity);
```

No need to modify class hierarchy!

### 2. Performance

Data-oriented = cache-friendly = fast.

Benchmarks show 10-100x speedup for tight loops over components.

### 3. Testability

Systems are stateless functions → easy to test:

```cpp
TEST(HealthSystemTest, RegeneratesHealth) {
    Registry registry;
    Entity entity = registry.create();
    registry.emplace<HealthComponent>(entity, 50.0f, 100.0f, 10.0f);

    HealthSystem system;
    system.Update(registry, 1.0f);  // 1 second

    auto& health = registry.get<HealthComponent>(entity);
    EXPECT_EQ(health.current, 60.0f);  // 50 + 10*1
}
```

### 4. Serialization

Components are plain data → easy to serialize:

```cpp
void SaveEntity(Entity entity) {
    for (auto& [type, component] : GetComponents(entity)) {
        SaveComponent(type, component);
    }
}
```

### 5. Tools/Debugging

Entity inspector shows all components:

```
Entity 42:
  ├─ PositionComponent: (100, 200, 50)
  ├─ HealthComponent: 75 / 100
  ├─ WeaponComponent: Rifle (30 rounds)
  └─ AIComponent: InCombat
```

## Disadvantages & Mitigations

### 1. More Boilerplate

**Problem:** Need to define many component structs and systems.

**Mitigation:**
- Code generation tools
- Libraries like EnTT reduce boilerplate
- Worth it for large projects

### 2. Steeper Learning Curve

**Problem:** Paradigm shift from OOP.

**Mitigation:**
- Good documentation
- Simple examples
- Gradual migration (ECS alongside OOP)

### 3. Indirection

**Problem:** `registry.get<Component>(entity)` vs `object.field`.

**Mitigation:**
- Optimized libraries (EnTT is very fast)
- Cache component pointers in tight loops

### 4. Relationships

**Problem:** Entity references can break (deleted entities).

**Mitigation:**
- Use weak references / entity handles
- Observer pattern for deletion events
- Validate references before use

## ECS for OpenMoHAA Bots

### Before (Monolithic)

```cpp
class BotController {
    // 50+ member variables
    SafePtr<Sentient> m_pEnemy;
    CoverPoint m_currentCover;
    FireMode m_fireMode;
    SquadInfo m_squad;
    // ... 40 more

    void Think() {
        // 300 lines of mixed logic
    }
};
```

### After (ECS)

```cpp
// Components (data)
struct EnemyComponent { Sentient* current; };
struct CoverComponent { CoverPoint current; CoverState state; };
struct CombatComponent { FireMode mode; float burstTimer; };
struct SquadComponent { SquadID squad; SquadRole role; };

// Systems (logic)
class PerceptionSystem { void Update(Registry&, float dt); };
class CombatSystem { void Update(Registry&, float dt); };
class SquadSystem { void Update(Registry&, float dt); };

// Update
void BotManager::Update(float dt) {
    perceptionSystem.Update(registry, dt);
    combatSystem.Update(registry, dt);
    squadSystem.Update(registry, dt);
}
```

**Benefits:**
- Clear separation of concerns
- Easy to test each system
- Easy to add new capabilities (just add component + system)
- Better performance (cache locality)

## Resources

### Articles
- [Data-Oriented Design](https://www.dataorienteddesign.com/dodbook/)
- [Overwatch Gameplay Architecture and Netcode](https://www.gdcvault.com/play/1024001/)

### Libraries
- [EnTT](https://github.com/skypjack/entt) - C++ ECS
- [flecs](https://github.com/SanderMertens/flecs) - Fast & Flexible ECS

### Talks
- GDC 2018: "ECS back and forth" - Jonathan Blow
- CppCon 2018: "Game Engine Entity Systems" - Bob Nystrom

### Books
- "Data-Oriented Design" - Richard Fabian
- "Game Programming Patterns" - Robert Nystrom (Component chapter)

## Summary

**ECS** = Entities (IDs) + Components (data) + Systems (logic)

**Key Principles:**
- Composition over inheritance
- Data-oriented design
- Cache-friendly iteration
- Single-responsibility systems

**Best For:**
- Large numbers of entities (100+)
- Performance-critical code
- Systems that need parallelization
- Games with dynamic entity capabilities

**Not Needed For:**
- Small projects
- UI (traditional OOP fine)
- One-off logic

For OpenMoHAA bots: **ECS is overkill for initial implementation, but valuable for Phase 4 optimization if you need 100+ bots at 60 FPS.**

See `examples/component-system-example.cpp` for complete implementation.
