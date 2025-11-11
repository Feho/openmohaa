#include "g_local.h"
#include "utility_evaluator.h"
#include "utility_curves.h"
#include "utility_considerations.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <yaml-cpp/yaml.h>

// Added in OPM - Phase 3 Task 3.4 Commit 3
//  Small epsilon to prevent division by zero in normalization
static constexpr float EPSILON = 0.0001f;

// Consideration constructor
UtilityEvaluator::Consideration::Consideration()
    : curveType(CurveType::LINEAR)
    , weight(1.0f)
    , exponent(2.0f)
    , threshold(0.5f)
    , minValue(0.0f)
    , maxValue(1.0f)
{}

// UtilityEvaluator constructor
UtilityEvaluator::UtilityEvaluator() {}

// UtilityEvaluator destructor
UtilityEvaluator::~UtilityEvaluator() {}

UtilityEvaluator::ScoredAction
UtilityEvaluator::SelectBestAction(const PerceptionSnapshot& perception, const Player *bot, const BotProfile *profile)
{
    // Score all actions and find the best one
    auto scoredActions = ScoreAllActions(perception, bot, profile);

    if (scoredActions->empty()) {
        // Fallback to idle if no actions configured
        return {"idle", 0.0f, "behaviors/idle.yaml"};
    }

    // Find action with highest score
    auto bestAction = std::max_element(
        scoredActions->begin(),
        scoredActions->end(),
        [](const ScoredAction& a, const ScoredAction& b) { return a.score < b.score; }
    );

    return *bestAction;
}

// Changed in OPM
//  Return shared_ptr to avoid unnecessary copy
std::shared_ptr<std::vector<UtilityEvaluator::ScoredAction>>
UtilityEvaluator::ScoreAllActions(const PerceptionSnapshot& perception, const Player *bot, const BotProfile *profile)
{
    auto scoredActions = std::make_shared<std::vector<ScoredAction>>();
    scoredActions->reserve(actions.size());

    for (const ActionConfig& action : actions) {
        float score = ScoreAction(action, perception, bot, profile);
        scoredActions->push_back({action.name, score, action.treeFile});
    }

    return scoredActions;
}

void UtilityEvaluator::LoadFromFile(const char *filename)
{
    // Added in OPM - Phase 3 Task 3.4 Commit 4
    //  Load utility AI configuration from YAML file

    try {
        // Fixed in OPM
        //  Added null termination and RAII for exception safety
        void *buffer = nullptr;
        long  length = gi.FS_ReadFile(filename, &buffer, qfalse);

        if (length <= 0 || !buffer) {
            gi.Printf("ERROR: Could not read utility config file: %s\n", filename);
            if (buffer) {
                gi.FS_FreeFile(buffer);
            }
            return;
        }

        // Create string from buffer to ensure null termination
        std::string yaml_content(static_cast<const char *>(buffer), static_cast<size_t>(length));
        gi.FS_FreeFile(buffer);

        // Parse YAML from string (exception-safe now)
        YAML::Node root = YAML::Load(yaml_content);

        // Validate root structure
        if (!root["actions"]) {
            gi.Printf("ERROR: Utility config missing 'actions' root: %s\n", filename);
            return;
        }

        // Clear existing actions
        actions.clear();

        // Parse each action
        YAML::Node actionsNode = root["actions"];
        for (const auto& actionNode : actionsNode) {
            ActionConfig action;

            // Parse action name
            if (actionNode["name"]) {
                action.name = actionNode["name"].as<std::string>();
            } else {
                gi.Printf("WARNING: Action missing 'name' field, skipping\n");
                continue;
            }

            // Parse tree file
            if (actionNode["tree_file"]) {
                action.treeFile = actionNode["tree_file"].as<std::string>();
            } else {
                gi.Printf("WARNING: Action '%s' missing 'tree_file' field, skipping\n", action.name.c_str());
                continue;
            }

            // Parse considerations
            if (actionNode["considerations"]) {
                for (const auto& considNode : actionNode["considerations"]) {
                    Consideration consideration;

                    // Parse consideration name (required)
                    if (considNode["name"]) {
                        consideration.name = considNode["name"].as<std::string>();
                    } else {
                        gi.Printf(
                            "WARNING: Consideration in action '%s' missing 'name' field, skipping\n",
                            action.name.c_str()
                        );
                        continue;
                    }

                    // Parse curve type (required)
                    if (considNode["curve"]) {
                        std::string curveStr = considNode["curve"].as<std::string>();
                        if (curveStr == "linear") {
                            consideration.curveType = CurveType::LINEAR;
                        } else if (curveStr == "exponential") {
                            consideration.curveType = CurveType::EXPONENTIAL;
                        } else if (curveStr == "inverse_linear") {
                            consideration.curveType = CurveType::INVERSE_LINEAR;
                        } else if (curveStr == "threshold") {
                            consideration.curveType = CurveType::THRESHOLD;
                        } else if (curveStr == "logistic") {
                            consideration.curveType = CurveType::LOGISTIC;
                        } else {
                            gi.Printf(
                                "WARNING: Unknown curve type '%s' for consideration '%s', using linear\n",
                                curveStr.c_str(),
                                consideration.name.c_str()
                            );
                            consideration.curveType = CurveType::LINEAR;
                        }
                    } else {
                        // Default to linear if not specified
                        consideration.curveType = CurveType::LINEAR;
                    }

                    // Parse optional parameters
                    if (considNode["weight"]) {
                        consideration.weight = considNode["weight"].as<float>();
                    }
                    if (considNode["exponent"]) {
                        consideration.exponent = considNode["exponent"].as<float>();
                    }
                    if (considNode["threshold"]) {
                        consideration.threshold = considNode["threshold"].as<float>();
                    }
                    if (considNode["min"]) {
                        consideration.minValue = considNode["min"].as<float>();
                    }
                    if (considNode["max"]) {
                        consideration.maxValue = considNode["max"].as<float>();
                    }

                    // Fixed in OPM
                    //  Validate min < max to prevent division by zero
                    if (consideration.minValue >= consideration.maxValue) {
                        gi.Printf(
                            "WARNING: Consideration '%s' has invalid range [%.2f, %.2f], using [0.0, 1.0]\n",
                            consideration.name.c_str(),
                            consideration.minValue,
                            consideration.maxValue
                        );
                        consideration.minValue = 0.0f;
                        consideration.maxValue = 1.0f;
                    }

                    action.considerations.push_back(consideration);
                }
            }

            // Add action to list
            actions.push_back(action);
            gi.Printf(
                "Loaded utility action '%s' with %zu considerations\n",
                action.name.c_str(),
                action.considerations.size()
            );
        }

        gi.Printf("Successfully loaded %zu utility actions from %s\n", actions.size(), filename);

    } catch (const YAML::Exception& e) {
        gi.Printf("ERROR: Failed to parse utility config file %s: %s\n", filename, e.what());
    } catch (const std::exception& e) {
        gi.Printf("ERROR: Exception loading utility config file %s: %s\n", filename, e.what());
    }
}

