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
//  Implementation of comprehensive debug visualization system for bot AI
// Changed in OPM - Code review fixes (Fix #6)
//  Removed <sstream> and <iomanip> includes - no longer needed after snprintf() migration

#include "bot_debug_viz.h"
#include "playerbot.h"
#include "player.h"
#include "bt_blackboard_keys.h"
#include <cstdio>

// Added in OPM - Code review fixes (Fix #7)
//  Named constants to replace magic numbers
namespace DebugVizConstants
{
    constexpr float BEHAVIOR_TREE_Y_OFFSET      = 40.0f;
    constexpr float UTILITY_SCORES_Y_OFFSET     = 60.0f;
    constexpr float NODE_INDENT_SIZE            = 20.0f;
    constexpr float NODE_HEIGHT                 = 15.0f;
    constexpr int   NODE_DISPLAY_TIMEOUT_FRAMES = 300; // 5 seconds at 60 FPS
    constexpr float SOUND_FADE_TIME_MS          = 3000.0f;
    constexpr float SOUND_PULSE_BASE_RADIUS     = 32.0f;
    constexpr float SOUND_PULSE_SPEED           = 0.01f;
    constexpr float THREAT_INDICATOR_RADIUS     = 64.0f;
    constexpr float DANGER_ZONE_RADIUS          = 512.0f;
    constexpr float TEXT_HEIGHT_OFFSET_ENEMY    = 64.0f;
    constexpr float TEXT_HEIGHT_OFFSET_MEMORY   = 32.0f;
    constexpr float TEXT_HEIGHT_OFFSET_ALLY     = 48.0f;
    constexpr float TEXT_HEIGHT_OFFSET_THREAT   = 80.0f;
    constexpr float TEXT_HEIGHT_OFFSET_AUDIO    = 16.0f;
    constexpr float ENEMY_SPHERE_RADIUS         = 16.0f;
    constexpr float MEMORY_SPHERE_RADIUS        = 12.0f;
    constexpr float MEMORY_PREDICTED_RADIUS     = 8.0f;
    constexpr float ALLY_SPHERE_RADIUS          = 12.0f;
} // namespace DebugVizConstants

//=================================================================================
// BotDebugViz - Main Manager
//=================================================================================

BotDebugViz::BotDebugViz()
    : currentMode(DEBUG_NONE)
{}

void BotDebugViz::SetMode(int mode)
{
    currentMode = mode;
}

bool BotDebugViz::IsModeActive(DebugVizMode mode) const
{
    return (currentMode & mode) != 0;
}

void BotDebugViz::ToggleMode(DebugVizMode mode)
{
    currentMode ^= mode;
}

// Changed in OPM - Code review fixes (Fix #1)
//  Replaced unsafe strcat() with std::string to prevent buffer overflow
const char *BotDebugViz::GetModeName() const
{
    if (currentMode == DEBUG_NONE) {
        return "none";
    }
    if (currentMode == DEBUG_ALL) {
        return "all";
    }

    static std::string modeStr;
    modeStr.clear();

    if (currentMode & DEBUG_PERCEPTION) {
        modeStr += "perception ";
    }
    if (currentMode & DEBUG_BEHAVIOR) {
        modeStr += "behavior ";
    }
    if (currentMode & DEBUG_UTILITY) {
        modeStr += "utility ";
    }
    if (currentMode & DEBUG_TACTICAL) {
        modeStr += "tactical ";
    }

    if (!modeStr.empty()) {
        modeStr.pop_back(); // Remove trailing space
    }

    return modeStr.c_str();
}

