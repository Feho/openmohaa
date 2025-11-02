# Task 3.1d: Cover System Integration

**Parent Task**: Task 3.1 - Migrate Attack Behavior  
**Status**: Ready to Execute  
**Duration**: 3 days  
**Priority**: MEDIUM  
**Dependencies**: Task 3.1f (Core Combat Tree)

---

## Overview

Integrate the existing cover system (`playerbot_cover.cpp`) into behavior trees. This adds tactical depth by allowing bots to find, move to, and utilize cover during combat.

### What This Achieves

- **Cover Finding**: Evaluates nearby positions for cover quality
- **Cover Movement**: Multi-frame action to move to selected cover
- **Cover Peeking**: Timed exposure from cover to fire
- **Cover States**: Manages MOVING_TO, IN_COVER, PEEKING states
- **Tactical Integration**: Cover disabled at close range (<384 units)

---

## Current Implementation

### File: `code/fgame/playerbot_cover.cpp`

#### FindBestCover() - Lines 42-122

```cpp
BotController::CoverPoint BotController::FindBestCover(Vector enemyPos)
{
    CoverPoint bestCover;
    bestCover.position        = vec_zero;
    bestCover.quality         = 0.0f;
    bestCover.protectionAngle = 0.0f;
    bestCover.distanceToEnemy = 0.0f;
    bestCover.hasEscapeRoute  = false;
    bestCover.evaluatedTime   = level.inttime;

    Vector      botPos       = controlledEnt->origin;
    float       searchRadius = g_bot_cover_search_radius->value;
    const int   gridSteps    = 8; // 8x8 grid
    const float stepSize     = searchRadius / gridSteps;

    // Sample positions in a grid around the bot
    for (int x = -gridSteps; x <= gridSteps; x++) {
        for (int y = -gridSteps; y <= gridSteps; y++) {
            if (x == 0 && y == 0) continue;

            Vector testPos = botPos;
            testPos.x += x * stepSize;
            testPos.y += y * stepSize;

            // Check if position is valid (on ground, not in solid)
            trace_t trace;
            Vector  start = testPos + Vector(0, 0, 32);
            Vector  end   = testPos - Vector(0, 0, 128);

            trace = G_Trace(start, controlledEnt->mins, controlledEnt->maxs, end,
                controlledEnt, MASK_PLAYERSOLID, false, "BotController::FindBestCover");

            if (trace.fraction == BotConstants::TRACE_COMPLETE 
                || trace.allsolid || trace.startsolid) {
                continue;  // No ground or inside solid
            }

            testPos = trace.endpos;

            // Evaluate cover quality
            float quality = EvaluateCoverQuality(testPos, enemyPos);

            if (quality > bestCover.quality) {
                bestCover.position        = testPos;
                bestCover.quality         = quality;
                bestCover.distanceToEnemy = (enemyPos - testPos).length();
                bestCover.evaluatedTime   = level.inttime;
            }
        }
    }

    return bestCover;
}
```

#### EvaluateCoverQuality() - Lines 131-236

```cpp
float BotController::EvaluateCoverQuality(Vector pos, Vector enemyPos)
{
    float quality = 0.0f;

    // 1. Check line of sight obstruction
    Vector eyePos      = pos + Vector(0, 0, controlledEnt->viewheight);
    Vector enemyEyePos = enemyPos + Vector(0, 0, 64);

    trace_t trace = G_Trace(eyePos, vec_zero, vec_zero, enemyEyePos, 
        controlledEnt, MASK_SHOT, false, "BotController::EvaluateCoverQuality");

    if (trace.fraction < BotConstants::TRACE_COMPLETE && trace.entityNum != ENTITYNUM_NONE) {
        quality += 0.5f;  // Something blocking = good cover

        // 2. Check protection angle (how much is protected)
        int protectedAngles = 0;
        const int angleSteps = 8;

        for (int i = 0; i < angleSteps; i++) {
            float  angle = (i * 360.0f / angleSteps) * M_PI / 180.0f;
            Vector offset(cos(angle) * 32, sin(angle) * 32, 0);
            Vector checkPos = eyePos + offset;

            trace_t angleTrace = G_Trace(eyePos, vec_zero, vec_zero, checkPos,
                controlledEnt, MASK_SHOT, false, "BotController::EvaluateCoverQuality");

            if (angleTrace.fraction < BotConstants::TRACE_COMPLETE) {
                protectedAngles++;
            }
        }

        float protectionRatio = protectedAngles / (float)angleSteps;
        quality += protectionRatio * 0.3f;
    }

    // 3. Distance factor (prefer cover not too close/far)
    float distToEnemy    = (enemyPos - pos).length();
    float idealDistance  = BotConstants::AWARENESS_RADIUS;
    float distanceFactor = 1.0f - fabs(distToEnemy - idealDistance) / 1024.0f;
    if (distanceFactor < 0.0f) distanceFactor = 0.0f;
    quality += distanceFactor * 0.2f;

    // 4. Check escape routes
    int escapeRoutes = 0;
    const Vector testDirections[4] = {
        Vector(64, 0, 0), Vector(-64, 0, 0),
        Vector(0, 64, 0), Vector(0, -64, 0)
    };

    for (int i = 0; i < 4; i++) {
        Vector escapePos = pos + testDirections[i];
        trace_t escapeTrace = G_Trace(pos, controlledEnt->mins, controlledEnt->maxs,
            escapePos, controlledEnt, MASK_PLAYERSOLID, false, "BotController::EvaluateCoverQuality");

        if (escapeTrace.fraction > 0.5f) {
            escapeRoutes++;
        }
    }

    if (escapeRoutes >= 2) {
        quality += 0.1f;
    }

    return (quality > 1.0f) ? 1.0f : (quality < 0.0f ? 0.0f : quality);
}
```

