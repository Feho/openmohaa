// bt_actions_idle.h
// Behavior tree actions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#ifndef __BT_ACTIONS_IDLE_H__
#define __BT_ACTIONS_IDLE_H__

#include "behavior_tree.h"

// Curious Investigation Actions
BTNode::Status Action_SetCuriousTarget_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_MoveToCuriousLocation_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_QuickLookAround_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_ClearCuriousState_Execute(Blackboard &blackboard, float deltaTime);

// Patrol System Actions
BTNode::Status Action_MoveToNextPatrolWaypoint_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_PauseAtWaypoint_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_AdvanceToNextWaypoint_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_ContinuePatrol_Execute(Blackboard &blackboard, float deltaTime);

// Wander System Actions
BTNode::Status Action_PickRandomWanderTarget_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_MoveToWanderTarget_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_PauseAfterWander_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_ClearWanderTarget_Execute(Blackboard &blackboard, float deltaTime);

// Attractive Node Actions
BTNode::Status Action_MoveToAttractiveNode_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_UseAttractiveNode_Execute(Blackboard &blackboard, float deltaTime);

// Idle Standing Actions
BTNode::Status Action_StandInPlace_Execute(Blackboard &blackboard, float deltaTime);
BTNode::Status Action_OccasionalLookAround_Execute(Blackboard &blackboard, float deltaTime);

#endif // __BT_ACTIONS_IDLE_H__
