# Task 3.5: Debug Visualization

**Status:** Ready to Execute  
**Duration:** 1.5 weeks  
**Priority:** MEDIUM  
**Phase:** 3 - Migration & Enhancement

---

## Context & Background

### What This Task Achieves
Implements comprehensive visual debugging tools that show what bots are "thinking" in real-time, dramatically reducing debugging time and enabling quick tuning of AI behaviors.

### Why This Matters
- **Debug Speed:** See issues in seconds instead of hours
- **Understanding:** Visually see decision-making process
- **Tuning:** Adjust parameters while watching effects live
- **Communication:** Show non-programmers how AI works
- **Development Velocity:** Faster iteration = better AI

### What's Already Complete

**Phase 2B (Behavior Trees):**
- ✅ Basic BT visualizer showing active nodes

**Phase 3.1-3.4 (Behaviors):**
- ✅ Complete combat, investigation, idle behaviors
- ✅ Utility AI with action scoring
- ✅ Perception system with rich data

**What Bots Can Do Now:**
- All behaviors functional
- Basic debug visualization exists

**What Bots Need:**
- Enhanced BT visualizer with timing and history
- Utility score visualization (bar chart)
- Perception overlay (FOV, enemies, sounds, memory)
- Tactical overlay (cover points, danger zones, paths)
- Console commands to toggle visualizations

---

## Technical Specification

### Visualization Components

#### 1. Enhanced Behavior Tree Visualizer

**Current State:** Shows active nodes only
**Enhanced State:** Shows execution history, timing, blackboard values

```cpp
// code/fgame/bot_debug_viz.cpp

class BTVisualizer {
public:
    struct NodeVisualization {
        BTNode* node;
        BTNode::Status lastStatus;
        float lastExecutionTime;  // ms
        int executionCount;
        float totalTime;  // Cumulative ms
    };
    
    void DrawBehaviorTree(Player* bot, BehaviorTree* tree, Vector screenPos);
    
private:
    void DrawNode(BTNode* node, Vector pos, int depth);
    Color GetStatusColor(BTNode::Status status);
    std::map<BTNode*, NodeVisualization> nodeHistory;
};

void BTVisualizer::DrawBehaviorTree(Player* bot, BehaviorTree* tree, Vector screenPos) {
    if (!tree) return;
    
    // Draw tree title
    DrawText(screenPos, "Behavior Tree: " + tree->GetName(), COLOR_WHITE);
    screenPos.y += 20;
    
    // Draw root node and children recursively
    DrawNode(tree->GetRoot(), screenPos, 0);
    
    // Draw blackboard values
    screenPos.y += tree->GetDepth() * 30 + 20;
    DrawText(screenPos, "Blackboard:", COLOR_YELLOW);
    screenPos.y += 15;
    
    auto blackboard = tree->GetBlackboard();
    auto keys = blackboard->GetAllKeys();
    for (const auto& key : keys) {
        std::string value = blackboard->GetAsString(key);
        DrawText(screenPos, "  " + key + ": " + value, COLOR_WHITE);
        screenPos.y += 12;
    }
}

void BTVisualizer::DrawNode(BTNode* node, Vector pos, int depth) {
    const float indentSize = 20.0f;
    const float nodeHeight = 25.0f;
    
    pos.x += depth * indentSize;
    
    // Get node visualization data
    auto& viz = nodeHistory[node];
    viz.node = node;
    
    // Determine color based on last status
    Color color = GetStatusColor(viz.lastStatus);
    
    // Draw node box
    Vector boxMin = pos;
    Vector boxMax = pos + Vector(200, 20, 0);
    DrawBox(boxMin, boxMax, color);
    
    // Draw node name and type
    std::string nodeText = node->GetName();
    if (node->GetType() == "action") {
        nodeText += " [A]";
    } else if (node->GetType() == "condition") {
        nodeText += " [C]";
    }
    DrawText(pos + Vector(5, 5, 0), nodeText, COLOR_BLACK);
    
    // Draw execution stats (right side)
    std::string stats = StringFormat("%.2fms (%d)", 
        viz.lastExecutionTime, viz.executionCount);
    DrawText(pos + Vector(210, 5, 0), stats, COLOR_GRAY);
    
    // Draw children
    if (node->IsComposite()) {
        CompositeNode* composite = (CompositeNode*)node;
        Vector childPos = pos + Vector(0, nodeHeight, 0);
        
        for (int i = 0; i < composite->GetChildCount(); i++) {
            DrawNode(composite->GetChild(i), childPos, depth + 1);
            childPos.y += nodeHeight;
        }
    }
}

Color BTVisualizer::GetStatusColor(BTNode::Status status) {
    switch (status) {
        case BTNode::Status::SUCCESS:  return COLOR_GREEN;
        case BTNode::Status::FAILURE:  return COLOR_RED;
        case BTNode::Status::RUNNING:  return COLOR_YELLOW;
        default:                       return COLOR_GRAY;
    }
}
```

