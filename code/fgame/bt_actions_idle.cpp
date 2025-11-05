// bt_actions_idle.cpp
// Behavior tree actions for idle behaviors (curious, patrol, wander, idle)
// Added in OPM - Phase 3 Task 3.3

#include "bt_actions_idle.h"
#include "bt_blackboard_keys.h"
#include "bt_action_registry.h"
#include "perception.h"
#include "playerbot.h"
#include "player.h"
#include "navigate.h"
#include "g_local.h"
#include <algorithm>

// Constants for idle behaviors
namespace IdleConstants
{
    // Curious behavior
    constexpr float CURIOUS_REACHED_DISTANCE = 64.0f;   // Distance to consider curious location reached
    constexpr float CURIOUS_TIMEOUT          = 5000.0f;  // 5 seconds timeout (shorter than investigation)
    constexpr float CURIOUS_LOOK_DURATION    = 0.5f;     // Look in each direction for 0.5 seconds
    constexpr int   CURIOUS_LOOK_DIRECTIONS  = 2;        // Look left and right only

    // Patrol behavior
    constexpr float PATROL_REACHED_DISTANCE  = 32.0f;    // Distance to consider waypoint reached
    constexpr float PATROL_PAUSE_MIN         = 1.0f;     // Minimum pause at waypoint (seconds)
    constexpr float PATROL_PAUSE_RANDOM      = 2.0f;     // Random additional pause time (seconds)
    constexpr float PATROL_MOVE_SPEED        = 0.5f;     // Patrol at half speed

    // Wander behavior
    constexpr float WANDER_MIN_DISTANCE      = 256.0f;   // Minimum wander distance
    constexpr float WANDER_MAX_DISTANCE      = 768.0f;   // Maximum wander distance
    constexpr float WANDER_REACHED_DISTANCE  = 64.0f;    // Distance to consider wander target reached
    constexpr float WANDER_TIMEOUT           = 10000.0f; // 10 seconds timeout
    constexpr float WANDER_PAUSE_MIN         = 2.0f;     // Minimum pause after wander (seconds)
    constexpr float WANDER_PAUSE_RANDOM      = 3.0f;     // Random additional pause time (seconds)
    constexpr float WANDER_MOVE_SPEED        = 0.4f;     // Slow wander speed
    constexpr float WANDER_COOLDOWN          = 5.0f;     // Minimum time between wanders (seconds)

    // Attractive node behavior
    constexpr float ATTRACTIVE_NODE_SEARCH_RADIUS = 1024.0f; // Search radius for attractive nodes
    constexpr float ATTRACTIVE_NODE_REACHED       = 64.0f;   // Distance to consider node reached
    constexpr float ATTRACTIVE_NODE_USE_MIN       = 10.0f;   // Minimum time to use node (seconds)
    constexpr float ATTRACTIVE_NODE_USE_RANDOM    = 5.0f;    // Random additional use time (seconds)
    constexpr float ATTRACTIVE_NODE_LOOK_SPEED    = 2.0f;    // Rotation speed when using node

    // Idle behavior
    constexpr float IDLE_LOOK_MIN            = 3.0f;     // Minimum time between idle looks (seconds)
    constexpr float IDLE_LOOK_RANDOM         = 3.0f;     // Random additional time (seconds)
}

// ============================================================================
// Helper Functions
// ============================================================================

// Check if position is reachable via pathfinding
static bool IsPositionReachable(const Vector &from, const Vector &to)
{
    PathSearch path;
    path.Heuristic.self = &path;
    path.Heuristic.setSize(sizeof(path));

    Vector pathStart = from;
    Vector pathEnd   = to;

    return path.FindPath(pathStart, pathEnd, nullptr, 0, nullptr, 0) != nullptr;
}

