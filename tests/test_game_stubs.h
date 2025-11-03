// Added in OPM - Phase 2B Task 2B.1
// test_game_stubs.h: Minimal stubs for testing behavior tree without full game context

#ifndef __TEST_GAME_STUBS_H__
#define __TEST_GAME_STUBS_H__

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

// Include q_shared.h to get errorParm_t enum and other shared types
#include "../code/qcommon/q_shared.h"

// Minimal game import interface for testing
struct game_import_stub_t {
    void Error(int level, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        // Exit on any error for testing purposes
        if (level != 0) {
            exit(1);
        }
    }
    
    void DPrintf(const char *fmt, ...) {
        // Debug print - can be silent in tests or print for debugging
        va_list args;
        va_start(args, fmt);
        // vfprintf(stderr, fmt, args);  // Uncomment for debug output
        va_end(args);
    }
};

// Global game import instance for tests
static game_import_stub_t gi;

#endif // __TEST_GAME_STUBS_H__
