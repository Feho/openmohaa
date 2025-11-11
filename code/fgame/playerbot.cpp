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
// playerbot.cpp: Main bot update and think logic

#include "g_local.h"
#include "playerbot.h"
#include "bot_debug_viz.h"

// Added in OPM - Phase 2B Task 2B.4
//  Include behavior tree loader for LoadProfile
#include "bt_yaml_loader.h"
// Added in OPM - Phase 3 Task 3.1f
//  Include blackboard keys for combat tree assembly
#include "bt_blackboard_keys.h"
// Added in OPM - Phase 3 Task 3.4 Commit 5 Fix
//  Include perception for minimal snapshot creation
#include "perception.h"

// We assume that we have limited access to the server-side
// and that most logic come from the playerstate_s structure

extern cvar_t *bot_manualmove;

// Added in OPM - Phase 3 Task 3.4 Fix
//  Static members for shared utility evaluator
UtilityEvaluator *BotController::s_sharedUtilityEvaluator = nullptr;
bool              BotController::s_utilityConfigLoaded    = false;

void BotController::UpdateBotStates(void)
{
    if (bot_manualmove->integer) {
        memset(&m_botCmd, 0, sizeof(usercmd_t));
        return;
    }

    m_botCmd.serverTime = level.svsTime;

    if (!controlledEnt->client->pers.dm_primary[0]) {
        Event *event;

        //
        // Primary weapon
        //
        event = new Event(EV_Player_PrimaryDMWeapon);
        event->AddString("auto");

        controlledEnt->ProcessEvent(event);
    }

    if (controlledEnt->GetTeam() == TEAM_NONE || controlledEnt->GetTeam() == TEAM_SPECTATOR) {
        float time;

        // Add some delay to avoid telefragging
        time = controlledEnt->entnum / 20.0;

        if (controlledEnt->EventPending(EV_Player_AutoJoinDMTeam)) {
            return;
        }

        //
        // Team
        //
        controlledEnt->PostEvent(EV_Player_AutoJoinDMTeam, time);
        return;
    }

    if (controlledEnt->IsDead() || controlledEnt->IsSpectator()) {
        // The bot should respawn
        m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        return;
    }

    m_botCmd.buttons |= BUTTON_RUN;

    m_botEyes.ofs[0]    = 0;
    m_botEyes.ofs[1]    = 0;
    m_botEyes.ofs[2]    = controlledEnt->viewheight;
    m_botEyes.angles[0] = 0;
    m_botEyes.angles[1] = 0;

    CheckStates();

    movement.MoveThink(m_botCmd);
    rotation.TurnThink(m_botCmd, m_botEyes);
    CheckUse();

    CheckValidWeapon();
}

// Added in OPM - Phase 2B Task 2B.4
//  Load bot profile and behavior tree
void BotController::LoadProfile(const char *profileName)
{
    if (!profileName || !profileName[0]) {
        gi.Printf("ERROR: LoadProfile called with empty profile name\n");
        return;
    }

    // Build profile file path
    char profilePath[256];
    Com_sprintf(profilePath, sizeof(profilePath), "profiles/%s.yaml", profileName);

    // Load profile
    profile = BotProfile::LoadFromFile(profilePath);
    if (!profile) {
        gi.Printf(
            "ERROR: Failed to load profile '%s' for bot %d, using default\n",
            profileName,
            controlledEnt ? controlledEnt->entnum : -1
        );
        // Try to load default balanced profile
        profile = BotProfile::LoadFromFile("profiles/balanced.yaml");
        if (!profile) {
            gi.Printf("CRITICAL ERROR: Could not load default balanced profile\n");
            return;
        }
    }

    // Load behavior tree if BT system is enabled
    if (g_bot_use_new_ai_system->integer) {
        const std::string& treeName = profile->GetBehaviorTree();
        char               treePath[256];
        Com_sprintf(treePath, sizeof(treePath), "behaviors/%s.yaml", treeName.c_str());

        behaviorTree = BTYamlLoader::LoadFromFile(treePath);
        if (!behaviorTree) {
            gi.Printf(
                "ERROR: Failed to load behavior tree '%s' for bot %d\n",
                treeName.c_str(),
                controlledEnt ? controlledEnt->entnum : -1
            );
        } else {
            gi.DPrintf(
                "Bot %d loaded profile '%s' with tree '%s'\n",
                controlledEnt ? controlledEnt->entnum : -1,
                profileName,
                treeName.c_str()
            );
        }
    }

    // Added in OPM - Phase 3 Task 3.4 Commit 5
    //  Load utility AI configuration once globally
    // Changed in OPM
    //  Moved to shared static evaluator to avoid redundant YAML parsing
    if (!s_utilityConfigLoaded) {
        if (!s_sharedUtilityEvaluator) {
            s_sharedUtilityEvaluator = new UtilityEvaluator();
        }
        s_sharedUtilityEvaluator->LoadFromFile("utility/bot_utility.yaml");
        s_utilityConfigLoaded = true;
        gi.DPrintf("Loaded shared utility AI configuration\n");
    }

    // Reference the shared evaluator (no copying)
    utilityEvaluator = s_sharedUtilityEvaluator;
}