float UtilityEvaluator::ScoreAction(
    const ActionConfig& action, const PerceptionSnapshot& perception, const Player *bot, const BotProfile *profile
)
{
    // Added in OPM - Gemini review suggestion
    //  Assertions for null checks in debug builds
    assert(bot && "Bot entity is null");
    assert(profile && "Bot profile is null");
    
    if (action.considerations.empty()) {
        return 0.0f;
    }

    // Added in OPM - Phase 3 Task 3.4 Commit 3
    //  Multiplicative utility scoring with compensation factor
    //  Using formula: score = (product of considerations) ^ (1 / N)
    //  This prevents scores from dropping too quickly with many considerations

    float product             = 1.0f;
    int   validConsiderations = 0;

    for (const Consideration& consideration : action.considerations) {
        // Added in OPM - Gemini review suggestion
        //  Validate weight is non-negative to catch config errors
        assert(consideration.weight >= 0.0f && "Consideration weights cannot be negative");
        
        float value = EvaluateConsideration(consideration, perception, bot, profile);

        // Changed in OPM
        //  Fixed weight application - use exponentiation instead of multiplication
        //  Weight as exponent properly scales influence: value^weight
        //  weight > 1.0 increases influence, weight < 1.0 decreases influence
        //  This keeps values in [0,1] range and respects utility theory
        float weightedValue = std::pow(value, consideration.weight);

        // Accumulate product
        product *= std::max(weightedValue, EPSILON);
        validConsiderations++;
    }

    // Fixed in OPM
    //  Check for zero considerations to prevent division by zero
    if (validConsiderations == 0) {
        return 0.0f;
    }

    // Apply compensation factor to prevent excessive penalization
    // Uses geometric mean: score = (product)^(1/N)
    float compensation = 1.0f / static_cast<float>(validConsiderations);
    float finalScore   = std::pow(product, compensation);

    return std::clamp(finalScore, 0.0f, 1.0f);
}

float UtilityEvaluator::EvaluateConsideration(
    const Consideration&      consideration,
    const PerceptionSnapshot& perception,
    const Player             *bot,
    const BotProfile         *profile
)
{
    // Step 1: Extract raw value from game state
    float rawValue = UtilityConsiderations::ExtractConsideration(consideration.name.c_str(), perception, bot, profile);

    // Step 2: Normalize to consideration's min/max range
    float range = consideration.maxValue - consideration.minValue;
    if (range < EPSILON) {
        // Avoid division by zero - if min == max, return 0 or 1
        return (rawValue >= consideration.minValue) ? 1.0f : 0.0f;
    }

    float normalized = (rawValue - consideration.minValue) / range;
    normalized       = std::clamp(normalized, 0.0f, 1.0f);

    // Step 3: Apply curve transformation
    float transformed =
        ApplyCurve(normalized, consideration.curveType, consideration.exponent, consideration.threshold);

    return transformed;
}
