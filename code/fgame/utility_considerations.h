// Added in OPM - Phase 3 Task 3.4
// utility_considerations.h: Extract consideration values from game state

#pragma once

// Forward declarations
class Player;
class BotProfile;
struct PerceptionSnapshot;

namespace UtilityConsiderations
{
    // Extract raw consideration value from game state
    float ExtractConsideration(
        const char* considerationName,
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
}
