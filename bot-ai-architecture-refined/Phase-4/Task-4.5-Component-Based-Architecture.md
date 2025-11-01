# Task 4.5: Component-Based Architecture (ECS)

**Epic:** Component-Based Architecture (`epics/05-component-based-architecture.md`)
**Estimate:** 4 weeks (OPTIONAL - big refactor)
**Priority:** LOW

## Goal
Refactor the monolithic `BotController` into a component-based architecture using the Entity-Component-System (ECS) pattern. This is an optional, large-scale refactoring that should only be undertaken if the performance benefits are deemed necessary.

## Business Value
- **Performance:** A data-oriented design is more cache-friendly and can lead to significant performance improvements.
- **Modularity:** Capabilities can be added or removed by simply adding or removing components.
- **Clarity:** Systems are single-purpose and stateless, making the code easier to understand.

## Current State
The `BotController` is a monolithic class with over 50 member variables, making it difficult to understand, maintain, and optimize.

## Target State
The `BotController` will be refactored into a set of components (data) and systems (logic). An ECS library such as EnTT will be used to manage entities, components, and systems.

## Acceptance Criteria
- [ ] An ECS library is integrated into the project.
- [ ] The `BotController` is refactored into 10+ components and 5+ systems.
- [ ] The refactoring results in a measurable performance improvement (e.g., 20%+ faster with many bots).
- [ ] It is easier to add new components without modifying existing code.

## Technical Components
- **ECS Library:** A library such as EnTT to manage entities, components, and systems.
- **Components:** Data-only structs representing different aspects of a bot (e.g., `TransformComponent`, `HealthComponent`, `WeaponComponent`).
- **Systems:** Logic-only classes that operate on components (e.g., `PerceptionSystem`, `CombatSystem`, `MovementSystem`).

## Subtasks

### Week 1: ECS Library Integration
- [ ] **4.5.1** Evaluate and select an ECS library.
- [ ] **4.5.2** Integrate the chosen library into the build system.
- [ ] **4.5.3** Design the component breakdown for the `BotController`.

### Week 2-3: Refactor to Components
- [ ] **4.5.4** Create the component structs.
- [ ] **4.5.5** Create the systems.

### Week 4: Migration & Testing
- [ ] **4.5.6** Migrate the `BotController` to the ECS architecture.
- [ ] **4.5.7** Test extensively to ensure all behaviors still work and performance is improved.

## Deliverable
A `BotController` that has been refactored into a component-based architecture.
