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

// test_weapon_switching.cpp
// Unit tests for weapon switching system
// Added in OPM - Phase 3 Task 3.1h

#include <gtest/gtest.h>
#include "bt_actions_weapon.h"
#include "bt_conditions_weapon.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "behavior_tree.h"
#include "test_game_stubs.h"

// ============================================================================
// Test Fixtures
// ============================================================================

class WeaponSwitchingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        InitGameStubs();
        
        // Create mock player with inventory
        player = new Player();
        
        // Create test weapons with different classes and ranges
        pistol = CreateMockWeapon(WEAPON_CLASS_PISTOL, 512.0f, 0.0f);
        rifle = CreateMockWeapon(WEAPON_CLASS_RIFLE, 2048.0f, 128.0f);
        smg = CreateMockWeapon(WEAPON_CLASS_SMG, 1024.0f, 64.0f);
        
        // Setup profile with preferences
        profile = new BotProfile();
        profile->weaponPreferences.pistol = 0.3f;
        profile->weaponPreferences.rifle = 0.8f;
        profile->weaponPreferences.smg = 0.7f;
        
        // Setup blackboard
        blackboard.Set<Player *>(BlackboardKeys::PLAYER, player);
        blackboard.Set<BotProfile *>(BlackboardKeys::PROFILE, profile);
    }
    
    void TearDown() override
    {
        delete player;
        delete pistol;
        delete rifle;
        delete smg;
        delete profile;
        CleanupGameStubs();
    }
    
    // Helper to create mock weapon
    Weapon* CreateMockWeapon(int weaponClass, float maxRange, float minRange)
    {
        Weapon *weapon = new Weapon();
        weapon->weapon_class = weaponClass;
        weapon->SetMaxRange(maxRange);
        weapon->SetMinRange(minRange);
        // Mock ammo availability
        weapon->startammo[FIRE_PRIMARY] = 100;
        return weapon;
    }
    
    Player      *player;
    Weapon      *pistol;
    Weapon      *rifle;
    Weapon      *smg;
    BotProfile  *profile;
    Blackboard   blackboard;
};

// ============================================================================
// Test 1: SelectBestWeapon_EmptyCurrent
// ============================================================================

TEST_F(WeaponSwitchingTest, SelectBestWeapon_EmptyCurrent)
{
    // Setup: Current weapon (pistol) has no ammo
    pistol->startammo[FIRE_PRIMARY] = 0;
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    
    // Rifle has ammo
    rifle->startammo[FIRE_PRIMARY] = 30;
    player->SetActiveWeapon(rifle, WEAPON_OFFHAND);
    
    // Set target distance to rifle's optimal range
    blackboard.Set<float>(BlackboardKeys::TARGET_DISTANCE, 1024.0f);
    
    // Execute action
    BTNode::Status status = Action_SelectBestWeapon_Execute(blackboard, 0.1f);
    
    // Verify: Should select rifle (has ammo, good range)
    ASSERT_EQ(status, BTNode::Status::SUCCESS);
    
    auto selectedWeapon = blackboard.TryGet<Weapon *>(BlackboardKeys::SELECTED_WEAPON);
    ASSERT_TRUE(selectedWeapon.has_value());
    EXPECT_EQ(*selectedWeapon, rifle);
}

// ============================================================================
// Test 2: SelectBestWeapon_RangeBased
// ============================================================================

TEST_F(WeaponSwitchingTest, SelectBestWeapon_RangeBased)
{
    // Setup: All weapons have ammo
    pistol->startammo[FIRE_PRIMARY] = 15;
    rifle->startammo[FIRE_PRIMARY] = 30;
    smg->startammo[FIRE_PRIMARY] = 30;
    
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    player->SetActiveWeapon(rifle, WEAPON_OFFHAND);
    player->AddWeapon(smg); // Add to inventory
    
    // Set target distance to long range (1500 units)
    blackboard.Set<float>(BlackboardKeys::TARGET_DISTANCE, 1500.0f);
    
    // Execute action
    BTNode::Status status = Action_SelectBestWeapon_Execute(blackboard, 0.1f);
    
    // Verify: Should select rifle (best for long range)
    ASSERT_EQ(status, BTNode::Status::SUCCESS);
    
    auto selectedWeapon = blackboard.TryGet<Weapon *>(BlackboardKeys::SELECTED_WEAPON);
    ASSERT_TRUE(selectedWeapon.has_value());
    EXPECT_EQ(*selectedWeapon, rifle) << "Rifle should be selected for long-range combat";
}

// ============================================================================
// Test 3: SelectBestWeapon_ProfilePreference
// ============================================================================