#### 2. Utility Score Visualization

```cpp
// code/fgame/bot_debug_viz.cpp

class UtilityVisualizer {
public:
    void DrawUtilityScores(Player* bot, const std::vector<UtilityEvaluator::ScoredAction>& scores, Vector screenPos);
    void DrawConsiderationBreakdown(const UtilityEvaluator::ScoredAction& action, Vector screenPos);
};

void UtilityVisualizer::DrawUtilityScores(
    Player* bot, 
    const std::vector<UtilityEvaluator::ScoredAction>& scores, 
    Vector screenPos
) {
    // Title
    DrawText(screenPos, "Utility Scores:", COLOR_YELLOW);
    screenPos.y += 20;
    
    // Find highest score for highlighting
    float maxScore = 0.0f;
    for (const auto& score : scores) {
        maxScore = std::max(maxScore, score.score);
    }
    
    // Draw bar chart for each action
    for (const auto& score : scores) {
        // Action name
        DrawText(screenPos, score.name, COLOR_WHITE);
        
        // Score bar
        Vector barStart = screenPos + Vector(100, 0, 0);
        float barWidth = 200.0f * score.score;
        Vector barEnd = barStart + Vector(barWidth, 15, 0);
        
        Color barColor = (score.score == maxScore) ? COLOR_GREEN : COLOR_CYAN;
        DrawFilledBox(barStart, barEnd, barColor);
        
        // Score value
        std::string scoreText = StringFormat("%.2f", score.score);
        DrawText(barStart + Vector(barWidth + 5, 0, 0), scoreText, COLOR_WHITE);
        
        screenPos.y += 20;
    }
}

void UtilityVisualizer::DrawConsiderationBreakdown(
    const UtilityEvaluator::ScoredAction& action, 
    Vector screenPos
) {
    DrawText(screenPos, "Considerations for: " + action.name, COLOR_YELLOW);
    screenPos.y += 20;
    
    for (const auto& consideration : action.considerations) {
        // Consideration name
        DrawText(screenPos, "  " + consideration.name, COLOR_WHITE);
        
        // Value bar (raw value)
        Vector barStart = screenPos + Vector(150, 0, 0);
        float barWidth = 100.0f * consideration.rawValue;
        Vector barEnd = barStart + Vector(barWidth, 10, 0);
        DrawFilledBox(barStart, barEnd, COLOR_BLUE);
        
        // Curved value (after curve application)
        barStart.y += 12;
        barWidth = 100.0f * consideration.curvedValue;
        barEnd = barStart + Vector(barWidth, 10, 0);
        DrawFilledBox(barStart, barEnd, COLOR_GREEN);
        
        // Values
        std::string valueText = StringFormat("raw:%.2f → %.2f (w:%.2f)", 
            consideration.rawValue, 
            consideration.curvedValue,
            consideration.weight);
        DrawText(barStart + Vector(110, -10, 0), valueText, COLOR_GRAY);
        
        screenPos.y += 25;
    }
}
```

#### 3. Perception Visualization

