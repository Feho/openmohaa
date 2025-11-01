cmake_minimum_required(VERSION 3.25)

# GoogleTest integration
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()
include(GoogleTest)

# Common include directories needed for bot tests
set(FGAME_SOURCE_DIR ${CMAKE_SOURCE_DIR}/code/fgame)
set(QCOMMON_SOURCE_DIR ${CMAKE_SOURCE_DIR}/code/qcommon)

# Phase 1 Fix (2025-10-27): Link tests to production code
# Production source files containing pure utility functions
# Tests now call actual game code instead of reimplementations
set(QCOMMON_MATH_SOURCES
    ${QCOMMON_SOURCE_DIR}/q_math.c
)

# Test utilities library
add_library(test_utilities STATIC
    test_utilities.cpp
    ${QCOMMON_MATH_SOURCES}
)

target_include_directories(test_utilities PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FGAME_SOURCE_DIR}
    ${QCOMMON_SOURCE_DIR}
)

target_link_libraries(test_utilities
    gtest
    gtest_main
)

set_property(TARGET test_utilities PROPERTY CXX_STANDARD 17)

# Helper function to add a bot test executable
# Reduces duplication and makes adding new tests easier
function(add_bot_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FGAME_SOURCE_DIR}
        ${QCOMMON_SOURCE_DIR}
    )

    target_link_libraries(${TEST_NAME}
        test_utilities
        gtest
        gtest_main
    )

    set_property(TARGET ${TEST_NAME} PROPERTY CXX_STANDARD 17)
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Add all bot tests using the helper function
add_bot_test(test_bot_movement)
add_bot_test(test_bot_rotation)
add_bot_test(test_bot_controller)

# Added in OPM - Phase 2 Task 2A.1.1
# Perception system structure tests
add_bot_test(test_perception)

# Added in OPM - Phase 2 Task 2A.1.2
# Vision sensor tests
add_bot_test(test_vision_sensor)

# Added in OPM - Phase 2 Task 2A.1.4
# Audio sensor tests
add_bot_test(test_audio_sensor)

# Added in OPM - Phase 2 Task 2A.1.5
# Memory system tests
add_bot_test(test_memory_system)

# Added in OPM - Phase 2 Task 2A.1.6
# PerceptionSystem integration tests
add_bot_test(test_perception_system)

# Added in OPM - Phase 2B Task 2B.1
# Behavior tree framework tests
add_bot_test(test_behavior_tree)

# Added in OPM - Phase 2 Task 2.0
# yaml-cpp integration test
add_executable(test_yaml_cpp test_yaml_cpp.cpp)

target_include_directories(test_yaml_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_yaml_cpp
    gtest
    gtest_main
    yaml-cpp
)

set_property(TARGET test_yaml_cpp PROPERTY CXX_STANDARD 17)

gtest_discover_tests(test_yaml_cpp)
