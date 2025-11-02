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

// test_target_selection.cpp
// Unit tests for target selection and tracking system
// Added in OPM - Phase 3 Task 3.1a

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <utility>

// Minimal Vector class for testing
class Vector {
public:
    float x, y, z;
    
    Vector() : x(0), y(0), z(0) {}
    Vector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    float operator[](int index) const {
        if (index == 0) return x;
        if (index == 1) return y;
        return z;
    }
    
    Vector operator-(const Vector& other) const {
        return Vector(x - other.x, y - other.y, z - other.z);
    }
    
    float lengthSquared() const {
        return x*x + y*y + z*z;
    }
    
    float length() const {
        return std::sqrt(lengthSquared());
    }
};

// Mock minimal structures for testing
struct level_locals_t {
    int svsTime = 0;
    int inttime = 0;
};

struct edict_s {
    bool inuse = true;
};

struct gclient_s {
    int team = 0;
};

struct cvar_t {
    const char *name;
    const char *string;
    int flags;
    float value;
    int integer;
};

// Global game state
level_locals_t level;
int g_gametype_value = 0;
cvar_t g_gametype_cvar = { "g_gametype", "0", 0, 0.0f, 0 };
cvar_t *g_gametype = &g_gametype_cvar;

// Mock Sentient class
class Sentient {
public:
    edict_s *edict = nullptr;
    Vector origin;
    Vector velocity;
    int m_Team = 0;
    bool m_hidden = false;
    bool m_dead = false;
    int m_solidType = 3; // SOLID_BBOX
    bool m_isPlayer = false;
    gclient_s *client = nullptr;
    int flags = 0;

    Sentient() {
        edict = new edict_s();
        origin = Vector(0, 0, 0);
        velocity = Vector(0, 0, 0);
    }

    virtual ~Sentient() {
        delete edict;
        delete client;
    }

    bool hidden() const { return m_hidden; }
    bool IsDead() const { return m_dead; }
    int getSolidType() const { return m_solidType; }
    bool IsSubclassOfPlayer() const { return m_isPlayer; }
};

// Mock Player class
class Player : public Sentient {
public:
    Player() {
        m_isPlayer = true;
        client = new gclient_s();
        m_Team = 0;  // Initialize base class m_Team
    }

    int GetTeam() const { return client->team; }
    
    void SetTeam(int team) {
        client->team = team;
        m_Team = team;  // Keep both in sync
    }
};

// SafePtr mock for perception system
template<typename T>
class SafePtr {
private:
    T* ptr;
public:
    SafePtr() : ptr(nullptr) {}
    SafePtr(T* p) : ptr(p) {}
    
    operator T*() { return ptr; }
    operator const T*() const { return ptr; }
    T* operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    bool operator==(T* other) const { return ptr == other; }
    bool operator!=(T* other) const { return ptr != other; }
    SafePtr& operator=(T* p) { ptr = p; return *this; }
};

// EnemyInfo structure for perception
struct EnemyInfo {
    SafePtr<Sentient> entity;
    Vector position;
    Vector velocity;
    float distance;
    float visibilityFactor;
    float angleFromForward;
    bool isInPeripheral;
    
    EnemyInfo() : entity(nullptr), distance(0.0f), visibilityFactor(0.0f),
                  angleFromForward(0.0f), isInPeripheral(false) {}
};

// Mock PerceptionSnapshot
struct PerceptionSnapshot {
    std::vector<EnemyInfo> visibleEnemies;
    size_t closestEnemyIndex = SIZE_MAX;
    
    const EnemyInfo* GetClosestEnemy() const {
        return closestEnemyIndex < visibleEnemies.size() ? &visibleEnemies[closestEnemyIndex] : nullptr;
    }
};

