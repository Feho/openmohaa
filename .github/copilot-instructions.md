# OpenMoHAA AI Coding Assistant Instructions

## Project Identity

OpenMoHAA is an open-source preservation project for Medal of Honor: Allied Assault (MOHAA/Spearhead/Breakthrough). Built on ioquake3 and F.A.K.K SDK, it provides full compatibility with the original game while adding modern features like bots, cross-platform support, and enhanced networking.

**Critical**: All changes must maintain retro-compatibility with original game assets, scripts, mods, and network protocols.

## Architecture Overview

### Core Structure (Quake III Arena-based)

```
Engine Layer (qcommon + sys)
├── Server (server/) + fgame/ - Authoritative game simulation
├── Client (client/) + cgame/ - Presentation and prediction
└── Shared Systems
    ├── Script VM (script/) - Custom .scr scripting
    ├── TIKI/Skeletor - Model/skeletal animation
    └── Parser (parser/) - Flex/Bison script compiler
```

**Key Directories**:
- `code/fgame/` - All gameplay logic (AI, physics, entities) lives here
- `code/cgame/` - Client-side visual effects, HUD, predictions
- `code/script/` - Complete scripting VM with event-driven architecture
- `code/parser/` - Flex (`lex_source.txt`) and Bison (`bison_source.txt`) parser sources generate `lex.yy.cpp`/`y.tab.cpp`
- `tests/` - GoogleTest unit tests (76 tests for bot AI)

### Game Module System

The engine loads game logic as modules (`cgame`, `game`, `ui`) that communicate through public interfaces (`g_public.h`, `bg_public.h`, `cg_public.h`). Server game (fgame) is authoritative; client game (cgame) handles presentation only.

### Event-Driven Architecture

All game classes inherit from `Listener` and respond to `Event` objects. This is fundamental:

```cpp
// Event declaration (always use this exact format)
Event EV_ClassName_MethodName
(
    "script_name",           // Script-callable name
    EV_DEFAULT,              // Flags
    "ifs",                   // Format: i=int f=float s=string e=entity v=vector b=bool (uppercase=optional)
    "num name path",         // Argument names
    "Description",
    EV_NORMAL
);

// Class declaration
CLASS_DECLARATION(ParentClass, MyClass, "script_id")
{
    {&EV_ClassName_MethodName, &MyClass::MethodHandler},
    {NULL, NULL}
};
```

Classes can be spawned from scripts using their class ID. Events enable script-to-code and code-to-script communication.

## Build System

**CMake 3.25+** is required. Key options:
- `-DCMAKE_BUILD_TYPE=Debug` - Adds `-dbg` suffix to binaries
- `-DBUILD_CLIENT=OFF` - Server-only build
- `-DTARGET_LOCAL_SYSTEM=1` - Remove architecture suffix
- `-DUSE_SYSTEM_YAML_CPP=ON` - Use system yaml-cpp (default: FetchContent auto-downloads 0.7.0)

**Dependencies**: SDL2, OpenAL, Flex ≥2.6.4, Bison ≥3.5.1, GoogleTest 1.15.2+ (auto-fetched), yaml-cpp 0.7.0+ (auto-fetched for bot profiles)

**Build workflow**:
```bash
mkdir .cmake && cd .cmake
cmake ../
cmake --build .
cmake --install .  # Installs to /usr/local/lib/openmohaa (Linux) or Program Files (Windows)
```

**Testing**: `ctest` or `ctest --output-on-failure` (76 bot AI tests + others)

## Bot AI System (Phase 2 Migration)

The bot system is undergoing architectural transformation (see `bot-ai-architecture/README.md`):

**Current State**: State-based AI in `code/fgame/playerbot*.cpp` with 5 states (Attack, Investigate, Curious, Grenade, Idle)

**Phase 2 Integration**: Behavior trees + data-driven YAML profiles alongside legacy system
- Behavior tree framework: `behavior_tree.h`, `behavior_tree_builder.h`, `bt_core_actions.h`
- YAML profiles: `profiles/*.yaml` define aggression, teamwork, skill parameters
- Behavior YAML: `behaviors/*.yaml` define tree structures loaded at runtime
- Test coverage: `tests/test_behavior_tree.cpp` (comprehensive BT node tests)

**Configuration**: All bot constants in `BotConstants` namespace (`playerbot.h`) - use named constants, never magic numbers.

**Debug commands** (bot indices are 1-based):
```
bot_debug_info <botIndex>           // Print detailed state
bot_force_state <botIndex> <state>  // 0=Attack 1=Investigate 2=Curious 3=Grenade 4=Idle
bot_show_perception <botIndex>      // Toggle debug visualization
```

## Code Standards

### Naming Conventions
- Variables: `camelCase`
- Functions/Classes: `PascalCase`
- Events: `EV_ClassName_EventName`
- Constants: `UPPER_SNAKE_CASE` (in namespaces like `BotConstants`)

### Formatting (clang-format)
- C++17 standard
- 4 spaces (no tabs), 120 char line limit
- Pointer/reference alignment: `Type *ptr` (right)
- Braces: Custom style (functions/classes on new line)
- **Always format code**: Run `clang-format` on all changes

### Code Annotations
Mark all modifications with context:
```cpp
// Added in OPM
//  Description of addition

// Changed in OPM
//  Description of change

// Fixed in OPM
//  Description of fix
```

