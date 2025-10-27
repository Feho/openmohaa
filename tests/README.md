# OpenMoHAA Bot AI Unit Tests

This directory contains unit tests for the OpenMoHAA bot AI system.

## Overview

The test suite uses GoogleTest framework and is integrated with CMake/CTest. Tests run automatically in CI via GitHub Actions.

## Test Structure

### Test Files

- **test_bot_movement.cpp** - Tests for BotMovement class
  - CalculateDir() function (11 tests)
  - Direction normalization and 2D projection

- **test_bot_rotation.cpp** - Tests for BotRotation class and angle math (46 tests)
  - AngleDifference() function
  - AngleMod() normalization
  - NormalizeAngle180() helper
  - Clamping and min/max utilities

- **test_bot_controller.cpp** - Tests for BotController class (18 tests)
  - IsValidEnemy() logic
  - Distance calculations
  - Target selection and switching thresholds

### Test Utilities

- **test_utilities.h** - Common test helpers
  - TestVector class for vector math
  - Mock structures for game entities
  - Comparison helpers (FloatEquals, Vec3Equals)
  - Math utilities (AngleMod, AngleDifference, etc.)

- **test_utilities.cpp** - Implementation (currently minimal)

## Building and Running Tests

### Configure with tests enabled:
```bash
cd .cmake
cmake .. -DBUILD_TESTING=ON
```

### Build all tests:
```bash
cmake --build . --target test_bot_movement test_bot_rotation test_bot_controller
```

### Run all tests:
```bash
ctest --output-on-failure
```

### Run specific test:
```bash
./tests/test_bot_movement
./tests/test_bot_rotation --gtest_filter=BotRotationTest.AngleDifference*
```

## Test Coverage

Current test coverage focuses on:
- Pure math functions (angle calculations, vector operations)
- Logic functions that can be tested in isolation (IsValidEnemy)
- Edge cases and boundary conditions

### What's NOT Covered (Yet)

Tests currently do NOT cover:
- Full bot state machine (requires game environment)
- Path finding integration (requires navigation mesh)
- Weapon firing and combat systems (requires full entity system)
- Network/multiplayer bot behavior

These will require more extensive mocking or integration test infrastructure.

## Adding New Tests

1. Create test file in `tests/` directory (e.g., `test_bot_feature.cpp`)
2. Add to `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_bot_feature test_bot_feature.cpp)
   target_link_libraries(test_bot_feature test_utilities gtest gtest_main)
   gtest_discover_tests(test_bot_feature)
   ```
3. Write tests using GoogleTest macros (TEST_F, EXPECT_TRUE, etc.)
4. Build and run: `cmake --build . --target test_bot_feature && ./tests/test_bot_feature`

## CI Integration

Tests run automatically in CI via `.github/workflows/unit-testing.yml`:
- Triggers on all pushes and pull requests
- Runs on Ubuntu with Clang
- Build fails if any test fails
- Test output shown on failure

## Test Results Summary

As of Task 1.3 completion:
- **Total Tests:** 65
- **Passing:** 65 (100%)
- **Test Execution Time:** ~0.10 seconds

### Test Breakdown:
- BotMovement: 11 tests
- BotRotation: 46 tests
- BotController: 18 tests

All tests validate critical bot AI math and logic functions that were identified in Phase 1 of the bot improvement project.