// Find nearby attractive node
static PathNode *FindNearbyAttractiveNode(const Vector &origin)
{
    const float     searchRadius   = IdleConstants::ATTRACTIVE_NODE_SEARCH_RADIUS;
    PathNode       *bestNode       = nullptr;
    float           bestScore      = 0.0f;
    const PathNode *allNodes       = AI_GetPathNodeList();
    int             numNodes       = AI_NumPathNodes();

    for (int i = 0; i < numNodes; i++) {
        const PathNode *node = &allNodes[i];

        // Check if node has attractive flags
        if (!(node->nodeflags & (AI_SNIPER | AI_CORNER | AI_COVER))) {
            continue;
        }

        float distance = (node->origin - origin).length();
        if (distance < searchRadius) {
            // Score based on distance and node type
            float score = 1.0f - (distance / searchRadius);

            // Bonus for sniper nodes
            if (node->nodeflags & AI_SNIPER) {
                score *= 1.5f;
            }

            if (score > bestScore) {
                bestScore = score;
                bestNode  = const_cast<PathNode *>(node);
            }
        }
    }

    return bestNode;
}

// ============================================================================
// Curious Behavior Actions
// ============================================================================

BTNode::Status Action_SetCuriousTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return BTNode::Status::FAILURE;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return BTNode::Status::FAILURE;
    }

    // Find curious sounds (footsteps, misc, ambient - NOT weapon fire or urgent)
    AudioEvent *curiousSound = nullptr;
    for (auto &sound : perception->recentSounds) {
        if (sound.type == AI_EVENT_FOOTSTEP || sound.type == AI_EVENT_MISC) {
            curiousSound = &sound;
            break;
        }
    }

    if (!curiousSound) {
        return BTNode::Status::FAILURE;
    }

    // Set curious target
    blackboard.Set<Vector>(BlackboardKeys::CURIOUS_TARGET, curiousSound->position);
    blackboard.Set<float>(BlackboardKeys::CURIOUS_START_TIME, level.svsTime);
    blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, false);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_MoveToCuriousLocation_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::CURIOUS_TARGET);

    if (!botOpt || !playerOpt || !targetOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    Vector         target = *targetOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Check if reached
    float distance = (target - player->origin).length();
    if (distance < IdleConstants::CURIOUS_REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, true);
        return BTNode::Status::SUCCESS;
    }

    // Check timeout
    float startTime = blackboard.GetOrDefault<float>(BlackboardKeys::CURIOUS_START_TIME, 0.0f);
    if (level.svsTime - startTime > IdleConstants::CURIOUS_TIMEOUT) {
        return BTNode::Status::FAILURE;
    }

    // Move to target (at walk speed - curious, not urgent)
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_QuickLookAround_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get or initialize look state
    float lookTimer = blackboard.GetOrDefault<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, 0.0f);
    int   lookCount = blackboard.GetOrDefault<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, 0);

    lookTimer += deltaTime;

    if (lookTimer > IdleConstants::CURIOUS_LOOK_DURATION) {
        lookCount++;
        lookTimer = 0.0f;

        if (lookCount >= IdleConstants::CURIOUS_LOOK_DIRECTIONS) {
            // Done looking
            blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, 0);
            blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, 0.0f);
            return BTNode::Status::SUCCESS;
        }

        // Look left or right
        Vector currentAngles = player->GetViewAngles();
        float  yawOffset     = (lookCount % 2 == 0) ? 90.0f : -90.0f;
        Vector newAngles     = currentAngles + Vector(0, yawOffset, 0);

        bot->GetRotation().SetTargetAngles(newAngles);
    }

    blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, lookTimer);
    blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, lookCount);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_ClearCuriousState_Execute(Blackboard &blackboard, float deltaTime)
{
    blackboard.Set<Vector>(BlackboardKeys::CURIOUS_TARGET, vec_zero);
    blackboard.Set<bool>(BlackboardKeys::REACHED_CURIOUS_LOCATION, false);
    blackboard.Set<float>(BlackboardKeys::CURIOUS_LOOK_TIMER, 0.0f);
    blackboard.Set<int>(BlackboardKeys::CURIOUS_LOOK_COUNT, 0);
    blackboard.Set<float>(BlackboardKeys::CURIOUS_START_TIME, 0.0f);

    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Patrol System Actions
// ============================================================================

BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt        = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt     = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto patrolRouteOpt = blackboard.TryGet<std::vector<PathNode *> *>(BlackboardKeys::PATROL_ROUTE);

    if (!botOpt || !playerOpt || !patrolRouteOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController            *bot         = *botOpt;
    Player                   *player      = *playerOpt;
    std::vector<PathNode *>  *patrolRoute = *patrolRouteOpt;

    if (!bot || !player || !patrolRoute || patrolRoute->empty()) {
        return BTNode::Status::FAILURE;
    }

    // Get current waypoint index
    int waypointIndex = blackboard.GetOrDefault<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, 0);

    // Validate index
    if (waypointIndex < 0 || waypointIndex >= static_cast<int>(patrolRoute->size())) {
        waypointIndex = 0;
        blackboard.Set<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, waypointIndex);
    }

    PathNode *waypoint = (*patrolRoute)[waypointIndex];

    // Check if reached
    float distance = (waypoint->origin - player->origin).length();
    if (distance < IdleConstants::PATROL_REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT, true);
        return BTNode::Status::SUCCESS;
    }

    // Move to waypoint
    bot->GetMovement().MoveTo(waypoint->origin, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    float pauseTimer = blackboard.GetOrDefault<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER, 0.0f);

    // Initialize pause duration if starting
    static float pauseDuration = IdleConstants::PATROL_PAUSE_MIN + (G_Random() * IdleConstants::PATROL_PAUSE_RANDOM);

    pauseTimer += deltaTime;

    if (pauseTimer >= pauseDuration) {
        // Pause complete
        blackboard.Set<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER, 0.0f);
        pauseDuration = IdleConstants::PATROL_PAUSE_MIN + (G_Random() * IdleConstants::PATROL_PAUSE_RANDOM);
        return BTNode::Status::SUCCESS;
    }

    blackboard.Set<float>(BlackboardKeys::WAYPOINT_PAUSE_TIMER, pauseTimer);
    return BTNode::Status::RUNNING;
}

BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    auto patrolRouteOpt = blackboard.TryGet<std::vector<PathNode *> *>(BlackboardKeys::PATROL_ROUTE);

    if (!patrolRouteOpt) {
        return BTNode::Status::FAILURE;
    }

    std::vector<PathNode *> *patrolRoute = *patrolRouteOpt;

    if (!patrolRoute || patrolRoute->empty()) {
        return BTNode::Status::FAILURE;
    }

    int  waypointIndex = blackboard.GetOrDefault<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, 0);
    bool patrolReverse = blackboard.GetOrDefault<bool>(BlackboardKeys::PATROL_REVERSE, false);
    std::string patrolMode = blackboard.GetOrDefault<std::string>(BlackboardKeys::PATROL_MODE, std::string("loop"));

    // Advance waypoint
    if (patrolReverse) {
        waypointIndex--;
        if (waypointIndex < 0) {
            waypointIndex  = 1;
            patrolReverse = false; // Reached start, go forward
        }
    } else {
        waypointIndex++;
        if (waypointIndex >= static_cast<int>(patrolRoute->size())) {
            if (patrolMode == "loop") {
                waypointIndex = 0; // Loop back to start
            } else if (patrolMode == "bounce") {
                waypointIndex = static_cast<int>(patrolRoute->size()) - 2;
                patrolReverse = true; // Reverse direction
            } else {
                waypointIndex = static_cast<int>(patrolRoute->size()) - 1; // Stay at end
            }
        }
    }

    blackboard.Set<int>(BlackboardKeys::PATROL_WAYPOINT_INDEX, waypointIndex);
    blackboard.Set<bool>(BlackboardKeys::PATROL_REVERSE, patrolReverse);
    blackboard.Set<bool>(BlackboardKeys::REACHED_PATROL_WAYPOINT, false);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_ContinuePatrol_Execute(Blackboard &blackboard, float deltaTime)
{
    // Placeholder action for status reporting
    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Wander System Actions
// ============================================================================

BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!playerOpt) {
        return BTNode::Status::FAILURE;
    }

    Player *player = *playerOpt;

    if (!player) {
        return BTNode::Status::FAILURE;
    }

    // Pick random direction and distance
    float angle    = G_Random() * 360.0f;
    float distance = IdleConstants::WANDER_MIN_DISTANCE
                   + (G_Random() * (IdleConstants::WANDER_MAX_DISTANCE - IdleConstants::WANDER_MIN_DISTANCE));

    float  radians    = angle * M_PI / 180.0f;
    Vector direction  = Vector(cos(radians), sin(radians), 0);
    Vector wanderTarget = player->origin + (direction * distance);

    // Check if reachable
    if (!IsPositionReachable(player->origin, wanderTarget)) {
        // Try closer position
        wanderTarget = player->origin + (direction * (distance * 0.5f));

        if (!IsPositionReachable(player->origin, wanderTarget)) {
            return BTNode::Status::FAILURE;
        }
    }

    blackboard.Set<Vector>(BlackboardKeys::WANDER_TARGET, wanderTarget);
    blackboard.Set<float>(BlackboardKeys::WANDER_START_TIME, level.svsTime);
    blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, false);

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto targetOpt = blackboard.TryGet<Vector>(BlackboardKeys::WANDER_TARGET);

    if (!botOpt || !playerOpt || !targetOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    Vector         target = *targetOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Check if reached
    float distance = (target - player->origin).length();
    if (distance < IdleConstants::WANDER_REACHED_DISTANCE) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, true);
        blackboard.Set<float>(BlackboardKeys::LAST_WANDER_TIME, level.svsTime);
        return BTNode::Status::SUCCESS;
    }

    // Check timeout
    float startTime = blackboard.GetOrDefault<float>(BlackboardKeys::WANDER_START_TIME, 0.0f);
    if (level.svsTime - startTime > IdleConstants::WANDER_TIMEOUT) {
        return BTNode::Status::FAILURE;
    }

    // Move to target (slow wander speed)
    bot->GetMovement().MoveTo(target, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_PauseAfterWander_Execute(Blackboard &blackboard, float deltaTime)
{
    float pauseTimer = blackboard.GetOrDefault<float>(BlackboardKeys::WANDER_PAUSE_TIMER, 0.0f);

    // Initialize pause duration if starting
    static float pauseDuration = IdleConstants::WANDER_PAUSE_MIN + (G_Random() * IdleConstants::WANDER_PAUSE_RANDOM);

    pauseTimer += deltaTime;

    if (pauseTimer >= pauseDuration) {
        // Pause complete
        blackboard.Set<float>(BlackboardKeys::WANDER_PAUSE_TIMER, 0.0f);
        pauseDuration = IdleConstants::WANDER_PAUSE_MIN + (G_Random() * IdleConstants::WANDER_PAUSE_RANDOM);
        return BTNode::Status::SUCCESS;
    }

    blackboard.Set<float>(BlackboardKeys::WANDER_PAUSE_TIMER, pauseTimer);
    return BTNode::Status::RUNNING;
}

BTNode::Status Action_ClearWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    blackboard.Set<Vector>(BlackboardKeys::WANDER_TARGET, vec_zero);
    blackboard.Set<bool>(BlackboardKeys::REACHED_WANDER_TARGET, false);
    blackboard.Set<float>(BlackboardKeys::WANDER_PAUSE_TIMER, 0.0f);

    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Attractive Node & Idle Actions
// ============================================================================

BTNode::Status Action_MoveToAttractiveNode_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get or find attractive node
    PathNode *attractiveNode = blackboard.GetOrDefault<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE, nullptr);

    if (!attractiveNode) {
        // Find new attractive node
        attractiveNode = FindNearbyAttractiveNode(player->origin);
        if (!attractiveNode) {
            return BTNode::Status::FAILURE;
        }
        blackboard.Set<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE, attractiveNode);
    }

    // Check if reached
    float distance = (attractiveNode->origin - player->origin).length();
    if (distance < IdleConstants::ATTRACTIVE_NODE_REACHED) {
        blackboard.Set<bool>(BlackboardKeys::REACHED_ATTRACTIVE_NODE, true);
        return BTNode::Status::SUCCESS;
    }

    // Move to node
    bot->GetMovement().MoveTo(attractiveNode->origin, nullptr, 0.0f);

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_UseAttractiveNode_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto nodeOpt   = blackboard.TryGet<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE);

    if (!botOpt || !playerOpt || !nodeOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;
    PathNode      *node   = *nodeOpt;

    if (!bot || !player || !node) {
        return BTNode::Status::FAILURE;
    }

    // Get or initialize use timer
    float useTimer = blackboard.GetOrDefault<float>(BlackboardKeys::ATTRACTIVE_NODE_TIMER, 0.0f);

    // Initialize use duration if starting
    static float useDuration = IdleConstants::ATTRACTIVE_NODE_USE_MIN
                              + (G_Random() * IdleConstants::ATTRACTIVE_NODE_USE_RANDOM);

    useTimer += deltaTime;

    // Look around slowly while using node
    Vector currentAngles = player->GetViewAngles();
    float  yawOffset     = IdleConstants::ATTRACTIVE_NODE_LOOK_SPEED * deltaTime * 60.0f; // degrees per second
    Vector newAngles     = currentAngles + Vector(0, yawOffset, 0);
    bot->GetRotation().SetTargetAngles(newAngles);

    if (useTimer >= useDuration) {
        // Done using node
        blackboard.Set<PathNode *>(BlackboardKeys::ATTRACTIVE_NODE, nullptr);
        blackboard.Set<bool>(BlackboardKeys::REACHED_ATTRACTIVE_NODE, false);
        blackboard.Set<float>(BlackboardKeys::ATTRACTIVE_NODE_TIMER, 0.0f);
        useDuration = IdleConstants::ATTRACTIVE_NODE_USE_MIN + (G_Random() * IdleConstants::ATTRACTIVE_NODE_USE_RANDOM);
        return BTNode::Status::SUCCESS;
    }

    blackboard.Set<float>(BlackboardKeys::ATTRACTIVE_NODE_TIMER, useTimer);
    return BTNode::Status::RUNNING;
}