void BotDebugViz::Draw(BotController *controller)
{
    if (!controller || currentMode == DEBUG_NONE) {
        return;
    }

    Player *bot = controller->getControlledEntity();
    if (!bot) {
        return;
    }

    // Get perception snapshot from blackboard
    auto perceptionOpt = controller->GetBlackboard().TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    PerceptionSnapshot *perception = perceptionOpt ? *perceptionOpt : nullptr;

    // Draw perception overlay
    if ((currentMode & DEBUG_PERCEPTION) && perception) {
        perceptionVisualizer.DrawPerceptionOverlay(bot, *perception, controller->GetProfile());
    }

    // Draw behavior tree
    if (currentMode & DEBUG_BEHAVIOR) {
        BehaviorTree *tree = controller->GetBehaviorTree();
        if (tree) {
            Vector screenPos = bot->origin + Vector(0, 0, bot->viewheight + DebugVizConstants::BEHAVIOR_TREE_Y_OFFSET);
            btVisualizer.DrawBehaviorTree(bot, tree, controller->GetBlackboard(), screenPos);
        }
    }

    // Draw utility scores
    if (currentMode & DEBUG_UTILITY) {
        auto scoresOpt =
            controller->GetBlackboard().TryGet<std::shared_ptr<std::vector<UtilityEvaluator::ScoredAction>>>(
                BlackboardKeys::UTILITY_SCORES
            );
        if (scoresOpt && *scoresOpt) {
            Vector screenPos = bot->origin + Vector(0, 0, bot->viewheight + DebugVizConstants::UTILITY_SCORES_Y_OFFSET);
            utilityVisualizer.DrawUtilityScores(bot, **scoresOpt, controller->GetCurrentStrategy(), screenPos);
        }
    }

    // Draw tactical overlay
    if (currentMode & DEBUG_TACTICAL) {
        tacticalVisualizer.DrawTacticalOverlay(bot);
    }
}

//=================================================================================
// BTVisualizer - Behavior Tree Visualization
//=================================================================================

void BTVisualizer::DrawBehaviorTree(Player *bot, BehaviorTree *tree, const Blackboard& blackboard, Vector screenPos)
{
    if (!tree || !bot) {
        return;
    }

    // Draw tree title
    G_DebugString(screenPos, 0.5f, 1.0f, 1.0f, 0.0f, "Behavior Tree");
    screenPos[2] -= 15.0f;

    // Draw root node and children recursively
    float   currentY = screenPos[2];
    BTNode *root     = tree->GetRoot();
    if (root) {
        DrawNode(root, screenPos, 0, level.framenum);
    }

    // Draw blackboard values below the tree
    screenPos[2] -= 20.0f;
    DrawBlackboard(blackboard, screenPos);
}

void BTVisualizer::DrawNode(const BTNode *node, Vector& pos, int depth, int currentFrame)
{
    if (!node) {
        return;
    }

    // Changed in OPM - Code review fixes (Fix #7)
    //  Use named constants instead of magic numbers
    const float indentSize = DebugVizConstants::NODE_INDENT_SIZE;
    const float nodeHeight = DebugVizConstants::NODE_HEIGHT;

    // Apply indentation
    Vector nodePos = pos;
    nodePos[0] += depth * indentSize;

    // Changed in OPM - Code review fixes (Fix #3)
    //  Use find() instead of operator[] for read-only access to avoid creating map entries
    auto it = nodeHistory.find(node);
    if (it == nodeHistory.end()) {
        // Node never executed, show default gray state
        // Changed in OPM - Code review fixes (Fix #4)
        //  Use snprintf() instead of ostringstream to avoid per-frame allocations
        char nodeText[256];
        int  offset = snprintf(nodeText, sizeof(nodeText), "%s", node->GetName());
        if (node->IsComposite()) {
            snprintf(nodeText + offset, sizeof(nodeText) - offset, " [%zu children]", node->GetChildCount());
        }
        G_DebugString(nodePos, 0.35f, 0.5f, 0.5f, 0.5f, "%s", nodeText);
        pos[2] -= nodeHeight;
        return;
    }

    const NodeVisualization& viz = it->second;

    // Determine color based on last status
    float r, g, b;
    GetNodeColor(viz.lastStatus, r, g, b);

    // Changed in OPM - Code review fixes (Fix #4)
    //  Use snprintf() instead of ostringstream to avoid per-frame allocations
    // Draw node name with type indicator and stats
    char nodeText[256];
    int  offset = snprintf(nodeText, sizeof(nodeText), "%s", node->GetName());

    // Changed in OPM - Code review fixes (Fix #2)
    //  Use virtual method instead of dynamic_cast for better performance
    // Add type indicator
    if (node->IsComposite()) {
        offset += snprintf(nodeText + offset, sizeof(nodeText) - offset, " [%zu children]", node->GetChildCount());
    }

    // Add execution stats if updated recently
    if (viz.lastUpdateFrame > 0
        && (currentFrame - viz.lastUpdateFrame) < DebugVizConstants::NODE_DISPLAY_TIMEOUT_FRAMES) {
        snprintf(
            nodeText + offset, sizeof(nodeText) - offset, " (%dx, %.1fms)", viz.executionCount, viz.lastExecutionTime
        );
    }

    G_DebugString(nodePos, 0.35f, r, g, b, "%s", nodeText);

    // Move to next line
    pos[2] -= nodeHeight;

    // Changed in OPM - Code review fixes (Fix #2)
    //  Use virtual method instead of dynamic_cast for better performance
    // Draw children if this is a composite node
    if (node->IsComposite()) {
        for (size_t i = 0; i < node->GetChildCount(); i++) {
            const BTNode *child = node->GetChild(i);
            if (child) {
                DrawNode(child, pos, depth + 1, currentFrame);
            }
        }
    }
}

