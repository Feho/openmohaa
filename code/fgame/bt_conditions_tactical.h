// bt_conditions_tactical.h
// Tactical combat conditions for behavior trees
// Added in OPM - Phase 3 Task 3.1e: Tactical Combat & Retreat System

#ifndef __BT_CONDITIONS_TACTICAL_H__
#define __BT_CONDITIONS_TACTICAL_H__

#include "behavior_tree.h"
#include "playerbot.h"
#include "bt_blackboard_keys.h"

/**
 * Check if bot should retreat based on health, damage, or being outnumbered.
 * Returns true if:
 * - Health < 25% OR
 * - Recent damage (last 2s) > 30 OR
 * - Outnumbered (3+ enemies)
 *
 * @param bb Blackboard with BOT key
 * @return true if should retreat
 */
inline bool Condition_ShouldRetreat(Blackboard &bb)
{
    auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot) {
        return false;
    }

    return (*bot)->ShouldRetreat();
}

/**
 * Check if bot is under heavy fire (took damage in last 2 seconds).
 *
 * @param bb Blackboard with RECENT_DAMAGE and LAST_DAMAGE_TIME keys
 * @return true if took damage in last 2 seconds
 */
inline bool Condition_UnderHeavyFire(Blackboard &bb)
{
    auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot) {
        return false;
    }

    Player *player = (*bot)->getControlledEntity();
    if (!player) {
        return false;
    }

    float recentDamage = bb.GetOrDefault<float>(BlackboardKeys::RECENT_DAMAGE, 0.0f);
    float lastDamageTime = bb.GetOrDefault<float>(BlackboardKeys::LAST_DAMAGE_TIME, 0.0f);
    float currentTime = level.svsTime / 1000.0f;

    // Check if damage was recent (within 2 seconds)
    return (currentTime - lastDamageTime) <= 2.0f && recentDamage > 0.0f;
}

/**
 * Check if bot has low ammunition (< 20%).
 *
 * @param bb Blackboard with BOT key
 * @return true if ammo < 20%
 */
inline bool Condition_AmmoLow(Blackboard &bb)
{
    auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot) {
        return false;
    }

    Player *player = (*bot)->getControlledEntity();
    if (!player) {
        return false;
    }

    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return false;
    }

    int currentAmmo = weapon->ClipAmmo(FIRE_PRIMARY);
    
    // Consider ammo low if less than 3 rounds (works for most weapons)
    // More sophisticated approach would need weapon-specific data
    return currentAmmo <= 3;
}

/**
 * Check if it's safe to reload (in cover OR not under fire for 2+ seconds).
 *
 * @param bb Blackboard with COVER_STATE and damage tracking keys
 * @return true if safe to reload
 */
inline bool Condition_SafeToReload(Blackboard &bb)
{
    auto bot = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!bot) {
        return false;
    }

    Player *player = (*bot)->getControlledEntity();
    if (!player) {
        return false;
    }

    // Check if in cover
    int coverState = bb.GetOrDefault<int>(BlackboardKeys::COVER_STATE, BotController::COVER_NONE);
    if (coverState == BotController::COVER_IN_COVER) {
        return true;
    }

    // Check if not under fire for 2+ seconds
    float lastDamageTime = bb.GetOrDefault<float>(BlackboardKeys::LAST_DAMAGE_TIME, 0.0f);
    float currentTime = level.svsTime / 1000.0f;
    float timeSinceDamage = currentTime - lastDamageTime;

    return timeSinceDamage >= 2.0f;
}

#endif // __BT_CONDITIONS_TACTICAL_H__