BTNode::Status Action_StandInPlace_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);

    if (!botOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot = *botOpt;

    if (!bot) {
        return BTNode::Status::FAILURE;
    }

    // Stop all movement
    bot->GetMovement().ClearMove();

    return BTNode::Status::SUCCESS;
}

BTNode::Status Action_OccasionalLookAround_Execute(Blackboard &blackboard, float deltaTime)
{
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);

    if (!botOpt || !playerOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot    = *botOpt;
    Player        *player = *playerOpt;

    if (!bot || !player) {
        return BTNode::Status::FAILURE;
    }

    // Get or initialize look timer
    float lookTimer = blackboard.GetOrDefault<float>(BlackboardKeys::IDLE_LOOK_TIMER, 0.0f);

    lookTimer += deltaTime;

    // Look around every 3-6 seconds
    float lookInterval = IdleConstants::IDLE_LOOK_MIN + (G_Random() * IdleConstants::IDLE_LOOK_RANDOM);

    if (lookTimer >= lookInterval) {
        // Pick random look direction
        float  lookAngle  = G_Random() * 360.0f;
        Vector newAngles  = Vector(0, lookAngle, 0);

        bot->GetRotation().SetTargetAngles(newAngles);

        lookTimer = 0.0f;
    }

    blackboard.Set<float>(BlackboardKeys::IDLE_LOOK_TIMER, lookTimer);

    return BTNode::Status::SUCCESS;
}

