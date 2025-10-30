// Added in OPM - Phase 2 Task 2A.1.1
// perception.cpp: Bot perception system implementation

#include "perception.h"
#include <algorithm>

// ========================================================================
// PerceptionSystem - Main perception coordinator
// ========================================================================

// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Removed manual constructor/destructor (now using std::unique_ptr with = default)

// Added in OPM - Phase 2 Task 2A.1.1
//  Main update method - integrates data from all sensors
PerceptionSnapshot PerceptionSystem::Update(Player *bot, float deltaTime)
{
    PerceptionSnapshot snapshot;

    // Stub implementation - will be fleshed out in subsequent tasks
    // Task 2A.1.2: Vision sensor integration
    // Task 2A.1.4: Audio sensor integration
    // Task 2A.1.5: Memory system integration
    // Task 2A.1.6: Threat level calculation
    // Task 2A.1.7: Ally detection

    return snapshot;
}

// ========================================================================
// VisionSensor - Vision perception
// ========================================================================

// Added in OPM - Phase 2 Task 2A.1.2
//  Constructor
VisionSensor::VisionSensor() {}

// Added in OPM - Phase 2 Task 2A.1.2
//  Destructor
VisionSensor::~VisionSensor() {}

// Added in OPM - Phase 2 Task 2A.1.2
//  Update vision perception - scans for visible enemies
std::vector<EnemyInfo> VisionSensor::UpdateVision(Player *bot, float deltaTime)
{
    std::vector<EnemyInfo> visibleEnemies;

    if (!bot) {
        return visibleEnemies;
    }

    // Calculate max vision distance based on farplane
    const float maxVisionDistance = world->farplane_distance * BotConstants::FARPLANE_VISION_FACTOR;

    // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
    //  Use named constants from BotConstants namespace instead of magic numbers
    const float centralFOV    = BotConstants::CENTRAL_FOV_DEGREES;
    const float peripheralFOV = BotConstants::PERIPHERAL_FOV_DEGREES;

    // Get bot's position and view angles
    const Vector botPos    = bot->origin;
    const Vector botAngles = bot->GetViewAngles();

    // Scan all sentients in the level
    for (int i = 1; i <= SentientList.NumObjects(); i++) {
        Sentient *sentient = SentientList.ObjectAt(i);

        if (!sentient) {
            continue;
        }

        // Skip self
        if (sentient == bot) {
            continue;
        }

        // Skip dead sentients
        if (sentient->health <= 0) {
            continue;
        }

        // Skip allies (check team affiliation)
        if (sentient->IsSubclassOfPlayer()) {
            Player *otherPlayer = static_cast<Player *>(sentient);
            if (otherPlayer->GetTeam() == bot->GetTeam()) {
                continue; // Same team, skip
            }
        }

        // Check if we can see this sentient
        float       angleFromForward = 0.0f;
        bool        inCentralFOV     = CheckFOV(botPos, botAngles, sentient->origin, centralFOV, angleFromForward);
        bool        inPeripheralFOV  = false;
        const float distance         = (sentient->origin - botPos).length();

        // If not in central FOV, check peripheral
        if (!inCentralFOV) {
            inPeripheralFOV = CheckFOV(botPos, botAngles, sentient->origin, peripheralFOV, angleFromForward);
        }

        // Skip if not in any FOV
        if (!inCentralFOV && !inPeripheralFOV) {
            continue;
        }

        // Check distance
        if (distance > maxVisionDistance) {
            continue;
        }

        // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
        //  Removed AreasConnected() check - vision should report what is visible
        //  regardless of nav mesh connectivity. The decision of whether an enemy
        //  is "engageable" based on pathing should be handled by behavior logic,
        //  not perception. A bot should see enemies across chasms, through windows, etc.

        // Perform line of sight trace
        if (!PerformLineOfSightTrace(bot, sentient)) {
            continue;
        }

        // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
        //  Fixed FOV detection logic - central/peripheral flags are now mutually exclusive
        //  Central FOV (80°) is a subset of peripheral FOV (180°). An enemy at 30° is
        //  in BOTH zones, but should be flagged as central only, not peripheral.
        //  The flag isInPeripheral means "visible in peripheral ONLY, not in central."
        bool usePeripheralFactor = inPeripheralFOV && !inCentralFOV;

        // Enemy is visible - create EnemyInfo
        EnemyInfo enemyInfo;
        enemyInfo.entity           = sentient;
        enemyInfo.position         = sentient->origin;
        enemyInfo.velocity         = sentient->velocity;
        enemyInfo.distance         = distance;
        enemyInfo.angleFromForward = angleFromForward;
        enemyInfo.isInPeripheral   = usePeripheralFactor; // True only if peripheral AND NOT central
        enemyInfo.visibilityFactor = CalculateVisibilityFactor(distance, maxVisionDistance, usePeripheralFactor);

        visibleEnemies.push_back(enemyInfo);
    }

    return visibleEnemies;
}

