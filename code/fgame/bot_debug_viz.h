/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Added in OPM - Phase 3 Task 3.5
//  Comprehensive debug visualization system for bot AI

#pragma once

#include "g_local.h"
#include "debug_drawing.h"
#include "behavior_tree.h"
#include "utility_evaluator.h"
#include "perception.h"
#include "bot_profile.h"
#include <string>
#include <vector>
#include <map>

// Forward declarations
class Player;
class BotController;

/**
 * Debug visualization modes.
 * These can be combined using bitwise OR.
 */
enum DebugVizMode {
    DEBUG_NONE       = 0,
    DEBUG_PERCEPTION = (1 << 0), // FOV, enemies, sounds, allies
    DEBUG_BEHAVIOR   = (1 << 1), // Behavior tree execution
    DEBUG_UTILITY    = (1 << 2), // Utility AI scores
    DEBUG_TACTICAL   = (1 << 3), // Cover, paths, danger zones
    DEBUG_ALL        = 0xFFFFFFFF
};

/**
 * Behavior Tree Visualizer
 * Displays hierarchical behavior tree with execution status, timing, and blackboard data.
 */
class BTVisualizer
{
public:
    struct NodeVisualization {
        const BTNode  *node;
        BTNode::Status lastStatus;
        float          lastExecutionTime; // milliseconds
        int            executionCount;
        float          totalTime;       // cumulative milliseconds
        int            lastUpdateFrame; // frame number when last updated

        NodeVisualization()
            : node(nullptr)
            , lastStatus(BTNode::Status::FAILURE)
            , lastExecutionTime(0.0f)
            , executionCount(0)
            , totalTime(0.0f)
            , lastUpdateFrame(0)
        {}
    };

    void DrawBehaviorTree(Player *bot, BehaviorTree *tree, const Blackboard& blackboard, Vector screenPos);
    void RecordNodeExecution(const BTNode *node, BTNode::Status status, float deltaTime);
    void Reset();

private:
    void DrawNode(const BTNode *node, Vector& pos, int depth, int currentFrame);
    void DrawBlackboard(const Blackboard& blackboard, Vector& pos);
    void GetNodeColor(BTNode::Status status, float& r, float& g, float& b) const;

    std::map<const BTNode *, NodeVisualization> nodeHistory;
};

/**
 * Utility AI Visualizer
 * Displays bar chart of action scores and consideration breakdowns.
 */
// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
class UtilityVisualizer
{
public:
    // Changed in OPM - Code review fixes (Fix #5)
    //  Pass Vector by reference to avoid unnecessary copy
    void DrawUtilityScores(
        Player                                            *bot,
        const std::vector<UtilityEvaluator::ScoredAction>& scores,
        const std::string&                                 currentStrategy,
        Vector&                                            screenPos
    ) const;
    void DrawConsiderationBreakdown(const UtilityEvaluator::ScoredAction& action, Vector screenPos) const;
};

/**
 * Perception Visualizer
 * Displays FOV cone, visible enemies, memory, audio events, and allies.
 */
// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
class PerceptionVisualizer
{
public:
    void DrawPerceptionOverlay(Player *bot, const PerceptionSnapshot& perception, const BotProfile *profile) const;

private:
    void DrawVisionCone(Player *bot, const BotProfile *profile) const;
    void DrawVisibleEnemies(Player *bot, const PerceptionSnapshot& perception) const;
    void DrawEnemyMemories(const PerceptionSnapshot& perception) const;
    void DrawAudioEvents(Player *bot, const PerceptionSnapshot& perception) const;
    void DrawNearbyAllies(Player *bot, const PerceptionSnapshot& perception) const;
    void DrawThreatLevel(Player *bot, const PerceptionSnapshot& perception) const;
};

/**
 * Tactical Visualizer
 * Displays cover points, danger zones, paths, and squad coordination.
 */
// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
class TacticalVisualizer
{
public:
    void DrawTacticalOverlay(Player *bot) const;

private:
    void DrawCoverPoints(Player *bot) const;
    void DrawDangerZones(Player *bot, const PerceptionSnapshot& perception) const;
    void DrawCurrentPath(Player *bot) const;
    void DrawSquadCoordination(Player *bot, const PerceptionSnapshot& perception) const;
};

/**
 * Bot Debug Visualization Manager
 * Centralized manager for all bot debug visualization.
 */
class BotDebugViz
{
public:
    BotDebugViz();
    ~BotDebugViz() = default;

    // Mode management
    void SetMode(int mode);

    int GetMode() const { return currentMode; }

    bool        IsModeActive(DebugVizMode mode) const;
    void        ToggleMode(DebugVizMode mode);
    const char *GetModeName() const;

    // Main rendering entry point
    void Draw(BotController *bot);

    // Component accessors
    BTVisualizer& GetBTVisualizer() { return btVisualizer; }

    UtilityVisualizer& GetUtilityVisualizer() { return utilityVisualizer; }

    PerceptionVisualizer& GetPerceptionVisualizer() { return perceptionVisualizer; }

    TacticalVisualizer& GetTacticalVisualizer() { return tacticalVisualizer; }

private:
    int currentMode;

    // Visualizers
    BTVisualizer         btVisualizer;
    UtilityVisualizer    utilityVisualizer;
    PerceptionVisualizer perceptionVisualizer;
    TacticalVisualizer   tacticalVisualizer;
};
