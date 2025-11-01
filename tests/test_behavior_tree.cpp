// Added in OPM - Phase 2B Task 2B.1
// test_behavior_tree.cpp: Unit tests for behavior tree system

#define BEHAVIOR_TREE_TESTING
#include "behavior_tree.h"
#include "behavior_tree_builder.h"
#include <gtest/gtest.h>

// Test 1: Selector returns first success
TEST(BehaviorTreeTest, SelectorReturnsFirstSuccess)
{
    int thirdActionCalled = 0;

    auto tree = BehaviorTreeBuilder()
                    .Selector()
                    .Action("Fail1", [](auto &bb, float dt) { return BTNode::Status::FAILURE; })
                    .Action("Success", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .Action("NotReached",
                            [&thirdActionCalled](auto &bb, float dt) {
                                thirdActionCalled++;
                                return BTNode::Status::SUCCESS;
                            })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
    EXPECT_EQ(thirdActionCalled, 0); // Should not have been called
}

// Test 2: Selector returns failure if all fail
TEST(BehaviorTreeTest, SelectorReturnsFailureIfAllFail)
{
    auto tree = BehaviorTreeBuilder()
                    .Selector()
                    .Action("Fail1", [](auto &bb, float dt) { return BTNode::Status::FAILURE; })
                    .Action("Fail2", [](auto &bb, float dt) { return BTNode::Status::FAILURE; })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);
}

// Test 3: Sequence returns first failure
TEST(BehaviorTreeTest, SequenceReturnsFirstFailure)
{
    int thirdActionCalled = 0;

    auto tree = BehaviorTreeBuilder()
                    .Sequence()
                    .Action("Success1", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .Action("Failure", [](auto &bb, float dt) { return BTNode::Status::FAILURE; })
                    .Action("NotReached",
                            [&thirdActionCalled](auto &bb, float dt) {
                                thirdActionCalled++;
                                return BTNode::Status::SUCCESS;
                            })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);
    EXPECT_EQ(thirdActionCalled, 0); // Should not have been called
}

