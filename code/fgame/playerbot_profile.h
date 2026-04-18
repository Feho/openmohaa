// playerbot_profile.h: Bot personality profile system.
//
// Each bot is assigned a BotProfile at first spawn. The profile drives per-bot
// behavioral variation without touching global cvars. BotProfileManager loads
// profiles from main/bots/profiles/*.cfg and picks one per bot via weighted
// random selection.

#pragma once

#include "../corepp/str.h"
#include "../corepp/container.h"

/**
 * @brief Per-bot personality data.
 *
 * Pure data struct - no game-engine dependencies. All fields have defaults
 * derived from the corresponding global cvars so unprofile'd bots behave
 * identically to the pre-profile baseline.
 */
struct BotProfile {
    // Identity
    str   name;
    float weight; // Relative probability weight for random selection (higher = more common)

    // Aim / rotation - fed into BotRotation members at spawn
    float turnSpeed;     // degrees per second (replaces g_bot_turn_speed)
    float aimNoise;      // hand tremor scale  (replaces g_bot_aim_noise)
    float aimOvershoot;  // flick-past magnitude (replaces g_bot_aim_overshoot)
    float aimSettleSpeed; // dampening rate after overshoot (replaces g_bot_aim_settle_speed)
    float aimLerpSpeed;  // interpolation speed (replaces g_bot_aim_lerp_speed)
    float aimSpreadMult; // accuracy multiplier on aim offset (replaces g_bot_attack_spreadmult)

    // Combat reaction
    float reactionMinDelay;    // seconds (replaces g_bot_attack_react_min_delay)
    float reactionRandomDelay; // seconds (replaces g_bot_attack_react_random_delay)

    // Fire control
    float burstMinTime;              // seconds (replaces g_bot_attack_burst_min_time)
    float burstRandomDelay;          // seconds (replaces g_bot_attack_burst_random_delay)
    float continuousFireMinTime;     // seconds (replaces g_bot_attack_continuousfire_min_firetime)
    float continuousFireRandomTime;  // seconds (replaces g_bot_attack_continuousfire_random_firetime)

    // Movement
    float crouchChance; // 0..100 integer percentage (replaces g_bot_crouch_chance)

    // Weapon preference: "auto", "rifle", "sniper", "smg", "mg", "heavy", "shotgun"
    // "auto" means use rank-based selection (existing behavior)
    str preferredWeapon;

    // Perception
    float visionDistanceMult = 1.0f; // Multiplier on base vision range

    bool IsSniperRole() const { return !Q_stricmp(preferredWeapon.c_str(), "sniper"); }
};

/**
 * @brief Loads and manages bot personality profiles.
 *
 * Profiles are loaded once at map start from main/bots/profiles/*.cfg.
 * If no profiles are found the built-in default profile (matching the
 * current global cvar defaults) is used so existing behavior is preserved.
 */
class BotProfileManager
{
public:
    BotProfileManager();

    void LoadProfiles(const char *directory);

    /**
     * @brief Pick a profile for a new bot.
     *
     * If overrideName is non-null and non-empty, returns the profile with that
     * name (case-insensitive). Falls back to weighted random selection.
     * Always returns a valid profile reference.
     */
    const BotProfile& PickProfile(const char *overrideName = nullptr) const;

    const BotProfile& GetDefault() const;

    int NumProfiles() const;

private:
    void             ParseProfileFile(const char *filename, const char *basedir);
    const BotProfile *FindByName(const char *name) const;
    void             InitDefault();

    Container<BotProfile> m_profiles;
    BotProfile            m_default;
};

extern BotProfileManager botProfileManager;
