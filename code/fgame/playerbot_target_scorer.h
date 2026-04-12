// playerbot_target_scorer.h: Time-to-kill target scoring for bot planner.

#pragma once

#include "sentient.h"

class BotController;

// Added in OPM
//  Scores candidate enemies by expected time-to-kill (TTK), using the
//  baked visibility matrix, the bot's current weapon, and the bot's
//  profile. Pure evaluator: no side effects, no state writes.
//
//  TTK = travelTime + aimTime + damageTime. The planner picks the
//  candidate with the lowest TTK. When the best shooting position is
//  not the bot's current position, the scorer returns the nav node to
//  move to in `shootFromNode`, which the planner turns into a Flank
//  goal that leads into Engage once line-of-sight is established.
class BotTargetScorer
{
public:
    struct Score {
        float ttk;           // total weighted time-to-kill in seconds
        float travelTime;    // seconds to reach shootFromNode (0 if visible)
        float aimTime;       // seconds to line up the shot + reaction delay
        float damageTime;    // seconds to deplete target HP at effective DPS
        int   shootFromNode; // nav node to engage from; -1 = already at one
        bool  reachable;     // false short-circuits the candidate entirely
    };

    Score Evaluate(const BotController& bot, Sentient *candidate) const;
};
