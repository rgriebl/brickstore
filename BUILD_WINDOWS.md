# Building BrickStore on Windows

## Prerequisites

### Required Software

1. **Qt 6.4.2 or newer** (Qt 6.10.1 recommended)
   - Install from: https://www.qt.io/download
   - Required components when installing:
     - Qt Quick 3D
     - Qt Multimedia
     - Qt Image Formats
     - Additional Libraries > Ninja
   - Compiler: MSVC 2019 or MSVC 2022 (included with Qt installer)

2. **Visual Studio 2019 or 2022**
   - Install the "Desktop development with C++" workload
   - Make sure to include the MSVC compiler and Windows SDK

3. **CMake** (3.19.0 or newer)
   - Download from: https://cmake.org/download/
   - During installation, choose "Add CMake to system PATH"
   - **Important:** Use CMake 3.x (not 4.x pre-release)

5. **InnoSetup 6** (optional, only needed for creating installers)
   - Download from: https://jrsoftware.org/isdl.php
   - Install to default location
   - Add to PATH: `C:\Program Files (x86)\Inno Setup 6`

## Build Instructions

### Quick Build

Simply run from any Command Prompt:

```batch
build.bat
```

This script automatically:
- ✓ Sets up the Visual Studio environment (no need for Developer Command Prompt)
- ✓ Configures the project with CMake
- ✓ Builds the project
- ✓ Prompts you to deploy Qt dependencies

**Important:** After building, you must deploy Qt dependencies (DLLs) before the executable can run. The build script will ask if you want to deploy automatically, or you can run:

```batch
deploy.bat
```

The executable will be in `bin\BrickStore.exe` and can be run after deployment.

### Clean Build

To completely clean and rebuild:

```batch
clean-all.bat
build.bat
```

### Deploying Qt Dependencies

Before you can run the executable, Qt dependencies (DLLs and plugins) must be copied to the bin directory:

```batch
deploy.bat
```

After deployment, `bin\BrickStore.exe` can be run directly.

**Note:** The build script (`build.bat`) automatically prompts you to deploy after building.

### Creating an Installer

To build and create an InnoSetup installer:

1. First build the project:
   ```batch
   build.bat
   ```

2. Create the installer:
   ```batch
   cmake --build . --target installer
   ```

The installer will be created as `BrickStore-<version>.exe`.

For debug builds, edit `configure.bat` line 39 to use `--debug` instead of `--release`.