```cpp
// code/fgame/bot_debug_viz.cpp

class PerceptionVisualizer {
public:
    void DrawPerceptionOverlay(Player* bot, const PerceptionSnapshot& perception);
    
private:
    void DrawVisionCone(Player* bot, const BotProfile* profile);
    void DrawVisibleEnemies(const PerceptionSnapshot& perception);
    void DrawEnemyMemories(const PerceptionSnapshot& perception);
    void DrawAudioEvents(const PerceptionSnapshot& perception);
    void DrawNearbyAllies(const PerceptionSnapshot& perception);
    void DrawThreatLevel(Player* bot, const PerceptionSnapshot& perception);
};

void PerceptionVisualizer::DrawPerceptionOverlay(Player* bot, const PerceptionSnapshot& perception) {
    BotProfile* profile = bot->GetProfile();
    
    DrawVisionCone(bot, profile);
    DrawVisibleEnemies(perception);
    DrawEnemyMemories(perception);
    DrawAudioEvents(perception);
    DrawNearbyAllies(perception);
    DrawThreatLevel(bot, perception);
}

void PerceptionVisualizer::DrawVisionCone(Player* bot, const BotProfile* profile) {
    float visionRange = profile->GetVisionRange();
    float fov = profile->GetFieldOfView();
    float peripheralFOV = profile->GetPeripheralFOV();
    
    Vector forward = bot->GetForwardVector();
    Vector origin = bot->GetEyePosition();
    
    // Draw main FOV cone (yellow)
    DrawCone(origin, forward, visionRange, fov, COLOR_YELLOW, 0.3f);
    
    // Draw peripheral FOV (gray)
    DrawCone(origin, forward, visionRange, peripheralFOV, COLOR_GRAY, 0.1f);
}

void PerceptionVisualizer::DrawVisibleEnemies(const PerceptionSnapshot& perception) {
    for (const auto& enemy : perception.visibleEnemies) {
        // Draw line to enemy (green = visible)
        DrawLine(bot->origin, enemy.position, COLOR_GREEN);
        
        // Draw sphere at enemy position
        DrawSphere(enemy.position, 16.0f, COLOR_GREEN);
        
        // Draw visibility factor as text
        std::string visText = StringFormat("Vis: %.2f", enemy.visibilityFactor);
        DrawText3D(enemy.position + Vector(0, 0, 64), visText, COLOR_WHITE);
        
        // Draw if in peripheral vision
        if (enemy.isInPeripheral) {
            DrawText3D(enemy.position + Vector(0, 0, 48), "[Peripheral]", COLOR_YELLOW);
        }
    }
}

void PerceptionVisualizer::DrawEnemyMemories(const PerceptionSnapshot& perception) {
    for (const auto& memory : perception.knownEnemies) {
        // Draw ghost at last known position (orange, fading with confidence)
        float alpha = memory.confidenceLevel;
        Color memoryColor = COLOR_ORANGE;
        memoryColor.a = alpha;
        
        DrawSphere(memory.lastKnownPosition, 12.0f, memoryColor);
        
        // Draw predicted position (lighter orange)
        Color predictColor = COLOR_ORANGE;
        predictColor.a = alpha * 0.5f;
        DrawSphere(memory.predictedPosition, 8.0f, predictColor);
        
        // Draw arrow from last known to predicted
        DrawArrow(memory.lastKnownPosition, memory.predictedPosition, memoryColor);
        
        // Draw confidence text
        std::string confText = StringFormat("Conf: %.2f", memory.confidenceLevel);
        DrawText3D(memory.lastKnownPosition + Vector(0, 0, 32), confText, COLOR_WHITE);
    }
}

void PerceptionVisualizer::DrawAudioEvents(const PerceptionSnapshot& perception) {
    for (const auto& sound : perception.recentSounds) {
        // Age-based fading (recent = bright, old = dim)
        float age = level.time - sound.timestamp;
        float alpha = std::max(0.0f, 1.0f - (age / 3.0f));  // Fade over 3 seconds
        
        Color soundColor = COLOR_BLUE;
        soundColor.a = alpha;
        
        // Draw pulsing circle at sound location
        float pulseRadius = 32.0f + (age * 10.0f);
        DrawCircle(sound.position, pulseRadius, soundColor);
        
        // Draw direction estimate
        if (sound.confidence > 0.5f) {
            Vector dirEnd = bot->origin + (sound.estimatedDirection * 100.0f);
            DrawArrow(bot->origin, dirEnd, soundColor);
        }
        
        // Draw sound type text
        std::string typeText = GetAudioEventName(sound.type);
        DrawText3D(sound.position + Vector(0, 0, 16), typeText, COLOR_CYAN);
    }
}

void PerceptionVisualizer::DrawNearbyAllies(const PerceptionSnapshot& perception) {
    for (const auto& ally : perception.nearbyAllies) {
        // Draw line to ally (blue)
        DrawLine(bot->origin, ally.position, COLOR_BLUE);
        
        // Draw sphere at ally position
        DrawSphere(ally.position, 12.0f, COLOR_BLUE);
        
        // Draw if ally can see bot
        if (ally.canSeeMe) {
            DrawText3D(ally.position + Vector(0, 0, 48), "[Sees Me]", COLOR_CYAN);
        }
    }
}

void PerceptionVisualizer::DrawThreatLevel(Player* bot, const PerceptionSnapshot& perception) {
    // Draw threat level as colored ring around bot
    Color threatColor;
    std::string threatText;
    
    switch (perception.threatLevel) {
        case THREAT_NONE:
            threatColor = COLOR_GREEN;
            threatText = "SAFE";
            break;
        case THREAT_LOW:
            threatColor = COLOR_YELLOW;
            threatText = "LOW THREAT";
            break;
        case THREAT_MEDIUM:
            threatColor = COLOR_ORANGE;
            threatText = "MEDIUM THREAT";
            break;
        case THREAT_HIGH:
            threatColor = COLOR_RED;
            threatText = "HIGH THREAT";
            break;
    }
    
    DrawCircle(bot->origin, 64.0f, threatColor, 2.0f);
    DrawText3D(bot->origin + Vector(0, 0, 80), threatText, threatColor);
}
```