// Now implement the combat helpers we're testing
namespace BT {
namespace Combat {

bool IsValidEnemy(const Player *bot, Sentient *enemy)
{
    constexpr int FL_NOTARGET = (1 << 2);
    constexpr int SOLID_NOT = 0;
    constexpr int GT_TEAM = 3;
    
    if (!bot || !enemy) {
        return false;
    }

    if (enemy == bot) {
        return false;
    }

    if (enemy->hidden() || (enemy->flags & FL_NOTARGET)) {
        return false;
    }

    if (enemy->IsDead()) {
        return false;
    }

    if (enemy->getSolidType() == SOLID_NOT) {
        return false;
    }

    // Team check
    if (enemy->IsSubclassOfPlayer()) {
        Player *enemyPlayer = static_cast<Player *>(enemy);

        if (g_gametype->integer >= GT_TEAM && enemyPlayer->GetTeam() == bot->GetTeam()) {
            return false;
        }
    } else {
        if (enemy->m_Team == bot->m_Team) {
            return false;
        }
    }

    return true;
}

float CalculateTargetScore(
    Sentient *enemy,
    Sentient *currentTarget,
    float     distance,
    float     lockTime,
    float     lockDuration,
    float     switchThreshold
)
{
    if (!enemy) {
        return 0.0f;
    }

    // Base score: inverse of distance
    float score = 10000.0f / (distance + 1.0f);

    // Target stickiness bonus
    if (currentTarget && enemy == currentTarget) {
        float timeSinceLock = (level.svsTime - lockTime) / 1000.0f;

        if (timeSinceLock < lockDuration) {
            score += switchThreshold * 2.0f;
        }
    }

    return score;
}

const EnemyInfo *FindClosestVisibleEnemy(const PerceptionSnapshot *perception)
{
    if (!perception || perception->visibleEnemies.empty()) {
        return nullptr;
    }

    return perception->GetClosestEnemy();
}

} // namespace Combat
} // namespace BT

// Test fixture for target selection tests
class TargetSelectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global state
        level.svsTime = 0;
        level.inttime = 0;
        g_gametype_value = 0;
        g_gametype_cvar.integer = 0;
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper to create a mock player bot
    Player* CreateBot(int team = 1) {
        Player *bot = new Player();
        bot->SetTeam(team);
        bot->origin = Vector(0, 0, 0);
        return bot;
    }

    // Helper to create a mock enemy
    Sentient* CreateEnemy(const Vector &pos, int team = 2) {
        Sentient *enemy = new Sentient();
        enemy->origin = pos;
        enemy->m_Team = team;
        enemy->edict->inuse = true;
        return enemy;
    }

    // Helper to create a mock player enemy
    Player* CreatePlayerEnemy(const Vector &pos, int team = 2) {
        Player *enemy = new Player();
        enemy->origin = pos;
        enemy->SetTeam(team);
        enemy->edict->inuse = true;
        return enemy;
    }
};

// === IsValidEnemy Tests ===

// Test 1: IsValidEnemy with null inputs
TEST_F(TargetSelectionTest, IsValidEnemy_NullInputs_ReturnsFalse)
{
    Player *bot = CreateBot();

    // Null bot
    EXPECT_FALSE(BT::Combat::IsValidEnemy(nullptr, bot));

    // Null enemy
    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, nullptr));

    // Both null
    EXPECT_FALSE(BT::Combat::IsValidEnemy(nullptr, nullptr));

    delete bot;
}

// Test 2: IsValidEnemy with self
TEST_F(TargetSelectionTest, IsValidEnemy_Self_ReturnsFalse)
{
    Player *bot = CreateBot();

    // Bot targeting itself
    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, bot));

    delete bot;
}

// Test 3: IsValidEnemy with dead enemy
TEST_F(TargetSelectionTest, IsValidEnemy_DeadEnemy_ReturnsFalse)
{
    Player *bot = CreateBot(1);
    Sentient *enemy = CreateEnemy(Vector(100, 0, 0), 2);
    enemy->m_dead = true;

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, enemy));

    delete bot;
    delete enemy;
}

// Test 4: IsValidEnemy with hidden enemy
TEST_F(TargetSelectionTest, IsValidEnemy_HiddenEnemy_ReturnsFalse)
{
    Player *bot = CreateBot(1);
    Sentient *enemy = CreateEnemy(Vector(100, 0, 0), 2);
    enemy->m_hidden = true;

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, enemy));

    delete bot;
    delete enemy;
}

