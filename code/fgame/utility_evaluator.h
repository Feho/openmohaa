// Added in OPM - Phase 3 Task 3.4
// utility_evaluator.h: Utility-based action selection for bot AI

#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class Player;
class BotProfile;
struct PerceptionSnapshot;
enum class CurveType;

class UtilityEvaluator
{
public:
    struct Consideration {
        std::string name;
        CurveType   curveType;
        float       weight;
        float       exponent;
        float       threshold;
        float       minValue;
        float       maxValue;

        Consideration();
    };

    struct ActionConfig {
        std::string                name;
        std::string                treeFile;
        std::vector<Consideration> considerations;
    };

    struct ScoredAction {
        std::string name;
        float       score;
        std::string treeFile;
    };

    UtilityEvaluator();
    ~UtilityEvaluator();

    // Main API
    ScoredAction SelectBestAction(
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );

    std::vector<ScoredAction> ScoreAllActions(
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );

    void LoadFromFile(const char* filename);

private:
    std::vector<ActionConfig> actions;

    float ScoreAction(
        const ActionConfig& action,
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );

    float EvaluateConsideration(
        const Consideration& consideration,
        const PerceptionSnapshot& perception,
        const Player* bot,
        const BotProfile* profile
    );
};
