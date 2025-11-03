// Added in OPM - Phase 2B Task 2B.2 Review Fixes
// bt_blackboard_keys.h: Constants for blackboard keys to prevent typos

#ifndef __BT_BLACKBOARD_KEYS_H__
#define __BT_BLACKBOARD_KEYS_H__

/**
 * Standard blackboard keys used by behavior tree actions and conditions.
 * Using constants prevents typos and makes refactoring easier.
 *
 * Usage:
 *   bb.Set<BotController *>(BlackboardKeys::BOT, botController);
 *   auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);
 */
namespace BlackboardKeys {

// Core bot data
constexpr const char *BOT        = "bot";        // BotController* - The bot AI controller
constexpr const char *PLAYER     = "player";     // Player* - The player entity
constexpr const char *PERCEPTION = "perception"; // PerceptionSnapshot* - Current perception data

// Target data (for complex behaviors)
constexpr const char *TARGET_ENTITY   = "target_entity";   // Sentient* - Current target entity
constexpr const char *TARGET_POSITION = "target_position"; // Vector* - Target position to move to

// Added in OPM - Phase 3 Task 3.1a
//  Target selection and tracking (combat system)
constexpr const char *SELECTED_TARGET   = "selectedTarget";   // Sentient* - Current attack target
constexpr const char *TARGET_DISTANCE   = "targetDistance";   // float - Distance to target (units)
constexpr const char *TARGET_LOCK_TIME  = "targetLockTime";   // float - When target was locked (level.svsTime)
constexpr const char *TARGET_SWITCHED   = "targetSwitched";   // bool - Whether we switched targets this frame
constexpr const char *PREVIOUS_TARGET   = "previousTarget";   // Sentient* - Previous target (for comparison)
constexpr const char *PROFILE           = "profile";          // BotProfile* - Bot profile with parameters

// Added in OPM - Phase 3 Task 3.1b
//  Aiming and fire control system
constexpr const char *AIM_OFFSET         = "aimOffset";         // Vector - Current aim inaccuracy offset
constexpr const char *AIM_UPDATE_TIME    = "aimUpdateTime";     // float - Last time aim offset was updated
constexpr const char *IS_AIMED_AT_TARGET = "isAimedAtTarget";   // bool - Whether aim is within tolerance
constexpr const char *ENEMY_EYES_TAG     = "enemyEyesTag";      // int - Cached eye bone tag for target
constexpr const char *BURST_STATE        = "burstState";        // int - 0=not firing, 1=burst, 2=pause
constexpr const char *BURST_START_TIME   = "burstStartTime";    // float - When current burst started
constexpr const char *CONTINUOUS_FIRE_TIME = "continuousFireTime"; // float - Total fire time in burst
constexpr const char *LAST_FIRE_TIME     = "lastFireTime";      // float - Last shot time

// Added in OPM - Phase 3 Task 3.1c
//  Combat movement system
constexpr const char *MOVING_TO_POSITION = "movingToPosition";  // Vector - Target position being moved to
constexpr const char *STRAFE_DIRECTION   = "strafeDirection";   // int - 1=right, -1=left, 0=not strafing
constexpr const char *STRAFE_TIMER       = "strafeTimer";       // float - Time when current strafe direction started
constexpr const char *OPTIMAL_RANGE      = "optimalRange";      // float - Weapon's optimal combat range

// Behavior state (for multi-frame actions)
constexpr const char *BEHAVIOR_STATE     = "behavior_state";     // int - State machine index
constexpr const char *BEHAVIOR_TIMESTAMP = "behavior_timestamp"; // float - When behavior started
constexpr const char *BEHAVIOR_COUNTER   = "behavior_counter";   // int - Generic counter for behaviors

// Added in OPM - Phase 3 Task 3.1h
//  Weapon switching system
constexpr const char *SELECTED_WEAPON     = "selectedWeapon";    // Weapon* - Weapon selected for switching
constexpr const char *WEAPON_SWITCH_TIME  = "weaponSwitchTime";  // float - When weapon switch was initiated

} // namespace BlackboardKeys

#endif // __BT_BLACKBOARD_KEYS_H__