// Added in OPM - Phase 2B Task 2B.4
//  Reload current profile from disk
void BotController::ReloadProfile()
{
    if (!profile) {
        gi.Printf("ERROR: ReloadProfile called but no profile loaded\n");
        return;
    }

    const std::string& profileName = profile->GetName();
    LoadProfile(profileName.c_str());
}

// Added in OPM - Phase 2B Task 2B.4
//  Populate blackboard with current bot state for BT execution
// Changed in OPM - Phase 3 Task 3.1f
//  Updated to use BlackboardKeys constants and add proper combat data
void BotController::PopulateBlackboard()
{
    // Core references
    blackboard.Set<BotController *>(BlackboardKeys::BOT, this);
    blackboard.Set<Player *>(BlackboardKeys::PLAYER, static_cast<Player *>(controlledEnt.Pointer()));
    blackboard.Set<BotProfile *>(BlackboardKeys::PROFILE, profile.get());

    // Added in OPM - Phase 3 Task 3.4 Commit 5 Fix
    //  Create minimal perception snapshot for utility AI
    //  Full PerceptionSystem integration will be added in future task
    static PerceptionSnapshot minimalPerception;
    minimalPerception = PerceptionSnapshot(); // Reset

    if (controlledEnt && m_pEnemy) {
        // Populate with current enemy data
        EnemyInfo enemy;
        enemy.entity           = m_pEnemy;
        enemy.position         = m_pEnemy->origin;
        enemy.distance         = (m_pEnemy->origin - controlledEnt->origin).length();
        enemy.visibilityFactor = 1.0f; // Assume visible if we have them as enemy
        minimalPerception.visibleEnemies.push_back(enemy);
        minimalPerception.closestEnemyIndex = 0;
    }

    blackboard.Set<const PerceptionSnapshot *>(BlackboardKeys::PERCEPTION, &minimalPerception);

    // Health state
    if (controlledEnt) {
        blackboard.Set<float>("health", controlledEnt->health);
        blackboard.Set<float>("maxHealth", controlledEnt->max_health);

        // Weapon and ammo state
        Weapon *weapon = controlledEnt->GetActiveWeapon(WEAPON_MAIN);
        if (weapon) {
            blackboard.Set<bool>("hasAmmo", weapon->HasAmmo(FIRE_PRIMARY) != qfalse);
            // Calculate ammo percent manually
            int   clipAmmo    = controlledEnt->client->ps.stats[STAT_CLIPAMMO];
            int   clipSize    = weapon->GetClipSize(FIRE_PRIMARY);
            float ammoPercent = clipSize > 0 ? (float)clipAmmo / (float)clipSize : 0.0f;
            blackboard.Set<float>("ammoPercent", ammoPercent);
        }
    }

    // Current enemy (for legacy compatibility with existing actions)
    if (m_pEnemy) {
        blackboard.Set<Sentient *>(BlackboardKeys::SELECTED_TARGET, static_cast<Sentient *>(m_pEnemy.Pointer()));

        // Calculate distance to enemy
        if (controlledEnt && m_pEnemy) {
            float distance = (m_pEnemy->origin - controlledEnt->origin).length();
            blackboard.Set<float>(BlackboardKeys::TARGET_DISTANCE, distance);
        }
    }
}

// Added in OPM - Phase 2B Task 2B.4
//  Execute behavior tree for one frame
void BotController::ExecuteBehaviorTree(float deltaTime)
{
    if (!behaviorTree) {
        return;
    }

    BTNode::Status status = behaviorTree->Execute(blackboard, deltaTime);

    // Debug output
    if (g_bot_debug->integer && controlledEnt && g_bot_debug->integer == controlledEnt->entnum) {
        const char *statusStr = (status == BTNode::Status::SUCCESS) ? "SUCCESS"
                              : (status == BTNode::Status::FAILURE) ? "FAILURE"
                                                                    : "RUNNING";
        gi.Printf("Bot %d BT status: %s\n", controlledEnt->entnum, statusStr);
    }
}