#### 4. Tactical Overlay

```cpp
// code/fgame/bot_debug_viz.cpp

class TacticalVisualizer {
public:
    void DrawTacticalOverlay(Player* bot);
    
private:
    void DrawCoverPoints(Player* bot);
    void DrawDangerZones(Player* bot);
    void DrawCurrentPath(Player* bot);
    void DrawSquadCoordination(Player* bot);
};

void TacticalVisualizer::DrawTacticalOverlay(Player* bot) {
    DrawCoverPoints(bot);
    DrawDangerZones(bot);
    DrawCurrentPath(bot);
    DrawSquadCoordination(bot);
}

void TacticalVisualizer::DrawCoverPoints(Player* bot) {
    const float searchRadius = 1024.0f;
    
    for (auto* cover : FindCoverPointsInRadius(bot->origin, searchRadius)) {
        // Color based on cover quality
        float quality = cover->GetQuality();
        Color coverColor;
        
        if (quality > 0.8f) {
            coverColor = COLOR_GREEN;  // Excellent cover
        } else if (quality > 0.5f) {
            coverColor = COLOR_YELLOW;  // Medium cover
        } else {
            coverColor = COLOR_ORANGE;  // Poor cover
        }
        
        // Draw cover marker
        DrawCube(cover->position, 16.0f, coverColor);
        
        // Draw cover facing direction
        Vector facing = cover->GetFacingDirection();
        DrawArrow(cover->position, cover->position + facing * 32.0f, coverColor);
        
        // Draw quality text
        std::string qualityText = StringFormat("Q: %.2f", quality);
        DrawText3D(cover->position + Vector(0, 0, 24), qualityText, COLOR_WHITE);
    }
}

void TacticalVisualizer::DrawDangerZones(Player* bot) {
    PerceptionSnapshot* perception = bot->GetPerception();
    
    // Draw danger zone around each visible enemy
    for (const auto& enemy : perception->visibleEnemies) {
        // Danger radius based on weapon range
        float dangerRadius = enemy.entity->GetWeaponRange();
        
        // Red gradient (darker = more dangerous)
        Color dangerColor = COLOR_RED;
        dangerColor.a = 0.3f;
        
        DrawCircle(enemy.position, dangerRadius, dangerColor, 2.0f);
        
        // Draw crosshairs at enemy position
        DrawCrosshair(enemy.position, 32.0f, COLOR_RED);
    }
}

void TacticalVisualizer::DrawCurrentPath(Player* bot) {
    auto path = bot->GetCurrentPath();
    
    if (path.empty()) return;
    
    // Draw path as connected waypoints
    Vector prevPos = bot->origin;
    
    for (const auto& waypoint : path) {
        // Draw line segment
        DrawLine(prevPos, waypoint, COLOR_CYAN, 2.0f);
        
        // Draw waypoint marker
        DrawSphere(waypoint, 8.0f, COLOR_CYAN);
        
        prevPos = waypoint;
    }
    
    // Draw goal marker (larger)
    if (!path.empty()) {
        DrawSphere(path.back(), 16.0f, COLOR_GREEN);
    }
}

void TacticalVisualizer::DrawSquadCoordination(Player* bot) {
    PerceptionSnapshot* perception = bot->GetPerception();
    
    // Draw lines to nearby allies
    for (const auto& ally : perception->nearbyAllies) {
        // Purple line = squad coordination
        DrawLine(bot->origin, ally.position, COLOR_PURPLE, 1.0f);
        
        // Draw ally's target if they have one
        if (ally.entity->GetEnemy()) {
            Vector allyTarget = ally.entity->GetEnemy()->origin;
            DrawLine(ally.position, allyTarget, COLOR_PURPLE, 0.5f, true);  // Dashed
        }
    }
}
```