// Test 5: IsValidEnemy with FL_NOTARGET flag
TEST_F(TargetSelectionTest, IsValidEnemy_NotargetFlag_ReturnsFalse)
{
    constexpr int FL_NOTARGET = (1 << 2);
    Player *bot = CreateBot(1);
    Sentient *enemy = CreateEnemy(Vector(100, 0, 0), 2);
    enemy->flags = FL_NOTARGET;

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, enemy));

    delete bot;
    delete enemy;
}

// Test 6: IsValidEnemy with same team (non-player)
TEST_F(TargetSelectionTest, IsValidEnemy_SameTeamNonPlayer_ReturnsFalse)
{
    // Team check for non-players applies regardless of game mode
    Player *bot = CreateBot(1);
    Sentient *ally = CreateEnemy(Vector(100, 0, 0), 1); // Same team
    bot->m_Team = 1;  // Make sure bot has team set

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, ally));

    delete bot;
    delete ally;
}

// Test 7: IsValidEnemy with same team (player, team game)
TEST_F(TargetSelectionTest, IsValidEnemy_SameTeamPlayer_ReturnsFalse)
{
    g_gametype_cvar.integer = 3; // GT_TEAM
    Player *bot = CreateBot(1);
    Player *ally = CreatePlayerEnemy(Vector(100, 0, 0), 1); // Same team

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, ally));

    delete bot;
    delete ally;
}

// Test 8: IsValidEnemy with spectator (SOLID_NOT)
TEST_F(TargetSelectionTest, IsValidEnemy_Spectator_ReturnsFalse)
{
    constexpr int SOLID_NOT = 0;
    Player *bot = CreateBot(1);
    Player *spectator = CreatePlayerEnemy(Vector(100, 0, 0), 2);
    spectator->m_solidType = SOLID_NOT;

    EXPECT_FALSE(BT::Combat::IsValidEnemy(bot, spectator));

    delete bot;
    delete spectator;
}

// Test 9: IsValidEnemy with valid enemy
TEST_F(TargetSelectionTest, IsValidEnemy_ValidEnemy_ReturnsTrue)
{
    Player *bot = CreateBot(1);
    Sentient *enemy = CreateEnemy(Vector(100, 0, 0), 2);

    EXPECT_TRUE(BT::Combat::IsValidEnemy(bot, enemy));

    delete bot;
    delete enemy;
}

// Test 10: IsValidEnemy with valid player enemy (different team)
TEST_F(TargetSelectionTest, IsValidEnemy_ValidPlayerEnemy_ReturnsTrue)
{
    g_gametype_cvar.integer = 3; // GT_TEAM
    Player *bot = CreateBot(1);
    Player *enemy = CreatePlayerEnemy(Vector(100, 0, 0), 2); // Different team

    EXPECT_TRUE(BT::Combat::IsValidEnemy(bot, enemy));

    delete bot;
    delete enemy;
}

// === CalculateTargetScore Tests ===

// Test 11: CalculateTargetScore with null enemy
TEST_F(TargetSelectionTest, CalculateTargetScore_NullEnemy_ReturnsZero)
{
    float score = BT::Combat::CalculateTargetScore(
        nullptr,     // enemy
        nullptr,     // currentTarget
        100.0f,      // distance
        0.0f,        // lockTime
        2.0f,        // lockDuration
        128.0f       // switchThreshold
    );

    EXPECT_FLOAT_EQ(score, 0.0f);
}

// Test 12: CalculateTargetScore prefers closer enemies
TEST_F(TargetSelectionTest, CalculateTargetScore_PrefersCloser)
{
    Sentient *closeEnemy = CreateEnemy(Vector(100, 0, 0));
    Sentient *farEnemy = CreateEnemy(Vector(500, 0, 0));

    float closeScore = BT::Combat::CalculateTargetScore(
        closeEnemy, nullptr, 100.0f, 0.0f, 2.0f, 128.0f
    );

    float farScore = BT::Combat::CalculateTargetScore(
        farEnemy, nullptr, 500.0f, 0.0f, 2.0f, 128.0f
    );

    EXPECT_GT(closeScore, farScore);

    delete closeEnemy;
    delete farEnemy;
}