void BTVisualizer::DrawBlackboard(const Blackboard& blackboard, Vector& pos)
{
    // Draw blackboard title
    G_DebugString(pos, 0.4f, 1.0f, 1.0f, 0.0f, "Blackboard:");
    pos[2] -= 12.0f;

    // Get all keys
    auto keys = blackboard.GetAllKeys();

    // Limit display to most important keys to avoid clutter
    const char *importantKeys[] = {
        BlackboardKeys::CURRENT_STRATEGY,
        BlackboardKeys::SELECTED_TARGET,
        BlackboardKeys::TARGET_DISTANCE,
        BlackboardKeys::COVER_STATE,
        BlackboardKeys::BEHAVIOR_STATE,
        BlackboardKeys::INVESTIGATING_MEMORY_INDEX,
        BlackboardKeys::PATROL_MODE,
        BlackboardKeys::WANDER_START_TIME
    };

    // Changed in OPM - Code review fixes (Fix #6)
    //  Use snprintf() instead of ostringstream to avoid per-frame allocations
    for (const char *key : importantKeys) {
        if (blackboard.Has(key)) {
            std::string value = blackboard.GetAsString(key);
            char        text[256];
            snprintf(text, sizeof(text), "  %s: %s", key, value.c_str());
            G_DebugString(pos, 0.3f, 0.8f, 0.8f, 0.8f, "%s", text);
            pos[2] -= 10.0f;
        }
    }
}

void BTVisualizer::GetNodeColor(BTNode::Status status, float& r, float& g, float& b) const
{
    switch (status) {
    case BTNode::Status::SUCCESS:
        r = 0.0f;
        g = 1.0f;
        b = 0.0f;
        break;
    case BTNode::Status::FAILURE:
        r = 1.0f;
        g = 0.0f;
        b = 0.0f;
        break;
    case BTNode::Status::RUNNING:
        r = 1.0f;
        g = 1.0f;
        b = 0.0f;
        break;
    default:
        r = 0.5f;
        g = 0.5f;
        b = 0.5f;
        break;
    }
}

void BTVisualizer::RecordNodeExecution(const BTNode *node, BTNode::Status status, float deltaTime)
{
    if (!node) {
        return;
    }

    NodeVisualization& viz = nodeHistory[node];
    viz.node               = node;
    viz.lastStatus         = status;
    viz.lastExecutionTime  = deltaTime * 1000.0f; // Convert to milliseconds
    viz.executionCount++;
    viz.totalTime += viz.lastExecutionTime;
    viz.lastUpdateFrame = level.framenum;
}

