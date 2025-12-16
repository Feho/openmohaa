# Developer FAQ & Troubleshooting

This guide covers common issues developers encounter when building, running, and debugging OpenMoHAA.

## Build Issues

### CMake Issues

<details>
<summary>CMake can't find Flex or Bison</summary>

**Symptoms:**
```
Could NOT find FLEX (missing: FLEX_EXECUTABLE)
Could NOT find BISON (missing: BISON_EXECUTABLE)
```

**Solution:**

**Linux:**
```sh
sudo apt-get install flex bison
```

**Windows:**
1. Download from https://github.com/lexxmark/winflexbison/releases/latest
2. Extract to a known location
3. Add to CMake command:
   ```sh
   cmake -DFLEX_EXECUTABLE=C:/path/to/win_flex.exe -DBISON_EXECUTABLE=C:/path/to/win_bison.exe ../
   ```
</details>

<details>
<summary>CMake can't find SDL2</summary>

**Symptoms:**
```
Could NOT find SDL2 (missing: SDL2_LIBRARY SDL2_INCLUDE_DIR)
```

**Solution:**

**Linux:**
```sh
sudo apt-get install libsdl2-dev
```

**Windows:**
- SDL2 should be found automatically if installed via vcpkg or in standard locations
- Otherwise, download from https://libsdl.org and set `SDL2_DIR`
</details>

<details>
<summary>CMake can't find OpenAL</summary>

**Symptoms:**
```
Could NOT find OpenAL
```

**Solution:**

**Linux:**
```sh
sudo apt-get install libopenal-dev
```

**Windows:**
1. Download from https://github.com/kcat/openal-soft/releases/latest
2. Rename `soft_oal.dll` to `OpenAL64.dll` (64-bit) or `OpenAL32.dll` (32-bit)
3. Add to CMake command:
   ```sh
   cmake -DOPENAL_INCLUDE_DIR="C:/path/to/oal/include" -DOPENAL_LIBRARY="C:/path/to/oal" ../
   ```
</details>

<details>
<summary>Compiler version too old</summary>

**Symptoms:**
```
error: 'string_view' is not a member of 'std'
error: expected constructor, destructor, or type conversion before '<' token
```

**Solution:**

OpenMoHAA targets **C++17**. Ensure your compiler supports C++17. Recommended minimums:
- **GCC**: 9.4.0 or newer
- **Clang**: 10 or newer
- **MSVC**: Visual Studio 2019 or newer

**Linux:**
```sh
# Install newer compiler
sudo apt-get install g++-11
# Or use clang
sudo apt-get install clang-12

# Specify compiler to CMake
cmake -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12 ../
```
</details>

### Linker Issues

<details>
<summary>Undefined reference errors</summary>

**Symptoms:**
```
undefined reference to `some_function'
```

**Common causes:**
1. Missing library in CMakeLists.txt
2. Missing source file in CMakeLists.txt
3. Circular dependencies

**Solution:**
- Check if the source file is listed in the appropriate `CMakeLists.txt`
- Ensure required libraries are linked
</details>

<details>
<summary>Multiple definition errors</summary>

**Symptoms:**
```
multiple definition of `SomeGlobal'
```

**Solution:**
- Variables in headers should be `extern` with definition in one `.cpp` file
- Check for accidental duplicate definitions
</details>

## Runtime Issues

### Startup Problems

<details>
<summary>Game crashes immediately on startup</summary>

**Possible causes:**
1. Missing game assets (pak files)
2. Wrong working directory
3. Missing DLLs (Windows)

**Solutions:**
1. Ensure OpenMoHAA is installed in the MOHAA directory
2. Run from the MOHAA directory or set `fs_basepath`
3. Install Visual C++ Redistributable (Windows)
</details>

<details>
<summary>"Couldn't load default.cfg" error</summary>

**Cause:** Game can't find base assets.

**Solution:**
```sh
# Run with explicit paths
./openmohaa +set fs_basepath /path/to/mohaa
```
</details>

<details>
<summary>OpenAL errors or no sound</summary>

