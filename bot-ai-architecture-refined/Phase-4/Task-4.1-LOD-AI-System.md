# Task 4.1: LOD AI System

**Epic:** Performance Optimization (`epics/08-performance-optimization.md`)
**Estimate:** 2 weeks
**Priority:** HIGH

## Goal
Implement a Level-of-Detail (LOD) system for bot AI to support 100+ bots at 60 FPS by reducing the update frequency of bots that are not relevant to the player.

## Business Value
- **Scalability:** Support massive battles with a large number of bots.
- **Smoothness:** Ensure a smooth player experience by preventing frame rate drops caused by AI calculations.
- **Smart Resource Allocation:** Focus CPU resources on bots that are actively engaged with the player.

## Current State
All bots run their full AI logic every frame, regardless of their distance from the player or their current situation. This limits the number of active bots to around 30-40 before performance degradation occurs.

## Target State
An `AIScheduler` will manage bot updates, assigning an LOD level to each bot based on its relevance to the player.

### LOD Levels
- **HIGH:** In combat, near the player. (60 FPS update)
- **MEDIUM:** Visible to the player, near combat. (20 FPS update)
- **LOW:** Far from the player, not in combat. (10 FPS update)
- **SLEEPING:** Very far from the player, just exists. (1 Hz update)

## Acceptance Criteria
- [ ] An LOD system is in place that updates bots at different rates based on their relevance.
- [ ] The system can support 100+ bots at 60 FPS.
- [ ] The average `Think()` time for a bot is less than 1ms.
- [ ] The LOD level for each bot can be visualized for debugging purposes.

## Technical Components
- **`AIScheduler` class:** Manages all bots and determines their LOD level each frame.
- **LOD Determination Logic:** A function to determine the LOD level based on factors like combat state, distance to players, visibility, and proximity to objectives.
- **Staggered Updates:** The scheduler will update bots at different frequencies based on their assigned LOD level.
- **`BotController` Integration:** The `BotController` will be updated to adjust its logic based on the assigned LOD level, skipping expensive operations at lower levels.

## Subtasks

### Week 1: LOD Framework
- [ ] **4.1.1** Design the LOD system, defining the levels and the criteria for each.
- [ ] **4.1.2** Implement the `AIScheduler` class to manage bot updates.
- [ ] **4.1.3** Implement the LOD determination logic.
- [ ] **4.1.4** Implement the staggered update mechanism in the scheduler.

### Week 2: Integration & Tuning
- [ ] **4.1.5** Integrate the LOD system with the `BotController`.
- [ ] **4.1.6** Tune the LOD thresholds and update rates to balance performance and AI quality.
- [ ] **4.1.7** Test the system with 100+ bots to measure FPS and verify AI behavior at low LODs.
- [ ] **4.1.8** Add debug visualizations to show the current LOD level of each bot.

## Deliverable
A functional LOD system that allows for 100+ bots to run at 60 FPS.