// Test 4: Sequence returns success if all succeed
TEST(BehaviorTreeTest, SequenceReturnsSuccessIfAllSucceed)
{
    auto tree = BehaviorTreeBuilder()
                    .Sequence()
                    .Action("Success1", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .Action("Success2", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 5: Parallel RequireAll policy
TEST(BehaviorTreeTest, ParallelRequireAllPolicy)
{
    auto tree = BehaviorTreeBuilder()
                    .Parallel(BTParallel::Policy::RequireAll)
                    .Action("Success1", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .Action("Success2", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 6: Parallel RequireOne policy
TEST(BehaviorTreeTest, ParallelRequireOnePolicy)
{
    auto tree = BehaviorTreeBuilder()
                    .Parallel(BTParallel::Policy::RequireOne)
                    .Action("Failure", [](auto &bb, float dt) { return BTNode::Status::FAILURE; })
                    .Action("Success", [](auto &bb, float dt) { return BTNode::Status::SUCCESS; })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 7: Condition evaluates correctly
TEST(BehaviorTreeTest, ConditionEvaluates)
{
    // Wrap condition in a selector to make valid tree
    auto tree = BehaviorTreeBuilder()
                    .Selector()
                    .Condition("TestCondition",
                               [](Blackboard &bb) { return bb.GetOrDefault<int>("value", 0) > 10; })
                    .End()
                    .Build();

    Blackboard bb;
    bb.Set<int>("value", 5);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::FAILURE);

    bb.Set<int>("value", 15);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::SUCCESS);
}

// Test 8: Blackboard Get/Set
TEST(BehaviorTreeTest, BlackboardGetSet)
{
    Blackboard bb;

    bb.Set<int>("health", 100);
    bb.Set<float>("speed", 1.5f);
    bb.Set<std::string>("name", "Bot1");

    EXPECT_EQ(bb.Get<int>("health"), 100);
    EXPECT_FLOAT_EQ(bb.Get<float>("speed"), 1.5f);
    EXPECT_EQ(bb.Get<std::string>("name"), "Bot1");
}

// Test 9: Blackboard Has
TEST(BehaviorTreeTest, BlackboardHas)
{
    Blackboard bb;

    bb.Set<int>("value", 42);

    EXPECT_TRUE(bb.Has("value"));
    EXPECT_FALSE(bb.Has("nonexistent"));
}

// Test 10: Blackboard Remove
TEST(BehaviorTreeTest, BlackboardRemove)
{
    Blackboard bb;

    bb.Set<int>("value", 42);
    EXPECT_TRUE(bb.Has("value"));

    bb.Remove("value");
    EXPECT_FALSE(bb.Has("value"));
}

// Test 11: RUNNING status pauses execution
TEST(BehaviorTreeTest, RunningStatusPausesExecution)
{
    int executionCount     = 0;
    int secondActionCalled = 0;

    auto tree = BehaviorTreeBuilder()
                    .Sequence()
                    .Action("RunningAction",
                            [&executionCount](auto &bb, float dt) {
                                executionCount++;
                                return BTNode::Status::RUNNING;
                            })
                    .Action("NeverReached",
                            [&secondActionCalled](auto &bb, float dt) {
                                secondActionCalled++;
                                return BTNode::Status::SUCCESS;
                            })
                    .End()
                    .Build();

    Blackboard bb;
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(executionCount, 2);
    EXPECT_EQ(secondActionCalled, 0); // Should not have been called
}

// Test 12: Integration test - Decision tree with perception-like data
TEST(BehaviorTreeIntegrationTest, DecisionTreeWithPerceptionData)
{
    // Mock perception data using blackboard
    struct MockPerception {
        bool  hasEnemy;
        float enemyDistance;
    };

    // Track which actions were executed
    bool aimCalled  = false;
    bool fireCalled = false;
    bool idleCalled = false;

    // Create decision tree similar to engage enemy tree
    auto tree = BehaviorTreeBuilder()
                    .Selector()
                    // Branch 1: Has enemy? Attack
                    .Sequence()
                    .Condition("HasEnemy", [](Blackboard &bb) { return bb.Get<MockPerception>("perception").hasEnemy; })
                    .Action("Aim",
                            [&aimCalled](Blackboard &bb, float dt) {
                                (void)bb; // Unused in this test action
                                aimCalled = true;
                                return BTNode::Status::SUCCESS;
                            })
                    .Action("Fire",
                            [&fireCalled](Blackboard &bb, float dt) {
                                auto perception = bb.Get<MockPerception>("perception");
                                if (perception.enemyDistance < 1024.0f) {
                                    fireCalled = true;
                                    return BTNode::Status::SUCCESS;
                                }
                                return BTNode::Status::FAILURE;
                            })
                    .End()
                    // Branch 2: No enemy? Idle
                    .Action("Idle",
                            [&idleCalled](Blackboard &bb, float dt) {
                                idleCalled = true;
                                return BTNode::Status::SUCCESS;
                            })
                    .End()
                    .Build();

    // Test 1: With enemy in range
    {
        Blackboard     bb;
        MockPerception perception = {true, 500.0f};
        bb.Set<MockPerception>("perception", perception);

        auto status = tree->Execute(bb, 0.1f);

        EXPECT_EQ(status, BTNode::Status::SUCCESS);
        EXPECT_TRUE(aimCalled);
        EXPECT_TRUE(fireCalled);
        EXPECT_FALSE(idleCalled);
    }

    // Reset flags
    aimCalled  = false;
    fireCalled = false;
    idleCalled = false;

    // Test 2: No enemy - should idle
    {
        Blackboard     bb;
        MockPerception perception = {false, 0.0f};
        bb.Set<MockPerception>("perception", perception);

        tree->Reset(); // Reset tree state
        auto status = tree->Execute(bb, 0.1f);

        EXPECT_EQ(status, BTNode::Status::SUCCESS);
        EXPECT_FALSE(aimCalled);
        EXPECT_FALSE(fireCalled);
        EXPECT_TRUE(idleCalled);
    }
}

// Test 13: Parallel RequireN policy
TEST(BehaviorTreeTest, ParallelRequireNPolicy)
{
    int action1Called = 0;
    int action2Called = 0;
    int action3Called = 0;

    auto tree = BehaviorTreeBuilder()
                    .Parallel(BTParallel::Policy::RequireN, 2) // Need 2 successes
                    .Action("Action1",
                            [&action1Called](auto &bb, float dt) {
                                action1Called++;
                                return BTNode::Status::SUCCESS;
                            })
                    .Action("Action2",
                            [&action2Called](auto &bb, float dt) {
                                action2Called++;
                                return BTNode::Status::SUCCESS;
                            })
                    .Action("Action3",
                            [&action3Called](auto &bb, float dt) {
                                action3Called++;
                                return BTNode::Status::FAILURE;
                            })
                    .End()
                    .Build();

    Blackboard bb;
    auto       status = tree->Execute(bb, 0.1f);

    // All children execute in parallel
    EXPECT_EQ(action1Called, 1);
    EXPECT_EQ(action2Called, 1);
    EXPECT_EQ(action3Called, 1);

    // Should succeed (2 out of 3 succeeded)
    EXPECT_EQ(status, BTNode::Status::SUCCESS);
}

// Test 14: Reset on RUNNING tree
TEST(BehaviorTreeTest, ResetOnRunningTree)
{
    int executionCount = 0;

    auto tree = BehaviorTreeBuilder()
                    .Sequence()
                    .Action("RunningAction",
                            [&executionCount](auto &bb, float dt) {
                                executionCount++;
                                if (executionCount < 3) {
                                    return BTNode::Status::RUNNING;
                                }
                                return BTNode::Status::SUCCESS;
                            })
                    .End()
                    .Build();

    Blackboard bb;

    // Execute twice - should be RUNNING
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(executionCount, 2);

    // Reset the tree
    tree->Reset();

    // Execute again - should start from beginning
    executionCount = 0; // Simulate reset of external state
    EXPECT_EQ(tree->Execute(bb, 0.1f), BTNode::Status::RUNNING);
    EXPECT_EQ(executionCount, 1); // Started over
}

// Test 15: Blackboard TryGet with type mismatch
TEST(BehaviorTreeTest, BlackboardTryGetTypeMismatch)
{
    Blackboard bb;

    bb.Set<int>("value", 42);

    // Try to get as wrong type - should return nullopt
    auto floatResult = bb.TryGet<float>("value");
    EXPECT_FALSE(floatResult.has_value());

    // Try to get as correct type - should succeed
    auto intResult = bb.TryGet<int>("value");
    EXPECT_TRUE(intResult.has_value());
    EXPECT_EQ(*intResult, 42);
}

// Test 16: Blackboard TryGet with missing key
TEST(BehaviorTreeTest, BlackboardTryGetMissingKey)
{
    Blackboard bb;

    bb.Set<int>("value", 42);

    // Try to get non-existent key
    auto result = bb.TryGet<int>("nonexistent");
    EXPECT_FALSE(result.has_value());

    // GetOrDefault should work
    int defaultValue = bb.GetOrDefault<int>("nonexistent", 99);
    EXPECT_EQ(defaultValue, 99);
}
