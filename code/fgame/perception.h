// Added in OPM - Phase 2 Task 2A.1.1
// perception.h: Bot perception system for sensing and memory

#pragma once

#include "glb_local.h"
#include "sentient.h"
#include "player.h"
#include "playerbot.h"
#include <vector>
#include <memory>

// Added in OPM - Phase 2 Task 2A.1.1
//  Threat level classification for tactical decision making
enum ThreatLevel {
    THREAT_NONE,   // No enemies detected
    THREAT_LOW,    // 1 enemy, distant
    THREAT_MEDIUM, // 1-2 enemies, medium range
    THREAT_HIGH    // 3+ enemies or very close
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Information about a detected enemy
// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Replaced raw pointer with SafePtr for entity safety
struct EnemyInfo {
    SafePtr<Sentient> entity;        // Pointer to enemy entity (auto-nullifies on destruction)
    Vector            position;      // Current position
    Vector            velocity;      // Current velocity
    float             distance;      // Distance to bot
    float             visibilityFactor; // 0.0 (barely visible) - 1.0 (clear view)
    float             angleFromForward; // Degrees off center of view
    bool              isInPeripheral; // True if in peripheral vision

    EnemyInfo()
        : entity(nullptr)
        , position(vec_zero)
        , velocity(vec_zero)
        , distance(0.0f)
        , visibilityFactor(0.0f)
        , angleFromForward(0.0f)
        , isInPeripheral(false)
    {
    }

    // Helper methods
    bool IsVisible() const { return visibilityFactor > BotConstants::VISIBILITY_THRESHOLD; }
    bool IsInPeripheral() const { return isInPeripheral; }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Information about a nearby ally
// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Replaced raw pointer with SafePtr for entity safety
struct AllyInfo {
    SafePtr<Player> entity;   // Pointer to ally player (auto-nullifies on destruction)
    Vector          position; // Current position
    float           distance; // Distance to bot
    bool            canSeeMe; // True if ally can see this bot

    AllyInfo()
        : entity(nullptr)
        , position(vec_zero)
        , distance(0.0f)
        , canSeeMe(false)
    {
    }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Audio event information for sound-based perception
struct AudioEvent {
    int    type;                // Event type (AI_EVENT_*)
    Vector position;            // Position of sound source
    Vector estimatedDirection;  // Estimated direction to source
    float  loudness;            // 0.0 - 1.0
    float  priority;            // 0.0 - 1.0 (importance)
    float  timestamp;           // When event occurred
    float  confidence;          // How confident about direction (0.0 - 1.0)

    AudioEvent()
        : type(0)
        , position(vec_zero)
        , estimatedDirection(vec_zero)
        , loudness(0.0f)
        , priority(0.0f)
        , timestamp(0.0f)
        , confidence(0.0f)
    {
    }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Memory of an enemy that is no longer visible
// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Replaced raw pointer with SafePtr for entity safety
struct EnemyMemory {
    SafePtr<Sentient> enemy;                // Pointer to enemy entity (auto-nullifies on destruction)
    Vector            lastKnownPosition;    // Last position where enemy was seen
    Vector            lastKnownVelocity;    // Last known velocity
    Vector            predictedPosition;    // Predicted current position
    float             lastSeenTime;         // Timestamp of last sighting
    float             confidenceLevel;      // 1.0 (just seen) -> 0.0 (very old)
    int               timesSpotted;         // Number of times spotted
    bool              investigationStarted; // True if bot started investigating

    EnemyMemory()
        : enemy(nullptr)
        , lastKnownPosition(vec_zero)
        , lastKnownVelocity(vec_zero)
        , predictedPosition(vec_zero)
        , lastSeenTime(0.0f)
        , confidenceLevel(0.0f)
        , timesSpotted(0)
        , investigationStarted(false)
    {
    }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Complete snapshot of what the bot perceives at a given moment
// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Replaced optional pointers with indices to prevent dangling pointer bugs
struct PerceptionSnapshot {
    // Enemies
    std::vector<EnemyInfo>   visibleEnemies;         // Currently visible enemies
    std::vector<EnemyMemory> knownEnemies;           // Enemies from memory
    size_t                   closestEnemyIndex;      // Index to closest visible enemy (SIZE_MAX = none)
    size_t                   mostDangerousEnemyIndex; // Index to most threatening enemy (SIZE_MAX = none)

    // Allies
    std::vector<AllyInfo> nearbyAllies; // Nearby friendly players

    // Audio
    std::vector<AudioEvent> recentSounds;      // Recent audio events
    size_t                  loudestSoundIndex; // Index to loudest recent sound (SIZE_MAX = none)

    // Threat assessment
    ThreatLevel threatLevel; // Overall threat level

    PerceptionSnapshot()
        : closestEnemyIndex(SIZE_MAX)
        , mostDangerousEnemyIndex(SIZE_MAX)
        , loudestSoundIndex(SIZE_MAX)
        , threatLevel(THREAT_NONE)
    {
    }

    // Helper methods for vector membership
    bool HasVisibleEnemy() const { return !visibleEnemies.empty(); }
    bool HasKnownEnemy() const { return !knownEnemies.empty(); }
    int  GetEnemyCount() const { return visibleEnemies.size(); }
    int  GetTotalKnownEnemies() const { return visibleEnemies.size() + knownEnemies.size(); }

