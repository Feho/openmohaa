#include "test_utilities.h"
#include <gtest/gtest.h>

// Mock structures for testing bot controller logic

// Mock entity flags
constexpr int FL_NOTARGET = (1 << 0);
constexpr int SOLID_NOT   = 0;
constexpr int SOLID_BBOX  = 1;

// Mock game types
constexpr int GT_FFA     = 1;
constexpr int GT_TEAM_DM = 3;
constexpr int GT_TEAM    = 3; // Any team-based mode

// Mock sentient struct for testing
struct MockSentient {
    bool  isPlayer;
    bool  isDead;
    bool  isHidden;
    int   flags;
    int   solidType;
    int   team;
    float health;

    MockSentient()
        : isPlayer(true)
        , isDead(false)
        , isHidden(false)
        , flags(0)
        , solidType(SOLID_BBOX)
        , team(1)
        , health(100.0f)
    {
    }
};

// Extracted logic from BotController::IsValidEnemy for testing
bool IsValidEnemy(const MockSentient *sent, const MockSentient *self, int gameType)
{
    if (sent == self) {
        return false;
    }

    if (sent->isHidden || (sent->flags & FL_NOTARGET)) {
        return false;
    }

    if (sent->isDead) {
        return false;
    }

    if (sent->solidType == SOLID_NOT) {
        return false;
    }

    // Team check for team-based modes
    if (gameType >= GT_TEAM) {
        if (sent->team == self->team) {
            return false;
        }
    }

    return true;
}

// Test fixture for bot controller tests
class BotControllerTest : public BotTestBase
{
protected:
    MockSentient bot;
    MockSentient target;

    void SetUp() override
    {
        BotTestBase::SetUp();
        // Bot is team 1 by default
        bot.team  = 1;
        bot.flags = 0;
        // Target is team 2 by default (enemy)
        target.team  = 2;
        target.flags = 0;
    }
};

// IsValidEnemy tests - basic cases
TEST_F(BotControllerTest, IsValidEnemy_ValidTarget)
{
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_SelfIsNotValid)
{
    EXPECT_FALSE(IsValidEnemy(&bot, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_HiddenTarget)
{
    target.isHidden = true;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_NoTargetFlag)
{
    target.flags = FL_NOTARGET;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_DeadTarget)
{
    target.isDead = true;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_NonSolidTarget)
{
    target.solidType = SOLID_NOT;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_FFA));
}

// Team-based game mode tests
TEST_F(BotControllerTest, IsValidEnemy_TeamMode_DifferentTeams)
{
    bot.team    = 1;
    target.team = 2;
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_TEAM_DM));
}

TEST_F(BotControllerTest, IsValidEnemy_TeamMode_SameTeam)
{
    bot.team    = 1;
    target.team = 1;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_TEAM_DM));
}

TEST_F(BotControllerTest, IsValidEnemy_FFAMode_IgnoresTeam)
{
    bot.team    = 1;
    target.team = 1;
    // In FFA, same team should still be valid enemy
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_FFA));
}

// Combined condition tests
TEST_F(BotControllerTest, IsValidEnemy_DeadAndHidden)
{
    target.isDead   = true;
    target.isHidden = true;
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_ValidButSameTeam)
{
    bot.team    = 1;
    target.team = 1;
    // Should be invalid in team mode
    EXPECT_FALSE(IsValidEnemy(&target, &bot, GT_TEAM_DM));
}

TEST_F(BotControllerTest, IsValidEnemy_AllConditionsValid)
{
    // Explicitly set all conditions to valid state
    bot.team         = 1;
    target.team      = 2;
    target.isHidden  = false;
    target.isDead    = false;
    target.flags     = 0;
    target.solidType = SOLID_BBOX;
    target.health    = 100.0f;

    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_TEAM_DM));
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_FFA));
}

// Edge cases
TEST_F(BotControllerTest, IsValidEnemy_ZeroHealth_ButNotDead)
{
    // Target has 0 health but isDead flag not set (edge case)
    target.health = 0.0f;
    target.isDead = false;
    // Should still be valid since we check isDead flag, not health
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_FFA));
}

TEST_F(BotControllerTest, IsValidEnemy_NegativeHealth)
{
    target.health = -10.0f;
    target.isDead = false;
    // Again, depends on flag not health value
    EXPECT_TRUE(IsValidEnemy(&target, &bot, GT_FFA));
}

// Vector distance calculations for target selection
TEST_F(BotControllerTest, DistanceCalculation_Simple)
{
    TestVector pos1(0.0f, 0.0f, 0.0f);
    TestVector pos2(3.0f, 4.0f, 0.0f);
    float      distSq = (pos2 - pos1).lengthSquared();

    EXPECT_TRUE(FloatEquals(distSq, 25.0f)); // 3^2 + 4^2 = 25
}

TEST_F(BotControllerTest, DistanceCalculation_3D)
{
    TestVector pos1(0.0f, 0.0f, 0.0f);
    TestVector pos2(1.0f, 2.0f, 2.0f);
    float      distSq = (pos2 - pos1).lengthSquared();

    EXPECT_TRUE(FloatEquals(distSq, 9.0f)); // 1^2 + 2^2 + 2^2 = 9
}

TEST_F(BotControllerTest, TargetCloserCheck)
{
    TestVector botPos(0.0f, 0.0f, 0.0f);
    TestVector target1Pos(10.0f, 0.0f, 0.0f);
    TestVector target2Pos(5.0f, 0.0f, 0.0f);

    float dist1Sq = (target1Pos - botPos).lengthSquared();
    float dist2Sq = (target2Pos - botPos).lengthSquared();

    EXPECT_TRUE(dist2Sq < dist1Sq); // target2 is closer
}

// Target switch threshold tests
TEST_F(BotControllerTest, TargetSwitchThreshold_SignificantlyCloser)
{
    float currentTargetDistSq = 100.0f;  // 10 units away
    float newTargetDistSq     = 25.0f;   // 5 units away
    float switchThreshold     = 200.0f;  // Config value

    float distAdvantage      = currentTargetDistSq - newTargetDistSq;
    float switchThresholdSq  = switchThreshold * switchThreshold;
    bool  shouldSwitch       = distAdvantage > switchThresholdSq;

    // Distance advantage is 75, but threshold is 40000, so should NOT switch
    EXPECT_FALSE(shouldSwitch);
}

TEST_F(BotControllerTest, TargetSwitchThreshold_VeryClose)
{
    float currentTargetDistSq = 10000.0f; // 100 units away
    float newTargetDistSq     = 25.0f;    // 5 units away
    float switchThreshold     = 50.0f;    // Config value

    float distAdvantage     = currentTargetDistSq - newTargetDistSq;
    float switchThresholdSq = switchThreshold * switchThreshold;
    bool  shouldSwitch      = distAdvantage > switchThresholdSq;

    // Distance advantage is 9975, threshold squared is 2500, so SHOULD switch
    EXPECT_TRUE(shouldSwitch);
}