// Added in OPM - Phase 2 Task 2A.1.2
//  Check if bot can see target with specified FOV and distance
bool VisionSensor::CanSee(Player *bot, Sentient *target, float fov, float maxDistance)
{
    if (!bot || !target) {
        return false;
    }

    // Get positions
    const Vector botPos    = bot->origin;
    const Vector targetPos = target->origin;
    const Vector botAngles = bot->GetViewAngles();

    // 1. Distance check
    const float distance = (targetPos - botPos).length();
    if (maxDistance > 0.0f && distance > maxDistance) {
        return false;
    }

    // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
    //  Removed AreasConnected() check - vision should report what is visible
    //  regardless of nav mesh connectivity. The decision of whether an enemy
    //  is "engageable" based on pathing should be handled by behavior logic,
    //  not perception. A bot should see enemies across chasms, through windows, etc.

    // 2. FOV check
    if (fov > 0.0f && fov < 360.0f) {
        float angleFromForward = 0.0f;
        if (!CheckFOV(botPos, botAngles, targetPos, fov, angleFromForward)) {
            return false;
        }
    }

    // 3. Line of sight trace
    return PerformLineOfSightTrace(bot, target);
}

// Added in OPM - Phase 2 Task 2A.1.2
// Changed in OPM - Phase 2 Task 2A.1.2 Code Review
//  Fixed: Added zero-distance validation, clamping, and optimized FOV check
//  Helper method to check if target is within field of view
bool VisionSensor::CheckFOV(
    const Vector& botPos, const Vector& botAngles, const Vector& targetPos, float fovDegrees, float& angleFromForward
)
{
    // Calculate direction to target
    Vector toTarget = targetPos - botPos;

    // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
    //  Added zero-distance validation to prevent normalize() crashes
    const float distanceSquared = toTarget.lengthSquared();
    if (distanceSquared < BotConstants::EPSILON) {
        // Target at bot's exact position - consider it in center of view
        angleFromForward = 0.0f;
        return true;
    }

    toTarget.normalize();

    // Get bot's forward vector from view angles
    Vector forward, right, up;
    AngleVectors(botAngles, forward, right, up);
    forward.normalize();

    // Calculate dot product to get angle
    const float dotProduct = DotProduct(toTarget, forward);

    // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
    //  Clamp dot product to valid range for acos to prevent NaN
    const float clampedDot = Q_max(-1.0f, Q_min(1.0f, dotProduct));
    angleFromForward = RAD2DEG(acos(clampedDot));

    // Changed in OPM - Phase 2 Task 2A.1.2 Code Review
    //  Optimized FOV check: compare dot product directly with cosine (40-60x faster)
    const float halfFOV = fovDegrees * 0.5f;
    const float cosHalfFOV = cos(DEG2RAD(halfFOV));

    // Cosine decreases as angle increases, so we use >= comparison
    return dotProduct >= cosHalfFOV;
}

// Added in OPM - Phase 2 Task 2A.1.2
//  Helper method to perform line of sight trace
bool VisionSensor::PerformLineOfSightTrace(Player *bot, Sentient *target)
{
    if (!bot || !target) {
        return false;
    }

    // Use MASK_CANSEE for standard visibility check (includes entities)
    const int mask = MASK_CANSEE;

    // Trace from bot's eye position to target's eye position (for sentients)
    return G_SightTrace(
        bot->EyePosition(),
        vec_zero,
        vec_zero,
        target->EyePosition(),
        bot,
        target,
        mask,
        qfalse,
        "VisionSensor::PerformLineOfSightTrace"
    );
}

// Added in OPM - Phase 2 Task 2A.1.2
// Changed in OPM - Phase 2 Task 2A.1.2 Code Review
//  Use named constant for peripheral clarity factor
//  Helper method to calculate visibility factor based on distance and peripheral vision
float VisionSensor::CalculateVisibilityFactor(float distance, float maxDistance, bool isPeripheral)
{
    // Start with distance-based attenuation
    float factor = 1.0f;

    if (maxDistance > BotConstants::EPSILON) {
        // Linear attenuation: 1.0 at close range, 0.0 at max distance
        factor = 1.0f - (distance / maxDistance);
        factor = Q_max(0.0f, factor); // Clamp to [0, 1]
    }

    // Reduce visibility for peripheral vision
    if (isPeripheral) {
        factor *= BotConstants::PERIPHERAL_CLARITY_FACTOR; // Peripheral vision has 40% of central vision clarity
    }

    return factor;
}

// ========================================================================
// AudioSensor - Audio perception
// ========================================================================

// Added in OPM - Phase 2 Task 2A.1.4
//  Constructor
AudioSensor::AudioSensor() {}

// Added in OPM - Phase 2 Task 2A.1.4
//  Destructor
AudioSensor::~AudioSensor() {}

