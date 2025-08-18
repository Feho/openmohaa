#ifndef __PLAYERBOT_CONST_H__
#define __PLAYERBOT_CONST_H__

// Bot Skill Levels
constexpr float BOT_MIN_SKILL = 0.0f;
constexpr float BOT_MAX_SKILL = 1.0f;

// Timeouts and Delays
constexpr int ATTACK_STATE_TIMEOUT_MS = 10000;
constexpr int CURIOUS_STATE_TIMEOUT_MS = 30000;
constexpr int MIN_STATE_TRANSITION_TIME_MS = 1000;
constexpr int ATTACK_STATE_TRANSITION_DELAY_MS = 2000;
constexpr int WEAPON_SWITCH_CHECK_DELAY_MIN_MS = 500;
constexpr int WEAPON_SWITCH_CHECK_DELAY_MAX_MS = 2000;
constexpr int RELOAD_DELAY_MIN_MS = 1000;
constexpr int RELOAD_DELAY_MAX_MS = 3000;
constexpr int TAUNT_DELAY_MS = 8000;
constexpr int CONTINUOUS_FIRE_TIME_MIN_MS = 500;
constexpr int CONTINUOUS_FIRE_TIME_MAX_MS = 2000;
constexpr int BURST_FIRE_TIME_MIN_MS = 100;
constexpr int BURST_FIRE_TIME_MAX_MS = 600;
constexpr int AIM_UPDATE_INTERVAL_MS = 100;
constexpr int ENEMY_SCAN_INTERVAL_MS = 250;
constexpr int ATTACK_STOP_AIM_DELAY_MS = 3000;
constexpr int LAST_SEEN_UNSEEN_DELAY_MS = 2000;
constexpr int IDLE_TIME_BEFORE_DEATH_POS_MS = 5000;
constexpr int CURIOUS_TIME_FROM_EVENT_MS = 15000;
constexpr int CURIOUS_TIME_FROM_MINOR_EVENT_MS = 10000;
constexpr int ATTACK_TIME_FROM_EVENT_MS = 5000;

// Distances
constexpr float MIN_ATTACK_DISTANCE = 128.0f;
constexpr float MIN_FIRING_RANGE = 2048.0f;
constexpr float MIN_MELEE_RANGE = 256.0f;
constexpr float STAND_STILL_DISTANCE_MEDIUM = 192.0f;
constexpr float STAND_STILL_DISTANCE_LONG = 384.0f;
constexpr float REACTION_DISTANCE_MAX = 1536.0f;
constexpr float IDLE_AVOID_RADIUS_MIN = 512.0f;
constexpr float IDLE_AVOID_RADIUS_MAX = 2048.0f;
constexpr float IDLE_AVOID_PREFERRED_DIR_DISTANCE = 1024.0f;

// Firing Ranges
constexpr float OPTIMAL_FIRING_RANGE_MIN = 128.0f;
constexpr float OPTIMAL_FIRING_RANGE_MAX = 512.0f;
constexpr float LONG_FIRING_RANGE = 1024.0f;

// Angles and FOV
constexpr float ENEMY_DETECTION_ANGLE = 90.0f;

// Movement and Strafing
constexpr int STRAFE_LEFT = -127;
constexpr int STRAFE_RIGHT = 127;
constexpr float STRAFE_INTENSITY_CROUCH_MULTIPLIER = 0.8f;
constexpr float STRAFE_INTENSITY_MIN = 0.6f;
constexpr float STRAFE_INTENSITY_MAX = 1.0f;
constexpr int STRAFE_TIME_MIN_MS = 300;
constexpr int STRAFE_TIME_MAX_MS = 1000;
constexpr int STRAFE_TIME_RANDOM_MIN_MS = 800;
constexpr int STRAFE_TIME_RANDOM_MAX_MS = 2000;
constexpr float STRAFE_DIRECTION_CHANGE_CHANCE_L = 0.35f;
constexpr float STRAFE_DIRECTION_CHANGE_CHANCE_R = 0.7f;
constexpr float STRAFE_DIRECTION_CHANGE_CHANCE_QUICK = 0.85f;
constexpr float STRAFE_RANDOM_MOVE_CHANCE = 0.3f;
constexpr float STRAFE_RANDOM_MOVE_INTENSITY = 64.0f;
constexpr float STRAFE_FORWARD_MOVE_CHANCE = 0.7f;
constexpr float STRAFE_FORWARD_MOVE_INTENSITY = 48.0f;
constexpr float STRAFE_BACKWARD_MOVE_INTENSITY = -24.0f;
constexpr float STRAFE_UNPREDICTABLE_MOVE_CHANCE = 0.1f;
constexpr float STRAFE_UNPREDICTABLE_MOVE_INTENSITY_R = 1.2f;
constexpr float STRAFE_UNPREDICTABLE_MOVE_INTENSITY_F = 0.8f;