### Console Commands

```cpp
// code/fgame/gamecmds.cpp

void Cmd_BotDebug_f() {
    if (gi.argc() < 3) {
        gi.Printf("Usage: bot_debug <botnum> <mode>\n");
        gi.Printf("Modes: none, perception, behavior, utility, tactical, all\n");
        return;
    }
    
    int botNum = atoi(gi.argv(1));
    std::string mode = gi.argv(2);
    
    Player* bot = GetBotByIndex(botNum);
    if (!bot) {
        gi.Printf("Invalid bot number\n");
        return;
    }
    
    BotDebugViz* debugViz = bot->GetDebugViz();
    
    if (mode == "none") {
        debugViz->SetMode(DEBUG_NONE);
    } else if (mode == "perception") {
        debugViz->SetMode(DEBUG_PERCEPTION);
    } else if (mode == "behavior") {
        debugViz->SetMode(DEBUG_BEHAVIOR_TREE);
    } else if (mode == "utility") {
        debugViz->SetMode(DEBUG_UTILITY);
    } else if (mode == "tactical") {
        debugViz->SetMode(DEBUG_TACTICAL);
    } else if (mode == "all") {
        debugViz->SetMode(DEBUG_ALL);
    } else {
        gi.Printf("Unknown mode: %s\n", mode.c_str());
    }
}

void Cmd_BotUtilityScores_f() {
    if (gi.argc() < 2) {
        gi.Printf("Usage: bot_utility_scores <botnum>\n");
        return;
    }
    
    int botNum = atoi(gi.argv(1));
    Player* bot = GetBotByIndex(botNum);
    
    if (!bot) {
        gi.Printf("Invalid bot number\n");
        return;
    }
    
    auto scores = bot->GetUtilityEvaluator()->ScoreAllActions(
        bot->GetPerception(),
        bot,
        bot->GetProfile()
    );
    
    gi.Printf("Utility Scores for Bot %d:\n", botNum);
    for (const auto& score : scores) {
        gi.Printf("  %s: %.3f\n", score.name.c_str(), score.score);
    }
}
```

---

## Implementation Steps

### Week 1: Core Visualizations

#### Day 1-2: Enhanced BT Visualizer (12 hours)
- [ ] Implement `BTVisualizer` class
- [ ] Add execution timing tracking
- [ ] Add execution count tracking
- [ ] Draw hierarchical tree with indentation
- [ ] Color-code by status (green/red/yellow/gray)
- [ ] Display blackboard values
- [ ] Test with complex trees

