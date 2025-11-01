# Bot AI Improvement - Task Overview

**Status:** All 18 task files created and ready for execution ✅  
**Total Duration:** ~20-25 weeks (5-6 months)  
**Location:** `bot-ai-architecture-refined/`

---

## Quick Start

1. **Start Here:** `Phase-2B/Task-2B.1-Behavior-Tree-Framework.md`
2. **Execute Sequentially:** Complete each task before moving to next
3. **Each Task is Self-Contained:** All context embedded, no need to read other docs
4. **Test Continuously:** Run tests after each task

---

## Phase 2B: Brain & Behavior (4-5 weeks)

Foundation for new AI system - behavior trees, profiles, integration.

| Task | File | Duration | Description |
|------|------|----------|-------------|
| 2B.1 | `Task-2B.1-Behavior-Tree-Framework.md` | 1-2 weeks | Core BT system: BTNode, Selector, Sequence, Parallel, Condition, Action, Blackboard |
| 2B.2 | `Task-2B.2-YAML-Tree-Loading.md` | 8 hours | YAML loader with action registry for data-driven trees |
| 2B.3 | `Task-2B.3-Complete-Profile-System.md` | 8 hours | Extended profiles with combat/movement/aim/tactics (5 profiles) |
| 2B.4 | `Task-2B.4-Integration-And-Polish.md` | 1 week | BotController integration, console commands, BT visualizer |

**Deliverables:**
- Working behavior tree system with 43 tests passing
- 5 bot profiles (aggressive, balanced, defensive, sniper, rusher)
- Console commands (bot_setprofile, bot_listprofiles, bot_blackboard, bot_reload_profiles)
- Debug visualization showing active BT nodes
- Feature flag `g_bot_use_new_ai_system` for safe deployment

---

## Phase 3: Migration & Enhancement (9-10 weeks)

Migrate all behaviors from old state machine to behavior trees, add Utility AI.

| Task | File | Duration | Description |
|------|------|----------|-------------|
| 3.1 | `Task-3.1-Migrate-Attack-Behavior.md` | 2 weeks | Combat behaviors: engage, retreat, distance management, reload |
| 3.2 | `Task-3.2-Investigation.md` | 1 week | Search patterns for investigating sounds/enemy positions |
| 3.3 | `Task-3.3-Idle-Behaviors.md` | 1 week | Patrol, wander, ambient investigation when no threat |
| 3.4 | `Task-3.4-Utility-AI.md` | 2 weeks | Action scoring with consideration curves for dynamic decisions |
| 3.5 | `Task-3.5-Debug-Visualization.md` | 1.5 weeks | BT recording/playback, tactical overlay |
| 3.6 | `Task-3.6-Remove-Old-State-Machine.md` | 1 week | Delete legacy code after verification |
| 3.7 | `Task-3.7-Performance-Tuning.md` | 1 week | Profile and optimize to <1ms per bot |

**Deliverables:**
- Complete behavior migration to BT system
- Utility AI for dynamic action selection
- Advanced debug tools (recording/playback)
- Old state machine removed
- Performance target: <1ms per bot per frame

---

## Phase 4: Optimization & Polish (10-11 weeks)

Production-ready optimizations, testing, documentation, content library.

| Task | File | Duration | Description |
|------|------|----------|-------------|
| 4.1 | `Task-4.1-LOD-AI-System.md` | 2 weeks | Level-of-detail AI (HIGH/MEDIUM/LOW/SLEEPING update rates) |
| 4.2 | `Task-4.2-Spatial-Indexing.md` | 1 week | Grid-based spatial hash for O(1) queries |
| 4.3 | `Task-4.3-Async-Pathfinding.md` | 2 weeks | Thread pool pathfinding to eliminate hitches |
| 4.4 | `Task-4.4-Plugin-System.md` | 3 weeks | IBehaviorPlugin interface for hot-reloadable behaviors |
| 4.5 | `Task-4.5-Full-Test-Coverage.md` | 2 weeks | Achieve 80%+ code coverage |
| 4.6 | `Task-4.6-Documentation.md` | 1.5 weeks | Doxygen API docs, designer guide, developer guide |
| 4.7 | `Task-4.7-Profile-Library.md` | 1 week | Create 10+ profiles and behavior tree library |

**Deliverables:**
- 100+ bots at 60 FPS
- Plugin system for custom behaviors
- 80%+ test coverage
- Complete documentation (API, designer guide, developer guide)
- Rich content library (10+ profiles, behavior trees)

---

## Key Technologies

- **C++**: OpenMoHAA game code
- **YAML (yaml-cpp 0.7.0+)**: Configuration files
- **GoogleTest**: Unit testing
- **Behavior Trees**: Hierarchical decision-making
- **Blackboard Pattern**: Shared state storage
- **Utility AI**: Action scoring with consideration curves

---

## Important Notes

### Feature Flag
`g_bot_use_new_ai_system` (cvar):
- `0` = Use old state machine (legacy)
- `1` = Use new BT system (Phase 2B+)

This allows safe incremental deployment.

### Testing Philosophy
- Write tests BEFORE implementation (TDD)
- Each task specifies exact tests to write
- Run all tests after each task
- No regressions allowed

### Vertical Slice Approach
- Build each feature to 100% completion
- Integrate immediately
- Test continuously
- Always have shippable code

### Performance Targets
- **Phase 2B:** <10% overhead vs old system
- **Phase 3:** <1ms per bot per frame
- **Phase 4:** 100+ bots at 60 FPS

---

## Execution Guide

### For Each Task:

1. **Read Task File:** Start to finish, understand context
2. **Review Code Examples:** Study implementations provided
3. **Create Files:** Follow "Files to Create/Modify" section
4. **Implement Code:** Use examples as templates
5. **Write Tests:** Follow "Testing" section
6. **Run Tests:** Verify all pass
7. **Check Acceptance Criteria:** Ensure all items complete
8. **Manual Testing:** Verify in-game behavior
9. **Move to Next Task**

### Console Commands (Phase 2B+)

```
g_bot_use_new_ai_system 1          # Enable new AI
bot_setprofile 0 aggressive        # Set bot 0 to aggressive profile
bot_listprofiles                   # List all profiles
bot_blackboard 0                   # Show bot 0's blackboard state
bot_reload_profiles                # Hot-reload all profiles
g_bot_debug 0                      # Enable debug viz for bot 0
```

---

## Reference Materials

- **Original Documentation:** `../bot-ai-architecture/`
- **Epics:** `../bot-ai-architecture/epics/` (10 files)
- **User Stories:** `../bot-ai-architecture/user-stories/`
- **Technical References:** `../bot-ai-architecture/references/`

**Note:** These refined tasks contain all necessary context extracted from original documentation. You don't need to read the original files to execute tasks.

---

## Questions?

Each task file is designed to be self-contained. If you need clarification:
1. Check the task's "Context & Background" section
2. Review code examples in the task
3. Refer to previous task files for patterns
4. Check the original documentation in `bot-ai-architecture/` folder

---

**Ready to start?** Open `Phase-2B/Task-2B.1-Behavior-Tree-Framework.md` and begin! 🚀