void BTVisualizer::Reset()
{
    nodeHistory.clear();
}

//=================================================================================
// UtilityVisualizer - Utility AI Visualization
//=================================================================================

// Changed in OPM - Code review fixes (Fix #5)
//  Pass Vector by reference to avoid unnecessary copy
// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
void UtilityVisualizer::DrawUtilityScores(
    Player                                            *bot,
    const std::vector<UtilityEvaluator::ScoredAction>& scores,
    const std::string&                                 currentStrategy,
    Vector&                                            screenPos
) const
{
    if (!bot || scores.empty()) {
        return;
    }

    // Title
    G_DebugString(screenPos, 0.5f, 1.0f, 1.0f, 0.0f, "Utility Scores:");
    screenPos[2] -= 15.0f;

    // Find highest score for scaling
    float maxScore = 0.0f;
    for (const auto& score : scores) {
        if (score.score > maxScore) {
            maxScore = score.score;
        }
    }

    if (maxScore < 0.01f) {
        maxScore = 1.0f; // Avoid division by zero
    }

    // Changed in OPM - Code review fixes (Fix #6)
    //  Use snprintf() instead of ostringstream to avoid per-frame allocations
    // Draw bar chart for each action
    for (const auto& score : scores) {
        // Action name
        bool  isActive = (score.name == currentStrategy);
        float r        = isActive ? 0.0f : 1.0f;
        float g        = isActive ? 1.0f : 1.0f;
        float b        = isActive ? 0.0f : 1.0f;

        char text[128];
        if (isActive) {
            snprintf(text, sizeof(text), "%s: %.3f [ACTIVE]", score.name.c_str(), score.score);
        } else {
            snprintf(text, sizeof(text), "%s: %.3f", score.name.c_str(), score.score);
        }

        G_DebugString(screenPos, 0.35f, r, g, b, "%s", text);

        screenPos[2] -= 12.0f;
    }
}

// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
void UtilityVisualizer::DrawConsiderationBreakdown(const UtilityEvaluator::ScoredAction& action, Vector screenPos) const
{
    // This would show individual consideration scores
    // Implementation omitted for brevity - can be added later if needed
}

//=================================================================================
// PerceptionVisualizer - Perception System Visualization
//=================================================================================

// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
void PerceptionVisualizer::DrawPerceptionOverlay(
    Player *bot, const PerceptionSnapshot& perception, const BotProfile *profile
) const
{
    if (!bot) {
        return;
    }

    DrawVisionCone(bot, profile);
    DrawVisibleEnemies(bot, perception);
    DrawEnemyMemories(perception);
    DrawAudioEvents(bot, perception);
    DrawNearbyAllies(bot, perception);
    DrawThreatLevel(bot, perception);
}

void PerceptionVisualizer::DrawVisionCone(Player *bot, const BotProfile *profile) const
{
    if (!bot || !profile) {
        return;
    }

    Vector origin = bot->origin + Vector(0, 0, bot->viewheight);
    Vector forward;
    bot->angles.AngleVectorsLeft(&forward, NULL, NULL);

    float visionRange = profile->GetVisionRange();
    float fov         = profile->GetVisionFOV();

    // Draw main FOV cone (yellow, transparent)
    DebugDrawing::DrawCone(origin, forward, visionRange, fov / 2.0f, 1.0f, 1.0f, 0.0f, 0.2f);
}

