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

// bot_profile.h
// Bot personality and behavior profiles
// Added in OPM - Phase 2B.3: Complete Profile System

#pragma once

#include <memory>
#include <string>
#include <utility>

class BotProfile
{
public:
    // Factory method to load profile from YAML file
    static std::unique_ptr<BotProfile> LoadFromFile(const char *filepath);

    // Destructor
    ~BotProfile() = default;

    // === METADATA ===
    const std::string& GetName() const { return metadata.name; }

    const std::string& GetDescription() const { return metadata.description; }

    const std::string& GetDifficulty() const { return metadata.difficulty; }

    const std::string& GetAuthor() const { return metadata.author; }

    const std::string& GetVersion() const { return metadata.version; }

    // === PERSONALITY ===
    float GetAggression() const { return personality.aggression; }

    float GetCaution() const { return personality.caution; }

    float GetTeamwork() const { return personality.teamwork; }

    float GetCreativity() const { return personality.creativity; }

    // === COMBAT ===
    float GetPreferredRange() const { return combat.preferredRange; }

    float GetFireDiscipline() const { return combat.fireDiscipline; }

    float GetBurstLengthMin() const { return combat.burstLength.first; }

    float GetBurstLengthMax() const { return combat.burstLength.second; }

    float GetBurstDelayMin() const { return combat.burstDelay.first; }

    float GetBurstDelayMax() const { return combat.burstDelay.second; }

    float GetAmmoConservation() const { return combat.ammoConservation; }

    bool GetReloadUnderFire() const { return combat.reloadUnderFire; }

    // Added in OPM - Phase 3 Task 3.1a
    //  Target selection accessors
    float GetTargetLockTime() const { return combat.targetLockTime; }

    float GetTargetSwitchThreshold() const { return combat.targetSwitchThreshold; }

    // === MOVEMENT ===
    float GetSpeedPreference() const { return movement.speedPreference; }

    float GetCrouchFrequency() const { return movement.crouchFrequency; }

    float GetJumpFrequency() const { return movement.jumpFrequency; }

    float GetStrafeUsage() const { return movement.strafeUsage; }

    // Added in OPM - Phase 3 Task 3.1c
    float GetPathDeviation() const { return movement.pathDeviation; }

    // === AIM ===
    float GetReactionTimeMin() const { return aim.reactionTime.first; }

    float GetReactionTimeMax() const { return aim.reactionTime.second; }

    float GetTrackingSmoothness() const { return aim.trackingSmoothness; }

    float GetSpreadMultiplier() const { return aim.spreadMultiplier; }

    float GetHeadshotBias() const { return aim.headshotBias; }

    // Added in OPM - Phase 3 Task 3.1b (Gemini review)
    float GetAimTolerance() const { return aim.aimTolerance; }

    // === TACTICS ===
    float GetCoverUsage() const { return tactics.coverUsage; }

    float GetRetreatThreshold() const { return tactics.retreatThreshold; }

    float GetFlankPreference() const { return tactics.flankPreference; }

    float GetGrenadeFrequency() const { return tactics.grenadeFrequency; }

    // === PERCEPTION ===
    float GetVisionFOV() const { return perception.vision.fov; }

    float GetVisionRange() const { return perception.vision.range; }

    float GetVisionPeripheralRange() const { return perception.vision.peripheralRange; }

    float GetHearingRange() const { return perception.hearing.range; }

    float GetHearingPriorityThreshold() const { return perception.hearing.priorityThreshold; }

    // === BEHAVIOR TREE ===
    const std::string& GetBehaviorTree() const { return behaviorTree; }

private:
    // Private constructor - use LoadFromFile()
    BotProfile() = default;

    // Profile validation
    static bool ValidateProfile(const BotProfile *profile);

    // === DATA STRUCTURES ===

    struct Metadata {
        std::string name        = "Unknown";
        std::string description = "No description";
        std::string difficulty  = "medium";
        std::string author      = "Unknown";
        std::string version     = "1.0";
    } metadata;

    struct Personality {
        float aggression = 0.5f; // 0.0 (passive) - 1.0 (aggressive)
        float caution    = 0.5f; // 0.0 (reckless) - 1.0 (careful)
        float teamwork   = 0.5f; // 0.0 (lone wolf) - 1.0 (squad-focused)
        float creativity = 0.5f; // 0.0 (predictable) - 1.0 (creative)
    } personality;

    struct Combat {
        float                   preferredRange        = 512.0f;       // Ideal combat distance (units)
        float                   fireDiscipline        = 0.5f;         // 0.0 (spray) - 1.0 (controlled bursts)
        std::pair<float, float> burstLength           = {0.3f, 0.8f}; // [min, max] seconds of continuous fire
        std::pair<float, float> burstDelay            = {0.2f, 0.5f}; // [min, max] seconds between bursts
        float                   ammoConservation      = 0.5f;         // 0.0 (spray) - 1.0 (conservative)
        bool                    reloadUnderFire       = false;        // Will reload while taking damage?
        // Added in OPM - Phase 3 Task 3.1a
        //  Target selection parameters
        float                   targetLockTime        = 2.0f;         // Seconds to lock onto target before allowing switch
        float                   targetSwitchThreshold = 128.0f;       // Distance advantage (units) needed to switch targets
    } combat;

    struct Movement {
        float speedPreference = 1.0f; // Multiplier on base speed
        float crouchFrequency = 0.3f; // 0.0 (never) - 1.0 (always)
        float jumpFrequency   = 0.3f; // How often to jump obstacles
        float strafeUsage     = 0.5f; // How much to strafe in combat
        // Added in OPM - Phase 3 Task 3.1c
        float pathDeviation   = 0.3f; // 0.0 (straight) - 1.0 (unpredictable path)
    } movement;

    struct Aim {
        std::pair<float, float> reactionTime       = {0.2f, 0.5f}; // [min, max] seconds to acquire target
        float                   trackingSmoothness = 0.7f;         // 0.0 (instant snap) - 1.0 (smooth)
        float                   spreadMultiplier   = 1.0f;         // Higher = less accurate
        float                   headshotBias       = 0.5f;         // 0.0 (center mass) - 1.0 (headshots)
        // Added in OPM - Phase 3 Task 3.1b (Gemini review)
        float                   aimTolerance       = 5.0f;         // Degrees - how precise aim must be before firing
    } aim;

    struct Tactics {
        float coverUsage       = 0.5f;  // 0.0 (ignore cover) - 1.0 (always use)
        float retreatThreshold = 0.25f; // HP % before retreating
        float flankPreference  = 0.5f;  // Likelihood to flank
        float grenadeFrequency = 0.3f;  // How often to use grenades
    } tactics;

    struct Perception {
        struct Vision {
            float fov             = 80.0f;   // Field of view (degrees)
            float range           = 2048.0f; // Vision range (units)
            float peripheralRange = 0.6f;    // Peripheral vision range multiplier
        } vision;

        struct Hearing {
            float range             = 1024.0f; // Hearing range (units)
            float priorityThreshold = 0.5f;    // Minimum priority to react to sounds
        } hearing;
    } perception;

    std::string behaviorTree = "engage_enemy"; // Which behavior tree to use
};