For version-specific changes, replace "OPM" with: `2.0`, `2.1`, `2.11`, `2.15`, `2.30`, `2.40`

### Best Practices
- **No magic numbers**: Use named constants from `BotConstants` or define new ones
- **Include minimally**: Only #include files actually needed
- **Const-correctness**: Enforce const where applicable (bot code is strict)
- **Zero warnings**: Bot code compiles with strict checking enabled
- **License headers**: Avoid adding license headers; maintainers add them manually
- **File organization**: Keep classes/functions related to the same feature together

## Version & Protocol Information

Version defined in `cmake/identity.cmake` and `code/qcommon/q_shared.h`:
- Product: `PRODUCT_NAME` = "OpenMoHAA"
- Version: 1.36 (pre-1.0.0 = beta status)
- Base game: `BASEGAME` = "main"
- Three targets: MOH (protocol 8), Spearhead (protocol 17), Breakthrough (protocol 17)
- Each expansion has separate protocol versions and content directories

## Scripting System

### Script Event Subscription
Scripts can subscribe to game events:
```scr
event_subscribe "player_spawned" script_label
event_unsubscribe "player_spawned" script_label
```

Available events: `player_connected`, `player_spawned`, `player_killed`, `player_damaged`, `player_disconnecting`, `player_textMessage`

### Script Testing Workflow
```
set cheats 1; set thereisnomonkey 1
set g_gametype 1; devmap dm/mohdm6
testthread tests/test_script.scr
```

### Debugging Commands
- `classevents <classname>` - List all events for a class
- `dumpclassevents` - Dump all class events
- `version` - Get full version string with build info

## Development Workflow

**Parser Changes**: Editing `code/parser/lex_source.txt` or `bison_source.txt` requires CMake rebuild to regenerate parser code.

**New Game Classes**: 
1. Inherit from `Listener` (or appropriate parent)
2. Use `CLASS_PROTOTYPE(ClassName)` in header
3. Use `CLASS_DECLARATION(Parent, ClassName, "class_id")` in source
4. Define event response table mapping events to methods
5. Scripts can spawn using class name or ID

**Adding Bot Behaviors**:
- For legacy system: Add methods in `playerbot_*.cpp` files, update state machine
- For Phase 2 system: Create/modify YAML in `behaviors/` or add actions in `bt_core_actions.cpp`
- Always add unit tests in `tests/test_*.cpp`

**Testing Priorities**:
1. Run `ctest` after changes (especially bot AI changes)
2. Test in-game with `devmap` command
3. Verify retro-compatibility with original game assets
4. Check network compatibility if touching protocol code

## Common Patterns

### Distance Calculations
```cpp
// Use squared distances to avoid sqrt (performance)
float distSq = (pos1 - pos2).lengthSquared();
if (distSq < BotConstants::CLOSE_RANGE_THRESHOLD * BotConstants::CLOSE_RANGE_THRESHOLD) {
    // Close range logic
}
```

### Angle Normalization
```cpp
// Always normalize angles to [-180, 180]
float NormalizeAngle(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}
```

### Trace/Raycast Pattern
```cpp
trace_t trace = G_Trace(start, vec_zero, vec_zero, end, entity, MASK_SOLID, qfalse, "TraceDescription");
if (trace.fraction < BotConstants::TRACE_COMPLETE) {
    // Hit something at trace.endpos
}
```

## Integration Points

- **Recast/Detour Navigation**: Integrated in `navigation_recast_*.cpp` for pathfinding (replaces legacy PathManager)
- **GameSpy SDK**: Multiplayer matchmaking in `code/gamespy/`
- **OpenAL**: Audio system with 3D positioning and attenuation (see `BotConstants::MAX_AUDIO_DISTANCE`)
- **SDL2**: Input, window management, platform abstraction

## Documentation

- **Main docs**: `docs/markdown/` (installation, configuration, scripting reference)
- **Bot AI architecture**: `bot-ai-architecture/README.md` (vision, epics, migration strategy)
- **Phase 2 tasks**: `bot-ai-architecture-refined/Phase-2B/` (current implementation phase)
- **API reference**: Generate with Doxygen (`docs/Doxyfile`)

## Anti-Patterns to Avoid

❌ Don't create documentation files for each change (unless explicitly requested)  
❌ Don't break retro-compatibility with original game  
❌ Don't use magic numbers (define in `BotConstants` namespace)  
❌ Don't modify parser source without rebuilding CMake  
❌ Don't add license headers (maintainers handle this)  
❌ Don't mix debug/release binaries (they have different suffixes)  

## Quick Reference

**Project constants**: See `BotConstants` namespace in `code/fgame/playerbot.h` (vision ranges, timing, movement thresholds, combat behavior)

**Entry points**:
- Game initialization: `code/fgame/g_main.cpp::G_InitGame()`
- Bot think loop: `code/fgame/playerbot_core.cpp::BotController::Think()`
- Client frame: `code/cgame/cg_main.cpp::CG_Frame()`

**Key files to understand first**:
1. `code/qcommon/q_shared.h` - Core types, constants, macros
2. `code/fgame/g_local.h` - Server game definitions
3. `code/fgame/entity.h` - Base entity class
4. `code/fgame/playerbot.h` - Bot system overview
