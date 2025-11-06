// bt_conditions_idle.h
// Behavior tree conditions for idle behavior system
// Added in OPM - Phase 3 Task 3.3

#ifndef __BT_CONDITIONS_IDLE_H__
#define __BT_CONDITIONS_IDLE_H__

#include "behavior_tree.h"

// Curious Investigation Conditions
bool Condition_HasCuriousSound_Check(Blackboard &blackboard);
bool Condition_ReachedCuriousLocation_Check(Blackboard &blackboard);

// Patrol System Conditions
bool Condition_HasPatrolRoute_Check(Blackboard &blackboard);
bool Condition_ReachedPatrolWaypoint_Check(Blackboard &blackboard);

// Wander System Conditions
bool Condition_ShouldWander_Check(Blackboard &blackboard);
bool Condition_ReachedWanderTarget_Check(Blackboard &blackboard);

// Attractive Node Conditions
bool Condition_HasNearbyAttractiveNode_Check(Blackboard &blackboard);
bool Condition_ReachedAttractiveNode_Check(Blackboard &blackboard);

#endif // __BT_CONDITIONS_IDLE_H__