void PerceptionVisualizer::DrawVisibleEnemies(Player *bot, const PerceptionSnapshot& perception) const
{
    if (!bot) {
        return;
    }

    Vector origin = bot->origin + Vector(0, 0, bot->viewheight);

    // Changed in OPM - Code review fixes (Fix #6)
    //  Use snprintf() instead of ostringstream to avoid per-frame allocations
    for (const auto& enemy : perception.visibleEnemies) {
        // Draw line to enemy (green = visible)
        G_DebugLine(origin, enemy.position, 0.0f, 1.0f, 0.0f, 1.0f);

        // Draw sphere at enemy position
        DebugDrawing::DrawSphere(enemy.position, DebugVizConstants::ENEMY_SPHERE_RADIUS, 0.0f, 1.0f, 0.0f, 1.0f);

        // Draw visibility factor as text
        char text[64];
        snprintf(text, sizeof(text), "Vis: %.2f", enemy.visibilityFactor);
        DebugDrawing::DrawText3D(
            enemy.position + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_ENEMY), text, 0.4f, 1.0f, 1.0f, 1.0f
        );

        // Draw if in peripheral vision
        if (enemy.isInPeripheral) {
            DebugDrawing::DrawText3D(
                enemy.position + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_ALLY),
                "[Peripheral]",
                0.35f,
                1.0f,
                1.0f,
                0.0f
            );
        }
    }
}

void PerceptionVisualizer::DrawEnemyMemories(const PerceptionSnapshot& perception) const
{
    // Changed in OPM - Code review fixes (Fix #6)
    //  Use snprintf() instead of ostringstream to avoid per-frame allocations
    for (const auto& memory : perception.knownEnemies) {
        // Draw ghost at last known position (orange, fading with confidence)
        float alpha = memory.confidenceLevel;
        DebugDrawing::DrawSphere(
            memory.lastKnownPosition, DebugVizConstants::MEMORY_SPHERE_RADIUS, 1.0f, 0.5f, 0.0f, alpha
        );

        // Draw predicted position (lighter orange)
        DebugDrawing::DrawSphere(
            memory.predictedPosition, DebugVizConstants::MEMORY_PREDICTED_RADIUS, 1.0f, 0.7f, 0.3f, alpha * 0.5f
        );

        // Draw arrow from last known to predicted
        DebugDrawing::DrawArrow(memory.lastKnownPosition, memory.predictedPosition, 1.0f, 0.5f, 0.0f, alpha);

        // Draw confidence text
        char text[64];
        snprintf(text, sizeof(text), "Conf: %.2f", memory.confidenceLevel);
        DebugDrawing::DrawText3D(
            memory.lastKnownPosition + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_MEMORY),
            text,
            0.35f,
            1.0f,
            1.0f,
            1.0f
        );
    }
}

void PerceptionVisualizer::DrawAudioEvents(Player *bot, const PerceptionSnapshot& perception) const
{
    if (!bot) {
        return;
    }

    for (const auto& sound : perception.recentSounds) {
        // Age-based fading (recent = bright, old = dim)
        float age   = level.svsTime - sound.timestamp;
        float alpha = 1.0f - (age / DebugVizConstants::SOUND_FADE_TIME_MS);
        if (alpha < 0.0f) {
            alpha = 0.0f;
        }

        // Draw pulsing circle at sound location
        float pulseRadius = DebugVizConstants::SOUND_PULSE_BASE_RADIUS + (age * DebugVizConstants::SOUND_PULSE_SPEED);
        DebugDrawing::DrawCircle(sound.position, pulseRadius, 0.0f, 0.5f, 1.0f, alpha, qfalse);

        // Draw sound type text
        DebugDrawing::DrawText3D(
            sound.position + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_AUDIO), "Sound", 0.35f, 0.0f, 1.0f, 1.0f
        );
    }
}

void PerceptionVisualizer::DrawNearbyAllies(Player *bot, const PerceptionSnapshot& perception) const
{
    if (!bot) {
        return;
    }

    Vector origin = bot->origin + Vector(0, 0, bot->viewheight);

    for (const auto& ally : perception.visibleAllies) {
        // Draw line to ally (blue)
        G_DebugLine(origin, ally.position, 0.0f, 0.0f, 1.0f, 1.0f);

        // Draw sphere at ally position
        DebugDrawing::DrawSphere(ally.position, DebugVizConstants::ALLY_SPHERE_RADIUS, 0.0f, 0.0f, 1.0f, 1.0f);

        // Draw if ally can see bot
        if (ally.canSeeMe) {
            DebugDrawing::DrawText3D(
                ally.position + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_ALLY),
                "[Sees Me]",
                0.35f,
                0.0f,
                1.0f,
                1.0f
            );
        }
    }
}