TEST_F(WeaponSwitchingTest, SelectBestWeapon_ProfilePreference)
{
    // Setup: All weapons have ammo, target at medium range (good for all)
    pistol->startammo[FIRE_PRIMARY] = 15;
    rifle->startammo[FIRE_PRIMARY] = 30;
    smg->startammo[FIRE_PRIMARY] = 30;
    
    // Adjust ranges so all are viable at 600 units
    pistol->SetMaxRange(800.0f);
    rifle->SetMaxRange(2048.0f);
    rifle->SetMinRange(0.0f);
    smg->SetMaxRange(1024.0f);
    smg->SetMinRange(0.0f);
    
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    player->SetActiveWeapon(rifle, WEAPON_OFFHAND);
    player->AddWeapon(smg);
    
    // Set target at medium range
    blackboard.Set<float>(BlackboardKeys::TARGET_DISTANCE, 600.0f);
    
    // Execute action
    BTNode::Status status = Action_SelectBestWeapon_Execute(blackboard, 0.1f);
    
    // Verify: Should prefer rifle (highest profile preference: 0.8)
    ASSERT_EQ(status, BTNode::Status::SUCCESS);
    
    auto selectedWeapon = blackboard.TryGet<Weapon *>(BlackboardKeys::SELECTED_WEAPON);
    ASSERT_TRUE(selectedWeapon.has_value());
    EXPECT_EQ(*selectedWeapon, rifle) << "Rifle should be selected due to highest profile preference (0.8)";
}

// ============================================================================
// Test 4: BetterWeaponAvailable
// ============================================================================

TEST_F(WeaponSwitchingTest, BetterWeaponAvailable)
{
    // Setup: Current weapon is pistol at long range (bad match)
    pistol->startammo[FIRE_PRIMARY] = 15;
    rifle->startammo[FIRE_PRIMARY] = 30;
    
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    player->SetActiveWeapon(rifle, WEAPON_OFFHAND);
    
    // Set target at long range where rifle excels
    blackboard.Set<float>(BlackboardKeys::TARGET_DISTANCE, 1500.0f);
    
    // Execute condition check
    bool betterAvailable = Condition_BetterWeaponAvailable_Check(blackboard);
    
    // Verify: Should return true (rifle is much better for this range)
    EXPECT_TRUE(betterAvailable) << "Rifle should be significantly better than pistol at 1500 units";
    
    // Now test opposite: using rifle at long range (optimal)
    player->SetActiveWeapon(rifle, WEAPON_MAIN);
    player->SetActiveWeapon(pistol, WEAPON_OFFHAND);
    
    betterAvailable = Condition_BetterWeaponAvailable_Check(blackboard);
    
    // Verify: Should return false (rifle is already optimal)
    EXPECT_FALSE(betterAvailable) << "No better weapon than rifle at long range";
}

// ============================================================================
// Test 5: CurrentWeaponEmpty
// ============================================================================

TEST_F(WeaponSwitchingTest, CurrentWeaponEmpty)
{
    // Setup: Pistol with no ammo
    pistol->startammo[FIRE_PRIMARY] = 0;
    pistol->ammo_in_clip[FIRE_PRIMARY] = 0;
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    
    // Execute condition check
    bool isEmpty = Condition_CurrentWeaponEmpty_Check(blackboard);
    
    // Verify: Should return true
    EXPECT_TRUE(isEmpty) << "Weapon with no ammo should be considered empty";
    
    // Now give ammo
    pistol->startammo[FIRE_PRIMARY] = 15;
    
    isEmpty = Condition_CurrentWeaponEmpty_Check(blackboard);
    
    // Verify: Should return false
    EXPECT_FALSE(isEmpty) << "Weapon with ammo should not be considered empty";
}

// ============================================================================
// Test 6: WeaponSwitchReady
// ============================================================================

TEST_F(WeaponSwitchingTest, WeaponSwitchReady)
{
    // Setup: Weapon in ready state
    pistol->ForceState(WEAPON_READY);
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    
    // Execute condition check
    bool isReady = Condition_WeaponSwitchReady_Check(blackboard);
    
    // Verify: Should return true
    EXPECT_TRUE(isReady) << "Weapon in ready state should allow switching";
    
    // Test switching state
    pistol->ForceState(WEAPON_CHANGING);
    
    isReady = Condition_WeaponSwitchReady_Check(blackboard);
    
    // Verify: Should return false
    EXPECT_FALSE(isReady) << "Cannot switch while already switching";
    
    // Test reloading state
    pistol->ForceState(WEAPON_RELOADING);
    
    isReady = Condition_WeaponSwitchReady_Check(blackboard);
    
    // Verify: Should return false
    EXPECT_FALSE(isReady) << "Cannot switch while reloading";
}

// ============================================================================
// Test 7: SwitchWeapon Action
// ============================================================================

TEST_F(WeaponSwitchingTest, SwitchWeaponAction)
{
    // Setup: Current weapon is pistol, want to switch to rifle
    player->SetActiveWeapon(pistol, WEAPON_MAIN);
    player->SetActiveWeapon(rifle, WEAPON_OFFHAND);
    
    // Set selected weapon in blackboard
    blackboard.Set<Weapon *>(BlackboardKeys::SELECTED_WEAPON, rifle);
    
    // Execute action
    BTNode::Status status = Action_SwitchWeapon_Execute(blackboard, 0.1f);
    
    // Verify: Should initiate switch (RUNNING or SUCCESS)
    EXPECT_NE(status, BTNode::Status::FAILURE) << "Weapon switch should not fail";
    
    // Test switching to already equipped weapon
    blackboard.Set<Weapon *>(BlackboardKeys::SELECTED_WEAPON, pistol);
    
    status = Action_SwitchWeapon_Execute(blackboard, 0.1f);
    
    // Verify: Should immediately succeed (already equipped)
    EXPECT_EQ(status, BTNode::Status::SUCCESS) << "Switching to current weapon should succeed immediately";
}
