---
layout: page
title: Building from Source
---
BrickStore can easily be built from source. Its only dependency is Qt:
 * for the commits until and including the tag `v2022.4.1`, the only supported version is Qt 5.15.2
 * for the commits after the tag `v2022.4.1`, the minimum supported version is Qt 6.2.4, but 6.4.2 is recommended

### Getting Qt
#### Qt SDK
The easiest way to get a Qt SDK, is to use Qt's online installer that will install everything you need: Get it from <https://www.qt.io/download-open-source> (scroll way down to `Download the Qt Online Installer`).

* When selecting the components, make sure to enable `Qt Quick 3D`, `Qt
  Multimedia` and `Qt Image Formats` under `Additional Libraries`, if you want to build a Qt 6
  based version.
* MingW on Windows is untested - please use the compiler from MSVC 2019 or
  2022.  If you still want to use MingW, be my guest and create a pull
  request if you get it running!

#### Dependencies on Windows
When building on Windows, you need two additional pieces of software:
1) InnoSetup 6, installed to the default location: [download here](https://jrsoftware.org/isdl.php)
1) OpenSSL libraries, ONLY needed for Qt 5.15 based builds: install the Win32/Win64 1.1 "Light" variant from SLProWeb [download here](https://slproweb.com/products/Win32OpenSSL.html)

#### Qt from Linux package repos
You can of course also compile against your system's Qt version to get nicer themeing integration. 
The needed packages for Debian and Ubuntu are:

```
  sudo apt install build-essential cmake ninja-build \
      libglvnd-dev libtbb-dev \
      qt6-tools-dev qt6-tools-dev-tools \
      qt6-l10n-tools qt6-documentation-tools \
      qt6-base-dev qt6-base-private-dev qt6-base-dev-tools \
      qt6-declarative-dev qt6-declarative-private-dev \
      qt6-quick3d-dev qt6-quick3d-dev-tools libqt6shadertools6-dev \
      qt6-gtk-platformtheme qt6-qpa-plugins
```

If something is missing, then have a look at the [GitHub build action](https://github.com/rgriebl/brickstore/blob/main/.github/workflows/build_cmake.yml) which always has the latest dependencies (look for `BRICKSTORE_QT6_MODULES` right at the top).

### Building
Either use Qt's Creator IDE (you got this automatically when installing an SDK) or stick to the command line:
* In Creator, just open `CMakeLists.txt`, assign a Qt version to the project, then first hit the *Build* (`Ctrl-B`) and then afterwards the *Run* (`Ctrl-R`) button.  
* On the command line, `cd` to the BrickStore sources, then run `./configure && cmake --build .`. If you want to build against a specific Qt version, you can set the path to that Qt installation's `qmake` executable via `./configure --qmake /path/to/qmake`.
After building, for Linux and Windows the binary is available in the `bin` sub-directory. Android, iOS and macOS bundles are directly created in the build directory.