void PerceptionVisualizer::DrawThreatLevel(Player *bot, const PerceptionSnapshot& perception) const
{
    if (!bot) {
        return;
    }

    // Draw threat level as colored ring around bot
    float       r, g, b;
    const char *threatText;

    switch (perception.threatLevel) {
    case THREAT_NONE:
        r          = 0.0f;
        g          = 1.0f;
        b          = 0.0f;
        threatText = "SAFE";
        break;
    case THREAT_LOW:
        r          = 1.0f;
        g          = 1.0f;
        b          = 0.0f;
        threatText = "LOW THREAT";
        break;
    case THREAT_MEDIUM:
        r          = 1.0f;
        g          = 0.5f;
        b          = 0.0f;
        threatText = "MEDIUM THREAT";
        break;
    case THREAT_HIGH:
        r          = 1.0f;
        g          = 0.0f;
        b          = 0.0f;
        threatText = "HIGH THREAT";
        break;
    default:
        r          = 0.5f;
        g          = 0.5f;
        b          = 0.5f;
        threatText = "UNKNOWN";
        break;
    }

    DebugDrawing::DrawCircle(bot->origin, DebugVizConstants::THREAT_INDICATOR_RADIUS, r, g, b, 1.0f, qtrue);
    DebugDrawing::DrawText3D(
        bot->origin + Vector(0, 0, DebugVizConstants::TEXT_HEIGHT_OFFSET_THREAT), threatText, 0.5f, r, g, b
    );
}

//=================================================================================
// TacticalVisualizer - Tactical Overlay Visualization
//=================================================================================

// Changed in OPM - Code review fixes (Fix #8)
//  Add const-correctness to methods that don't modify state
void TacticalVisualizer::DrawTacticalOverlay(Player *bot) const
{
    if (!bot) {
        return;
    }

    DrawCurrentPath(bot);
    // Note: DrawCoverPoints, DrawDangerZones, DrawSquadCoordination
    // require access to BotController internals and perception
    // These can be implemented once the integration is complete
}

// Changed in OPM - Code review fixes (Fix #9)
//  Add clear TODO comments to stub implementations
void TacticalVisualizer::DrawCurrentPath(Player *bot) const
{
    // TODO: Implement path visualization
    // Will show waypoints and goal marker once pathfinding integration is complete
    // Expected visualization:
    //   - Green spheres for waypoints
    //   - Lines connecting waypoints
    //   - Yellow sphere for final goal
}

void TacticalVisualizer::DrawCoverPoints(Player *bot) const
{
    // TODO: Implement cover point visualization
    // Will show color-coded cover markers once cover system data structures are finalized
    // Expected visualization:
    //   - Green = good cover (high quality, safe)
    //   - Yellow = partial cover (medium quality)
    //   - Red = compromised cover (enemy can see it)
}

void TacticalVisualizer::DrawDangerZones(Player *bot, const PerceptionSnapshot& perception) const
{
    // Draw danger zones around each visible enemy
    for (const auto& enemy : perception.visibleEnemies) {
        DebugDrawing::DrawCircle(enemy.position, DebugVizConstants::DANGER_ZONE_RADIUS, 1.0f, 0.0f, 0.0f, 0.3f, qtrue);
    }
}

void TacticalVisualizer::DrawSquadCoordination(Player *bot, const PerceptionSnapshot& perception) const
{
    // TODO: Implement squad coordination visualization
    // Will show ally connections and shared targets once squad system is complete
    // Expected visualization:
    //   - Blue lines connecting squad members
    //   - Shared target indicators
    //   - Squad formation indicators
}
