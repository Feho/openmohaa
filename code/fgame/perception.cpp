// Added in OPM - Phase 2 Task 2A.1.1
// perception.cpp: Bot perception system implementation

#include "perception.h"
#include <algorithm>
#include <cfloat>

// ========================================================================
// PerceptionSystem - Main perception coordinator
// ========================================================================

// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Removed manual constructor/destructor (now using std::unique_ptr with = default)

// Added in OPM - Phase 2 Task 2A.1.6
//  Main update method - integrates data from all sensors
PerceptionSnapshot PerceptionSystem::Update(Player *bot, float deltaTime)
{
    if (!bot) {
        return PerceptionSnapshot{};
    }

    PerceptionSnapshot snapshot;
    const float        currentTime = level.svsTime * 0.001f; // Convert ms to seconds

    // Step 1: Update vision - get currently visible enemies
    snapshot.visibleEnemies = visionSensor->UpdateVision(bot, deltaTime);

    // Added in OPM - Phase 2 Task 2A.1.7
    //  Step 1b: Update allies
    snapshot.visibleAllies = visionSensor->UpdateAllies(bot, deltaTime);

    // Step 2: Update memory with visible enemies
    for (const auto &enemyInfo : snapshot.visibleEnemies) {
        memory->UpdateMemory(enemyInfo, currentTime);
    }

    // Step 3: Get known enemies from memory (includes decay and prediction)
    snapshot.knownEnemies = memory->GetKnownEnemies(currentTime);

    // Step 4: Get recent audio events (5 second window per plan)
    snapshot.recentSounds = audioSensor->GetRecentSounds(bot, currentTime, 5.0f);

    // Step 5: Calculate closest enemy index
    snapshot.closestEnemyIndex = SIZE_MAX;
    if (!snapshot.visibleEnemies.empty()) {
        float minDistance = FLT_MAX;
        for (size_t i = 0; i < snapshot.visibleEnemies.size(); i++) {
            if (snapshot.visibleEnemies[i].distance < minDistance) {
                minDistance                = snapshot.visibleEnemies[i].distance;
                snapshot.closestEnemyIndex = i;
            }
        }
    }

    // Step 6: Calculate most dangerous enemy index
    snapshot.mostDangerousEnemyIndex = SIZE_MAX;
    if (!snapshot.visibleEnemies.empty()) {
        // For now, use closest enemy as most dangerous
        // In future (Phase 3), consider: distance + weapon threat + behavior
        snapshot.mostDangerousEnemyIndex = snapshot.closestEnemyIndex;
    }

    // Step 7: Calculate loudest sound index
    snapshot.loudestSoundIndex = SIZE_MAX;
    if (!snapshot.recentSounds.empty()) {
        // Fixed in OPM
        //  Start with first sound as baseline to handle 0.0 and negative loudness correctly
        float maxLoudness = snapshot.recentSounds[0].loudness;
        snapshot.loudestSoundIndex = 0;

        for (size_t i = 1; i < snapshot.recentSounds.size(); i++) {
            if (snapshot.recentSounds[i].loudness > maxLoudness) {
                maxLoudness                = snapshot.recentSounds[i].loudness;
                snapshot.loudestSoundIndex = i;
            }
        }
    }

    // Added in OPM - Phase 2 Task 2A.1.7
    //  Step 7b: Calculate closest ally index
    snapshot.closestAllyIndex = SIZE_MAX;
    if (!snapshot.visibleAllies.empty()) {
        float minDistance = FLT_MAX;
        for (size_t i = 0; i < snapshot.visibleAllies.size(); i++) {
            if (snapshot.visibleAllies[i].distance < minDistance) {
                minDistance               = snapshot.visibleAllies[i].distance;
                snapshot.closestAllyIndex = i;
            }
        }
    }

    // Step 8: Cleanup old events (prevents unbounded growth)
    audioSensor->CleanupOldEvents(currentTime, 5.0f); // Match GetRecentSounds window
    memory->CleanupOldMemories(currentTime, BotConstants::MEMORY_MAX_AGE_SECONDS);

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

// Added in OPM - Phase 2 Task 2A.1.7
//  Update vision perception - scans for visible allies
std::vector<AllyInfo> VisionSensor::UpdateAllies(Player *bot, float deltaTime)
{
    std::vector<AllyInfo> visibleAllies;

    if (!bot) {
        return visibleAllies;
    }

    // Calculate max vision distance based on farplane
    const float maxVisionDistance = world->farplane_distance * BotConstants::FARPLANE_VISION_FACTOR;

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

        // Only process Players (allies must be players)
        if (!sentient->IsSubclassOfPlayer()) {
            continue;
        }

        Player *otherPlayer = static_cast<Player *>(sentient);

        // Skip enemies (check team affiliation)
        if (otherPlayer->GetTeam() != bot->GetTeam()) {
            continue; // Different team, skip
        }

        // Check if we can see this ally
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

        // Perform line of sight trace
        if (!PerformLineOfSightTrace(bot, sentient)) {
            continue;
        }

        // Ally is visible - create AllyInfo
        AllyInfo allyInfo;
        allyInfo.entity           = otherPlayer;
        allyInfo.position         = sentient->origin;
        allyInfo.velocity         = sentient->velocity;
        allyInfo.distance         = distance;
        allyInfo.angleFromForward = angleFromForward;
        allyInfo.canSeeMe         = false; // Will be calculated in future if needed

        visibleAllies.push_back(allyInfo);
    }

    return visibleAllies;
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
    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
    //  Use named constant instead of magic number
    event.priority = priority / static_cast<float>(BotConstants::AUDIO_PRIORITY_MAX); // Normalize to 0.0-1.0
    event.timestamp = level.svsTime * 0.001f; // Convert ms to seconds

    // For now, we don't have bot position, so we can't calculate direction
    // This will be done by PerceptionSystem when it integrates bot context
    event.estimatedDirection = vec_zero;
    event.confidence = 0.0f;

    // Add to queue
    eventQueue.push_back(event);

    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
    //  Use pop_front() for O(1) removal (now using deque instead of vector)
    //  Use named constant instead of magic number
    if (eventQueue.size() > BotConstants::MAX_AUDIO_EVENTS) {
        eventQueue.pop_front();
    }
}

