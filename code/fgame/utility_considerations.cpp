#include "g_local.h"
#include "utility_considerations.h"
#include "player.h"
#include "bot_profile.h"
#include "perception.h"
#include "playerbot.h"
#include "weapon.h"
#include "navigate.h"
#include "bt_combat_helpers.h"
#include <cstring>
#include <algorithm>

// Added in OPM
//  Constants for consideration normalization (avoids magic numbers)
namespace ConsiderationConstants
{
    constexpr float MAX_ENEMY_DISTANCE_NORM = 1000.0f; // Maximum enemy distance for normalization
    constexpr float MAX_ENEMY_COUNT_NORM    = 5.0f;    // Maximum enemy count for normalization
    constexpr float MAX_COVER_DISTANCE_NORM = 500.0f;  // Maximum cover distance for normalization
    constexpr float MAX_ALLY_COUNT_NORM     = 4.0f;    // Maximum ally count for normalization
    constexpr float GUNFIRE_LOUDNESS_MIN    = 0.5f;    // Minimum loudness to consider gunfire
    constexpr float GUNFIRE_LOUDNESS_SCALE  = 2.0f;    // Scale factor for gunfire normalization
    constexpr float DAMAGE_TIME_WINDOW      = 2.0f;    // Time window for recent damage (seconds)
} // namespace ConsiderationConstants

// Helper function to get bot controller from player entity
// Added in OPM - Phase 3 Task 3.4 Commit 3
//  Need access to BotController for IsInCover check
static BotController *GetBotController(const Player *player)
{
    if (!player) {
        return nullptr;
    }
    return botManager.getControllerManager().findController((Entity *)player);
}