    // Safe accessors for optional indices
    EnemyInfo *GetClosestEnemy()
    {
        return closestEnemyIndex < visibleEnemies.size() ? &visibleEnemies[closestEnemyIndex] : nullptr;
    }

    const EnemyInfo *GetClosestEnemy() const
    {
        return closestEnemyIndex < visibleEnemies.size() ? &visibleEnemies[closestEnemyIndex] : nullptr;
    }

    EnemyInfo *GetMostDangerousEnemy()
    {
        return mostDangerousEnemyIndex < visibleEnemies.size() ? &visibleEnemies[mostDangerousEnemyIndex] : nullptr;
    }

    const EnemyInfo *GetMostDangerousEnemy() const
    {
        return mostDangerousEnemyIndex < visibleEnemies.size() ? &visibleEnemies[mostDangerousEnemyIndex] : nullptr;
    }

    AudioEvent *GetLoudestSound()
    {
        return loudestSoundIndex < recentSounds.size() ? &recentSounds[loudestSoundIndex] : nullptr;
    }

    const AudioEvent *GetLoudestSound() const
    {
        return loudestSoundIndex < recentSounds.size() ? &recentSounds[loudestSoundIndex] : nullptr;
    }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Forward declarations for sensor classes
class VisionSensor;
class AudioSensor;
class MemorySystem;

// Added in OPM - Phase 2 Task 2A.1.1
//  Main perception system that integrates vision, hearing, and memory
// Changed in OPM - Phase 2 Task 2A.1.1 Code Review
//  Fixed: Replaced manual memory management with std::unique_ptr
class PerceptionSystem
{
public:
    PerceptionSystem() = default;
    ~PerceptionSystem() = default;

    // Delete copy operations (resource-owning class)
    PerceptionSystem(const PerceptionSystem &)            = delete;
    PerceptionSystem &operator=(const PerceptionSystem &) = delete;

    // Default move operations
    PerceptionSystem(PerceptionSystem &&)            = default;
    PerceptionSystem &operator=(PerceptionSystem &&) = default;

    // Main update method - returns snapshot of current perception state
    PerceptionSnapshot Update(Player *bot, float deltaTime);

    // Accessor methods for individual sensors
    VisionSensor &GetVision() { return *visionSensor; }
    AudioSensor  &GetHearing() { return *audioSensor; }
    MemorySystem &GetMemory() { return *memory; }

    // Const overloads for accessors
    const VisionSensor &GetVision() const { return *visionSensor; }
    const AudioSensor  &GetHearing() const { return *audioSensor; }
    const MemorySystem &GetMemory() const { return *memory; }

private:
    std::unique_ptr<VisionSensor> visionSensor = std::make_unique<VisionSensor>();
    std::unique_ptr<AudioSensor>  audioSensor  = std::make_unique<AudioSensor>();
    std::unique_ptr<MemorySystem> memory       = std::make_unique<MemorySystem>();
};

// Added in OPM - Phase 2 Task 2A.1.2
//  Vision sensor for detecting visible entities
class VisionSensor
{
public:
    VisionSensor();
    ~VisionSensor();

    // Update vision and return visible enemies
    std::vector<EnemyInfo> UpdateVision(Player *bot, float deltaTime);

    // Check if bot can see a specific entity
    bool CanSee(Player *bot, Sentient *target, float fov, float maxDistance);

private:
    // Added in OPM - Phase 2 Task 2A.1.2
    //  Helper method to check if target is within field of view
    bool CheckFOV(
        const Vector &botPos,
        const Vector &botAngles,
        const Vector &targetPos,
        float         fovDegrees,
        float        &angleFromForward
    );

    // Added in OPM - Phase 2 Task 2A.1.2
    //  Helper method to perform line of sight trace
    bool PerformLineOfSightTrace(Player *bot, Sentient *target);

    // Added in OPM - Phase 2 Task 2A.1.2
    //  Helper method to calculate visibility factor based on distance and peripheral vision
    float CalculateVisibilityFactor(float distance, float maxDistance, bool isPeripheral);
};

// Added in OPM - Phase 2 Task 2A.1.4
//  Audio sensor for detecting sound events
class AudioSensor
{
public:
    AudioSensor();
    ~AudioSensor();

    // Process audio events
    void ProcessEvent(int eventType, const Vector &position, float loudness);

    // Get recent audio events
    std::vector<AudioEvent> GetRecentSounds(float currentTime, float timeWindow);

private:
    std::vector<AudioEvent> eventQueue;
    // Audio implementation details (to be implemented in Task 2A.1.4)
};

// Added in OPM - Phase 2 Task 2A.1.5
//  Memory system for tracking previously seen enemies
class MemorySystem
{
public:
    MemorySystem();
    ~MemorySystem();

    // Update memory with newly seen enemy
    void UpdateMemory(const EnemyInfo &enemyInfo, float currentTime);

    // Get all remembered enemies
    std::vector<EnemyMemory> GetKnownEnemies(float currentTime);

    // Clear old memories
    void CleanupOldMemories(float currentTime, float maxAge);

private:
    std::vector<EnemyMemory> memories;
    // Memory implementation details (to be implemented in Task 2A.1.5)
};
