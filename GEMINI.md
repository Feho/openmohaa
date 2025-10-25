# OpenMoHAA

## Project Overview

OpenMoHAA is an open-source project aimed at preserving and enhancing Medal of Honor: Allied Assault (including Spearhead and Breakthrough expansions) by providing more features and bugfixes, across modern platforms and architectures.

It is written in C/C++ and uses CMake as its build system. The project is a reimplementation of the original game and is powered by the ioquake3 engine and the F.A.K.K SDK.

## Architecture

OpenMoHAA follows the classic Quake III Arena architecture with Medal of Honor-specific extensions:

- **Engine (qcommon + sys):** The core of the game, handling shared systems like file I/O, networking, and console commands.
- **Client (client/):** The game client, responsible for rendering, sound, and user input.
- **cgame (cgame/):** The client-side game logic, responsible for presentation and effects.
- **Server (server/):** The game server, responsible for managing game state and client connections.
- **fgame (fgame/):** The server-side game logic, where all game simulation happens (AI, physics, entities).

## Building and Running

### Dependencies

- CMake >= 3.12
- Flex (>= 2.6.4) and Bison (>= 3.5.1)
- A C++11 compiler
- SDL2
- OpenAL SDK
- cURL (optional)

### Building on Linux

```sh
sudo apt-get install -y cmake ninja-build clang lld flex bison libsdl2-dev libopenal-dev libcurl4-openssl-dev
mkdir .cmake && cd .cmake
cmake ../
cmake --build .
cmake --install .
```

### Building on Windows

- Visual Studio 2019 or 2022
- Flex/Bison from https://github.com/lexxmark/winflexbison/releases/latest
- OpenAL from https://github.com/kcat/openal-soft/releases/latest

```sh
mkdir .cmake && cd .cmake
cmake -DFLEX_EXECUTABLE=path/to/win_flex.exe -DBISON_EXECUTABLE=path/to/win_bison.exe -DOPENAL_INCLUDE_DIR="path/to/oal/include" -DOPENAL_LIBRARY="path/to/oal" ../
cmake --build .
cmake --install .
```

### Build Options

- `-DCMAKE_BUILD_TYPE=Debug`: Build debug binaries.
- `-DCMAKE_INSTALL_PREFIX=/path/to/mohaa`: Set the installation directory.
- `-DBUILD_CLIENT=OFF`: Build only the dedicated server.

## Coding Standards

- **Naming Conventions:** `camelCase` for variables, `PascalCase` for functions and classes.
- **Formatting:** Use `clang-format` for all code formatting. The project includes a `.clang-format` configuration file.
- **Code Annotations:** Annotate changes with comments like `// Added in OPM`, `// Changed in OPM`, `// Fixed in OPM`, or `// Removed in OPM`.

## Testing

### CTest

The project uses CTest for unit testing.

```bash
cd .cmake
ctest
```

### Script Testing

Test scripts manually from the in-game console:

1.  Enable cheats: `set cheats 1; set thereisnomonkey 1`
2.  Load a map: `set g_gametype 1; devmap dm/mohdm6`
3.  Execute a test script: `testthread tests/test_script.scr`