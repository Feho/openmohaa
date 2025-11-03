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

// bot_profile.cpp
// Bot personality and behavior profiles implementation
// Added in OPM - Phase 2B.3: Complete Profile System

#include "bot_profile.h"
#include "g_local.h"
#include "bg_public.h"
#include <yaml-cpp/yaml.h>

std::unique_ptr<BotProfile> BotProfile::LoadFromFile(const char *filepath)
{
    try {
        // Load YAML file using game filesystem
        void *buffer = nullptr;
        long  length = gi.FS_ReadFile(filepath, &buffer, qfalse);
        
        if (length < 0 || !buffer) {
            gi.Printf("ERROR: Could not read profile file: %s\n", filepath);
            return nullptr;
        }

        // Parse YAML from buffer
        YAML::Node root = YAML::Load(static_cast<const char *>(buffer));
        gi.FS_FreeFile(buffer);

        // Validate root structure
        if (!root["profile"]) {
            gi.Printf("ERROR: Profile missing 'profile' root: %s\n", filepath);
            return nullptr;
        }

        YAML::Node                  profile = root["profile"];
        std::unique_ptr<BotProfile> bp(new BotProfile());

        // === METADATA ===
        if (profile["metadata"]) {
            YAML::Node meta = profile["metadata"];
            if (meta["name"]) {
                bp->metadata.name = meta["name"].as<std::string>();
            }
            if (meta["description"]) {
                bp->metadata.description = meta["description"].as<std::string>();
            }
            if (meta["difficulty"]) {
                bp->metadata.difficulty = meta["difficulty"].as<std::string>();
            }
            if (meta["author"]) {
                bp->metadata.author = meta["author"].as<std::string>();
            }
            if (meta["version"]) {
                bp->metadata.version = meta["version"].as<std::string>();
            }
        }

        // === PERSONALITY ===
        if (profile["personality"]) {
            YAML::Node pers = profile["personality"];
            if (pers["aggression"]) {
                bp->personality.aggression = pers["aggression"].as<float>();
            }
            if (pers["caution"]) {
                bp->personality.caution = pers["caution"].as<float>();
            }
            if (pers["teamwork"]) {
                bp->personality.teamwork = pers["teamwork"].as<float>();
            }
            if (pers["creativity"]) {
                bp->personality.creativity = pers["creativity"].as<float>();
            }
        }

        // === COMBAT ===
        if (profile["combat"]) {
            YAML::Node combat = profile["combat"];
            if (combat["preferred_range"]) {
                bp->combat.preferredRange = combat["preferred_range"].as<float>();
            }
            if (combat["fire_discipline"]) {
                bp->combat.fireDiscipline = combat["fire_discipline"].as<float>();
            }
            if (combat["burst_length"]) {
                auto bl = combat["burst_length"];
                if (bl.IsSequence() && bl.size() == 2) {
                    bp->combat.burstLength = {bl[0].as<float>(), bl[1].as<float>()};
                }
            }
            if (combat["burst_delay"]) {
                auto bd = combat["burst_delay"];
                if (bd.IsSequence() && bd.size() == 2) {
                    bp->combat.burstDelay = {bd[0].as<float>(), bd[1].as<float>()};
                }
            }
            if (combat["ammo_conservation"]) {
                bp->combat.ammoConservation = combat["ammo_conservation"].as<float>();
            }
            if (combat["reload_under_fire"]) {
                bp->combat.reloadUnderFire = combat["reload_under_fire"].as<bool>();
            }
            // Added in OPM - Phase 3 Task 3.1a
            //  Target selection parameters
            if (combat["target_lock_time"]) {
                bp->combat.targetLockTime = combat["target_lock_time"].as<float>();
            }
            if (combat["target_switch_threshold"]) {
                bp->combat.targetSwitchThreshold = combat["target_switch_threshold"].as<float>();
            }
            // Added in OPM - Phase 3 Task 3.1h
            //  Weapon preferences
            if (combat["weapon_preferences"]) {
                YAML::Node wp = combat["weapon_preferences"];
                if (wp["pistol"]) {
                    bp->weaponPreferences.pistol = wp["pistol"].as<float>();
                }
                if (wp["rifle"]) {
                    bp->weaponPreferences.rifle = wp["rifle"].as<float>();
                }
                if (wp["shotgun"]) {
                    bp->weaponPreferences.shotgun = wp["shotgun"].as<float>();
                }
                if (wp["sniper"]) {
                    bp->weaponPreferences.sniper = wp["sniper"].as<float>();
                }
                if (wp["smg"]) {
                    bp->weaponPreferences.smg = wp["smg"].as<float>();
                }
                if (wp["mg"]) {
                    bp->weaponPreferences.mg = wp["mg"].as<float>();
                }
            }
        }

        // === MOVEMENT ===
        if (profile["movement"]) {
            YAML::Node move = profile["movement"];
            if (move["speed_preference"]) {
                bp->movement.speedPreference = move["speed_preference"].as<float>();
            }
            if (move["crouch_frequency"]) {
                bp->movement.crouchFrequency = move["crouch_frequency"].as<float>();
            }
            if (move["jump_frequency"]) {
                bp->movement.jumpFrequency = move["jump_frequency"].as<float>();
            }
            if (move["strafe_usage"]) {
                bp->movement.strafeUsage = move["strafe_usage"].as<float>();
            }
            // Added in OPM - Phase 3 Task 3.1c
            if (move["path_deviation"]) {
                bp->movement.pathDeviation = move["path_deviation"].as<float>();
            }
        }

        // === AIM ===
        if (profile["aim"]) {
            YAML::Node aim = profile["aim"];
            if (aim["reaction_time"]) {
                auto rt = aim["reaction_time"];
                if (rt.IsSequence() && rt.size() == 2) {
                    bp->aim.reactionTime = {rt[0].as<float>(), rt[1].as<float>()};
                }
            }
            if (aim["tracking_smoothness"]) {
                bp->aim.trackingSmoothness = aim["tracking_smoothness"].as<float>();
            }
            if (aim["spread_multiplier"]) {
                bp->aim.spreadMultiplier = aim["spread_multiplier"].as<float>();
            }
            if (aim["headshot_bias"]) {
                bp->aim.headshotBias = aim["headshot_bias"].as<float>();
            }
            // Added in OPM - Phase 3 Task 3.1b (Gemini review)
            if (aim["aim_tolerance"]) {
                bp->aim.aimTolerance = aim["aim_tolerance"].as<float>();
            }
        }

        // === TACTICS ===
        if (profile["tactics"]) {
            YAML::Node tac = profile["tactics"];
            if (tac["cover_usage"]) {
                bp->tactics.coverUsage = tac["cover_usage"].as<float>();
            }
            if (tac["retreat_threshold"]) {
                bp->tactics.retreatThreshold = tac["retreat_threshold"].as<float>();
            }
            if (tac["flank_preference"]) {
                bp->tactics.flankPreference = tac["flank_preference"].as<float>();
            }
            if (tac["grenade_frequency"]) {
                bp->tactics.grenadeFrequency = tac["grenade_frequency"].as<float>();
            }
        }

        // === PERCEPTION ===
        if (profile["perception"]) {
            YAML::Node perc = profile["perception"];

            // Vision
            if (perc["vision"]) {
                YAML::Node vis = perc["vision"];
                if (vis["fov"]) {
                    bp->perception.vision.fov = vis["fov"].as<float>();
                }
                if (vis["range"]) {
                    bp->perception.vision.range = vis["range"].as<float>();
                }
                if (vis["peripheral_range"]) {
                    bp->perception.vision.peripheralRange = vis["peripheral_range"].as<float>();
                }
            }

            // Hearing
            if (perc["hearing"]) {
                YAML::Node hear = perc["hearing"];
                if (hear["range"]) {
                    bp->perception.hearing.range = hear["range"].as<float>();
                }
                if (hear["priority_threshold"]) {
                    bp->perception.hearing.priorityThreshold = hear["priority_threshold"].as<float>();
                }
            }
        }

        // === BEHAVIOR TREE ===
        if (profile["behavior_tree"]) {
            bp->behaviorTree = profile["behavior_tree"].as<std::string>();
        }

        // Apply default behavior tree if empty (before validation)
        // Changed in OPM - Moved default assignment from validation to construction
        if (bp->behaviorTree.empty()) {
            gi.Printf("WARNING: behavior_tree is empty in %s, defaulting to 'engage_enemy'\n", filepath);
            bp->behaviorTree = "engage_enemy";
        }

        // Validate the loaded profile
        if (!ValidateProfile(bp.get())) {
            gi.Printf("WARNING: Profile validation found issues in %s\n", filepath);
            // Don't delete - still usable with warnings
        }

        gi.DPrintf("Loaded bot profile: %s (%s)\n", bp->GetName().c_str(), filepath);
        return bp;

    } catch (const YAML::Exception& e) {
        gi.Printf("ERROR: YAML parse error in %s: %s\n", filepath, e.what());
        return nullptr;
    } catch (const std::exception& e) {
        gi.Printf("ERROR: Exception loading profile %s: %s\n", filepath, e.what());
        return nullptr;
    }
}