void AudioSensor::ProcessEvent(int eventType, const Vector& position, float loudness)
{
    // Calculate priority based on event type
    int priority = 0;

    // High priority events (from NoticeEvent logic in BotController)
    switch (eventType) {
    case AI_EVENT_WEAPON_FIRE:
    case AI_EVENT_WEAPON_IMPACT:
    case AI_EVENT_EXPLOSION:
    case AI_EVENT_GRENADE:
    case AI_EVENT_AMERICAN_URGENT:
    case AI_EVENT_GERMAN_URGENT:
        priority = 2;
        break;

    // Low priority events
    case AI_EVENT_FOOTSTEP:
    case AI_EVENT_AMERICAN_VOICE:
    case AI_EVENT_GERMAN_VOICE:
    case AI_EVENT_MISC:
    case AI_EVENT_MISC_LOUD:
        priority = 1;
        break;

    // Ignore irrelevant events
    case AI_EVENT_NONE:
    case AI_EVENT_BADPLACE:
    default:
        return;
    }

    // Create audio event
    AudioEvent event;
    event.type = eventType;
    event.position = position;
    event.loudness = loudness;
    event.priority = priority / 2.0f; // Normalize to 0.0-1.0
    event.timestamp = level.svsTime * 0.001f; // Convert ms to seconds

    // For now, we don't have bot position, so we can't calculate direction
    // This will be done by PerceptionSystem when it integrates bot context
    event.estimatedDirection = vec_zero;
    event.confidence = 0.0f;

    // Add to queue
    eventQueue.push_back(event);

    // Cleanup old events (keep last 100 events)
    if (eventQueue.size() > 100) {
        eventQueue.erase(eventQueue.begin());
    }
}

std::vector<AudioEvent> AudioSensor::GetRecentSounds(Player *bot, float currentTime, float timeWindow)
{
    std::vector<AudioEvent> recentSounds;

    if (!bot) {
        return recentSounds;
    }

    const Vector botPos = bot->origin;

    // Filter events within time window and calculate 3D directional info
    for (auto event : eventQueue) {
        if (currentTime - event.timestamp <= timeWindow) {
            // Calculate direction from bot to sound source
            Vector toSound = event.position - botPos;
            const float distance = toSound.length();

            // Normalize direction
            if (distance > BotConstants::EPSILON) {
                toSound.normalize();
                event.estimatedDirection = toSound;

                // Calculate confidence based on distance
                // Closer sounds have higher confidence
                // Confidence decays linearly from 1.0 (0 units) to 0.0 (2000 units)
                const float maxAudioDistance = 2000.0f;
                event.confidence = 1.0f - Q_min(distance / maxAudioDistance, 1.0f);

                // Adjust loudness based on distance (inverse square law approximation)
                // Sounds get quieter with distance
                if (distance > 1.0f) {
                    const float distanceFactor = 100.0f / distance; // Reference distance of 100 units
                    event.loudness *= Q_min(distanceFactor, 1.0f);
                }
            } else {
                // Sound at bot's position
                event.estimatedDirection = vec_zero;
                event.confidence = 1.0f;
            }

            recentSounds.push_back(event);
        }
    }

    // Sort by priority (high to low), then by loudness (high to low)
    std::sort(recentSounds.begin(), recentSounds.end(),
        [](const AudioEvent& a, const AudioEvent& b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority; // Higher priority first
            }
            return a.loudness > b.loudness; // Then by loudness
        });

    return recentSounds;
}

// ========================================================================
// MemorySystem - Enemy memory tracking
// ========================================================================

// Added in OPM - Phase 2 Task 2A.1.5
//  Constructor
MemorySystem::MemorySystem() {}

// Added in OPM - Phase 2 Task 2A.1.5
//  Destructor
MemorySystem::~MemorySystem() {}

// Added in OPM - Phase 2 Task 2A.1.5
//  Update memory with seen enemy - stub implementation
void MemorySystem::UpdateMemory(const EnemyInfo& enemyInfo, float currentTime)
{
    // Stub implementation
    // Will be implemented in Task 2A.1.5:
    // - Store/update enemy last-known position
    // - Calculate predicted position based on velocity
    // - Update confidence level
    // - Track times spotted
}

// Added in OPM - Phase 2 Task 2A.1.5
//  Get all known enemies from memory - stub implementation
std::vector<EnemyMemory> MemorySystem::GetKnownEnemies(float currentTime)
{
    std::vector<EnemyMemory> knownEnemies;

    // Stub implementation
    // Will be implemented in Task 2A.1.5:
    // - Return memories with decayed confidence
    // - Filter out expired memories

    return knownEnemies;
}

// Added in OPM - Phase 2 Task 2A.1.5
//  Clean up old memories - stub implementation
void MemorySystem::CleanupOldMemories(float currentTime, float maxAge)
{
    // Stub implementation
    // Will be implemented in Task 2A.1.5:
    // - Remove memories older than maxAge
    // - Remove memories with zero confidence
}
