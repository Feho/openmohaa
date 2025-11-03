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

// Added in OPM - Phase 3 Task 3.1d
//  Cover system integration
constexpr const char *SELECTED_COVER = "selectedCover";  // CoverPoint - Selected cover position
constexpr const char *COVER_QUALITY  = "coverQuality";   // float - Quality of selected cover (0.0-1.0)
constexpr const char *COVER_STATE    = "coverState";     // int - Current cover state (CoverState enum)
constexpr const char *PEEK_STATE     = "peekState";      // int - Peek state (0=init, 1=peeking, 2=done)
constexpr const char *PEEK_START_TIME = "peekStartTime"; // float - When peek began (milliseconds)
constexpr const char *PEEK_DURATION   = "peekDuration";  // float - How long to peek (milliseconds)

// Added in OPM - Phase 3 Task 3.1e
//  Tactical combat and retreat system
constexpr const char *RECENT_DAMAGE          = "recentDamage";          // float - Damage taken in last 2 seconds
constexpr const char *LAST_DAMAGE_TIME       = "lastDamageTime";        // float - When last took damage (level.svsTime)
constexpr const char *COMBAT_PROFILE         = "combatProfile";         // int - CombatProfile enum
constexpr const char *RETREAT_POSITION       = "retreatPosition";       // Vector - Target retreat position
constexpr const char *SUPPRESS_START_TIME    = "suppressStartTime";     // float - When suppression fire started
constexpr const char *RELOAD_START_TIME      = "reloadStartTime";       // float - When reload started
constexpr const char *LAST_KNOWN_ENEMY_POS   = "lastKnownEnemyPos";     // Vector - Last known enemy position
constexpr const char *ENEMY_COUNT            = "enemyCount";            // int - Number of enemies in awareness radius

// Added in OPM - Phase 3 Task 3.1g
//  Grenade system
constexpr const char *GRENADE_TARGET_POSITION = "grenadeTargetPosition"; // Vector - Target position for grenade throw
constexpr const char *LAST_GRENADE_TIME       = "lastGrenadeTime";       // float - When last grenade was thrown (level.svsTime)

// Added in OPM - Phase 3 Task 3.2
//  Investigation system
constexpr const char *INVESTIGATION_TARGET         = "investigationTarget";        // Vector - Position being investigated
constexpr const char *INVESTIGATION_START_TIME     = "investigationStartTime";     // float - When investigation started (level.svsTime)
constexpr const char *INVESTIGATION_RADIUS         = "investigationRadius";        // float - Search radius around target
constexpr const char *INVESTIGATING_MEMORY_INDEX   = "investigatingMemoryIndex";   // size_t - Index of memory being investigated
constexpr const char *REACHED_INVESTIGATION_TARGET = "reachedInvestigationTarget"; // bool - Whether reached target
constexpr const char *SEARCH_PHASE                 = "searchPhase";                // int - Current search phase
constexpr const char *SEARCH_ANGLE                 = "searchAngle";                // float - Current search angle
constexpr const char *SEARCH_TIMER                 = "searchTimer";                // float - Search timer
constexpr const char *SEARCH_COMPLETE              = "searchComplete";             // bool - Whether search is complete
constexpr const char *INVESTIGATING_SOUND          = "investigatingSound";         // bool - Whether investigating a sound
constexpr const char *REACHED_SOUND_LOCATION       = "reachedSoundLocation";       // bool - Whether reached sound location
constexpr const char *LOOK_TIMER                   = "lookTimer";                  // float - Look-around timer
constexpr const char *LOOK_COUNT                   = "lookCount";                  // int - Number of look directions
constexpr const char *LOOK_COMPLETE                = "lookComplete";               // bool - Whether look-around is complete

} // namespace BlackboardKeys

#endif // __BT_BLACKBOARD_KEYS_H__
