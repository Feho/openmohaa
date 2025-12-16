# Architecture Overview

This document provides a high-level overview of the OpenMoHAA codebase for developers looking to understand or contribute to the project.

## Project Background

OpenMoHAA is an open-source preservation project for Medal of Honor: Allied Assault (including Spearhead and Breakthrough expansions). The codebase is built on top of [ioquake3](https://ioquake3.org/) and the F.A.K.K SDK, providing full compatibility with the original game while adding modern features.

## High-Level Architecture

OpenMoHAA follows the classic Quake III Arena architecture with Medal of Honor-specific extensions:

```
Engine (qcommon + sys)
    |
    +-- Client (client/)
    |   +-- Client Game (cgame/) - Client-side game presentation
    |   +-- Renderer (renderergl1/renderergl2/)
    |
    +-- Server (server/)
    |   +-- Server Game (fgame/) - Server-side game logic
    |
    +-- Shared Systems
        +-- Script Engine (script/) - Custom scripting VM
        +-- TIKI Engine (tiki/) - Model/animation format
        +-- Skeletor (skeletor/) - Skeletal animation system
        +-- Parser (parser/) - Flex/Bison-based script parser
```

## Key Components

### fgame/ - Server Game Logic

This is where all authoritative game simulation happens:
- AI behavior and pathfinding
- Physics simulation
- Entity management
- Player logic
- Weapon mechanics
- Game modes (FFA, TDM, Objective, etc.)

Key files:
- `actor.cpp` - AI actor behavior
- `player.cpp` - Player logic
- `weapon.cpp` - Weapon mechanics
- `g_main.cpp` - Game initialization

Scripts interact with game logic through the event system defined here.

### cgame/ - Client Game

Handles client-side presentation only:
- Visual effects and particles
- HUD rendering
- Client-side prediction
- Sound playback
- Camera control

The client game receives snapshots from the server and presents them visually. It never makes authoritative game state changes.

### script/ - Scripting System

A complete scripting VM with compiler and runtime:
- Scripts use `.scr` extension
- Event-driven architecture with subscription system
- Supports local variables, self references, and return values

See [Script Events](02-scripting/01-script-events.md) for the event subscription system.

### tiki/ - TIKI Model System

From the F.A.K.K SDK, handles:
- Model loading and management
- Animation definitions
- Model attachments and properties

Files use `.tik` extension and define model properties, animations, and bone attachments.

### skeletor/ - Skeletal Animation

Advanced skeletal animation engine:
- Bone manipulation
- Inverse kinematics (IK)
- Procedural animation
- Animation blending

### parser/ - Script Parser

Flex/Bison-based parser for game scripts:
- Source files: `code/parser/lex_source.txt` and `bison_source.txt`
- Generated files: `lex.yy.cpp`, `y.tab.cpp` (and CMake also generates `generated/yyLexer.cpp` and `generated/yyParser.cpp` via `flex_target`/`bison_target`)


## Event-Driven Class System

Most gameplay entity classes derive from `Listener` (commonly via `SimpleEntity`) and use an event-driven architecture. This is the core pattern used throughout the gameplay code; utility classes may not follow this pattern.

### Class Declaration Pattern

**Header file:**
```cpp
#include "simpleentity.h"

class ExampleObject : public SimpleEntity {
    CLASS_PROTOTYPE(ExampleObject);

public:
    ExampleObject();
    ~ExampleObject();

    void SomeMethod(Event *ev);
};
```

**Source file:**
```cpp
#include "exampleobject.h"

Event EV_ExampleObject_SomeMethod
(
    "some_method",      // Command name
    EV_DEFAULT,         // Flags
    "i",                // Format: i=int, f=float, s=string, v=vector, e=entity, b=bool
    "argument_name",    // Argument names
    "Description",      // Documentation
    EV_NORMAL           // Type: EV_NORMAL, EV_RETURN, EV_GETTER, EV_SETTER
);

CLASS_DECLARATION(SimpleEntity, ExampleObject, "info_exampleobject")
{
    {&EV_ExampleObject_SomeMethod, &ExampleObject::SomeMethod},
    {NULL, NULL}  // Required terminator
};
```

Classes can be spawned from scripts using either the class name or class ID:
```cpp
local.ent = spawn ExampleObject
local.ent = spawn info_exampleobject  // Using class ID
```

### Event Format Specifiers

| Specifier | Type | Description |
|-----------|------|-------------|
| `e` | Entity | Entity reference |
| `v` | Vector | 3D vector |
| `i` | Integer | Integer value |
| `f` | Float | Floating point |
| `s` | String | String value |
| `b` | Boolean | Boolean value |

Uppercase letters indicate optional parameters.

## Network Architecture

Based on ioquake3's snapshot system:
- Client-server model with prediction and lag compensation
- Protocol versioning for each expansion
- Separate protocols per game version

| Game | Protocol Version |
|------|-----------------|
| Allied Assault | 8 |
| Spearhead | 17 |
| Breakthrough | 17 |

## Directory Structure

```
code/
+-- fgame/        Server-side game logic (most gameplay code)
+-- cgame/        Client-side presentation (effects, HUD)
+-- client/       Client networking and snapshots
+-- server/       Server networking and client handling
+-- script/       Script VM and compiler
+-- parser/       Flex/Bison script parser
+-- qcommon/      Shared code (math, networking, file I/O)
+-- renderergl1/  OpenGL 1.x renderer
+-- renderergl2/  OpenGL 2.x renderer
+-- tiki/         TIKI model system
+-- skeletor/     Skeletal animation
+-- uilib/        UI library (Ubertools)
+-- sys/          Platform-specific system code
+-- gamespy/      GameSpy SDK integration
+-- thirdparty/   Embedded third-party libraries

docs/             Documentation
cmake/            CMake modules
```

## Game Modules

The project builds game modules that interface with the engine through public interfaces:

| Module | Interface | Purpose |
|--------|-----------|---------|
| Server Game (fgame) | `g_public.h` | Gameplay logic |
| Client Game (cgame) | `cg_public.h` | Client presentation |
| Shared | `bg_public.h` | Common definitions |

## Version Information

Version and product identity are defined in `cmake/identity.cmake` (for example `PROJECT_NAME` and `PROJECT_VERSION`).
- Product name: "OpenMoHAA"
- The project uses `main` as the base game directory and `mainta`/`maintt` for expansions by convention; CMake and docs consistently reference these directories.

## Compatibility Requirements

Any changes must maintain retro-compatibility:
- Assets must load correctly from original game files
- Networking changes must be compatible with original MOHAA clients/servers
- Scripts and mods from original game must work
- Single-player campaigns must remain fully playable

## Further Reading

- [Creating a New Class](01-code/01-creating-class.md) - Detailed guide on adding new classes
- [Script Events](02-scripting/01-script-events.md) - Event subscription system
- [Compiling](01-compiling.md) - Build instructions
- [Differences from Original](../01-intro/04-differences.md) - Changes and fixes in OpenMoHAA
