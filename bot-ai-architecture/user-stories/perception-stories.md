# User Stories: Perception System

## Epic: Perception System
Related to: `epics/02-perception-system.md`

---

## Story 1: Clean Perception Interface
**As a** developer
**I want** a clear separation between sensing and decision logic
**So that** I can test AI decisions independently from perception

### Acceptance Criteria
- [ ] PerceptionSystem provides PerceptionSnapshot
- [ ] Decision layer only accesses snapshot (not raw game state)
- [ ] Mock snapshots for testing
- [ ] Clear documentation of snapshot contents

### Priority: HIGH
### Estimate: 2 days

---

## Story 2: Realistic Vision (FOV + Occlusion)
**As a** player
**I want** bots to have realistic vision with field-of-view limits
**So that** I can sneak behind bots and flank them

### Acceptance Criteria
- [ ] Bots have configurable FOV (default 80 degrees)
- [ ] Cannot see through walls (occlusion testing)
- [ ] Distance attenuation (harder to see far enemies)
- [ ] Peripheral vision less effective than center vision

### Priority: HIGH
### Estimate: 4 days

---

## Story 3: Enemy Memory Persistence
**As a** player
**I want** bots to remember where they last saw me
**So that** combat feels intelligent (they search, predict movement)

### Acceptance Criteria
- [ ] Bots remember last known enemy position
- [ ] Confidence decays over time (0.0 after 10 seconds)
- [ ] Predicted position based on velocity
- [ ] Bots investigate last known position when enemy disappears

### Priority: HIGH
### Estimate: 3 days

---

## Story 4: 3D Positional Audio
**As a** player
**I want** bots to hear sounds from specific directions
**So that** I must be careful about noise (footsteps, gunfire)

### Acceptance Criteria
- [ ] Audio events have 3D position
- [ ] Bots estimate direction (with some inaccuracy)
- [ ] Distance affects loudness
- [ ] High-priority sounds (gunfire) override low-priority (footsteps)

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 5: Configurable Perception Parameters
**As a** game designer
**I want** different bot profiles to have different perception abilities
**So that** I can create varied difficulties (eagle-eye sniper, deaf rusher)

### Acceptance Criteria
- [ ] Vision range configurable per profile
- [ ] FOV configurable per profile
- [ ] Hearing range configurable per profile
- [ ] Example profiles: Sniper (far vision), Rusher (close vision, bad hearing)

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 6: Perception Debug Visualization
**As a** developer
**I want** to see what bots perceive in real-time
**So that** I can debug perception issues quickly

### Acceptance Criteria
- [ ] Draw FOV cone
- [ ] Draw visible enemies (green lines)
- [ ] Draw remembered enemies (orange ghosts with confidence indicator)
- [ ] Draw audio events (blue circles, size = loudness)
- [ ] Console command to toggle visualization

### Priority: HIGH
### Estimate: 3 days

---

## Story 7: Threat Level Assessment
**As a** AI system
**I want** to assess overall threat level based on perception
**So that** bots can choose appropriate combat profile

### Acceptance Criteria
- [ ] ThreatLevel enum (NONE, LOW, MEDIUM, HIGH, EXTREME)
- [ ] Considers: enemy count, enemy proximity, recent damage
- [ ] Exposed in PerceptionSnapshot
- [ ] Used by decision layer to pick strategy

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 8: Ally Awareness
**As a** bot
**I want** to know where my allies are
**So that** I can coordinate tactics and avoid friendly fire

### Acceptance Criteria
- [ ] PerceptionSnapshot includes nearby allies
- [ ] Ally positions, distances, states (in combat, retreating)
- [ ] Squad leader identification
- [ ] Friendly fire avoidance in combat

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 9: Tactical Point Awareness
**As a** bot
**I want** to know about nearby cover, objectives, and items
**So that** I can make tactical decisions

### Acceptance Criteria
- [ ] Perception identifies cover points within radius
- [ ] Identifies objectives (flags, bomb sites)
- [ ] Identifies items (health, ammo)
- [ ] Cached for performance (not computed every frame)

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 10: Performance Optimization
**As a** developer
**I want** perception updates to be fast
**So that** many bots can use perception without FPS drop

### Acceptance Criteria
- [ ] Perception update < 0.3ms per bot
- [ ] Spatial indexing for enemy queries
- [ ] Cached results (don't recompute every frame)
- [ ] LOD: distant bots update less frequently

### Priority: HIGH
### Estimate: 3 days

---

## Total Stories: 10
## Total Estimated Time: 27 days
