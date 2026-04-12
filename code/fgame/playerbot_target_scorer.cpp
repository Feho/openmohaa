// playerbot_target_scorer.cpp: Time-to-kill target scoring for bot planner.
//
// TTK is a single metric that folds travel, aim and damage time into
// one number. The planner picks the lowest-TTK reachable candidate.
// Profile parameters make sniper/rusher profiles score the same scene
// differently without any subclassing.

#include "playerbot.h"
#include "playerbot_target_scorer.h"
#include "weapon.h"

static constexpr float kDefaultRunSpeed      = 250.0f;   // units/sec
static constexpr float kFallbackTurnRate     = 3.14159f; // rad/sec, ~180 deg/sec
static constexpr float kFallbackDps          = 20.0f;
static constexpr float kNominalEnemyHp       = 100.0f;
static constexpr float kBigTtk               = 999999.0f;
static constexpr float kMaxShootNodeDistance = 4096.0f;
static constexpr float kHugeDistanceSq       = 1.0e30f;

// Find the nearest path-node index to a world position. Returns -1 if
// the nav graph is empty. Linear scan mirrors the visibility matrix's
// DrawDebug helper, which is the closest in-tree precedent.
static int NearestNodeIndex(const Vector& pos)
{
    int   bestIdx  = -1;
    float bestDist = kHugeDistanceSq;

    for (int i = 0; i < PathSearch::nodecount; i++) {
        PathNode *node = PathSearch::pathnodes[i];
        if (!node) {
            continue;
        }

        float dx = pos.x - node->origin[0];
        float dy = pos.y - node->origin[1];
        float dz = pos.z - node->origin[2];
        float d  = dx * dx + dy * dy + dz * dz;

        if (d < bestDist) {
            bestDist = d;
            bestIdx  = i;
        }
    }

    return bestIdx;
}

// Estimate a weapon's primary-fire DPS from its own properties. No
// hardcoded per-weapon table: damage * bulletcount / fire_delay.
static float EstimateWeaponDps(Weapon *weapon)
{
    if (!weapon) {
        return kFallbackDps;
    }

    float delay  = weapon->FireDelay(FIRE_PRIMARY);
    float damage = weapon->GetBulletDamage(FIRE_PRIMARY);
    float count  = weapon->GetBulletCount(FIRE_PRIMARY);

    if (delay <= 0.0f) {
        delay = 0.25f;
    }
    if (damage <= 0.0f) {
        damage = 10.0f;
    }
    if (count <= 0.0f) {
        count = 1.0f;
    }

    return (damage * count) / delay;
}

// Profile-driven effective DPS penalty. A target outside the bot's
// preferred range band scores worse, so snipers prefer long shots and
// rushers prefer close ones without any subclassing.
static float RangePenalty(float engageRange, const BotProfile& profile)
{
    float penalty = 0.0f;

    if (profile.preferredRangeMax > 0.0f && engageRange > profile.preferredRangeMax) {
        penalty += (engageRange - profile.preferredRangeMax) / profile.preferredRangeMax;
    }
    if (profile.preferredRangeMin > 0.0f && engageRange < profile.preferredRangeMin) {
        penalty += (profile.preferredRangeMin - engageRange) / profile.preferredRangeMin;
    }

    return penalty;
}

static bool CanReachNode(const BotController& bot, int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= PathSearch::nodecount) {
        return false;
    }

    PathNode *node = PathSearch::pathnodes[nodeIndex];
    if (!node) {
        return false;
    }

    Player *me = bot.getControlledEntity();
    if (!me) {
        return false;
    }

    PathSearchParameter parameters;
    parameters.leashHome  = vec_zero;
    parameters.leashDist  = 0.0f;
    parameters.entity     = me;
    parameters.fallHeight = 400;

    IPather *pather = IPather::CreatePather();
    if (!pather) {
        return false;
    }

    const bool canReach =
        pather->TestPath(me->origin, Vector(node->origin[0], node->origin[1], node->origin[2]), parameters);
    delete pather;

    return canReach;
}