### CoverPoint Structure

```cpp
struct CoverPoint {
    Vector position;        // Cover position
    float quality;          // 0.0 (no cover) - 1.0 (excellent)
    float protectionAngle;  // How much protection
    float distanceToEnemy;  // Distance from enemy
    bool hasEscapeRoute;    // Can escape from cover
    int evaluatedTime;      // When evaluated
};
```

### Cover States

```cpp
enum CoverState {
    COVER_NONE,          // Not using cover
    COVER_MOVING_TO,     // Moving to selected cover
    COVER_IN_COVER,      // At cover position
    COVER_PEEKING,       // Exposing to fire
    COVER_REPOSITIONING  // Moving to different cover
};
```

---

## Behavior Tree Design

### Actions

#### `FindCover`
- **Purpose**: Evaluates nearby positions and selects best cover
- **Returns**: SUCCESS if cover found, FAILURE if none
- **Blackboard**: Writes `SELECTED_COVER` (CoverPoint)

#### `MoveToCover`
- **Purpose**: Multi-frame movement to selected cover point
- **Returns**: RUNNING (moving), SUCCESS (arrived)
- **Blackboard**: Reads `SELECTED_COVER`

#### `PeekFromCover`
- **Purpose**: Temporarily exposes from cover to fire, then returns
- **Returns**: RUNNING (peeking), SUCCESS (peek complete)
- **Blackboard**: Reads `SELECTED_COVER`, writes `PEEK_START_TIME`, `PEEK_DURATION`

#### `ReturnToCover`
- **Purpose**: Returns to cover position after peeking
- **Returns**: SUCCESS when in cover
- **Blackboard**: Reads `SELECTED_COVER`

### Conditions

#### `HasCoverAvailable`
- **Returns**: true if `SELECTED_COVER` exists with quality > threshold

#### `IsInCover`
- **Returns**: true if bot at cover position

#### `ShouldUseCover`
- **Returns**: false if enemy < 384 units (close range disables cover), true otherwise

---

## Implementation Steps

### Day 1: Cover Finding (8 hours)
- [ ] Port `FindBestCover()` logic to `Action_FindCover`
- [ ] Port `EvaluateCoverQuality()` helper
- [ ] Add `CoverPoint` struct to blackboard keys
- [ ] Add CVars: `g_bot_cover_search_radius`, `g_bot_cover_min_quality`
- [ ] Write 3 unit tests

### Day 2: Cover Movement & Peeking (8 hours)
- [ ] Implement `Action_MoveToCover` (multi-frame)
- [ ] Implement `Action_PeekFromCover` with timing
- [ ] Implement `Action_ReturnToCover`
- [ ] Add profile parameters: `cover_usage`, `peek_duration`
- [ ] Write 3 unit tests

### Day 3: Integration & Testing (8 hours)
- [ ] Implement conditions (HasCoverAvailable, IsInCover, ShouldUseCover)
- [ ] Integrate into combat tree (optional selector branch)
- [ ] Test in-game with cover objects
- [ ] Write 2 integration tests
- [ ] Profile tuning (aggressive vs defensive cover usage)

---

## Testing Strategy

### Unit Tests (8 tests)

1. **FindCover_NoCover**: Returns FAILURE when no suitable cover
2. **FindCover_GoodCover**: Selects high-quality cover point
3. **EvaluateCoverQuality_Obstruction**: Quality increases with LOS block
4. **EvaluateCoverQuality_Distance**: Prefers ideal distance
5. **MoveToCover_Arrives**: SUCCESS when reaching cover
6. **PeekFromCover_Timing**: Peek lasts correct duration
7. **ShouldUseCover_CloseRange**: FALSE when enemy < 384 units
8. **ShouldUseCover_NormalRange**: TRUE when enemy > 384 units

---

## Files to Create/Modify

```
code/fgame/bt_actions_cover.cpp
code/fgame/bt_actions_cover.h
code/fgame/bt_conditions_cover.cpp
code/fgame/bt_conditions_cover.h
tests/test_cover_system.cpp
```

---

## Blackboard Keys

```cpp
namespace BlackboardKeys
{
    constexpr const char* SELECTED_COVER = "selectedCover";       // CoverPoint
    constexpr const char* COVER_STATE = "coverState";             // int (CoverState enum)
    constexpr const char* PEEK_START_TIME = "peekStartTime";      // float
    constexpr const char* PEEK_DURATION = "peekDuration";         // float
}
```

---

## Profile Parameters

```yaml
tactics:
  cover_usage: 0.7               # 0.0 (never) - 1.0 (always)
  peek_min_time: 1.0             # seconds
  peek_max_time: 2.5             # seconds
  hide_min_time: 2.0             # seconds
  hide_max_time: 4.0             # seconds
```

---

## Integration with Combat Tree

Add optional cover branch to `behaviors/combat.yaml`:

```yaml
# Before main combat engagement
- type: selector
  name: "Use Cover If Available"
  children:
    - type: sequence
      name: "Find And Move To Cover"
      children:
        - type: condition
          check: "ShouldUseCover"
        - type: condition
          check: "HasCoverAvailable"
          invert: true
        - type: action
          action: "FindCover"
        - type: action
          action: "MoveToCover"
    
    - type: action
      action: "DoNothing"  # Skip cover
```
