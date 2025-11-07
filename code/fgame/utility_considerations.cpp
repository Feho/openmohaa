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

    // EnemyDistance: Distance to closest enemy (normalized)
    if (std::strcmp(considerationName, "EnemyDistance") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 1.0f; // Max distance if no enemy
        }
        // Normalize distance: 0-1000 units -> 0.0-1.0
        float normalized = std::clamp(enemy->distance / 1000.0f, 0.0f, 1.0f);
        return normalized;
    }

    // EnemyCount: Number of visible enemies (normalized)
    if (std::strcmp(considerationName, "EnemyCount") == 0) {
        // Normalize: 0-5 enemies -> 0.0-1.0
        float count      = static_cast<float>(perception.visibleEnemies.size());
        float normalized = std::clamp(count / 5.0f, 0.0f, 1.0f);
        return normalized;
    }

    // Health: Bot's current health ratio
    if (std::strcmp(considerationName, "Health") == 0) {
        float health    = bot->health;
        float maxHealth = bot->max_health;
        if (maxHealth <= 0.0f) {
            return 0.0f;
        }
        return std::clamp(health / maxHealth, 0.0f, 1.0f);
    }

    // Ammo: Current ammo ratio for main weapon
    if (std::strcmp(considerationName, "Ammo") == 0) {
        Weapon *weapon = bot->GetActiveWeapon(WEAPON_MAIN);
        if (!weapon) {
            return 0.0f;
        }
        int currentAmmo = weapon->AmmoAvailable(FIRE_PRIMARY);
        int clipSize    = weapon->GetClipSize(FIRE_PRIMARY);
        if (clipSize <= 0) {
            return 1.0f; // Assume full if no clip system
        }
        return std::clamp(static_cast<float>(currentAmmo) / static_cast<float>(clipSize), 0.0f, 1.0f);
    }

    // InCover: Whether bot is currently in cover
    if (std::strcmp(considerationName, "InCover") == 0) {
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
    if (std::strcmp(considerationName, "CoverNearby") == 0) {
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
        // Normalize: 0-500 units -> 1.0-0.0 (closer = better)
        float normalized = 1.0f - std::clamp(coverDistance / 500.0f, 0.0f, 1.0f);
        return normalized;
    }

    // EnemyDistracted: Whether closest enemy is targeting someone else
    if (std::strcmp(considerationName, "EnemyDistracted") == 0) {
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
    if (std::strcmp(considerationName, "FlankPath") == 0) {
        const EnemyInfo *enemy = perception.GetClosestEnemy();
        if (!enemy || !enemy->entity) {
            return 0.0f;
        }
        Vector flankPos   = BT::Combat::CalculateFlankPosition(bot->origin, enemy->position, 256.0f);
        bool   pathExists = BT::Combat::PathExistsTo(bot, flankPos);
        return pathExists ? 1.0f : 0.0f;
    }

    // Aggression: Bot's personality trait
    if (std::strcmp(considerationName, "Aggression") == 0) {
        return std::clamp(profile->GetAggression(), 0.0f, 1.0f);
    }

    // Caution: Bot's personality trait
    if (std::strcmp(considerationName, "Caution") == 0) {
        return std::clamp(profile->GetCaution(), 0.0f, 1.0f);
    }

    // Teamwork: Bot's personality trait
    if (std::strcmp(considerationName, "Teamwork") == 0) {
        return std::clamp(profile->GetTeamwork(), 0.0f, 1.0f);
    }

    // Creativity: Bot's personality trait
    if (std::strcmp(considerationName, "Creativity") == 0) {
        return std::clamp(profile->GetCreativity(), 0.0f, 1.0f);
    }

    // AlliesNearby: Number of nearby visible allies
    if (std::strcmp(considerationName, "AlliesNearby") == 0) {
        // Normalize: 0-4 allies -> 0.0-1.0
        float count      = static_cast<float>(perception.visibleAllies.size());
        float normalized = std::clamp(count / 4.0f, 0.0f, 1.0f);
        return normalized;
    }

    // Unknown consideration - return 0.0f
    return 0.0f;
}
