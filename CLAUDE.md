# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenMoHAA is an open-source preservation project for Medal of Honor: Allied Assault (including Spearhead and Breakthrough expansions). The codebase is built on top of ioquake3 and the F.A.K.K SDK, providing full compatibility with the original game while adding modern features like bots, improved networking, and cross-platform support.

## Build System

### Building the Project

OpenMoHAA uses CMake as its build system. Basic build commands:

```bash
# Standard build
mkdir .cmake && cd .cmake
cmake ../
cmake --build .
cmake --install .
```

### Build Options

Key CMake options:
- `-DCMAKE_BUILD_TYPE=Debug` - Build debug binaries (adds `-dbg` suffix)
- `-DCMAKE_INSTALL_PREFIX=/path/to/mohaa` - Set installation directory
- `-DBUILD_CLIENT=OFF` - Build only dedicated server binaries
- `-DTARGET_LOCAL_SYSTEM=1` - Remove architecture suffix from binaries
- `-DUSE_SYSTEM_LIBS=1` - Use system libraries instead of embedded ones
- `-DNO_MODERN_DMA=1` - Use basic DMA sound (not recommended)

### Platform-Specific Requirements

**Linux:**
```bash
# One-line install for dependencies
sudo apt-get install -y cmake ninja-build clang lld flex bison libsdl2-dev libopenal-dev libcurl4-openssl-dev
```

**Windows:**
- Visual Studio 2019 or 2022
- Flex/Bison from https://github.com/lexxmark/winflexbison/releases/latest
- Append `-DFLEX_EXECUTABLE=path/to/win_flex.exe -DBISON_EXECUTABLE=path/to/win_bison.exe -DOPENAL_INCLUDE_DIR="path/to/oal/include" -DOPENAL_LIBRARY="path/to/oal"` to CMake command

### Dependencies

The project requires the following external libraries:

- **CMake** 3.25+ - Build system
- **SDL2** - Graphics and input
- **OpenAL** - Audio
- **Flex/Bison** - Script parser generation
- **GoogleTest** 1.15.2+ - Unit testing (automatically fetched via CMake)
- **yaml-cpp** 0.7.0+ - YAML configuration parsing for bot profiles and behavior trees (Phase 2+, automatically fetched via CMake)

yaml-cpp is automatically fetched via CMake FetchContent. To use system yaml-cpp instead:
```bash
cmake -DUSE_SYSTEM_YAML_CPP=ON ..
```

## Architecture

### High-Level Structure

OpenMoHAA follows the classic Quake III Arena architecture with Medal of Honor-specific extensions:

```
Engine (qcommon + sys)
    ├── Client (client/)
    │   ├── Client Game (cgame/) - client-side game presentation
    │   └── Renderer (renderergl1/renderergl2/)
    │
    ├── Server (server/)
    │   └── Server Game (fgame/) - server-side game logic
    │
    └── Shared Systems
        ├── Script Engine (script/) - Custom scripting VM
        ├── TIKI Engine (tiki/) - Model/animation format
        ├── Skeletor (skeletor/) - Skeletal animation system
        └── Parser (parser/) - Flex/Bison-based script parser
```

### Key Components