**Symptoms:**
```
ALSA lib pcm.c:8526:(snd_pcm_recover) underrun occurred
AL_INVALID_OPERATION
```

**Solutions:**
1. Update OpenAL drivers
2. Check audio device:
   ```
   set s_device "default"
   ```
3. Try disabling OpenAL:
   ```sh
   cmake -DNO_MODERN_DMA=1 ../
   ```
</details>

### Script Errors

<details>
<summary>"Unknown command" when running testthread</summary>

**Symptom:**
```
Unknown command "testthread"
```

**Solution:**
Enable cheats first:
```
set cheats 1
set thereisnomonkey 1
devmap dm/mohdm6
testthread tests/your_script.scr
```
</details>

<details>
<summary>Script syntax errors</summary>

**Symptom:**
```
Parse error: unexpected token...
```

**Common causes:**
1. Missing `end` statement
2. Unclosed strings
3. Wrong indentation (tabs vs spaces don't usually matter, but consistency helps)

**Debug tip:** Add print statements to narrow down the issue:
```cpp
main:
    println "Checkpoint 1"
    // ... code ...
    println "Checkpoint 2"
end
```
</details>

<details>
<summary>Entity spawning fails</summary>

**Symptom:**
```
Could not spawn "MyClass"
```

**Solution:**
- Verify `CLASS_DECLARATION` is correct
- Check that source file is compiled (listed in CMakeLists.txt)
- Try using the class ID instead of class name
</details>

### Networking Issues

<details>
<summary>Server doesn't appear in server list</summary>

**Possible causes:**
1. Firewall blocking ports
2. `sv_gamespy` is 0
3. IPv6 only mode (master server requires IPv4)

**Solution:**
```
set sv_gamespy 1
set net_enabled 1  // IPv4 only, or 3 for both
```

Open UDP ports: 12203, 12300
</details>

<details>
<summary>Clients disconnect with "Invalid protocol"</summary>

**Cause:** Protocol version mismatch.

**Solution:**
- Ensure client and server are using the same `com_target_game` value
- Original MOHAA clients can only connect to servers running compatible versions
</details>

## Debugging Techniques

### Using Developer Mode

Enable verbose logging:
```
set developer 1
set developer 2  // Even more verbose
```

### Finding Class Events

List all events a class responds to:
```
classevents Player
classevents Actor
classevents Weapon
```

Dump all events to file:
```
dumpclassevents
```

### Tracing Script Execution

Add debug output to scripts:
```cpp
main:
    dprintln("Starting script")
    local.ent = spawn SimpleEntity
    dprintln("Entity spawned: " + local.ent)
end
```

### Memory Debugging (Linux)

Use Valgrind to find memory issues:
```sh
valgrind --leak-check=full ./openmohaa-dbg +devmap dm/mohdm1
```

Use AddressSanitizer:
```sh
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" -DCMAKE_C_FLAGS="-fsanitize=address" ../
```

### Debugging with GDB

```sh
gdb ./openmohaa-dbg
(gdb) break Player::Spawn
(gdb) run +devmap dm/mohdm1
```

Useful GDB commands:
- `bt` - Print backtrace
- `info locals` - Show local variables
- `p variable` - Print variable value
- `c` - Continue execution

## Performance Issues

<details>
<summary>Low FPS / stuttering</summary>

**Solutions:**
1. Check if running debug build (use Release for performance testing)
2. Lower graphics settings:
   ```
   r_mode -1
   r_customwidth 1280
   r_customheight 720
   ```
3. Disable features:
   ```
   r_dynamiclight 0
   cg_shadows 0
   ```
</details>

<details>
<summary>Server lagging with many bots</summary>

**Solutions:**
1. Reduce bot count
2. Use simpler maps
3. Enable network optimization:
   ```
   set sv_netoptimize 2
   ```
</details>

## Getting More Help

If your issue isn't covered here:

1. **Search existing issues:** https://github.com/openmoh/openmohaa/issues
4. **Open a new issue** with:
   - Full error message
   - Steps to reproduce
   - `qconsole.log` contents
   - Output of `version` command
