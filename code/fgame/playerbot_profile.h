// playerbot_profile.h: Per-bot personality parameters.

#pragma once

#include "../corepp/str.h"
#include "../corepp/container.h"

// Added in OPM
//  Per-bot personality parameters. Feeds existing combat/movement/
//  perception code — no behavioral logic lives here. A profile is
//  pure data, loaded once at spawn and never mutated.
struct BotProfile {
    str name; // "sniper", "rusher", "gunner", ...

    // --- Combat reaction ---
    float reactionMinDelay;    // seconds
    float reactionRandomDelay; // seconds
    float aimSpreadMult;
    float aimSettleSpeed;      // speed at which aim settles after overshoot
    float aimOvershoot;        // overshoot scale on flick
    float aimNoise;            // micro-noise scale (hand tremor)
    float aimLerpSpeed;        // speed of aim offset interpolation

    // --- Engagement preferences ---
    float preferredRangeMin; // units; below this the bot wants to back off
    float preferredRangeMax; // units; above this the bot wants to close in
    float riskTolerance;     // 0..1; willingness to engage unfavorable fights
    float aggression;        // 0..1; bias toward pushing vs. holding

    // --- Fire control ---
    float burstMinTime;                // seconds; minimum burst duration
    float burstRandomDelay;            // seconds; random pause between bursts
    float continuousFireMinTime;       // seconds; minimum continuous fire time
    float continuousFireRandomTime;    // seconds; random additional fire time

    // --- Movement / exploration ---
    float turnSpeed;    // rotation speed
    float roamRadius;   // units
    float walkChance;   // 0..1
    float pauseChance;  // 0..1
    int   crouchChance; // 0..100; chance to crouch when standing still in combat

    // --- Perception ---
    float hearingRange;    // multiplier on sound range factor
    float visionFovDegrees; // replaces hardcoded 360° scan

    // --- Selection weight ---
    float weight; // relative probability of being picked

    void setDefaults();
};

// Added in OPM
//  Loads and manages bot personality profiles from main/bots/profiles/*.cfg.
//  Each controller copies a profile on spawn. If no profiles are loaded,
//  a default profile built from cvar values is used.
class BotProfileManager
{
public:
    BotProfileManager();

    void                   Init();
    const BotProfile&      PickProfile() const;
    const BotProfile&      FindProfile(const char *name) const;
    const BotProfile&      GetDefaultProfile() const;
    int                    NumProfiles() const;

private:
    void LoadProfileFile(const char *path);

    Container<BotProfile> m_profiles;
    BotProfile            m_defaultProfile;
};

extern BotProfileManager botProfileManager;
