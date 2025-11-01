// Added in OPM - Phase 2B Task 2B.1
// test_game_stubs.h: Minimal stubs for testing behavior tree without full game context

#ifndef __TEST_GAME_STUBS_H__
#define __TEST_GAME_STUBS_H__

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

// Error types
#define ERR_DROP 1

// Minimal game import interface for testing
struct game_import_stub_t {
    void Error(int level, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        if (level == ERR_DROP) {
            exit(1);
        }
    }
};

// Global game import instance for tests
static game_import_stub_t gi;

#endif // __TEST_GAME_STUBS_H__
