// bt_actions_tactical.cpp
// Tactical combat actions implementation
// Added in OPM - Phase 3 Task 3.1e: Tactical Combat & Retreat System

#include "bt_actions_tactical.h"
#include "bt_blackboard_keys.h"
#include "playerbot.h"
#include "player.h"
#include "weapon.h"

BTNode::Status Action_TacticalRetreat(Blackboard &bb, float deltaTime)
{
    auto botOpt = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!botOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot = *botOpt;
    Player *player = bot->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    BotMovement &movement = bot->GetMovement();

    // Check if we have a retreat position set
    auto retreatPosOpt = bb.TryGet<Vector>(BlackboardKeys::RETREAT_POSITION);
    
    if (!retreatPosOpt) {
        // First frame: find retreat position
        Vector playerPos = player->origin;
        Vector retreatDirection = vec_zero;

        // Get enemy position to retreat away from
        auto enemyOpt = bb.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
        if (enemyOpt && *enemyOpt) {
            Sentient *enemy = *enemyOpt;
            Vector toEnemy = enemy->origin - playerPos;
            toEnemy.normalize();
            
            // Retreat in opposite direction
            retreatDirection = -toEnemy;
        } else {
            // No enemy - retreat backwards
            Vector angles = player->GetViewAngles();
            angles.AngleVectors(&retreatDirection, nullptr, nullptr);
            retreatDirection = -retreatDirection;
        }

        // Find safe position 512 units away
        float retreatDistance = 512.0f;
        Vector targetPos = playerPos + retreatDirection * retreatDistance;

        // Trace to ensure position is reachable
        trace_t trace = G_Trace(
            playerPos,
            player->mins,
            player->maxs,
            targetPos,
            player,
            MASK_PLAYERSOLID,
            qfalse,
            "Action_TacticalRetreat"
        );

        if (trace.fraction > 0.5f) {
            // Found valid retreat position
            Vector retreatPos = trace.endpos;
            bb.Set<Vector>(BlackboardKeys::RETREAT_POSITION, retreatPos);
            movement.MoveTo(retreatPos);
            return BTNode::Status::RUNNING;
        } else {
            // Can't find retreat position
            return BTNode::Status::FAILURE;
        }
    }

    // Check if reached retreat position
    Vector retreatPos = *retreatPosOpt;
    float distSq = (player->origin - retreatPos).lengthSquared();
    
    if (distSq < BotConstants::WAYPOINT_REACHED_DISTANCE * BotConstants::WAYPOINT_REACHED_DISTANCE) {
        // Reached safety
        bb.Remove(BlackboardKeys::RETREAT_POSITION);
        return BTNode::Status::SUCCESS;
    }

    // Still retreating
    if (!movement.IsMoving()) {
        // Movement failed, try again
        movement.MoveTo(retreatPos);
    }

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_SafeReload(Blackboard &bb, float deltaTime)
{
    auto botOpt = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!botOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot = *botOpt;
    Player *player = bot->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return BTNode::Status::FAILURE;
    }

    // Check if already reloading
    auto reloadStartOpt = bb.TryGet<float>(BlackboardKeys::RELOAD_START_TIME);
    
    if (!reloadStartOpt) {
        // First frame: initiate reload
        float currentTime = level.svsTime / 1000.0f;
        bb.Set<float>(BlackboardKeys::RELOAD_START_TIME, currentTime);
        
        // Trigger reload command
        usercmd_t &botcmd = bot->GetBotCmd();
        botcmd.buttons |= BUTTON_USE;
        
        return BTNode::Status::RUNNING;
    }

    // Check if reload complete
    float reloadStartTime = *reloadStartOpt;
    float currentTime = level.svsTime / 1000.0f;
    float reloadDuration = currentTime - reloadStartTime;

    // Assume reload takes 2 seconds (weapon-specific would be better)
    if (reloadDuration >= 2.0f) {
        bb.Remove(BlackboardKeys::RELOAD_START_TIME);
        return BTNode::Status::SUCCESS;
    }

    // Still reloading - continue reload command
    usercmd_t &botcmd = bot->GetBotCmd();
    botcmd.buttons |= BUTTON_USE;

    return BTNode::Status::RUNNING;
}

BTNode::Status Action_SuppressFire(Blackboard &bb, float deltaTime)
{
    auto botOpt = bb.TryGet<BotController *>(BlackboardKeys::BOT);
    if (!botOpt) {
        return BTNode::Status::FAILURE;
    }

    BotController *bot = *botOpt;
    Player *player = bot->getControlledEntity();
    if (!player) {
        return BTNode::Status::FAILURE;
    }

    Weapon *weapon = player->GetActiveWeapon(WEAPON_MAIN);
    if (!weapon) {
        return BTNode::Status::FAILURE;
    }

    // Get last known enemy position
    auto lastKnownPosOpt = bb.TryGet<Vector>(BlackboardKeys::LAST_KNOWN_ENEMY_POS);
    if (!lastKnownPosOpt) {
        return BTNode::Status::FAILURE;
    }

    Vector lastKnownPos = *lastKnownPosOpt;

    // Check if suppression started
    auto suppressStartOpt = bb.TryGet<float>(BlackboardKeys::SUPPRESS_START_TIME);
    
    if (!suppressStartOpt) {
        // First frame: start suppression
        float currentTime = level.svsTime / 1000.0f;
        bb.Set<float>(BlackboardKeys::SUPPRESS_START_TIME, currentTime);
    }

    float suppressStartTime = bb.GetOrDefault<float>(BlackboardKeys::SUPPRESS_START_TIME, 0.0f);
    float currentTime = level.svsTime / 1000.0f;
    float suppressDuration = currentTime - suppressStartTime;

    // Suppress for 2-3 seconds
    BotProfile *profile = bot->GetProfile();
    float maxSuppressDuration = 2.5f;
    if (profile) {
        // More aggressive bots suppress longer
        maxSuppressDuration = 2.0f + profile->GetAggression();
    }

    if (suppressDuration >= maxSuppressDuration) {
        // Suppression complete
        bb.Remove(BlackboardKeys::SUPPRESS_START_TIME);
        return BTNode::Status::SUCCESS;
    }

    // Aim at last known position
    BotRotation &rotation = bot->GetRotation();
    rotation.AimAt(lastKnownPos);

    // Fire weapon
    usercmd_t &botcmd = bot->GetBotCmd();
    botcmd.buttons |= BUTTON_ATTACKLEFT;

    return BTNode::Status::RUNNING;
}