// Changed in OPM - Phase 2 Task 2A.1.4 Code Review
//  Added const qualifiers to method and bot parameter
std::vector<AudioEvent> AudioSensor::GetRecentSounds(const Player *bot, float currentTime, float timeWindow) const
{
    std::vector<AudioEvent> recentSounds;

    if (!bot) {
        return recentSounds;
    }

    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
    //  Reserve capacity to avoid reallocations
    recentSounds.reserve(eventQueue.size());

    const Vector botPos = bot->origin;

    // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
    //  Use const reference to avoid copying 60-byte structs in loop
    // Filter events within time window and calculate 3D directional info
    for (const auto& event : eventQueue) {
        if (currentTime - event.timestamp <= timeWindow) {
            // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
            //  Create explicit copy only when needed (for modification)
            AudioEvent modifiedEvent = event;

            // Calculate direction from bot to sound source
            Vector toSound = modifiedEvent.position - botPos;
            const float distance = toSound.length();

            // Normalize direction
            if (distance > BotConstants::EPSILON) {
                toSound.normalize();
                modifiedEvent.estimatedDirection = toSound;

                // Calculate confidence based on distance
                // Closer sounds have higher confidence
                // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
                //  Use named constant instead of magic number
                // Confidence decays linearly from 1.0 (0 units) to 0.0 (MAX_AUDIO_DISTANCE units)
                modifiedEvent.confidence = 1.0f - Q_min(distance / BotConstants::MAX_AUDIO_DISTANCE, 1.0f);

                // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
                //  Implement true inverse square law for loudness attenuation
                //  Use named constants instead of magic numbers
                // Adjust loudness based on distance (inverse square law)
                // Sounds get quieter with distance squared
                if (distance > BotConstants::AUDIO_MIN_DISTANCE) {
                    const float refDist = BotConstants::AUDIO_REFERENCE_DISTANCE;
                    // Inverse square: loudness ∝ 1/distance²
                    const float attenuationFactor = (refDist * refDist) / (distance * distance);
                    modifiedEvent.loudness *= Q_min(attenuationFactor, 1.0f); // Cap at 1.0 to prevent amplification
                }
            } else {
                // Sound at bot's position
                modifiedEvent.estimatedDirection = vec_zero;
                modifiedEvent.confidence = 1.0f;
            }

            // Changed in OPM - Phase 2 Task 2A.1.4 Code Review
            //  Use move semantics for efficiency
            recentSounds.push_back(std::move(modifiedEvent));
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

void AudioSensor::CleanupOldEvents(float currentTime, float maxAge)
{
    eventQueue.erase(
        std::remove_if(
            eventQueue.begin(),
            eventQueue.end(),
            [currentTime, maxAge](const AudioEvent &event) {
                const float age = currentTime - event.timestamp;
                return age > maxAge;
            }
        ),
        eventQueue.end()
    );
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
//  Update or create memory for a single enemy
//  If enemy already exists in memory, updates position, velocity, and resets confidence to 1.0
//  If enemy is new, creates a new memory entry
void MemorySystem::UpdateMemory(const EnemyInfo& enemyInfo, float currentTime)
{
    if (!enemyInfo.entity) {
        return;
    }

    // Check if we already have a memory for this enemy
    bool found = false;
    for (auto& memory : memories) {
        if (memory.enemy == enemyInfo.entity) {
            // Update existing memory
            memory.lastKnownPosition = enemyInfo.position;
            memory.lastKnownVelocity = enemyInfo.velocity;
            memory.lastSeenTime = currentTime;
            memory.confidenceLevel = 1.0f; // Reset confidence to maximum
            memory.timesSpotted++;
            memory.predictedPosition = enemyInfo.position; // Will be updated when queried
            found = true;
            break;
        }
    }

    // If not found, create new memory
    if (!found) {
        EnemyMemory newMemory;
        newMemory.enemy = enemyInfo.entity;
        newMemory.lastKnownPosition = enemyInfo.position;
        newMemory.lastKnownVelocity = enemyInfo.velocity;
        newMemory.lastSeenTime = currentTime;
        newMemory.confidenceLevel = 1.0f;
        newMemory.timesSpotted = 1;
        newMemory.predictedPosition = enemyInfo.position;
        newMemory.investigationStarted = false;

        memories.push_back(newMemory);
    }
}

std::vector<EnemyMemory> MemorySystem::GetKnownEnemies(float currentTime) const
{
    std::vector<EnemyMemory> knownEnemies;

    for (const auto& memory : memories) {
        // Skip if entity no longer exists (SafePtr returns null)
        if (!memory.enemy) {
            continue;
        }

        // Calculate time since last seen (clamp to 0 to handle clock skew)
        const float timeSinceLastSeen = Q_max(0.0f, currentTime - memory.lastSeenTime);

        // Skip very old memories
        if (timeSinceLastSeen > BotConstants::MEMORY_MAX_AGE_SECONDS) {
            continue;
        }

        // Apply confidence decay (create a copy with decayed confidence)
        const float decayAmount = timeSinceLastSeen * BotConstants::MEMORY_CONFIDENCE_DECAY_RATE;
        const float decayedConfidence = Q_max(0.0f, 1.0f - decayAmount);

        // Skip memories below minimum confidence threshold
        if (decayedConfidence < BotConstants::MEMORY_MIN_CONFIDENCE) {
            continue;
        }

        // Create a copy with updated confidence and predicted position
        EnemyMemory decayedMemory = memory;
        decayedMemory.confidenceLevel = decayedConfidence;
        decayedMemory.predictedPosition = memory.lastKnownPosition + (memory.lastKnownVelocity * timeSinceLastSeen);

        knownEnemies.push_back(decayedMemory);
    }

    return knownEnemies;
}

void MemorySystem::CleanupOldMemories(float currentTime, float maxAge)
{
    // Use erase-remove idiom to efficiently remove stale memories
    memories.erase(
        std::remove_if(
            memories.begin(),
            memories.end(),
            [currentTime, maxAge](const EnemyMemory& memory) {
                // Remove if entity no longer exists
                if (!memory.enemy) {
                    return true;
                }

                // Remove if too old
                const float age = currentTime - memory.lastSeenTime;
                if (age > maxAge) {
                    return true;
                }

                return false;
            }
        ),
        memories.end()
    );
}
