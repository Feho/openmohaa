# Task 4.3: Async Pathfinding

**Epic:** Performance Optimization (`epics/08-performance-optimization.md`)
**Estimate:** 2 weeks
**Priority:** MEDIUM

## Goal
Implement an asynchronous pathfinding system to eliminate frame hitches caused by pathfinding calculations.

## Business Value
- **Smoothness:** Ensures a smooth player experience by preventing pathfinding calculations from blocking the main thread.
- **Responsiveness:** Allows bots to remain responsive while a new path is being calculated.

## Current State
Pathfinding is performed synchronously, which can cause frame hitches, especially with a large number of bots or in complex environments.

## Target State
An `AsyncPathfinder` will be implemented that uses a thread pool to compute paths in the background. The bot will continue to follow its current path while waiting for the new path to be calculated.

## Acceptance Criteria
- [ ] An asynchronous pathfinding system is in place.
- [ ] Pathfinding calculations do not block the main thread.
- [ ] There are zero frame hitches from pathfinding.
- [ ] The system is tested with 100+ bots to verify performance.

## Technical Components
- **`AsyncPathfinder` class:** Manages path requests and results using a thread pool.
- **Thread Pool:** A pool of worker threads to compute paths in the background.
- **Integration with `BotMovement`:** The `BotMovement` system will be updated to request paths asynchronously and apply the new path when it is ready.

## Subtasks

### Week 1: Async Infrastructure
- [ ] **4.3.1** Design the asynchronous pathfinding system.
- [ ] **4.3.2** Implement the `AsyncPathfinder` class.
- [ ] **4.3.3** Create a thread pool for path computation.
- [ ] **4.3.4** Write unit tests and load tests for the async pathfinder.

### Week 2: Integration
- [ ] **4.3.5** Integrate the async pathfinder with the `BotMovement` system.
- [ ] **4.3.6** Handle edge cases such as path request timeouts and invalidation.
- [ ] **4.3.7** Conduct performance testing to measure the improvement in frame time.
- [ ] **4.3.8** Tune the number of threads in the thread pool for optimal performance.

## Deliverable
An asynchronous pathfinding system that causes zero frame hitches.
