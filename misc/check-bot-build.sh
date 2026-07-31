#!/usr/bin/env bash

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

build_dir="${BUILD_DIR:-.cmake}"
target="${TARGET:-game}"
log_file="${LOG:-build.log}"

cd "$repo_root" || exit 1

echo "Checking bot movement side-effect allowlist..."

allowlist_output="$(
    awk '
    function allowed(fn) {
        return fn == "BotController::ExecuteResolvedCommand" \
            || fn == "BotController::ScriptHoldPosition" \
            || fn == "BotController::ScriptHoldPositionAt" \
            || fn == "BotController::ScriptStop" \
            || fn == "BotController::ScriptMoveTo" \
            || fn == "BotController::ScriptMoveNear" \
            || fn == "BotController::ScriptReleaseControl" \
            || fn == "BotController::Spawned"
    }

    match($0, /BotController::[A-Za-z0-9_]+[[:space:]]*\(/) {
        current_function = substr($0, RSTART, RLENGTH)
        sub(/[[:space:]]*\($/, "", current_function)
    }

    /movement\.(ClearMove|MoveTo|MoveNear|AvoidPath|MoveToBestAttractivePoint)[[:space:]]*\(/ {
        if (!allowed(current_function)) {
            printf("%s:%d: %s\n", FILENAME, FNR, $0)
            violations = 1
        }
    }

    END {
        exit violations ? 1 : 0
    }
    ' code/fgame/playerbot.cpp
)"
allowlist_status=$?

if [ "$allowlist_status" -ne 0 ]; then
    echo "Allowlist check failed. Unexpected movement side effects:"
    echo "$allowlist_output"
    exit "$allowlist_status"
fi

echo "Allowlist check passed."
echo "Building: ninja -C $build_dir $target"
echo "Log: $log_file"

ninja -C "$build_dir" "$target" > "$log_file" 2>&1
build_status=$?

if [ "$build_status" -eq 0 ]; then
    echo "Build passed."
    exit 0
fi

echo "Build failed with exit code $build_status."
echo "Last 40 log lines:"
tail -n 40 "$log_file"
exit "$build_status"