bool BotProfile::ValidateProfile(const BotProfile *profile)
{
    bool valid = true;

    // Helper lambda for range validation
    auto validateRange = [&](float value, float min, float max, const char *name)
    {
        if (value < min || value > max) {
            gi.Printf("WARNING: %s out of range [%.2f-%.2f]: %.2f\n", name, min, max, value);
            valid = false;
        }
    };

    // === VALIDATE PERSONALITY (0.0 - 1.0) ===
    validateRange(profile->personality.aggression, 0.0f, 1.0f, "aggression");
    validateRange(profile->personality.caution, 0.0f, 1.0f, "caution");
    validateRange(profile->personality.teamwork, 0.0f, 1.0f, "teamwork");
    validateRange(profile->personality.creativity, 0.0f, 1.0f, "creativity");

    // === VALIDATE COMBAT ===
    if (profile->combat.preferredRange < 0.0f) {
        gi.Printf("WARNING: preferred_range cannot be negative: %.2f\n", profile->combat.preferredRange);
        valid = false;
    }

    validateRange(profile->combat.fireDiscipline, 0.0f, 1.0f, "fire_discipline");
    validateRange(profile->combat.ammoConservation, 0.0f, 1.0f, "ammo_conservation");

    // Validate burst timing
    if (profile->combat.burstLength.first > profile->combat.burstLength.second) {
        gi.Printf(
            "WARNING: burst_length min (%.2f) > max (%.2f)\n",
            profile->combat.burstLength.first,
            profile->combat.burstLength.second
        );
        valid = false;
    }
    if (profile->combat.burstLength.first < 0.0f || profile->combat.burstLength.second < 0.0f) {
        gi.Printf("WARNING: burst_length values cannot be negative\n");
        valid = false;
    }

    if (profile->combat.burstDelay.first > profile->combat.burstDelay.second) {
        gi.Printf(
            "WARNING: burst_delay min (%.2f) > max (%.2f)\n",
            profile->combat.burstDelay.first,
            profile->combat.burstDelay.second
        );
        valid = false;
    }
    if (profile->combat.burstDelay.first < 0.0f || profile->combat.burstDelay.second < 0.0f) {
        gi.Printf("WARNING: burst_delay values cannot be negative\n");
        valid = false;
    }

    // === VALIDATE MOVEMENT ===
    if (profile->movement.speedPreference < 0.0f) {
        gi.Printf("WARNING: speed_preference cannot be negative: %.2f\n", profile->movement.speedPreference);
        valid = false;
    }

    validateRange(profile->movement.crouchFrequency, 0.0f, 1.0f, "crouch_frequency");
    validateRange(profile->movement.jumpFrequency, 0.0f, 1.0f, "jump_frequency");
    validateRange(profile->movement.strafeUsage, 0.0f, 1.0f, "strafe_usage");
    // Added in OPM - Phase 3 Task 3.1c
    validateRange(profile->movement.pathDeviation, 0.0f, 1.0f, "path_deviation");

    // === VALIDATE AIM ===
    if (profile->aim.reactionTime.first > profile->aim.reactionTime.second) {
        gi.Printf(
            "WARNING: reaction_time min (%.2f) > max (%.2f)\n",
            profile->aim.reactionTime.first,
            profile->aim.reactionTime.second
        );
        valid = false;
    }
    if (profile->aim.reactionTime.first < 0.0f || profile->aim.reactionTime.second < 0.0f) {
        gi.Printf("WARNING: reaction_time values cannot be negative\n");
        valid = false;
    }

    validateRange(profile->aim.trackingSmoothness, 0.0f, 1.0f, "tracking_smoothness");
    validateRange(profile->aim.headshotBias, 0.0f, 1.0f, "headshot_bias");

    if (profile->aim.spreadMultiplier < 0.0f) {
        gi.Printf("WARNING: spread_multiplier cannot be negative: %.2f\n", profile->aim.spreadMultiplier);
        valid = false;
    }

    // === VALIDATE TACTICS ===
    validateRange(profile->tactics.coverUsage, 0.0f, 1.0f, "cover_usage");
    validateRange(profile->tactics.retreatThreshold, 0.0f, 1.0f, "retreat_threshold");
    validateRange(profile->tactics.flankPreference, 0.0f, 1.0f, "flank_preference");
    validateRange(profile->tactics.grenadeFrequency, 0.0f, 1.0f, "grenade_frequency");

    // === VALIDATE PERCEPTION ===
    if (profile->perception.vision.fov <= 0.0f || profile->perception.vision.fov > 180.0f) {
        gi.Printf("WARNING: vision FOV out of range (0-180]: %.2f\n", profile->perception.vision.fov);
        valid = false;
    }

    if (profile->perception.vision.range < 0.0f) {
        gi.Printf("WARNING: vision range cannot be negative: %.2f\n", profile->perception.vision.range);
        valid = false;
    }

    validateRange(profile->perception.vision.peripheralRange, 0.0f, 1.0f, "peripheral_range");

    if (profile->perception.hearing.range < 0.0f) {
        gi.Printf("WARNING: hearing range cannot be negative: %.2f\n", profile->perception.hearing.range);
        valid = false;
    }

    validateRange(profile->perception.hearing.priorityThreshold, 0.0f, 1.0f, "hearing priority_threshold");

    // === VALIDATE BEHAVIOR TREE ===
    // Changed in OPM - Default assignment moved to LoadFromFile
    if (profile->behaviorTree.empty()) {
        gi.Printf("WARNING: behavior_tree is empty\n");
        valid = false;
    }

    return valid;
}

// ============================================================================
// GetWeaponPreference
// ============================================================================

/**
 * Added in OPM - Phase 3 Task 3.1h
 *  Returns weapon preference based on weapon class bitmask
 *
 * @param weaponClass Weapon class from weaponclass_e enum (can be combined with |)
 * @return Preference value 0.0-1.0, or 0.5 (neutral) if class unknown
 */
float BotProfile::GetWeaponPreference(int weaponClass) const
{
    // Handle single weapon class
    if (weaponClass & WEAPON_CLASS_PISTOL) {
        return weaponPreferences.pistol;
    }
    if (weaponClass & WEAPON_CLASS_RIFLE) {
        return weaponPreferences.rifle;
    }
    if (weaponClass & WEAPON_CLASS_SMG) {
        return weaponPreferences.smg;
    }
    if (weaponClass & WEAPON_CLASS_MG) {
        return weaponPreferences.mg;
    }
    
    // Note: shotgun and sniper don't have separate weapon classes in MOHAA
    // They use WEAPON_CLASS_RIFLE with different stats
    // Could be extended if needed
    
    return 0.5f; // Neutral preference for unknown classes
}