void BotController::Think()
{
    usercmd_t  ucmd;
    usereyes_t eyeinfo;

    // Changed in OPM - Phase 2B Task 2B.4
    //  Modified to support behavior tree execution based on feature flag
    if (g_bot_use_new_ai_system->integer) {
        if (!controlledEnt) {
            return;
        }

        m_botCmd.serverTime = level.svsTime;

        // Handle weapon selection
        if (!controlledEnt->client->pers.dm_primary[0]) {
            Event *event = new Event(EV_Player_PrimaryDMWeapon);
            event->AddString("auto");
            controlledEnt->ProcessEvent(event);
        }

        // Handle team joining
        bool needsTeam = (controlledEnt->GetTeam() == TEAM_NONE || controlledEnt->GetTeam() == TEAM_SPECTATOR);
        if (needsTeam) {
            float time = controlledEnt->entnum / 20.0;
            if (!controlledEnt->EventPending(EV_Player_AutoJoinDMTeam)) {
                controlledEnt->PostEvent(EV_Player_AutoJoinDMTeam, time);
            }
        }

        // Fixed in OPM
        //  Toggle respawn button when dead/spectator to create repeating "new button press" detection
        //  This matches the old system's XOR toggle behavior to trigger respawn
        bool isDeadOrSpectator = controlledEnt->IsDead() || controlledEnt->IsSpectator();
        if (needsTeam || isDeadOrSpectator) {
            m_botCmd.buttons ^= BUTTON_ATTACKLEFT;
        } else {
            m_botCmd.buttons = 0;
            m_botCmd.buttons |= BUTTON_RUN;

            m_botEyes.ofs[0]    = 0;
            m_botEyes.ofs[1]    = 0;
            m_botEyes.ofs[2]    = controlledEnt->viewheight;
            m_botEyes.angles[0] = 0;
            m_botEyes.angles[1] = 0;

            CheckStates();

            float deltaTime = level.frametime;

            PopulateBlackboard();

            // Added in OPM - Phase 3 Task 3.4 Commit 5
            //  Re-evaluate strategy every 0.5 seconds
            strategyChangeTimer += deltaTime;
            if (strategyChangeTimer > 0.5f) {
                EvaluateStrategy(deltaTime);
                strategyChangeTimer = 0.0f;
            }

            ExecuteBehaviorTree(deltaTime);

            movement.MoveThink(m_botCmd);
            rotation.TurnThink(m_botCmd, m_botEyes);

            CheckUse();
            CheckValidWeapon();
        }
    } else {
        UpdateBotStates();
    }

    GetUsercmd(&ucmd);
    GetEyeInfo(&eyeinfo);

    G_ClientThink(controlledEnt->edict, &ucmd, &eyeinfo);

    // Added in OPM
    //  Draw debug visualization if enabled
    if (m_bShowPerception || m_bShowPath || m_bShowEnemy || m_bShowState) {
        DrawDebugVisualization();
    }

    // Added in OPM - Phase 3 Task 3.5
    //  Draw enhanced debug visualization if enabled
    if (debugViz) {
        debugViz->Draw(this);
    }
}

// Added in OPM - Phase 3 Task 3.4 Commit 5
//  Strategy evaluation for utility-based AI
void BotController::EvaluateStrategy(float deltaTime)
{
    if (!profile || !behaviorTree) {
        return;
    }

    // Get perception snapshot from blackboard
    auto perceptionPtr = blackboard.TryGet<const PerceptionSnapshot *>(BlackboardKeys::PERCEPTION);
    if (!perceptionPtr || !*perceptionPtr) {
        return;
    }

    const PerceptionSnapshot *perception = *perceptionPtr;
    Player                   *player     = getControlledEntity();

    if (!player) {
        return;
    }

    // Select best action using utility AI
    auto allScores = utilityEvaluator->ScoreAllActions(*perception, player, profile.get());
    
    // Store all scores for debugging
    blackboard.Set(BlackboardKeys::UTILITY_SCORES, allScores);
    
    // Find best action and current strategy score
    UtilityEvaluator::ScoredAction bestAction;
    float currentStrategyScore = 0.0f;
    float bestScore = -1.0f;
    
    for (const auto& action : *allScores) {
        if (action.score > bestScore) {
            bestScore = action.score;
            bestAction = action;
        }
        if (action.name == currentStrategy) {
            currentStrategyScore = action.score;
        }
    }

    // Apply hysteresis: require improvement threshold to switch strategies
    float hysteresisThreshold = g_bot_utility_hysteresis->value;

    if (bestAction.name != currentStrategy) {
        // Changed in OPM
        //  Fixed hysteresis to compare against current score, not stale lastStrategyScore
        //  Require X% improvement over current strategy to switch (prevents oscillation)
        if (bestAction.score > currentStrategyScore * (1.0f + hysteresisThreshold)) {
            SwitchStrategy(bestAction.name, bestAction.treeFile);
            lastStrategyScore = bestAction.score;
        }
    } else {
        // Update score for current strategy
        lastStrategyScore = bestAction.score;
    }
}

// Added in OPM - Phase 3 Task 3.4 Commit 5
//  Switch to a new behavior tree strategy
void BotController::SwitchStrategy(const std::string& strategyName, const std::string& treeFile)
{
    currentStrategy = strategyName;

    // Load new behavior tree
    auto newTree = BTYamlLoader::LoadFromFile(treeFile.c_str());
    if (newTree) {
        behaviorTree = std::move(newTree);
        behaviorTree->Reset();

        if (g_bot_debug && g_bot_debug->integer) {
            Player *player = getControlledEntity();
            if (player) {
                gi.DPrintf(
                    "Bot %d switching to strategy: %s (score: %.2f)\n",
                    player->entnum,
                    strategyName.c_str(),
                    lastStrategyScore
                );
            }
        }
    }
}
