// Added in OPM - Phase 2 Task 2A.1.1
// perception.h: Bot perception system for sensing and memory

#pragma once

#include "glb_local.h"
#include "sentient.h"
#include "player.h"
#include <vector>

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
struct EnemyInfo {
    Sentient *entity;        // Pointer to enemy entity
    Vector    position;      // Current position
    Vector    velocity;      // Current velocity
    float     distance;      // Distance to bot
    float     visibilityFactor; // 0.0 (barely visible) - 1.0 (clear view)
    float     angleFromForward; // Degrees off center of view
    bool      isInPeripheral; // True if in peripheral vision

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
    bool IsVisible() const { return visibilityFactor > 0.1f; }
    bool IsInPeripheral() const { return isInPeripheral; }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Information about a nearby ally
struct AllyInfo {
    Player *entity;   // Pointer to ally player
    Vector  position; // Current position
    float   distance; // Distance to bot
    bool    canSeeMe; // True if ally can see this bot

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
struct EnemyMemory {
    Sentient *enemy;                // Pointer to enemy entity
    Vector    lastKnownPosition;    // Last position where enemy was seen
    Vector    lastKnownVelocity;    // Last known velocity
    Vector    predictedPosition;    // Predicted current position
    float     lastSeenTime;         // Timestamp of last sighting
    float     confidenceLevel;      // 1.0 (just seen) -> 0.0 (very old)
    int       timesSpotted;         // Number of times spotted
    bool      investigationStarted; // True if bot started investigating

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
struct PerceptionSnapshot {
    // Enemies
    std::vector<EnemyInfo>   visibleEnemies; // Currently visible enemies
    std::vector<EnemyMemory> knownEnemies;   // Enemies from memory
    EnemyInfo               *closestEnemy;   // Pointer to closest visible enemy
    EnemyInfo               *mostDangerousEnemy; // Most threatening enemy

    // Allies
    std::vector<AllyInfo> nearbyAllies; // Nearby friendly players

    // Audio
    std::vector<AudioEvent> recentSounds; // Recent audio events
    AudioEvent             *loudestSound; // Loudest recent sound

    // Threat assessment
    ThreatLevel threatLevel; // Overall threat level

    PerceptionSnapshot()
        : closestEnemy(nullptr)
        , mostDangerousEnemy(nullptr)
        , loudestSound(nullptr)
        , threatLevel(THREAT_NONE)
    {
    }

    // Helper methods
    bool HasVisibleEnemy() const { return !visibleEnemies.empty(); }
    bool HasKnownEnemy() const { return !knownEnemies.empty(); }
    int  GetEnemyCount() const { return visibleEnemies.size(); }
    int  GetTotalKnownEnemies() const { return visibleEnemies.size() + knownEnemies.size(); }
};

// Added in OPM - Phase 2 Task 2A.1.1
//  Forward declarations for sensor classes
class VisionSensor;
class AudioSensor;
class MemorySystem;

// Added in OPM - Phase 2 Task 2A.1.1
//  Main perception system that integrates vision, hearing, and memory
class PerceptionSystem
{
public:
    PerceptionSystem();
    ~PerceptionSystem();

    // Main update method - returns snapshot of current perception state
    PerceptionSnapshot Update(Player *bot, float deltaTime);

    // Accessor methods for individual sensors
    VisionSensor  &GetVision() { return *visionSensor; }
    AudioSensor   &GetHearing() { return *audioSensor; }
    MemorySystem  &GetMemory() { return *memory; }

private:
    VisionSensor *visionSensor;
    AudioSensor  *audioSensor;
    MemorySystem *memory;
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
    // Vision implementation details (to be implemented in Task 2A.1.2)
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
