// Entity Component System (ECS) Example
// Shows how to refactor BotController into data-oriented architecture

#include <entt/entt.hpp>  // Using EnTT library as example
#include <vector>
#include <memory>

// ============================================================================
// Components (Pure Data)
// ============================================================================

struct TransformComponent {
    Vector position;
    Vector velocity;
    Vector angles;
    Vector angularVelocity;
};

struct HealthComponent {
    float current;
    float max;
    float regenRate;
    float lastDamageTime;
};

struct WeaponComponent {
    Weapon* currentWeapon;
    std::map<WeaponType, int> ammoByType;
    float spread;
    float recoilAccumulation;
    float lastFireTime;
};

struct CombatComponent {
    enum FireMode { ACCURATE, BURST, SUPPRESSION, MELEE };
    FireMode fireMode;

    float burstDuration;
    float burstDelay;
    float burstTimer;
    float continuousFireTimer;

    bool requireLowSpread;
    bool isReloading;
};

struct PerceptionComponent {
    std::vector<EntityID> visibleEnemies;
    std::vector<EntityID> knownEnemies;
    std::vector<AudioEvent> recentSounds;

    float visionRange;
    float fov;
    float hearingRange;
};

struct MemoryComponent {
    struct EnemyMemory {
        EntityID enemy;
        Vector lastKnownPosition;
        Vector predictedPosition;
        float lastSeenTime;
        float confidenceLevel;
    };

    std::map<EntityID, EnemyMemory> enemyMemories;
    int maxMemories = 5;
};

struct CoverComponent {
    enum State { NONE, MOVING_TO, IN_COVER, PEEKING, REPOSITIONING };

    Vector coverPosition;
    float coverQuality;
    State state;
    float nextPeekTime;
    float peekDuration;
};

struct SquadComponent {
    enum Role { NONE, AGGRESSOR, FLANKER, SUPPORT, DEFENDER };

    EntityID squadID;
    Role role;
    std::vector<EntityID> squadMembers;
    EntityID leader;
    EntityID sharedTarget;
};

struct BehaviorTreeComponent {
    std::unique_ptr<BehaviorTree> tree;
    Blackboard blackboard;
    float lastExecuteTime;
};

struct AIStateComponent {
    // General AI state
    int lastUpdateTime;
    int stateEntryTime;
    bool isInCombat;
    bool recentlyDamaged;

    // LOD
    enum LOD { HIGH, MEDIUM, LOW, SLEEPING };
    LOD currentLOD;
};

// ============================================================================
// Systems (Logic that operates on components)
// ============================================================================

class PerceptionSystem {
public:
    void Update(entt::registry& registry, float dt) {
        // Process all entities with perception
        auto view = registry.view<TransformComponent, PerceptionComponent, MemoryComponent>();

        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& perception = view.get<PerceptionComponent>(entity);
            auto& memory = view.get<MemoryComponent>(entity);

            UpdateVision(entity, transform, perception, memory);
            UpdateHearing(entity, transform, perception);
            UpdateMemories(memory, dt);
        }
    }

private:
    void UpdateVision(
        entt::entity entity,
        TransformComponent& transform,
        PerceptionComponent& perception,
        MemoryComponent& memory
    ) {
        perception.visibleEnemies.clear();

        // Find all potential enemies in range
        for (auto potentialEnemy : GetEntitiesInRange(transform.position, perception.visionRange)) {
            if (!IsEnemy(entity, potentialEnemy)) continue;

            // Check FOV and occlusion
            if (CanSee(transform, potentialEnemy, perception.fov)) {
                perception.visibleEnemies.push_back(potentialEnemy);

                // Update memory
                StoreEnemyMemory(memory, potentialEnemy);
            }
        }
    }

    void UpdateHearing(
        entt::entity entity,
        TransformComponent& transform,
        PerceptionComponent& perception
    ) {
        // Process audio events from event queue
        for (const auto& event : GetAudioEvents()) {
            float distance = (event.position - transform.position).length();
            if (distance > perception.hearingRange) continue;

            perception.recentSounds.push_back(event);
        }

        // Remove old sounds (older than 5 seconds)
        auto now = GetCurrentTime();
        perception.recentSounds.erase(
            std::remove_if(perception.recentSounds.begin(), perception.recentSounds.end(),
                [now](const auto& sound) { return (now - sound.timestamp) > 5.0f; }),
            perception.recentSounds.end()
        );
    }

    void UpdateMemories(MemoryComponent& memory, float dt) {
        // Decay confidence over time
        for (auto& [id, mem] : memory.enemyMemories) {
            mem.confidenceLevel -= 0.1f * dt;  // Decay rate

            // Update predicted position
            float timeSinceLastSeen = GetCurrentTime() - mem.lastSeenTime;
            mem.predictedPosition = mem.lastKnownPosition + mem.velocity * timeSinceLastSeen;
        }

        // Remove low-confidence memories
        std::erase_if(memory.enemyMemories,
            [](const auto& pair) { return pair.second.confidenceLevel < 0.2f; });
    }
};

