// Added in OPM - Phase 2B Task 2B.1
// bot_behaviors.cpp: Behavior tree definitions for bot AI

#include "behavior_tree.h"
#include "behavior_tree_builder.h"
#include "perception.h"
#include "playerbot.h"

// Named constants for behavior tree logic
namespace BTConstants
{
    constexpr float COMBAT_ENGAGEMENT_RANGE = 1024.0f; // Maximum engagement distance for shooting
}

/**
 * Create a simple engage enemy behavior tree.
 * This is a proof-of-concept before YAML loading.
 * Demonstrates integration with perception system from Phase 2A.
 */
std::unique_ptr<BehaviorTree> CreateEngageEnemyTree()
{
    return BehaviorTreeBuilder()
        .Selector()
        // Has enemy? Attack
        .Sequence()
        .Condition("HasVisibleEnemy",
                   [](Blackboard &bb) {
                       auto snapshot = bb.Get<PerceptionSnapshot *>("perception");
                       return snapshot->HasVisibleEnemy();
                   })
        .Action("AimAtEnemy",
                [](Blackboard &bb, float dt) {
                    auto               bot      = bb.Get<BotController *>("bot");
                    auto               snapshot = bb.Get<PerceptionSnapshot *>("perception");
                    const EnemyInfo *closestEnemy = snapshot->GetClosestEnemy();

                    if (closestEnemy && closestEnemy->entity) {
                        bot->GetRotation().AimAt(closestEnemy->position);
                        return BTNode::Status::SUCCESS;
                    }
                    return BTNode::Status::FAILURE;
                })
        .Action("ShootEnemy",
                [](Blackboard &bb, float dt) {
                    auto               bot      = bb.Get<BotController *>("bot");
                    auto               snapshot = bb.Get<PerceptionSnapshot *>("perception");
                    const EnemyInfo *closestEnemy = snapshot->GetClosestEnemy();

                    if (closestEnemy && closestEnemy->entity
                        && closestEnemy->distance < BTConstants::COMBAT_ENGAGEMENT_RANGE) {
                        // Note: Actual firing mechanism will be integrated in Task 2B.4
                        // For now, this is a placeholder demonstrating the concept
                        return BTNode::Status::SUCCESS;
                    }
                    return BTNode::Status::FAILURE;
                })
        .End()

        // No enemy? Idle
        .Action("Idle",
                [](Blackboard &bb, float dt) {
                    // Just stand still for now
                    return BTNode::Status::SUCCESS;
                })
        .End()
        .Build();
}