float UtilityConsiderations::ExtractConsideration(
    const char *considerationName, const PerceptionSnapshot& perception, const Player *bot, const BotProfile *profile
)
{
    if (!considerationName || !bot || !profile) {
        return 0.0f;
    }

    // Changed in OPM
    //  Convert independent if statements to if/else if chain for early exit optimization

    // EnemyDistance: Distance to closest enemy (normalized)
    if (std::strcmp(considerationName, "EnemyDistance") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 1.0f; // Max distance if no enemy
        }
        float normalized = std::clamp(enemy->distance / ConsiderationConstants::MAX_ENEMY_DISTANCE_NORM, 0.0f, 1.0f);
        return normalized;
    }

    // EnemyCount: Number of visible enemies (normalized)
    else if (std::strcmp(considerationName, "EnemyCount") == 0) {
        float count      = static_cast<float>(perception.visibleEnemies.size());
        float normalized = std::clamp(count / ConsiderationConstants::MAX_ENEMY_COUNT_NORM, 0.0f, 1.0f);
        return normalized;
    }

    // Health: Bot's current health ratio
    else if (std::strcmp(considerationName, "Health") == 0) {
        float health    = bot->health;
        float maxHealth = bot->max_health;
        if (maxHealth <= 0.0f) {
            return 0.0f;
        }
        return std::clamp(health / maxHealth, 0.0f, 1.0f);
    }

    // Ammo: Current ammo ratio for main weapon
    // Fixed in OPM
    //  Use clip ammo instead of total ammo inventory
    else if (std::strcmp(considerationName, "Ammo") == 0) {
        Weapon *weapon = bot->GetActiveWeapon(WEAPON_MAIN);
        if (!weapon) {
            return 0.0f;
        }

        // Use clip ammo, not total ammo
        int currentAmmo = bot->client->ps.stats[STAT_CLIPAMMO];
        int clipSize    = weapon->GetClipSize(FIRE_PRIMARY);

        if (clipSize <= 0) {
            return 1.0f; // Weapon doesn't use clips
        }

        return std::clamp(static_cast<float>(currentAmmo) / static_cast<float>(clipSize), 0.0f, 1.0f);
    }

    // InCover: Whether bot is currently in cover
    else if (std::strcmp(considerationName, "InCover") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 0.0f;
        }
        BotController *controller = GetBotController(bot);
        if (!controller) {
            return 0.0f;
        }
        bool inCover = controller->CheckCover(bot->origin, enemy->position);
        return inCover ? 1.0f : 0.0f;
    }

    // CoverNearby: Availability of cover points near bot
    else if (std::strcmp(considerationName, "CoverNearby") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 0.0f;
        }
        Vector    botPos    = bot->origin;
        PathNode *coverNode = PathSearch::FindNearestCover((Entity *)bot, botPos, enemy->entity);
        if (!coverNode) {
            return 0.0f;
        }
        // Calculate distance to cover
        float coverDistance = (coverNode->origin - botPos).length();
        float normalized =
            1.0f - std::clamp(coverDistance / ConsiderationConstants::MAX_COVER_DISTANCE_NORM, 0.0f, 1.0f);
        return normalized;
    }

    // EnemyDistracted: Whether closest enemy is targeting someone else
    else if (std::strcmp(considerationName, "EnemyDistracted") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 0.0f;
        }
        // Check if enemy is targeting this bot
        Sentient *enemySentient = enemy->entity;
        if (enemySentient->m_Enemy == bot) {
            return 0.0f; // Enemy is focused on us
        }
        return 1.0f; // Enemy is distracted
    }

    // FlankPath: Availability of flanking route
    else if (std::strcmp(considerationName, "FlankPath") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 0.0f;
        }
        Vector flankPos   = BT::Combat::CalculateFlankPosition(bot->origin, enemy->position, 256.0f);
        bool   pathExists = BT::Combat::PathExistsTo(bot, flankPos);
        return pathExists ? 1.0f : 0.0f;
    }

    // Aggression: Bot's personality trait
    else if (std::strcmp(considerationName, "Aggression") == 0) {
        return std::clamp(profile->GetAggression(), 0.0f, 1.0f);
    }

    // Caution: Bot's personality trait
    else if (std::strcmp(considerationName, "Caution") == 0) {
        return std::clamp(profile->GetCaution(), 0.0f, 1.0f);
    }

    // Teamwork: Bot's personality trait
    else if (std::strcmp(considerationName, "Teamwork") == 0) {
        return std::clamp(profile->GetTeamwork(), 0.0f, 1.0f);
    }

    // Creativity: Bot's personality trait
    else if (std::strcmp(considerationName, "Creativity") == 0) {
        return std::clamp(profile->GetCreativity(), 0.0f, 1.0f);
    }

    // AlliesNearby: Number of nearby visible allies
    else if (std::strcmp(considerationName, "AlliesNearby") == 0) {
        float count      = static_cast<float>(perception.visibleAllies.size());
        float normalized = std::clamp(count / ConsiderationConstants::MAX_ALLY_COUNT_NORM, 0.0f, 1.0f);
        return normalized;
    }

    // Added in OPM - Phase 3 Task 3.4 Fix
    //  Audio perception considerations for gunfire reaction

    // RecentGunfire: Has bot heard recent gunfire/combat sounds
    else if (std::strcmp(considerationName, "RecentGunfire") == 0) {
        if (perception.recentSounds.empty()) {
            return 0.0f;
        }

        // Check if any recent sound is loud enough to be gunfire
        // Higher loudness = more likely to be gunfire
        float maxLoudness = 0.0f;
        for (const auto& sound : perception.recentSounds) {
            if (sound.loudness > maxLoudness) {
                maxLoudness = sound.loudness;
            }
        }

        // Normalize: 0.5-1.0 loudness considered gunfire
        if (maxLoudness < ConsiderationConstants::GUNFIRE_LOUDNESS_MIN) {
            return 0.0f;
        }

        return std::clamp(
            (maxLoudness - ConsiderationConstants::GUNFIRE_LOUDNESS_MIN)
                * ConsiderationConstants::GUNFIRE_LOUDNESS_SCALE,
            0.0f,
            1.0f
        );
    }

    // UnderFire: Is bot taking damage recently
    else if (std::strcmp(considerationName, "UnderFire") == 0) {
        // Check if bot has taken damage in last 2 seconds
        if (!bot->client) {
            return 0.0f;
        }

        float timeSinceDamage = (level.svsTime - bot->client->ps.stats[STAT_LAST_PAIN]) / 1000.0f;

        // Normalize: 0-2 seconds ago -> 1.0-0.0
        if (timeSinceDamage > ConsiderationConstants::DAMAGE_TIME_WINDOW) {
            return 0.0f;
        }

        return 1.0f - (timeSinceDamage / ConsiderationConstants::DAMAGE_TIME_WINDOW);
    }

    // Unknown consideration - return 0.0f
    return 0.0f;
}
