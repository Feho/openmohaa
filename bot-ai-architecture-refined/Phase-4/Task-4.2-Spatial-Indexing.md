# Task 4.2: Spatial Indexing

**Epic:** Performance Optimization (`epics/08-performance-optimization.md`)
**Estimate:** 1 week
**Priority:** MEDIUM

## Goal
Implement a spatial indexing system to accelerate queries for nearby bots, replacing linear searches with O(1) or O(log n) operations.

## Business Value
- **Performance:** Significantly speeds up neighbor queries, which are common in perception and targeting systems.
- **Scalability:** Allows the AI system to scale to a large number of bots without a proportional increase in query time.

## Current State
Queries for nearby bots are performed using linear searches, which are inefficient and do not scale well with a large number of bots.

## Target State
A `BotSpatialIndex` will be implemented using a grid-based spatial hash. This will allow for near-constant time queries for bots within a given radius or for the nearest bot.

## Acceptance Criteria
- [ ] A spatial indexing system is in place for fast neighbor queries.
- [ ] All linear searches for bots are replaced with queries to the spatial index.
- [ ] Neighbor queries are O(1) on average.
- [ ] The system is benchmarked and shows a significant performance improvement with 200+ bots.

## Technical Components
- **`BotSpatialIndex` class:** A grid-based spatial hash that supports insertion, removal, range queries, and nearest neighbor queries.
- **Integration with `BotManager`:** Bots will be registered with the spatial index on spawn, updated on move, and unregistered on death.

## Subtasks
- [ ] **4.2.1** Implement the `BotSpatialIndex` class.
- [ ] **4.2.2** Integrate the spatial index with the `BotManager`.
- [ ] **4.2.3** Replace all linear searches for bots with queries to the spatial index.
- [ ] **4.2.4** Conduct performance testing to benchmark the improvement and verify correctness.

## Deliverable
A spatial indexing system that provides O(1) neighbor queries, replacing all O(n) searches.
