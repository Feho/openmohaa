# Epic 7: Debug Visualization & Tools

## Overview
Create rich visual debugging tools that show what bots are "thinking" in real-time, dramatically reducing debugging time.

## Business Value
- **Debug Speed:** See issues in seconds instead of hours
- **Understanding:** Visually see decision-making process
- **Tuning:** Adjust parameters while watching effects live
- **Communication:** Show non-programmers how AI works

## Current State
```cpp
// Text-based debugging
Com_Printf("Bot attack state: %d\n", attackState);
gi.DPrintf("Enemy distance: %f\n", dist);

// Basic path visualization
if (ai_debugpath->integer) {
    DrawLine(start, end, COLOR_RED);
}
```

**Problems:**
- Hard to understand complex AI state
- Printf spam clutters console
- No way to see "why" bot made decision
- Limited visualization

## Target State

### In-Game Overlays
```cpp
void DebugDrawBot(Bot* bot) {
    // Behavior Tree - shows active nodes
    DrawBehaviorTreeState(bot);

    // Utility Scores - bar chart of action scores
    DrawUtilityScores(bot);

    // Perception - FOV cone, visible enemies, sounds
    DrawPerceptionData(bot);

    // Tactical - cover points, threat map, paths
    DrawTacticalOverlay(bot);
}
```

### External Tools
- **Behavior Tree Editor:** Drag-drop visual editor
- **Profile Editor:** GUI for editing bot profiles
- **Playback Recorder:** Record/replay bot sessions
- **Heatmap Generator:** Where bots go, die, fight

## Acceptance Criteria
- [ ] In-game behavior tree visualization
- [ ] Utility score overlay (bar chart)
- [ ] Perception visualization (FOV, sounds, memory)
- [ ] Tactical overlay (cover, paths, threats)
- [ ] Console commands to toggle visualizations
- [ ] Recording/playback system for bug reproduction
- [ ] Performance heatmaps

## Technical Components

### Visualization Types

#### 1. Behavior Tree Viewer
```
Selector (RUNNING)
├─ Sequence (FAILURE) ❌
│  └─ Condition: Health < 25% ❌
├─ Sequence (RUNNING) ⏵
│  ├─ Condition: HasEnemy ✓
│  └─ Parallel (RUNNING) ⏵
│     ├─ Action: Aim ⏵
│     └─ Action: Fire ⏵
└─ Action: Patrol
```

#### 2. Utility Scores
```
Aggress     |████████░░| 0.8
Defend      |██████░░░░| 0.6
Retreat     |███░░░░░░░| 0.3
Investigate |████░░░░░░| 0.4
Support     |██████████| 1.0 ⭐
```

#### 3. Perception Overlay
- Yellow cone: FOV
- Green lines: Visible enemies
- Orange ghosts: Remembered enemy positions
- Blue circles: Audio events
- Red zones: Danger areas

#### 4. Tactical Overlay
- Green squares: High-quality cover
- Yellow squares: Medium-quality cover
- Red circles: Danger zones
- Blue lines: Path to goal
- Purple lines: Squad coordination

### Console Commands
```
bot_debug <botnum> <mode>
  - none: Disable
  - perception: Show FOV, enemies, sounds
  - behavior: Show behavior tree
  - utility: Show action scores
  - tactical: Show cover, paths
  - all: Show everything

bot_record_start <botnum>
bot_record_stop
bot_record_playback <filename>
```

## Dependencies
- Debug rendering system
- UI framework (optional, for external tools)

## Related Epics
- Epic 1 (visualize behavior trees)
- Epic 2 (visualize perception)
- Epic 3 (visualize utility scores)

## References
- Unreal Engine's AI debugging tools
- [Game AI Pro: Debug Visualization](http://www.gameaipro.com/)