BotTargetScorer::Score BotTargetScorer::Evaluate(const BotController& bot, Sentient *candidate) const
{
    Score score;
    score.ttk           = kBigTtk;
    score.travelTime    = kBigTtk;
    score.aimTime       = 0.0f;
    score.damageTime    = kBigTtk;
    score.shootFromNode = -1;
    score.reachable     = false;

    Player *me = bot.getControlledEntity();
    if (!me || !candidate) {
        return score;
    }

    const BotProfile& profile     = bot.GetProfile();
    Weapon           *weapon      = me->GetActiveWeapon(WEAPON_MAIN);
    const Vector      myPos       = me->origin;
    const Vector      enemyPos    = candidate->origin;
    const float       weaponRange = weapon ? weapon->GetBulletRange(FIRE_PRIMARY) / 1.25f : 0.0f;

    // --- Travel time ---
    //
    // If the bot already has a direct line of sight (same check the
    // execution layer uses), travel time is zero and we fight from
    // here. Otherwise query the visibility matrix for every node the
    // candidate can see, and pick the one closest to us.
    float maxVisionDist  = Q_min(world->m_fAIVisionDistance, world->farplane_distance * 0.828f);
    bool  alreadyVisible = me->CanSee(candidate, 120.0f, maxVisionDist, false);

    if (alreadyVisible) {
        score.travelTime    = 0.0f;
        score.shootFromNode = -1;
    } else {
        const BotVisibilityMatrix& visMatrix = botManager.getVisibilityMatrix();
        if (!visMatrix.IsBaked()) {
            return score;
        }

        int enemyNode = NearestNodeIndex(enemyPos);
        if (enemyNode < 0) {
            return score;
        }

        Container<int> visibleFromEnemy;
        visMatrix.GetVisibleNodes(enemyNode, kMaxShootNodeDistance, visibleFromEnemy);
        if (visibleFromEnemy.NumObjects() == 0) {
            return score;
        }

        int   bestNode   = -1;
        float bestDistSq = kHugeDistanceSq;
        for (int i = 1; i <= visibleFromEnemy.NumObjects(); i++) {
            int       idx  = visibleFromEnemy.ObjectAt(i);
            PathNode *node = PathSearch::pathnodes[idx];
            if (!node) {
                continue;
            }

            Vector nodePos(node->origin[0], node->origin[1], node->origin[2]);
            float  engageRange = (enemyPos - nodePos).length();
            if (weaponRange > 0.0f && engageRange > weaponRange) {
                continue;
            }
            if (!CanReachNode(bot, idx)) {
                continue;
            }

            float dx  = myPos.x - node->origin[0];
            float dy  = myPos.y - node->origin[1];
            float dz  = myPos.z - node->origin[2];
            float dSq = dx * dx + dy * dy + dz * dz;

            if (dSq < bestDistSq) {
                bestDistSq = dSq;
                bestNode   = idx;
            }
        }

        if (bestNode < 0) {
            return score;
        }

        float distToShoot   = sqrtf(bestDistSq);
        score.travelTime    = distToShoot / kDefaultRunSpeed;
        score.shootFromNode = bestNode;
    }

    // --- Aim time ---
    //
    // Angle between current look direction and the candidate, over a
    // fixed turn rate, plus the profile's reaction floor. Turn rate is
    // fixed here because BotProfile::turnSpeed is expressed in the
    // rotation layer's internal units, not rad/sec.
    Vector eyePos = myPos;
    eyePos.z += me->viewheight;

    Vector desired    = enemyPos - eyePos;
    float  desiredLen = desired.length();
    if (desiredLen > 0.0f) {
        desired = desired * (1.0f / desiredLen);
    }

    Vector vAngles = me->GetVAngles();
    Vector forward;
    AngleVectors(vAngles, forward, NULL, NULL);

    float dotVal = forward * desired;
    if (dotVal > 1.0f) {
        dotVal = 1.0f;
    } else if (dotVal < -1.0f) {
        dotVal = -1.0f;
    }
    float angleRad = acosf(dotVal);

    score.aimTime = (angleRad / kFallbackTurnRate) + profile.reactionMinDelay;

    // --- Damage time ---
    //
    // DPS from the weapon itself, then penalized by profile aim spread
    // and by how far the target is from the profile's preferred range
    // band. We use the straight-line engagement distance from the
    // shooting position (the bot, if visible, otherwise shootFromNode).
    Vector shootPos = myPos;
    if (score.shootFromNode >= 0) {
        PathNode *node = PathSearch::pathnodes[score.shootFromNode];
        if (node) {
            shootPos = Vector(node->origin[0], node->origin[1], node->origin[2]);
        }
    }

    float engageRange = (enemyPos - shootPos).length();
    if (weaponRange > 0.0f && engageRange > weaponRange) {
        return score;
    }

    float dps = EstimateWeaponDps(weapon);

    float spread = profile.aimSpreadMult;
    if (spread < 0.1f) {
        spread = 0.1f;
    }
    float penalty = RangePenalty(engageRange, profile);

    float effectiveDps = dps / (spread * (1.0f + penalty));
    if (effectiveDps <= 0.0f) {
        effectiveDps = 1.0f;
    }

    score.damageTime = kNominalEnemyHp / effectiveDps;

    // --- Total ---
    //
    // Aggression weights how much the bot dislikes walking to the
    // shot. A rusher (aggression ~1) discounts travel; a sniper
    // (aggression ~0) pays full price for the walk, so holding a lane
    // beats running to a different one.
    float travelWeight = 1.5f - profile.aggression; // [0.5 .. 1.5]
    score.ttk          = (score.travelTime * travelWeight) + score.aimTime + score.damageTime;
    score.reachable    = true;

    return score;
}
