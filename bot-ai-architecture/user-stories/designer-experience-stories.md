# User Stories: Designer Experience

## Cross-Epic: Overall Designer Experience
Related to: Multiple epics (Behavior Trees, Configuration, Debugging)

---

## Story 1: No-Code Behavior Creation
**As a** game designer (non-programmer)
**I want** to create new bot behaviors without writing C++
**So that** I can experiment with AI independently

### Acceptance Criteria
- [ ] Define complete behaviors in YAML
- [ ] Use pre-built action library
- [ ] Create custom conditions via simple expressions
- [ ] Example: Create "Sniper" behavior in 1 hour

### Priority: HIGH
### Estimate: Covered by other epics

---

## Story 2: Rapid Iteration Workflow
**As a** game designer
**I want** changes to apply without restarting the game
**So that** I can iterate quickly

### Acceptance Criteria
- [ ] Hot-reload profiles
- [ ] Hot-reload behavior trees
- [ ] See changes immediately (< 1 second)
- [ ] No loss of game state

### Priority: HIGH
### Estimate: Covered by other epics

---

## Story 3: Visual Feedback
**As a** game designer
**I want** to see what bots are thinking in real-time
**So that** I understand why they behave certain ways

### Acceptance Criteria
- [ ] Behavior tree visualization
- [ ] Utility score overlay
- [ ] Perception visualization (what they see/hear)
- [ ] Toggle visualization per bot

### Priority: HIGH
### Estimate: Covered by Epic 7

---

## Story 4: A/B Testing Support
**As a** game designer
**I want** to easily compare two bot configurations
**So that** I can find the best balance

### Acceptance Criteria
- [ ] Spawn bots with different profiles in same match
- [ ] Track performance metrics (K/D, objective completion)
- [ ] Side-by-side comparison tools
- [ ] Export results for analysis

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 5: Profile Templates
**As a** game designer
**I want** starting templates for common bot types
**So that** I don't start from scratch

### Acceptance Criteria
- [ ] 10+ profile templates (Aggressive, Sniper, Rusher, etc.)
- [ ] Each template well-documented
- [ ] Easy to copy and modify
- [ ] Covers various playstyles

### Priority: MEDIUM
### Estimate: Covered by Epic 4

---

## Story 6: Clear Error Messages
**As a** game designer
**I want** helpful error messages when configurations are invalid
**So that** I can fix issues quickly

### Acceptance Criteria
- [ ] Errors show file, line number, parameter
- [ ] Suggest fixes ("aggression must be 0.0-1.0, got 1.5")
- [ ] Don't crash game (fallback to default)
- [ ] Show errors in console and log

### Priority: HIGH
### Estimate: Covered by Epic 4

---

## Story 7: Parameter Tooltips
**As a** game designer
**I want** inline documentation for parameters
**So that** I understand what each setting does

### Acceptance Criteria
- [ ] Comments in YAML templates explain each parameter
- [ ] Reference doc accessible in-game (console command)
- [ ] Examples showing parameter effects
- [ ] Value ranges clearly specified

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 8: Bot Performance Metrics
**As a** game designer
**I want** to see bot performance metrics
**So that** I know if my changes improved AI

### Acceptance Criteria
- [ ] Track: K/D ratio, accuracy, objective completions
- [ ] Track: time in states (combat, retreat, patrol)
- [ ] Compare before/after metrics
- [ ] Export to CSV for analysis

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 9: Learning Resources
**As a** new game designer
**I want** tutorials and examples
**So that** I can learn the system quickly

### Acceptance Criteria
- [ ] Getting started tutorial
- [ ] Video walkthrough (optional)
- [ ] Annotated example profiles
- [ ] Annotated example behavior trees
- [ ] FAQ for common issues

### Priority: MEDIUM
### Estimate: 3 days

---

## Story 10: Community Sharing
**As a** game designer
**I want** to share my bot configurations with others
**So that** the community can benefit

### Acceptance Criteria
- [ ] Profiles/trees are plain text (git-friendly)
- [ ] Easy to package and distribute
- [ ] Import/export commands
- [ ] Attribution/licensing support

### Priority: LOW
### Estimate: 1 day

---

## Total Stories: 10
## Total Estimated Time: 12 days (many covered by other epics)