class BehaviorTreeSystem {
public:
    void Update(entt::registry& registry, float dt) {
        auto view = registry.view<BehaviorTreeComponent, PerceptionComponent, AIStateComponent>();

        for (auto entity : view) {
            auto& btree = view.get<BehaviorTreeComponent>(entity);
            auto& perception = view.get<PerceptionComponent>(entity);
            auto& aiState = view.get<AIStateComponent>(entity);

            // Skip if low LOD
            if (aiState.currentLOD == AIStateComponent::SLEEPING) {
                continue;
            }

            // Populate blackboard with current state
            PopulateBlackboard(entity, registry, btree.blackboard);

            // Execute behavior tree
            btree.tree->Execute(btree.blackboard, dt);
            btree.lastExecuteTime = GetCurrentTime();
        }
    }

private:
    void PopulateBlackboard(entt::entity entity, entt::registry& registry, Blackboard& bb) {
        // Get components
        auto* perception = registry.try_get<PerceptionComponent>(entity);
        auto* health = registry.try_get<HealthComponent>(entity);
        auto* weapon = registry.try_get<WeaponComponent>(entity);
        auto* cover = registry.try_get<CoverComponent>(entity);

        // Populate blackboard
        if (perception) {
            bb.Set("visibleEnemies", perception->visibleEnemies);
            bb.Set("hasVisibleEnemy", !perception->visibleEnemies.empty());
        }

        if (health) {
            bb.Set("health", health->current);
            bb.Set("healthPercent", health->current / health->max);
        }

        if (weapon) {
            bb.Set("weapon", weapon->currentWeapon);
            bb.Set("ammo", weapon->currentWeapon->GetAmmo());
            bb.Set("hasAmmo", weapon->currentWeapon->GetAmmo() > 0);
        }

        if (cover) {
            bb.Set("isInCover", cover->state == CoverComponent::IN_COVER);
            bb.Set("coverQuality", cover->coverQuality);
        }
    }
};

class CombatSystem {
public:
    void Update(entt::registry& registry, float dt) {
        auto view = registry.view<CombatComponent, WeaponComponent, PerceptionComponent>();

        for (auto entity : view) {
            auto& combat = view.get<CombatComponent>(entity);
            auto& weapon = view.get<WeaponComponent>(entity);
            auto& perception = view.get<PerceptionComponent>(entity);

            // Update fire mode based on situation
            UpdateFireMode(combat, weapon, perception);

            // Update burst timing
            UpdateBurstTiming(combat, dt);

            // Handle recoil accumulation
            UpdateRecoil(weapon, dt);
        }
    }

private:
    void UpdateFireMode(
        CombatComponent& combat,
        WeaponComponent& weapon,
        PerceptionComponent& perception
    ) {
        // Determine appropriate fire mode
        if (perception.visibleEnemies.empty()) {
            return;  // No target, no fire mode update
        }

        int enemyCount = perception.visibleEnemies.size();
        float range = GetRangeToClosestEnemy(perception);

        if (range < 256.0f) {
            combat.fireMode = CombatComponent::BURST;  // Close range, bursts
        } else if (range > 1024.0f) {
            combat.fireMode = CombatComponent::ACCURATE;  // Long range, accurate
        } else if (enemyCount >= 3) {
            combat.fireMode = CombatComponent::SUPPRESSION;  // Many enemies, suppress
        } else {
            combat.fireMode = CombatComponent::BURST;  // Default
        }
    }

    void UpdateBurstTiming(CombatComponent& combat, float dt) {
        if (combat.burstTimer > 0) {
            combat.burstTimer -= dt;
        }

        if (combat.continuousFireTimer > 0) {
            combat.continuousFireTimer -= dt;
        }
    }

    void UpdateRecoil(WeaponComponent& weapon, float dt) {
        // Decay recoil over time
        weapon.recoilAccumulation -= 0.5f * dt;
        weapon.recoilAccumulation = std::max(0.0f, weapon.recoilAccumulation);
    }
};