// ============================================================================
// Registration Function
// ============================================================================

void RegisterIdleActions()
{
    auto &registry = BTActionRegistry::Instance();

    // Curious behavior
    registry.RegisterAction("SetCuriousTarget", Action_SetCuriousTarget_Execute,
        BTNodeMetadata("SetCuriousTarget", "Sets target for curious investigation",
            {"perception"}, "Idle"));

    registry.RegisterAction("MoveToCuriousLocation", Action_MoveToCuriousLocation_Execute,
        BTNodeMetadata("MoveToCuriousLocation", "Moves to curious location",
            {"bot", "player", "curiousTarget"}, "Idle"));

    registry.RegisterAction("QuickLookAround", Action_QuickLookAround_Execute,
        BTNodeMetadata("QuickLookAround", "Briefly looks around at curious location",
            {"bot", "player"}, "Idle"));

    registry.RegisterAction("ClearCuriousState", Action_ClearCuriousState_Execute,
        BTNodeMetadata("ClearCuriousState", "Clears curious investigation state",
            {}, "Idle"));

    // Patrol system
    registry.RegisterAction("MoveToNextPatrolWaypoint", Action_MoveToNextPatrolWaypoint_Execute,
        BTNodeMetadata("MoveToNextPatrolWaypoint", "Moves to next patrol waypoint",
            {"bot", "player", "patrolRoute"}, "Idle"));

    registry.RegisterAction("PauseAtWaypoint", Action_PauseAtWaypoint_Execute,
        BTNodeMetadata("PauseAtWaypoint", "Pauses at patrol waypoint",
            {}, "Idle"));

    registry.RegisterAction("AdvanceToNextWaypoint", Action_AdvanceToNextWaypoint_Execute,
        BTNodeMetadata("AdvanceToNextWaypoint", "Advances to next patrol waypoint",
            {"patrolRoute"}, "Idle"));

    registry.RegisterAction("ContinuePatrol", Action_ContinuePatrol_Execute,
        BTNodeMetadata("ContinuePatrol", "Continues patrol",
            {}, "Idle"));

    // Wander system
    registry.RegisterAction("PickRandomWanderTarget", Action_PickRandomWanderTarget_Execute,
        BTNodeMetadata("PickRandomWanderTarget", "Picks random wander target",
            {"player"}, "Idle"));

    registry.RegisterAction("MoveToWanderTarget", Action_MoveToWanderTarget_Execute,
        BTNodeMetadata("MoveToWanderTarget", "Moves to wander target",
            {"bot", "player", "wanderTarget"}, "Idle"));

    registry.RegisterAction("PauseAfterWander", Action_PauseAfterWander_Execute,
        BTNodeMetadata("PauseAfterWander", "Pauses after reaching wander target",
            {}, "Idle"));

    registry.RegisterAction("ClearWanderTarget", Action_ClearWanderTarget_Execute,
        BTNodeMetadata("ClearWanderTarget", "Clears wander state",
            {}, "Idle"));

    // Attractive node & idle
    registry.RegisterAction("MoveToAttractiveNode", Action_MoveToAttractiveNode_Execute,
        BTNodeMetadata("MoveToAttractiveNode", "Moves to attractive node",
            {"bot", "player"}, "Idle"));

    registry.RegisterAction("UseAttractiveNode", Action_UseAttractiveNode_Execute,
        BTNodeMetadata("UseAttractiveNode", "Uses attractive node",
            {"bot", "player", "attractiveNode"}, "Idle"));

    registry.RegisterAction("StandInPlace", Action_StandInPlace_Execute,
        BTNodeMetadata("StandInPlace", "Stops all movement",
            {"bot"}, "Idle"));

    registry.RegisterAction("OccasionalLookAround", Action_OccasionalLookAround_Execute,
        BTNodeMetadata("OccasionalLookAround", "Occasionally looks around",
            {"bot", "player"}, "Idle"));
}