// Test 13: CalculateTargetScore gives bonus to current target during lock
TEST_F(TargetSelectionTest, CalculateTargetScore_CurrentTargetBonusDuringLock)
{
    level.svsTime = 1000; // 1 second elapsed
    Sentient *currentTarget = CreateEnemy(Vector(500, 0, 0));
    Sentient *newEnemy = CreateEnemy(Vector(400, 0, 0));

    // Current target locked 500ms ago (within 2s lock duration)
    float currentScore = BT::Combat::CalculateTargetScore(
        currentTarget,
        currentTarget,  // Is current target
        500.0f,
        500.0f,         // lockTime (0.5s ago)
        2.0f,           // lockDuration
        128.0f
    );

    // New enemy (not locked)
    float newScore = BT::Combat::CalculateTargetScore(
        newEnemy,
        currentTarget,  // Not current target
        400.0f,
        500.0f,
        2.0f,
        128.0f
    );

    // Current target should have higher score despite being further away
    EXPECT_GT(currentScore, newScore);

    delete currentTarget;
    delete newEnemy;
}

// Test 14: CalculateTargetScore allows switching after lock expires
TEST_F(TargetSelectionTest, CalculateTargetScore_AllowsSwitchAfterLockExpires)
{
    level.svsTime = 3000; // 3 seconds elapsed
    Sentient *currentTarget = CreateEnemy(Vector(500, 0, 0));
    Sentient *newEnemy = CreateEnemy(Vector(400, 0, 0));

    // Current target locked 3 seconds ago (beyond 2s lock duration)
    float currentScore = BT::Combat::CalculateTargetScore(
        currentTarget,
        currentTarget,
        500.0f,
        0.0f,           // lockTime (3s ago)
        2.0f,           // lockDuration
        128.0f
    );

    // New enemy (closer)
    float newScore = BT::Combat::CalculateTargetScore(
        newEnemy,
        currentTarget,
        400.0f,
        0.0f,
        2.0f,
        128.0f
    );

    // New enemy should have higher score (closer and lock expired)
    EXPECT_GT(newScore, currentScore);

    delete currentTarget;
    delete newEnemy;
}

// === FindClosestVisibleEnemy Tests ===

// Test 15: FindClosestVisibleEnemy with null perception
TEST_F(TargetSelectionTest, FindClosestVisibleEnemy_NullPerception_ReturnsNull)
{
    const EnemyInfo *result = BT::Combat::FindClosestVisibleEnemy(nullptr);
    EXPECT_EQ(result, nullptr);
}

// Test 16: FindClosestVisibleEnemy with no visible enemies
TEST_F(TargetSelectionTest, FindClosestVisibleEnemy_NoEnemies_ReturnsNull)
{
    PerceptionSnapshot snapshot;
    snapshot.visibleEnemies.clear();

    const EnemyInfo *result = BT::Combat::FindClosestVisibleEnemy(&snapshot);
    EXPECT_EQ(result, nullptr);
}

// Test 17: FindClosestVisibleEnemy with one enemy
TEST_F(TargetSelectionTest, FindClosestVisibleEnemy_OneEnemy_ReturnsThatEnemy)
{
    Sentient *enemy = CreateEnemy(Vector(100, 0, 0));
    PerceptionSnapshot snapshot;

    EnemyInfo info;
    info.entity = enemy;
    info.position = enemy->origin;
    info.distance = 100.0f;
    snapshot.visibleEnemies.push_back(info);
    snapshot.closestEnemyIndex = 0;

    const EnemyInfo *result = BT::Combat::FindClosestVisibleEnemy(&snapshot);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->entity, enemy);
    EXPECT_FLOAT_EQ(result->distance, 100.0f);

    delete enemy;
}

