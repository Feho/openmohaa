// bt_actions_idle.cpp
// Behavior tree actions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#include "bt_actions_idle.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "perception.h"
#include "idle_helpers.h"
#include "g_local.h"

// Curious Investigation Actions

BTNode::Status Action_SetCuriousTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 2
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_MoveToCuriousLocation_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 2
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_QuickLookAround_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 2
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_ClearCuriousState_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 2
    return BTNode::Status::FAILURE;
}

// Patrol System Actions

BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_ContinuePatrol_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

// Wander System Actions

BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_PauseAfterWander_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_ClearWanderTarget_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 3
    return BTNode::Status::FAILURE;
}

// Attractive Node Actions

BTNode::Status Action_MoveToAttractiveNode_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_UseAttractiveNode_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

// Idle Standing Actions

BTNode::Status Action_StandInPlace_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}

BTNode::Status Action_OccasionalLookAround_Execute(Blackboard &blackboard, float deltaTime)
{
    // To be implemented in Commit 4
    return BTNode::Status::FAILURE;
}
