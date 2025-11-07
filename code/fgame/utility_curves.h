// Added in OPM - Phase 3 Task 3.4
// utility_curves.h: Curve types and transformation functions for utility AI

#pragma once

enum class CurveType {
    LINEAR,          // output = input
    EXPONENTIAL,     // output = input^exponent
    INVERSE_LINEAR,  // output = 1.0 - input
    THRESHOLD,       // output = input > threshold ? 1.0 : 0.0
    LOGISTIC         // output = 1/(1 + e^(-k*(input-0.5)))
};

// Apply curve transformation to normalized input [0.0, 1.0]
float ApplyCurve(float input, CurveType type, float exponent = 2.0f, float threshold = 0.5f);