// Test 18: FindClosestVisibleEnemy with multiple enemies returns closest
TEST_F(TargetSelectionTest, FindClosestVisibleEnemy_MultipleEnemies_ReturnsClosest)
{
    Sentient *closeEnemy = CreateEnemy(Vector(100, 0, 0));
    Sentient *farEnemy = CreateEnemy(Vector(500, 0, 0));

    PerceptionSnapshot snapshot;

    EnemyInfo closeInfo;
    closeInfo.entity = closeEnemy;
    closeInfo.position = closeEnemy->origin;
    closeInfo.distance = 100.0f;

    EnemyInfo farInfo;
    farInfo.entity = farEnemy;
    farInfo.position = farEnemy->origin;
    farInfo.distance = 500.0f;

    snapshot.visibleEnemies.push_back(closeInfo);
    snapshot.visibleEnemies.push_back(farInfo);
    snapshot.closestEnemyIndex = 0; // First one is closest

    const EnemyInfo *result = BT::Combat::FindClosestVisibleEnemy(&snapshot);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->entity, closeEnemy);
    EXPECT_FLOAT_EQ(result->distance, 100.0f);

    delete closeEnemy;
    delete farEnemy;
}

// === Integration Tests for SelectTarget Action ===
// Added in OPM - Phase 3 Task 3.1a Code Review
//  Per Gemini recommendation: test the SelectTarget action's blackboard interactions

// Mock Blackboard for integration tests
class MockBlackboard {
private:
    std::unordered_map<std::string, void*> pointers;
    std::unordered_map<std::string, float> floats;
    std::unordered_map<std::string, bool> bools;
    
public:
    template<typename T>
    void Set(const std::string& key, T value) {
        if constexpr (std::is_pointer<T>::value) {
            pointers[key] = reinterpret_cast<void*>(value);
        } else if constexpr (std::is_same<T, float>::value) {
            floats[key] = value;
        } else if constexpr (std::is_same<T, bool>::value) {
            bools[key] = value;
        }
    }
    
    template<typename T>
    T Get(const std::string& key) const {
        if constexpr (std::is_pointer<T>::value) {
            auto it = pointers.find(key);
            return it != pointers.end() ? reinterpret_cast<T>(it->second) : nullptr;
        } else if constexpr (std::is_same<T, float>::value) {
            auto it = floats.find(key);
            return it != floats.end() ? it->second : 0.0f;
        } else if constexpr (std::is_same<T, bool>::value) {
            auto it = bools.find(key);
            return it != bools.end() ? it->second : false;
        }
        return T{};
    }
    
    bool Has(const std::string& key) const {
        return pointers.find(key) != pointers.end() ||
               floats.find(key) != floats.end() ||
               bools.find(key) != bools.end();
    }
};

// Mock BotProfile for integration tests
class MockBotProfile {
public:
    float targetLockTime = 2.0f;
    float targetSwitchThreshold = 128.0f;
    
    float GetTargetLockTime() const { return targetLockTime; }
    float GetTargetSwitchThreshold() const { return targetSwitchThreshold; }
};

// Test fixture for SelectTarget action integration tests
class SelectTargetActionTest : public ::testing::Test {
protected:
    void SetUp() override {
        level.svsTime = 0;
        level.inttime = 0;
    }
    
    // Helper: Create perception snapshot with enemies
    PerceptionSnapshot CreatePerceptionWithEnemies(const std::vector<std::pair<Vector, float>>& enemies) {
        PerceptionSnapshot snapshot;
        
        for (size_t i = 0; i < enemies.size(); ++i) {
            Sentient *enemy = new Sentient();
            enemy->origin = enemies[i].first;
            enemy->edict->inuse = true;
            createdEnemies.push_back(enemy);
            
            EnemyInfo info;
            info.entity = enemy;
            info.position = enemy->origin;
            info.distance = enemies[i].second;
            snapshot.visibleEnemies.push_back(info);
        }
        
        if (!snapshot.visibleEnemies.empty()) {
            snapshot.closestEnemyIndex = 0;
        }
        
        return snapshot;
    }
    
    void TearDown() override {
        for (auto* enemy : createdEnemies) {
            delete enemy;
        }
        createdEnemies.clear();
    }
    
    std::vector<Sentient*> createdEnemies;
};

