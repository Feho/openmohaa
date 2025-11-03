// bt_conditions_grenade.cpp
// Grenade-related conditions for behavior trees
// Added in OPM - Phase 3 Task 3.1g: Grenade System

#include "bt_conditions_grenade.h"
#include "bt_blackboard_keys.h"
#include "bt_combat_helpers.h"
#include "playerbot.h"
#include "player.h"
#include "bot_profile.h"
#include "perception.h"

// ============================================================================
// Condition_HasGrenades
// ============================================================================

bool Condition_HasGrenades(Blackboard &bb)
{
    auto playerOpt = bb.TryGet<Player *>(BlackboardKeys::PLAYER);
    if (!playerOpt) {
        return false;
    }

    Player *player = *playerOpt;
    if (!player) {
        return false;
    }

    return player->HasGrenades();
}

// ============================================================================
// Condition_EnemiesClustered
// ============================================================================

bool Condition_EnemiesClustered(Blackboard &bb)
{
    auto perceptionOpt = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    // Need at least 2 enemies
    if (perception->visibleEnemies.size() < 2) {
        return false;
    }

    // Check if clustered
    return BT::Combat::AreEnemiesClustered(perception->visibleEnemies, BotConstants::GRENADE_CLUSTER_RADIUS);
}

// ============================================================================
// Condition_ShouldThrowGrenade
// ============================================================================

bool Condition_ShouldThrowGrenade(Blackboard &bb)
{
    // 1. Must have grenades
    if (!Condition_HasGrenades(bb)) {
        return false;
    }

    // 2. Enemies must be clustered
    if (!Condition_EnemiesClustered(bb)) {
        return false;
    }

    auto perceptionOpt = bb.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    auto profileOpt = bb.TryGet<BotProfile *>(BlackboardKeys::PROFILE);

    if (!perceptionOpt || !profileOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    BotProfile *profile = *profileOpt;

    if (!perception || !profile) {
        return false;
    }

    // 3. Calculate cluster center for ally safety check
    Vector clusterCenter = BT::Combat::CalculateClusterCenter(perception->visibleEnemies);

    // 4. Check if any allies near blast zone
    if (BT::Combat::HasAlliesNearPosition(clusterCenter, BotConstants::GRENADE_ALLY_SAFETY, perception)) {
        return false; // Unsafe - allies too close
    }

    // 5. Check cooldown
    auto lastGrenadeTimeOpt = bb.TryGet<float>(BlackboardKeys::LAST_GRENADE_TIME);
    if (lastGrenadeTimeOpt) {
        float lastGrenadeTime = *lastGrenadeTimeOpt;
        float timeSinceGrenade = (level.svsTime - lastGrenadeTime) / 1000.0f; // Convert to seconds

        if (timeSinceGrenade < BotConstants::GRENADE_COOLDOWN) {
            return false; // Cooldown not expired
        }
    }

    // 6. Profile grenade frequency check
    float grenadeFrequency = profile->GetGrenadeFrequency();
    if (G_Random() > grenadeFrequency) {
        return false; // Random check failed based on personality
    }

    // All checks passed
    return true;
}