#### Day 3: Utility Score Visualization (6 hours)
- [ ] Implement `UtilityVisualizer` class
- [ ] Draw bar chart of action scores
- [ ] Highlight selected action
- [ ] Implement consideration breakdown view
- [ ] Show raw vs. curved values
- [ ] Test with different scenarios

#### Day 4-5: Perception Visualization (12 hours)
- [ ] Implement `PerceptionVisualizer` class
- [ ] Draw vision cone (FOV + peripheral)
- [ ] Draw visible enemies with visibility factor
- [ ] Draw enemy memories with confidence fading
- [ ] Draw audio events with age-based fading
- [ ] Draw nearby allies
- [ ] Draw threat level indicator
- [ ] Test with various perception states

### Week 2: Tactical & Integration

#### Day 6: Tactical Overlay (6 hours)
- [ ] Implement `TacticalVisualizer` class
- [ ] Draw cover points with quality colors
- [ ] Draw danger zones around enemies
- [ ] Draw current path
- [ ] Draw squad coordination lines
- [ ] Test with different tactical situations

#### Day 7: Console Commands (4 hours)
- [ ] Implement `Cmd_BotDebug_f` command
- [ ] Implement `Cmd_BotUtilityScores_f` command
- [ ] Add command registration
- [ ] Test all visualization modes
- [ ] Add help text

#### Day 8-9: Integration & Polish (12 hours)
- [ ] Create `BotDebugViz` manager class
- [ ] Integrate all visualizers
- [ ] Add mode switching
- [ ] Add per-bot debug state
- [ ] Ensure visualizations don't impact performance
- [ ] Add cvars for visualization settings
- [ ] Test with multiple bots visualized simultaneously

#### Day 10: Testing & Documentation (6 hours)
- [ ] **Write unit tests** (5 tests: color selection, text formatting, bounds checking, mode switching, multi-bot)
- [ ] Test all visualization modes
- [ ] Document console commands
- [ ] Create visual examples/screenshots
- [ ] Performance testing (visualization overhead)
- [ ] Fix any bugs

---

## Files to Create/Modify

### New Files
```
code/fgame/bot_debug_viz.h           # BotDebugViz manager and visualizer classes
code/fgame/bot_debug_viz.cpp         # Implementation
code/fgame/debug_drawing.h           # Drawing primitives (if not exists)
code/fgame/debug_drawing.cpp         # DrawCone, DrawSphere, DrawText3D, etc.
tests/test_debug_viz.cpp             # Unit tests
```

### Modified Files
```
code/fgame/playerbot.h               # Add BotDebugViz member
code/fgame/playerbot.cpp             # Call debug rendering in Think()
code/fgame/gamecmds.cpp              # Add console commands
```

---

## Acceptance Criteria

### Functionality
- [ ] Enhanced BT visualizer with timing/history
- [ ] Utility score bar chart with highlighting
- [ ] Perception overlay with all elements
- [ ] Tactical overlay with cover/danger/paths
- [ ] Console commands functional
- [ ] Multiple visualization modes
- [ ] Per-bot debug state

### Quality
- [ ] 5 unit tests pass
- [ ] Clear, readable visualizations
- [ ] No performance impact (< 0.5ms per bot)
- [ ] Works with multiple bots
- [ ] Code follows OpenMoHAA standards

### Usability
- [ ] Easy to toggle modes via console
- [ ] Visualizations clearly labeled
- [ ] Colors intuitive (green=good, red=bad, etc.)
- [ ] Text readable at various distances

---

## Success Metrics

### Development Speed
- **Bug Identification:** See issues immediately
- **Tuning:** Adjust parameters with instant feedback
- **Understanding:** New developers grasp AI quickly

### Visualization Quality
- **Clarity:** Information easily understood
- **Performance:** No frame drops with viz enabled
- **Completeness:** All AI state visible

---

## Troubleshooting

### Visualization Overlaps
- Adjust spacing/positioning
- Use layered rendering
- Add transparency

### Performance Impact
- Limit visualization to nearby bots
- Reduce update frequency
- Simplify drawing primitives

### Text Unreadable
- Increase font size
- Add background boxes
- Use contrasting colors

---

**Next Task:** Task 3.6 - Remove Old State Machine