class MovementSystem {
public:
    void Update(entt::registry& registry, float dt) {
        auto view = registry.view<TransformComponent, AIStateComponent>();

        for (auto entity : view) {
            auto& transform = view.get<TransformComponent>(entity);
            auto& aiState = view.get<AIStateComponent>(entity);

            // Update position based on velocity
            transform.position += transform.velocity * dt;

            // Update angles based on angular velocity
            transform.angles += transform.angularVelocity * dt;
        }
    }
};

class SquadSystem {
public:
    void Update(entt::registry& registry, float dt) {
        auto view = registry.view<SquadComponent, TransformComponent, PerceptionComponent>();

        // Build squad groups
        std::map<EntityID, std::vector<entt::entity>> squads;
        for (auto entity : view) {
            auto& squad = view.get<SquadComponent>(entity);
            squads[squad.squadID].push_back(entity);
        }

        // Update each squad
        for (auto& [squadID, members] : squads) {
            UpdateSquad(registry, members);
        }
    }

private:
    void UpdateSquad(entt::registry& registry, const std::vector<entt::entity>& members) {
        // Share target information
        EntityID sharedTarget = nullptr;
        for (auto entity : members) {
            auto& perception = registry.get<PerceptionComponent>(entity);
            if (!perception.visibleEnemies.empty()) {
                sharedTarget = perception.visibleEnemies[0];
                break;
            }
        }

        // Set shared target for all squad members
        if (sharedTarget != nullptr) {
            for (auto entity : members) {
                auto& squad = registry.get<SquadComponent>(entity);
                squad.sharedTarget = sharedTarget;
            }
        }

        // Assign roles based on positions and capabilities
        AssignSquadRoles(registry, members);
    }

    void AssignSquadRoles(entt::registry& registry, const std::vector<entt::entity>& members) {
        // Simple role assignment logic
        for (size_t i = 0; i < members.size(); i++) {
            auto& squad = registry.get<SquadComponent>(members[i]);

            if (i == 0) {
                squad.role = SquadComponent::AGGRESSOR;
            } else if (i == 1) {
                squad.role = SquadComponent::FLANKER;
            } else {
                squad.role = SquadComponent::SUPPORT;
            }
        }
    }
};

// ============================================================================
// Main Update Loop
// ============================================================================

class BotAIManager {
public:
    void Update(float dt) {
        // Update systems in order
        perceptionSystem.Update(registry, dt);
        behaviorTreeSystem.Update(registry, dt);
        combatSystem.Update(registry, dt);
        movementSystem.Update(registry, dt);
        squadSystem.Update(registry, dt);
    }

    entt::entity CreateBot(Vector position) {
        auto entity = registry.create();

        // Add components
        registry.emplace<TransformComponent>(entity, position, vec_zero, vec_zero, vec_zero);
        registry.emplace<HealthComponent>(entity, 100.0f, 100.0f, 0.0f, 0.0f);
        registry.emplace<WeaponComponent>(entity);
        registry.emplace<CombatComponent>(entity);
        registry.emplace<PerceptionComponent>(entity, 2048.0f, 80.0f, 1024.0f);
        registry.emplace<MemoryComponent>(entity);
        registry.emplace<CoverComponent>(entity);
        registry.emplace<SquadComponent>(entity);
        registry.emplace<BehaviorTreeComponent>(entity);
        registry.emplace<AIStateComponent>(entity);

        return entity;
    }

    void DestroyBot(entt::entity entity) {
        registry.destroy(entity);
    }

private:
    entt::registry registry;

    // Systems
    PerceptionSystem perceptionSystem;
    BehaviorTreeSystem behaviorTreeSystem;
    CombatSystem combatSystem;
    MovementSystem movementSystem;
    SquadSystem squadSystem;
};

// ============================================================================
// Benefits of ECS
// ============================================================================

/*
Data-Oriented Design:
- Components stored contiguously in memory (cache-friendly)
- Systems iterate only over relevant components
- No vtable overhead, better performance

Flexibility:
- Easy to add/remove capabilities (just add/remove components)
- Systems are independent, can be disabled/enabled
- Can have entities with different component combinations

Parallelization:
- Systems can run concurrently (if independent)
- Component iteration is thread-friendly
- Easy to batch operations

Clarity:
- Single responsibility: each system does ONE thing
- Clean separation of data and logic
- Easy to test systems in isolation

Example:
Instead of:
    BotController bot;  // 50+ member variables, complex Think() method

You have:
    Entity bot = CreateEntity();
    AddComponent<Transform>(bot, ...);
    AddComponent<Health>(bot, ...);
    AddComponent<Weapon>(bot, ...);
    // Clean, composable, testable
*/
