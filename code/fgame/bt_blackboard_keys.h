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

// Behavior state (for multi-frame actions)
constexpr const char *BEHAVIOR_STATE     = "behavior_state";     // int - State machine index
constexpr const char *BEHAVIOR_TIMESTAMP = "behavior_timestamp"; // float - When behavior started
constexpr const char *BEHAVIOR_COUNTER   = "behavior_counter";   // int - Generic counter for behaviors

} // namespace BlackboardKeys

#endif // __BT_BLACKBOARD_KEYS_H__