**fgame/** - Server-side game logic
- All game simulation happens here (AI, physics, entities)
- Uses an event-driven class system based on `Listener` and `Event`
- Scripts interact with game logic through this module
- Examples: `actor.cpp`, `player.cpp`, `weapon.cpp`

**cgame/** - Client-side presentation
- Handles visual effects, HUD, client-side predictions
- No authoritative game state modifications
- Receives snapshots from server game

**script/** - Custom scripting system
- Complete scripting VM with compiler and runtime
- Scripts use `.scr` extension
- Event-driven architecture with subscription system
- Commands like `event_subscribe` allow monitoring game events

**tiki/** - TIKI model system (from F.A.K.K SDK)
- Handles model loading, animation definitions
- `.tik` files define model properties, animations, and attachments

**skeletor/** - Skeletal animation engine
- Advanced skeletal animation system
- Supports bone manipulation, IK, procedural animation

**parser/** - Script parser
- Flex/Bison-based parser for game scripts
- Parser sources in `code/parser/lex_source.txt` and `bison_source.txt`
- Generated files: `lex.yy.cpp`, `y.tab.cpp`

### Network Architecture

- Based on ioquake3's snapshot system
- Client-server model with prediction and lag compensation
- Protocol versioning for each expansion (MOHAA, Spearhead, Breakthrough)
- Separate protocols: `TARGET_GAME_PROTOCOL_MOH` (8), `TARGET_GAME_PROTOCOL_MOHTA` (17), `TARGET_GAME_PROTOCOL_MOHTT` (17)

### Game Modules

The project builds game modules that interface with the engine:
- Server game module (fgame) - Contains all gameplay logic
- Client game module (cgame) - Contains client presentation
- Modules communicate through public interfaces: `g_public.h`, `bg_public.h`, `cg_public.h`

## Coding Standards

### Naming Conventions

- **Variables**: `camelCase`
- **Functions**: `PascalCase`
- **Classes**: `PascalCase`
- **Events**: `EV_ClassName_EventName` (e.g., `EV_ExampleObject_TestMethod`)

### Formatting

Use `clang-format` for all code formatting. The project includes a `.clang-format` configuration:
- C++17 standard
- 4-space indentation (no tabs)
- 120 character line limit
- Pointer alignment right (`Type *ptr`)
- Braces: custom style with functions/classes on new line

### Code Annotations

When modifying code, annotate changes with comments:

**Additions:**
```cpp
// Added in OPM
//  Description of what was added
```

**Changes:**
```cpp
// Changed in OPM
//  Description of what was changed
```

**Fixes:**
```cpp
// Fixed in OPM
//  Description of what was fixed
```

**Removals:**
```cpp
// Removed in OPM
//  Description of what was removed
```

For changes matching specific game versions, replace "OPM" with version: `2.0`, `2.1`, `2.11`, `2.15`, `2.30`, `2.40`.

Group related changes:
```cpp
// Added in OPM
//====
void Function1();
void Function2();
void Function3();
//====
```

### Event Declaration Format

Always use this exact structure for Event declarations:
```cpp
Event EV_YourEventName  // Pascal Case naming
(
    "name",             // Each parameter on a new line
    flags,
    "format specifiers...",
    "argument names...",
    "description"
);
```

Format specifiers: `e` (Entity), `v` (Vector), `i` (Integer), `f` (Float), `s` (String), `b` (Boolean). Uppercase = optional.

### Class System

All game classes derive from `Listener` and use an event-driven architecture:

1. Use `CLASS_PROTOTYPE(ClassName)` in header
2. Use `CLASS_DECLARATION(ParentClass, ClassName, "class_id")` in source
3. Define event response table mapping events to methods
4. Classes can be spawned from scripts using class name or class ID

Example:
```cpp
// Header
class ExampleObject : public SimpleEntity {
    CLASS_PROTOTYPE(ExampleObject);
public:
    void TestMethod(Event *ev);
};

// Source
Event EV_ExampleObject_TestMethod
(
    "test_method",
    EV_DEFAULT,
    "i",
    "num_to_print",
    "This is a test method.",
    EV_NORMAL
);

CLASS_DECLARATION(SimpleEntity, ExampleObject, "info_exampleobject")
{
    {&EV_ExampleObject_TestMethod, &ExampleObject::TestMethod},
    {NULL, NULL}
};
```

## Bot AI System

### Overview

OpenMoHAA includes an advanced bot AI system for single-player and multiplayer matches. The bot system is built on an event-driven architecture and includes:

- **State-based behavior**: Attack, Investigate, Curious, Grenade, Idle states
- **Squad coordination**: Bots can work in squads with shared targets
- **Cover system**: Dynamic cover evaluation and usage
- **Pathfinding**: Integration with Recast/Detour navigation
- **Debug tools**: Console commands and visualization for development

### Architecture

The bot system consists of:
- `BotController` - Main bot AI controller (in `code/fgame/playerbot.h`)
- `BotMovement` - Movement and pathfinding logic
- `BotRotation` - Aiming and rotation control
- State implementations in `playerbot_*.cpp` files

### Debug Commands

- `bot_debug_info <botIndex>` - Print detailed bot state information
- `bot_force_state <botIndex> <stateIndex>` - Force bot into specific state for testing
- `bot_show_perception <botIndex>` - Toggle debug visualization (paths, enemies, perception)

Bot indices are 1-based (first bot = 1, second bot = 2, etc.)

State indices for `bot_force_state`:
- 0 = Attack state
- 1 = Investigate state
- 2 = Curious state
- 3 = Grenade state (placeholder)
- 4 = Idle state

### Code Quality

Bot code follows strict quality standards:
- Zero compiler warnings (strict checking enabled)
- Const-correctness enforced
- Named constants (no magic numbers)
- Formatted with clang-format

See `code/fgame/playerbot.h` for `BotConstants` namespace with all configuration values.

## Testing

### Running Tests

The project uses CTest with GoogleTest for unit testing (enabled in CMakeLists.txt line 56).

```bash
cd .cmake
ctest
# For verbose output:
ctest --output-on-failure
```

### Bot AI Test Suite

The bot system includes comprehensive unit tests (76 tests):
- Movement direction calculations (14 tests)
- Rotation and angle math (43 tests)
- Enemy validation and targeting (19 tests)

Test files are located in `tests/` directory:
- `tests/test_bot_movement.cpp` - Bot movement tests
- `tests/test_bot_rotation.cpp` - Rotation and angle tests
- `tests/test_bot_controller.cpp` - Controller logic tests
- `tests/test_utilities.h` - Test helpers and mocks

### Script Testing

Test scripts manually:
1. Start the game and enable cheats in console:
   ```
   set cheats 1
   set thereisnomonkey 1
   ```
2. Load a map: `set g_gametype 1;devmap dm/mohdm6`
3. Execute test script: `testthread tests/test_script.scr`

### In-Game Debugging Commands

- `classevents <classname>` - List all events for a class
- `dumpclassevents` - Dump all class events
- `version` - Get full version string with build info

## Important Compatibility Requirements

Any changes MUST maintain retro-compatibility:
- Assets must load correctly from original game files
- Networking changes must be compatible with original MOHAA clients/servers
- Scripts and mods from original game must work
- Single-player campaigns must remain fully playable

## Scripting System

### Event Subscription

Scripts can subscribe to game events:
```cpp
event_subscribe "event_name" script_label
event_unsubscribe "event_name" script_label
```

Available events include:
- `player_connected` - Player entered game
- `player_spawned` - Player spawned in battle
- `player_killed` - Player was killed
- `player_damaged` - Player took damage
- `player_disconnecting` - Player is leaving
- `player_textMessage` - Player sent chat message

### Script Variables

The scripting system supports:
- Local variables (`local.varname`)
- Self references (`self`)
- Event parameters passed to script labels
- Return values from script functions

## File Structure

Key directories:
- `code/fgame/` - Server game logic (most gameplay code)
  - `playerbot*.cpp` - Bot AI system implementation
  - `playerbot.h` - Bot controller and data structures
- `code/cgame/` - Client game (visual effects, HUD)
- `code/client/` - Client networking and snapshots
- `code/server/` - Server networking and client handling
- `code/script/` - Script VM and compiler
- `code/parser/` - Flex/Bison script parser
- `code/qcommon/` - Shared code (math, networking, file I/O)
- `code/renderer*` - Rendering backends (GL1, GL2)
- `code/tiki/` - TIKI model system
- `code/skeletor/` - Skeletal animation
- `code/uilib/` - UI library (Ubertools)
- `code/sys/` - Platform-specific system code
- `code/gamespy/` - GameSpy SDK integration
- `code/thirdparty/` - Embedded third-party libraries
- `tests/` - Unit tests (GoogleTest)
- `docs/markdown/` - Documentation
- `cmake/` - CMake modules

## Version Information

Version is defined in `cmake/identity.cmake` and `code/qcommon/q_shared.h`:
- Product name: `PRODUCT_NAME` = "OpenMoHAA"
- Base game: `BASEGAME` = "main"
- Supports three target games: MOH (Allied Assault), MOHTA (Spearhead), MOHTT (Breakthrough)
- Each has different protocol versions and content directories

## Additional Notes

- The project is pre-1.0.0 and considered beta
- Uses GNU GPL v2 license (see COPYING.txt and file headers)
- Avoid including the license header in new source files, it will be manually added by the user
- Only #include files that are actually needed
- Source files should contain classes/functions related to the feature
- Avoid creating documentation files unless explicitly requested