// Stance
constexpr float CROUCH_CHANCE_BASE = 0.15f;
constexpr float CROUCH_CHANCE_SKILL_MULTIPLIER = 0.2f;
constexpr float RUNNING_CHANCE_BASE = 0.6f;
constexpr float RUNNING_CHANCE_SKILL_MULTIPLIER = 0.2f;
constexpr int STANCE_CHECK_INTERVAL_MS = 2000;
constexpr float RUNNING_CHANCE_DISTANCE_MULTIPLIER_MEDIUM = 0.7f;
constexpr float RUNNING_CHANCE_DISTANCE_MULTIPLIER_LONG = 0.5f;
constexpr float RUNNING_CHANCE_SKILL_MULTIPLIER_HIGH = 0.8f;
constexpr float CROUCH_CHANCE_DISTANCE_BONUS_LONG = 0.1f;
constexpr float CROUCH_CHANCE_DISTANCE_BONUS_VERY_LONG = 0.1f;
constexpr float CROUCH_CHANCE_DISTANCE_MULTIPLIER_CLOSE = 0.3f;
constexpr float CROUCH_CHANCE_SKILL_MULTIPLIER_HIGH = 0.7f;
constexpr int CROUCH_TIME_MIN_MS = 800;
constexpr int CROUCH_TIME_MAX_MS = 1800;
constexpr int STAND_TIME_MIN_MS = 500;
constexpr int STAND_TIME_MAX_MS = 1300;

// Aiming and Accuracy
constexpr float AIM_INACCURACY_BASE = 1.5f;
constexpr float AIM_INACCURACY_SKILL_MULTIPLIER = 1.0f;
constexpr float AIM_DISTANCE_FACTOR_DIVISOR = 512.0f;
constexpr float AIM_MOVEMENT_PENALTY_DIVISOR = 200.0f;
constexpr float AIM_STANCE_MODIFIER_CROUCH = 0.4f;
constexpr float AIM_STANCE_MODIFIER_WALK = 0.7f;
constexpr float AIM_WEAPON_SPREAD_FACTOR_MULTIPLIER = 2.0f;
constexpr float AIM_BURST_PENALTY_TIME_DIVISOR = 1000.0f;
constexpr float AIM_BURST_PENALTY_MULTIPLIER = 0.5f;
constexpr float AIM_TARGET_MOVEMENT_PENALTY_DIVISOR = 300.0f;
constexpr float AIM_HEAD_CHANCE_SKILL_MULTIPLIER = 0.4f;
constexpr float AIM_HEAD_SPREAD_MULTIPLIER = 2.0f;
constexpr float AIM_BODY_SPREAD_MULTIPLIER = 3.0f;
constexpr float AIM_ORIGIN_SPREAD_MULTIPLIER = 4.0f;
constexpr float AIM_DRIFT_AMOUNT_SKILL_MULTIPLIER = 8.0f;
constexpr int AIM_DRIFT_SUSTAINED_FIRE_TIME_MS = 500;

// Reloading
constexpr float RELOAD_FORGET_CHANCE_BASE = 0.5f;
constexpr float RELOAD_FORGET_CHANCE_SKILL_MULTIPLIER = 0.4f;

#endif // __PLAYERBOT_CONST_H__
