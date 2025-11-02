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

// bt_actions_aim.cpp
// Aiming system implementation
// Added in OPM - Phase 3 Task 3.1b

#include "bt_actions_aim.h"
#include "bt_blackboard_keys.h"
#include "bot_profile.h"
#include "g_local.h"

void Action_AimAtTarget::Reset()
{
    lastStatus = Status::FAILURE;
}

BTNode::Status Action_AimAtTarget::Execute(Blackboard &blackboard, float deltaTime)
{
    // Get required data from blackboard
    auto targetOpt = blackboard.TryGet<Sentient *>(BlackboardKeys::SELECTED_TARGET);
    auto botOpt    = blackboard.TryGet<BotController *>(BlackboardKeys::BOT);
    auto playerOpt = blackboard.TryGet<Player *>(BlackboardKeys::PLAYER);
    auto profileOpt = blackboard.TryGet<BotProfile *>(BlackboardKeys::PROFILE);

    if (!targetOpt || !botOpt || !playerOpt || !profileOpt) {
        return Status::FAILURE;
    }

    Sentient    *target  = *targetOpt;
    BotController *bot   = *botOpt;
    Player      *player  = *playerOpt;
    BotProfile  *profile = *profileOpt;

    if (!target || !bot || !player || !profile) {
        blackboard.Set<bool>(BlackboardKeys::IS_AIMED_AT_TARGET, false);
        return Status::FAILURE;
    }

    // Get cached eye tag or find it
    int eyesTag = -1;
    auto eyesTagOpt = blackboard.TryGet<int>(BlackboardKeys::ENEMY_EYES_TAG);
    if (eyesTagOpt) {
        eyesTag = *eyesTagOpt;
    }

    if (eyesTag == -1) {
        eyesTag = gi.Tag_NumForName(target->edict->tiki, "eyes bone");
        blackboard.Set<int>(BlackboardKeys::ENEMY_EYES_TAG, eyesTag);
    }

    // Determine target position (eyes or center)
    Vector targetPos;
    if (eyesTag != -1) {
        orientation_t eyes_or;
        target->GetTag(eyesTag, &eyes_or);
        targetPos = eyes_or.origin;
    } else {
        // No eye tag, aim at upper body with headshot bias
        float headshotBias = profile->GetHeadshotBias();
        float heightOffset = target->mins.z + (target->maxs.z - target->mins.z) * (0.7f + headshotBias * 0.3f);
        targetPos = target->origin;
        targetPos.z += heightOffset;
    }

    // Update aim offset periodically for human-like inaccuracy
    float currentTime = level.svsTime;
    float lastUpdateTime = 0.0f;
    auto lastUpdateOpt = blackboard.TryGet<float>(BlackboardKeys::AIM_UPDATE_TIME);
    if (lastUpdateOpt) {
        lastUpdateTime = *lastUpdateOpt;
    }

    Vector aimOffset = vec_zero;
    auto aimOffsetOpt = blackboard.TryGet<Vector>(BlackboardKeys::AIM_OFFSET);
    if (aimOffsetOpt) {
        aimOffset = *aimOffsetOpt;
    }

    if (currentTime - lastUpdateTime >= AIM_UPDATE_INTERVAL) {
        // Calculate new aim offset based on profile spread multiplier
        float spreadMult = profile->GetSpreadMultiplier();
        float headshotBias = profile->GetHeadshotBias();

        if (eyesTag != -1) {
            // Have eye tag, aim near head with some inaccuracy
            float bboxWidth = (target->maxs.x - target->mins.x) * 0.5f;
            float bboxDepth = (target->maxs.y - target->mins.y) * 0.5f;
            float bboxHeight = target->maxs.z * 0.5f;

            aimOffset.x = G_CRandom(bboxWidth * spreadMult);
            aimOffset.y = G_CRandom(bboxDepth * spreadMult);
            aimOffset.z = -G_Random(bboxHeight * spreadMult);
        } else {
            // No eye tag, aim at body with spread
            float bboxWidth = (target->maxs.x - target->mins.x) * 0.5f;
            float bboxDepth = (target->maxs.y - target->mins.y) * 0.5f;

            aimOffset.x = G_CRandom(bboxWidth * spreadMult);
            aimOffset.y = G_CRandom(bboxDepth * spreadMult);
            
            // Apply headshot bias - more accuracy means less vertical spread
            float verticalRange = target->viewheight - 16.0f;
            aimOffset.z = 16.0f + G_Random(verticalRange * spreadMult * (1.0f - headshotBias));
        }

        blackboard.Set<Vector>(BlackboardKeys::AIM_OFFSET, aimOffset);
        blackboard.Set<float>(BlackboardKeys::AIM_UPDATE_TIME, currentTime);
    }

    // Calculate final aim position with offset
    Vector finalAimPos = targetPos + aimOffset;

    // Get bot rotation system and aim at target
    BotRotation &rotation = bot->GetRotation();
    rotation.AimAt(finalAimPos);

    // Check if we're aimed within tolerance
    Vector toTarget = finalAimPos - player->origin;
    VectorNormalizeFast(toTarget);

    Vector forward;
    AngleVectors(player->GetViewAngles(), forward, NULL, NULL);

    float dotProduct = DotProduct(forward, toTarget);
    float angleToTarget = RAD2DEG(acos(dotProduct));

    // Changed in OPM - Phase 3 Task 3.1b (Gemini review)
    //  Use profile-configured aim tolerance instead of hardcoded constant
    float aimTolerance = profile->GetAimTolerance();
    bool isAimed = (angleToTarget <= aimTolerance);
    blackboard.Set<bool>(BlackboardKeys::IS_AIMED_AT_TARGET, isAimed);

    if (isAimed) {
        return Status::SUCCESS;
    } else {
        return Status::RUNNING;
    }
}
