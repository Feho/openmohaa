#!/usr/bin/env bash

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

build_dir="${BUILD_DIR:-.cmake}"
target="${TARGET:-game}"
log_file="${LOG:-build.log}"

cd "$repo_root" || exit 1

echo "Building: ninja -C $build_dir $target"
echo "Full output: $log_file"

ninja -C "$build_dir" "$target" > "$log_file" 2>&1
build_status=$?

if ((build_status == 0)); then
    echo "Build passed."
    exit 0
fi

echo "Build failed with exit code $build_status."
echo "Last 40 log lines:"
tail -n 40 "$log_file"
exit "$build_status"
