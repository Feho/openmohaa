# OpenMoHAA development commands.
# Run `just` to list recipes.

build_dir := env_var_or_default("BUILD_DIR", ".cmake")
build_type := env_var_or_default("BUILD_TYPE", "Release")
build_log := env_var_or_default("BUILD_LOG", "build.log")
clang_format := env_var_or_default("CLANG_FORMAT", "clang-format")
mohaa_dir := env_var_or_default("MOHAA_DIR", home_directory() + "/MOHAA")
server_home := env_var_or_default("SERVER_HOME", "")
server_bin := env_var_or_default("SERVER_BIN", build_dir + "/" + build_type + "/omohaaded")

# List available recipes.
default:
    @just --list

# Check the local tools and paths used by the recipes.
doctor:
    @command -v cmake >/dev/null && cmake --version | head -n 1
    @command -v ninja >/dev/null && ninja --version | sed 's/^/ninja /'
    @command -v {{clang_format}} >/dev/null && {{clang_format}} --version
    @command -v just >/dev/null && just --version
    @test -d "{{mohaa_dir}}/main" && echo "MOHAA assets: {{mohaa_dir}}" || { echo "Missing MOHAA assets: {{mohaa_dir}} (set MOHAA_DIR)"; exit 1; }

# Configure or refresh the Ninja build. Pass extra CMake options after `--`.
configure +args="":
    cmake -S . -B "{{build_dir}}" -G Ninja -DCMAKE_BUILD_TYPE="{{build_type}}" {{args}}

# Refresh an existing CMake build without changing its cached build type.
reconfigure +args="":
    @test -f "{{build_dir}}/CMakeCache.txt" || { echo "Missing configured build: {{build_dir}} (run 'just configure')"; exit 1; }
    cmake -S . -B "{{build_dir}}" {{args}}

# Quietly build any Ninja target, writing compiler output to BUILD_LOG.
build target="all":
    BUILD_DIR="{{build_dir}}" TARGET="{{target}}" LOG="{{build_log}}" ./misc/check-game-build.sh

# Quietly build the server game module.
game:
    @just build game

# Check the bot movement allowlist, then quietly build the game module.
bots:
    BUILD_DIR="{{build_dir}}" TARGET=game LOG="{{build_log}}" ./misc/check-bot-build.sh

# Refresh CMake's source glob, then quietly build the game module.
game-refresh:
    @just reconfigure
    @just game

# Quietly build the dedicated-server executable.
server-build:
    @just build omohaaded

# Quietly build all configured targets.
all:
    @just build all

# Run CTest, printing details only for failures. Pass extra CTest options after `--`.
test +args="":
    ctest --test-dir "{{build_dir}}" --output-on-failure {{args}}

# Build the game module, run tests, and check the patch for whitespace errors.
check:
    @just game-refresh
    @just test
    @just diff-check

# Run the guarded bot build, tests, and patch whitespace check.
check-bots:
    @just bots
    @just test
    @just diff-check

# Check the current patch for whitespace errors.
diff-check:
    git diff --check

# Format only explicitly named C/C++ files.
format +files:
    {{clang_format}} -i {{files}}

# Verify formatting without modifying explicitly named C/C++ files.
format-check +files:
    {{clang_format}} --dry-run --Werror {{files}}

# Show the tail of the quiet build log.
build-log lines="40":
    @tail -n "{{lines}}" "{{build_log}}"

# List configured Ninja targets.
targets:
    @ninja -C "{{build_dir}}" -t targets | sort

# Show concise repository status.
status:
    @git status --short

# Run a recipe from the MOHAA operational justfile, e.g. `just mohaa logs`.
mohaa +args="":
    @test -f "{{mohaa_dir}}/justfile" || { echo "Missing operational justfile: {{mohaa_dir}}/justfile"; exit 1; }
    just --justfile "{{mohaa_dir}}/justfile" {{args}}

# Run the built dedicated server against the normal MOHAA asset tree.
server map="dm/mohdm1" gametype="1" port="12204" gamespy_port="12301" +args="":
    @test -x "{{server_bin}}" || { echo "Missing server binary: {{server_bin}} (run 'just server-build')"; exit 1; }
    "{{server_bin}}" +set fs_basepath "{{mohaa_dir}}" +set com_target_game 0 +set dedicated 2 +set g_gametype "{{gametype}}" +set net_port "{{port}}" +set net_gamespy_port "{{gamespy_port}}" +set thereisnomonkey 1 +set cheats 1 +set developer 1 +map "{{map}}" {{args}}

# Run against only the original Pak0-Pak5 archives, isolated from installed mods.
# Set SERVER_HOME to overlay scripts/configuration; otherwise a temporary home is used.
server-clean map="dm/mohdm1" gametype="1" port="12204" gamespy_port="12301" +args="":
    #!/usr/bin/env bash
    set -euo pipefail

    mohaa_dir="{{mohaa_dir}}"
    server_bin="{{server_bin}}"

    if [[ ! -x "$server_bin" ]]; then
        echo "Missing server binary: $server_bin (run 'just server-build')"
        exit 1
    fi

    runtime_root="$(mktemp -d "${TMPDIR:-/tmp}/openmohaa-just.XXXXXX")"
    trap 'rm -rf -- "$runtime_root"' EXIT

    clean_base="$runtime_root/base"
    generated_home="$runtime_root/home"
    runtime_home="{{server_home}}"
    if [[ -z "$runtime_home" ]]; then
        runtime_home="$generated_home"
    fi

    mkdir -p "$clean_base/main" "$runtime_home/main/bots"

    for pak_number in {0..5}; do
        pak="$mohaa_dir/main/Pak${pak_number}.pk3"
        if [[ ! -f "$pak" ]]; then
            echo "Missing original archive: $pak"
            exit 1
        fi
        ln -s "$pak" "$clean_base/main/Pak${pak_number}.pk3"
    done

    profiles="{{justfile_directory()}}/main/bots/profiles"
    if [[ -d "$profiles" && ! -e "$runtime_home/main/bots/profiles" ]]; then
        ln -s "$profiles" "$runtime_home/main/bots/profiles"
    fi

    echo "Clean base: $clean_base"
    echo "Server home: $runtime_home"
    "$server_bin" \
        +set fs_basepath "$clean_base" \
        +set fs_homepath "$runtime_home" \
        +set fs_homedatapath "$runtime_home" \
        +set com_target_game 0 \
        +set dedicated 2 \
        +set g_gametype "{{gametype}}" \
        +set net_port "{{port}}" \
        +set net_gamespy_port "{{gamespy_port}}" \
        +set sv_maxbots 4 \
        +set g_bot_initial_spawn_delay 0 \
        +set logfile 0 \
        +set thereisnomonkey 1 \
        +set cheats 1 \
        +set developer 1 \
        +map "{{map}}" {{args}}
