# Bot AI Architecture - Product Backlog

## Vision

Transform OpenMoHAA's bot AI from a functional but messy system into a **world-class, modular, data-driven AI architecture** that rivals commercial games. This backlog documents the ideal architecture and provides a roadmap for incremental migration.

## Current State vs. Ideal State

| Aspect | Current | Ideal |
|--------|---------|-------|
| **Decision Making** | Priority-based state machine | Behavior Trees + Utility AI + GOAP |
| **Architecture** | Monolithic BotController | Clean Perception → Decision → Action pipeline |
| **Configuration** | Hardcoded cvars | Data-driven YAML/JSON profiles |
| **Code Structure** | 400+ line functions | Small, focused, testable functions |
| **Testing** | Manual in-game | Automated unit/integration/scenario tests |
| **Debugging** | Text printfs | Visual behavior tree viewer, heatmaps |
| **Performance** | All bots full logic | LOD system, spatial indexing, async |
| **Extensibility** | Modify core files | Plugin architecture |

## Why This Matters

**For Players:**
- More believable, unpredictable AI opponents
- Better performance (100+ bots without FPS drop)
- Richer tactical behaviors

**For Developers:**
- Faster feature development (hours vs. days)
- Fewer bugs (tests catch regressions)
- Joy to work with clean code

**For Designers:**
- Create bot behaviors without coding
- Iterate quickly with hot-reload
- A/B test different profiles

## Repository Structure

```
bot-ai-architecture/
├── README.md                      # You are here
├── ARCHITECTURE_VISION.md         # Detailed technical vision
├── MIGRATION_STRATEGY.md          # How to get from current to ideal
│
├── epics/                         # 10 major feature areas
│   ├── 01-behavior-tree-system.md
│   ├── 02-perception-system.md
│   └── ...
│
├── user-stories/                  # User-centric feature descriptions
│   ├── behavior-tree-stories.md
│   └── ...
│
├── tasks/                         # Concrete implementation tasks
│   ├── phase-1-foundation.md      # Quick wins (1-2 weeks)
│   ├── phase-2-core-systems.md    # Major rewrites (1-2 months)
│   ├── phase-3-advanced-features.md
│   └── phase-4-polish.md
│
├── examples/                      # Target code examples
│   ├── behavior-tree-example.yaml
│   ├── bot-profile-example.yaml
│   └── ...
│
└── references/                    # Theory and resources
    ├── behavior-trees.md
    └── ...
```

## Roadmap

### Phase 1: Foundation (1-2 weeks)
**Goal:** Quick wins that improve current code without major rewrites

- Extract small, testable functions
- Add unit tests for critical algorithms
- Create data structures for cleaner state management
- Basic debug visualization improvements

**Value:** Immediate code quality improvement, foundation for bigger changes

### Phase 2: Core Systems (1-2 months)
**Goal:** Implement new architecture alongside old (no breaking changes)

- Perception system (clean separation of sensing logic)
- Basic behavior tree framework
- Component-based entity system
- Data-driven bot profiles (YAML configs)

**Value:** New features can use clean architecture, old code still works

### Phase 3: Advanced Features (1-2 months)
**Goal:** AI capabilities that weren't possible before

- Utility AI for dynamic decision making
- GOAP tactical planner
- Advanced debugging tools (behavior tree viewer)
- Performance optimizations (LOD, spatial indexing)

**Value:** Truly intelligent, emergent bot behaviors

### Phase 4: Polish & Migration (ongoing)
**Goal:** Complete the transition, retire old code

- Migrate remaining behaviors to new system
- Plugin architecture for extensibility
- Full test coverage
- Documentation and examples

**Value:** Clean, maintainable codebase ready for future

## How to Use This Backlog

### For Project Planning
1. Review epics to understand major feature areas
2. Read user stories to see end-user value
3. Estimate tasks in each phase
4. Prioritize based on your goals

### For Implementation
1. Start with Phase 1 tasks (low risk, high value)
2. Implement features incrementally
3. Keep old system working while building new
4. Use examples as reference implementations

### For Learning
1. Read `ARCHITECTURE_VISION.md` for the big picture
2. Study examples to see target code style
3. Read references to understand theory
4. Experiment with small prototypes

## Key Principles

### 1. Incremental Migration
Never "big bang" rewrite. Build new system alongside old, migrate piece by piece.

### 2. Always Shippable
Every phase produces working, testable code. No half-finished features.

### 3. Data-Driven
Behavior in configs, not code. Non-programmers can tweak and experiment.

### 4. Clean Code
Small functions, clear names, single responsibility. Joy to read and modify.

### 5. Test Everything
Unit tests for algorithms, integration tests for behaviors, scenario tests for gameplay.

### 6. Visual Debugging
See what bots are thinking. Debug in seconds, not hours.

## Success Metrics

**Code Quality:**
- Average function length < 50 lines
- Test coverage > 80%
- Zero memory leaks
- Compile warnings = 0

**Performance:**
- 100+ bots at 60 FPS (vs. current ~30 bots)
- Bot decision time < 1ms average
- Pathfinding async (no frame hitches)

**Development Speed:**
- Add new behavior: 1 hour (vs. current ~8 hours)
- Fix typical bug: 15 minutes (vs. current ~2 hours)
- Create new bot profile: 5 minutes (no code change)

**AI Quality:**
- Players report "bots feel human"
- Emergent tactics surprise even developers
- Diverse playstyles across difficulty levels

## Getting Started

**New to the project?**
1. Read `ARCHITECTURE_VISION.md` first
2. Look at `examples/` to see target code
3. Try one Phase 1 task to get familiar

**Ready to implement?**
1. Pick an epic that interests you
2. Read associated user stories
3. Start with Phase 1 tasks for that epic
4. Use examples as templates

**Want to understand theory?**
1. Read `references/behavior-trees.md`
2. Study existing commercial AI systems
3. Play games with great AI, analyze what they do

## Contributing

This is a living document. As you learn more:
- Add new user stories
- Refine task estimates
- Update examples with better patterns
- Share lessons learned

## Questions?

- **"This seems like a lot of work"** - Yes, but it's designed for incremental progress. Even Phase 1 alone is valuable.
- **"Can we just fix current code?"** - You can, but you'll hit complexity limits. This backlog plans for long-term scalability.
- **"What if requirements change?"** - Data-driven + modular = easy to adapt. Change configs, not code.
- **"Is this overkill for a mod?"** - Not if you want professional-quality AI. Start small, grow as needed.

## License

Same as OpenMoHAA - GPL v2. Use freely, share improvements.

---

**Next Steps:** Read `ARCHITECTURE_VISION.md` for detailed technical design, then explore epics that interest you most.
