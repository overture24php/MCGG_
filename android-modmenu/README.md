# MCGG Mod Menu

Mod menu for Mobile Chess GP (MCGG) - Unity IL2CPP game.

## Features

- Auto Win
- Auto Stack Heroes
- Scav Free Buy
- Bypass Anti-Cheat
- God Mode
- Infinite Coins
- Unlock All Heroes
- Speed Hack

## Build Instructions

### Local Build

1. Install Android NDK
2. Install CMake 3.18+
3. Place ImGui source files in `include/imgui/`
4. Build:

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      ..
make
```

### GitHub Actions

See `.github/workflows/build.yml` for CI/CD configuration.

## Installation

### Method 1: JsHook (Recommended)

1. Install JsHook Magisk module
2. Copy `libmcgg_mod_menu.so` to `/data/adb/jshook/packages/com.mobilechess.gp/nativelib_mcggmod.so`
3. Restart game

### Method 2: Direct Injection

```bash
su
cp libmcgg_mod_menu.so /data/local/tmp/MEOW.so
./ptr_inject_ToolVIP.sh $(pidof com.mobilechess.gp)
```

## Project Structure

```
MCGG-ModMenu/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── main.cpp            # JNI entry point & ImGui setup
│   ├── mod_menu.cpp         # Menu rendering
│   ├── mod_menu.h           # Menu definitions
│   ├── mcgg_mod.cpp         # Game-specific hooks
│   └── mcgg_mod.h           # Hook declarations
├── include/
│   └── imgui/               # Dear ImGui source
└── .github/workflows/
    └── build.yml            # CI/CD pipeline
```
