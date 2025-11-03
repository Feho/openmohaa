// bt_conditions_investigation.cpp
// Behavior tree conditions for investigation system
// Added in OPM - Phase 3 Task 3.2

#include "bt_conditions_investigation.h"
#include "bt_blackboard_keys.h"
#include "perception.h"
#include "g_local.h"

bool Condition_HasHighConfidenceMemory_Check(Blackboard &blackboard)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    for (const auto &memory : perception->knownEnemies) {
        if (memory.confidenceLevel > 0.5f) {
            return true;
        }
    }

    return false;
}

bool Condition_HasInterestingSound_Check(Blackboard &blackboard)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    const AudioEvent *sound = perception->GetLoudestSound();
    if (!sound) {
        return false;
    }

    // Only investigate important sounds
    bool isImportantType = (sound->type == AI_EVENT_WEAPON_FIRE || sound->type == AI_EVENT_FOOTSTEP
                            || sound->type == AI_EVENT_GRENADE);

    return isImportantType && sound->priority > 0.5f;
}

bool Condition_ReachedInvestigationTarget_Check(Blackboard &blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_INVESTIGATION_TARGET);
    return reachedOpt && *reachedOpt;
}

bool Condition_InvestigationTimedOut_Check(Blackboard &blackboard)
{
    auto startTimeOpt = blackboard.TryGet<float>(BlackboardKeys::INVESTIGATION_START_TIME);
    if (!startTimeOpt) {
        return false;
    }

    float startTime = *startTimeOpt;
    float elapsed   = static_cast<float>(level.svsTime) - startTime;

    return elapsed > 10000.0f; // 10 seconds in milliseconds
}

bool Condition_ReachedSoundLocation_Check(Blackboard &blackboard)
{
    auto reachedOpt = blackboard.TryGet<bool>(BlackboardKeys::REACHED_SOUND_LOCATION);
    return reachedOpt && *reachedOpt;
}

bool Condition_HasVisibleEnemy_Check(Blackboard &blackboard)
{
    auto perceptionOpt = blackboard.TryGet<PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionOpt) {
        return false;
    }

    PerceptionSnapshot *perception = *perceptionOpt;
    if (!perception) {
        return false;
    }

    return perception->HasVisibleEnemy();
}
