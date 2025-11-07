#include "g_local.h"
#include "utility_curves.h"
#include <cmath>
#include <algorithm>

float ApplyCurve(float input, CurveType type, float exponent, float threshold)
{
    // Clamp input to [0.0, 1.0] range
    float normalized = std::clamp(input, 0.0f, 1.0f);

    float output = 0.0f;

    switch (type) {
        case CurveType::LINEAR:
            // Direct passthrough: f(x) = x
            output = normalized;
            break;

        case CurveType::EXPONENTIAL:
            // Exponential curve: f(x) = x^n
            // Emphasizes high values, penalizes low values
            output = std::pow(normalized, exponent);
            break;

        case CurveType::INVERSE_LINEAR:
            // Inverted linear: f(x) = 1 - x
            // Flips the relationship
            output = 1.0f - normalized;
            break;

        case CurveType::THRESHOLD:
            // Binary threshold: f(x) = x > threshold ? 1 : 0
            // All-or-nothing decision
            output = (normalized > threshold) ? 1.0f : 0.0f;
            break;

        case CurveType::LOGISTIC:
            // S-curve (sigmoid): f(x) = 1 / (1 + e^(-k*(x-0.5)))
            // Smooth transition with adjustable steepness (k = exponent)
            {
                float k = exponent;  // Steepness parameter
                output = 1.0f / (1.0f + std::exp(-k * (normalized - 0.5f)));
            }
            break;

        default:
            output = 0.0f;
            break;
    }

    // Clamp output to [0.0, 1.0] range
    return std::clamp(output, 0.0f, 1.0f);
}