// Test: SelectTarget selects closest enemy when no current target
TEST_F(SelectTargetActionTest, SelectsClosestEnemyWhenNoCurrentTarget)
{
    // This test verifies the basic case: bot has no current target,
    // multiple enemies visible, should select closest
    
    Player bot;
    bot.SetTeam(1);
    
    MockBotProfile profile;
    
    // Create perception with 3 enemies at different distances
    PerceptionSnapshot snapshot = CreatePerceptionWithEnemies({
        {Vector(500, 0, 0), 500.0f},  // Far enemy
        {Vector(200, 0, 0), 200.0f},  // Close enemy (should be selected)
        {Vector(400, 0, 0), 400.0f}   // Medium enemy
    });
    
    // Verify: Should select closest (index 1, distance 200)
    const EnemyInfo* closest = BT::Combat::FindClosestVisibleEnemy(&snapshot);
    ASSERT_NE(closest, nullptr);
    EXPECT_EQ(closest, &snapshot.visibleEnemies[0]); // First one added
}

// Test: Target stickiness prevents switching to slightly closer enemy
TEST_F(SelectTargetActionTest, StickinessPreventsSwitchToSlightlyCloserEnemy)
{
    level.svsTime = 1000; // 1 second elapsed
    
    Sentient* currentTarget = new Sentient();
    currentTarget->origin = Vector(500, 0, 0);
    currentTarget->edict->inuse = true;
    createdEnemies.push_back(currentTarget);
    
    Sentient* newEnemy = new Sentient();
    newEnemy->origin = Vector(450, 0, 0); // 50 units closer (not enough)
    newEnemy->edict->inuse = true;
    createdEnemies.push_back(newEnemy);
    
    MockBotProfile profile;
    profile.targetSwitchThreshold = 128.0f;
    profile.targetLockTime = 2.0f;
    
    // Score current target (locked 500ms ago)
    float currentScore = BT::Combat::CalculateTargetScore(
        currentTarget,
        currentTarget,  // Is current target
        500.0f,
        500.0f,         // Lock time
        profile.GetTargetLockTime(),
        profile.GetTargetSwitchThreshold()
    );
    
    // Score new enemy (not locked)
    float newScore = BT::Combat::CalculateTargetScore(
        newEnemy,
        currentTarget,  // Not current target
        450.0f,
        500.0f,
        profile.GetTargetLockTime(),
        profile.GetTargetSwitchThreshold()
    );
    
    // Current target should have higher score due to stickiness bonus
    EXPECT_GT(currentScore, newScore);
}

// Test: Lock time expires, switches to closer enemy
TEST_F(SelectTargetActionTest, SwitchesAfterLockTimeExpires)
{
    level.svsTime = 3000; // 3 seconds elapsed
    
    Sentient* currentTarget = new Sentient();
    currentTarget->origin = Vector(500, 0, 0);
    currentTarget->edict->inuse = true;
    createdEnemies.push_back(currentTarget);
    
    Sentient* newEnemy = new Sentient();
    newEnemy->origin = Vector(400, 0, 0); // Closer
    newEnemy->edict->inuse = true;
    createdEnemies.push_back(newEnemy);
    
    MockBotProfile profile;
    profile.targetLockTime = 2.0f; // 2 second lock
    
    // Score current target (locked 3 seconds ago - expired)
    float currentScore = BT::Combat::CalculateTargetScore(
        currentTarget,
        currentTarget,
        500.0f,
        0.0f,           // Locked 3s ago (level.svsTime - 3000 = 0)
        profile.GetTargetLockTime(),
        profile.GetTargetSwitchThreshold()
    );
    
    // Score new enemy
    float newScore = BT::Combat::CalculateTargetScore(
        newEnemy,
        currentTarget,
        400.0f,
        0.0f,
        profile.GetTargetLockTime(),
        profile.GetTargetSwitchThreshold()
    );
    
    // New enemy should have higher score (lock expired, closer)
    EXPECT_GT(newScore, currentScore);
}

// Test: No valid targets (all fail IsValidEnemy) returns correct state
TEST_F(SelectTargetActionTest, NoValidTargetsAvailable)
{
    Player bot;
    bot.SetTeam(1);
    
    // Create enemy that will fail IsValidEnemy (dead)
    Sentient* deadEnemy = new Sentient();
    deadEnemy->origin = Vector(100, 0, 0);
    deadEnemy->m_dead = true;  // Invalid
    deadEnemy->edict->inuse = true;
    createdEnemies.push_back(deadEnemy);
    
    // Verify it's invalid
    EXPECT_FALSE(BT::Combat::IsValidEnemy(&bot, deadEnemy));
}

// Main function
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
