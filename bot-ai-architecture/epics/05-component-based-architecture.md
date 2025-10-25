# Epic 5: Component-Based Architecture (ECS)

## Overview
Refactor monolithic BotController (50+ member variables) into Entity-Component-System architecture for better performance, modularity, and maintainability.

## Business Value
- **Performance:** Data-oriented design = cache-friendly = faster
- **Modularity:** Add/remove capabilities via components
- **Clarity:** Systems are single-purpose and stateless
- **Scalability:** Easy to parallelize system updates

## Current State
```cpp
class BotController {
    // 50+ member variables mixed together
    int m_iCuriousTime;
    int m_iAttackTime;
    SafePtr<Sentient> m_pEnemy;
    CoverPoint m_currentCover;
    FireMode m_fireMode;
    SquadInfo m_squad;
    // ... 40 more
};
```

**Problems:**
- Monolithic class (hard to understand)
- Poor cache locality (variables accessed together aren't stored together)
- Difficult to add/remove capabilities
- Hard to parallelize updates

## Target State
```cpp
// Components are pure data
struct CombatComponent {
    FireMode fireMode;
    float burstTimer;
    float reloadTimer;
};

struct CoverComponent {
    CoverPoint current;
    CoverState state;
    int nextPeekTime;
};

struct SquadComponent {
    SquadID squad;
    SquadRole role;
    std::vector<EntityID> allies;
};

// Systems operate on components
class CombatSystem : public System {
    void Update(Registry& registry, float dt) {
        for (auto [entity, combat, weapon] : registry.view<CombatComponent, WeaponComponent>()) {
            UpdateFireMode(combat, weapon);
        }
    }
};
```

## Acceptance Criteria
- [ ] ECS library integrated (EnTT or custom)
- [ ] BotController refactored into 10+ components
- [ ] 5+ systems for different AI aspects
- [ ] Performance improvement: 20%+ faster with many bots
- [ ] Easy to add new components without modifying existing code

## Technical Components

### Core Components
- TransformComponent (position, angles, velocity)
- HealthComponent (current, max, regen)
- WeaponComponent (current weapon, ammo, accuracy)
- CombatComponent (fire mode, burst timing)
- PerceptionComponent (visible enemies, sounds)
- MemoryComponent (enemy memories)
- CoverComponent (cover state, peek timing)
- SquadComponent (squad membership, role)
- BehaviorTreeComponent (tree, blackboard)

### Core Systems
- PerceptionSystem (update vision, hearing)
- BehaviorTreeSystem (execute behavior trees)
- CombatSystem (fire mode, burst timing)
- MovementSystem (pathfinding, collision)
- SquadSystem (coordinate with allies)

## Dependencies
- ECS library (EnTT recommended) or custom implementation
- Refactoring time (gradual migration)

## Risks
- **HIGH:** Large refactor, potential for bugs
- **Mitigation:** Gradual migration, thorough testing, keep old code working in parallel

## Related Epics
- All other epics benefit from cleaner architecture

## References
- `references/ecs-architecture.md`
- [EnTT library](https://github.com/skypjack/entt)
- [Data-Oriented Design Book](https://www.dataorienteddesign.com/dodbook/)
