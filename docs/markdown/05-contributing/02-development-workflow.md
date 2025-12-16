# Development Workflow

This guide covers practical development tasks for OpenMoHAA contributors. For coding standards and PR guidelines, see the main [CONTRIBUTING.md](https://github.com/openmoh/openmohaa/blob/main/CONTRIBUTING.md).

## Development Environment Setup

### Prerequisites

Ensure you have the build tools installed. See [Compiling](../04-coding/01-compiling.md) for platform-specific instructions.

### Recommended Setup

1. **Clone the repository:**
   ```sh
   git clone https://github.com/openmoh/openmohaa.git
   cd openmohaa
   ```

2. **Create a build directory:**
   ```sh
   mkdir .cmake && cd .cmake
   ```

3. **Configure for development:**
   ```sh
   # Debug build (recommended for development)
   cmake -DCMAKE_BUILD_TYPE=Debug ../

   # Set install path to your MOHAA directory
   cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/path/to/mohaa ../
   ```

4. **Build:**
   ```sh
   cmake --build .
   ```

5. **Install to MOHAA directory:**
   ```sh
   cmake --install .
   ```

Debug builds may append `-dbg` to binary names via CMake's `CMAKE_DEBUG_POSTFIX` (see `cmake/compilers/all.cmake`). Common binary names are `openmohaa` (client) and `omohaaded` (dedicated server); check your build directory for the exact filename (it may be `openmohaa`, `openmohaa-dbg`, or similar).

### IDE Setup

#### Visual Studio Code

1. Install the C/C++ extension
2. Install the CMake Tools extension
3. Open the project folder
4. Select your kit (compiler) when prompted
5. Use the CMake panel to configure and build

#### Visual Studio (Windows)

1. Open the folder with **File > Open > Folder**
2. Visual Studio will detect the CMakeLists.txt automatically
3. Select the configuration (Debug/Release) from the toolbar
4. Build with **Build > Build All**

## Code Formatting

The project uses `clang-format` for consistent formatting.

### Format Before Committing

```sh
# Format a single file
clang-format -i code/fgame/myfile.cpp

# Format all changed files (using git)
git diff --name-only | grep -E '\.(cpp|h)$' | xargs clang-format -i
```

### VS Code Integration

1. Install the C/C++ extension
2. Set `clang-format` as the default formatter
3. Enable "Format on Save" in settings

The project includes a `.clang-format` configuration file that defines:
- 4-space indentation (no tabs)
- 120 character line limit
- Right-aligned pointers (`Type *ptr`)

## Testing Your Changes

### Running Unit Tests

The project uses CTest:

```sh
cd .cmake
ctest
```

## Debugging

### Console Commands

| Command | Description |
|---------|-------------|
| `classevents <classname>` | List all events for a class |
| `dumpclassevents` | Dump all class events to a file |
| `version` | Display full version string with build info |
| `developer 1` | Enable developer messages |
| `devmap <mapname>` | Load map with cheats enabled |

### Using Debug Builds

Debug builds (`-DCMAKE_BUILD_TYPE=Debug`) enable:
- Debug symbols for debugger attachment
- Additional runtime checks
- Verbose logging

### Debugging on Linux

```sh
# Run with GDB
gdb ./openmohaa-dbg

# Inside GDB
(gdb) run +set developer 1
```

### Debugging on Windows

1. Build with Debug configuration
2. Open Visual Studio
3. **Debug > Attach to Process** or press F5 if using CMake integration
4. Set breakpoints in your code

### Print Debugging

Use `gi.Printf()` for server-side debug output:

```cpp
gi.Printf("Debug: value = %d\n", someValue);
```

Use `cgi.Printf()` for client-side output.

## Working with the Event System

### Finding Existing Events

Search for event declarations:

```sh
# Find all events for a class
grep -r "Event EV_Player" code/fgame/

# Find event registration
grep -r "CLASS_DECLARATION" code/fgame/player.cpp
```

### Listing Events at Runtime

In the game console:

```
classevents Player
```

This lists all events the `Player` class responds to.

## Common Development Tasks

### Adding a New Console Variable (Cvar)

```cpp
// In header or at file scope
extern cvar_t *sv_myfeature;

// In initialization
sv_myfeature = gi.cvar("sv_myfeature", "0", CVAR_ARCHIVE);

// Usage
if (sv_myfeature->integer) {
    // Feature enabled
}
```

### Adding a New Script Command

See [Creating a New Class](../04-coding/01-code/01-creating-class.md) for the full guide.

Quick reference:

```cpp
// 1. Declare the event
Event EV_MyClass_NewCommand
(
    "new_command",
    EV_DEFAULT,
    "s",
    "param",
    "Description of command",
    EV_NORMAL
);

// 2. Add to class declaration
CLASS_DECLARATION(ParentClass, MyClass, "my_class")
{
    {&EV_MyClass_NewCommand, &MyClass::NewCommand},
    {NULL, NULL}
};

// 3. Implement the method
void MyClass::NewCommand(Event *ev)
{
    str param = ev->GetString(1);
    // Implementation
}
```

### Modifying Network Protocol

> [!WARNING]
> Network changes must maintain compatibility with original MOHAA clients/servers.

If you must modify networking:
1. Document the change thoroughly
2. Test with original game clients
3. Consider protocol versioning

## Build Configurations

| Option | Purpose |
|--------|---------|
| `-DCMAKE_BUILD_TYPE=Debug` | Debug build with symbols |
| `-DCMAKE_BUILD_TYPE=Release` | Optimized release build |
| `-DBUILD_CLIENT=OFF` | Build server only |
| `-DTARGET_LOCAL_SYSTEM=1` | Remove arch suffix from binaries |
| `-DUSE_SYSTEM_LIBS=1` | Use system libraries |

## Troubleshooting Build Issues

See [Developer FAQ](../04-coding/03-developer-faq.md) for common issues and solutions.
