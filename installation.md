---
layout: page
title: Installation Instructions
---

## Windows
Only Windows 10 and 11, and only 64 bit installations are supported.  As
Windows doesn't have a concept of "universal apps", there are two
installers: one for Intel (**x64**) and one for ARM (**arm64**).

Start the installer, then when Windows tells you that it "protected your
PC", click `More info` to reveal the `Run anyway` button.  Click that button
and from there on out, it's a standard installation process.

If you get an error message saying `Error 5: Access is denied`, then please
start the installer as Administrator (in Explorer, right-click the file and
select `Run as administrator`).  This shouldn't happen, but then again we
are talking about Windows...

***

## macOS
The application is a universal one, so you will automatically use the native
ARM version when running on an M1 or M2 powered machine and the Intel
version otherwise.

Open the DMG image, drag the BrickStore icon to the `Applications` folder
and eject the BrickStore image again, as you would on any other software
installation.  Then start BrickStore from your computer's `Applications`
folder.

> **Only for versions from 2021 and 2022**: These are not digitally signed,
> so when you start BrickStore from your computer's `Applications` folder,
> macOS will tell you it "can't be opened", but gives you no way to remove
> this block directly.  You have to go to your `System Preferences` (Apple
> menu), then to `Security & Privacy`, `General` and there you will find an
> `Open Anyway` button to finally unblock BrickStore.

***

## Linux

Please note that the Debian *Backend* package is a command-line only utility and its only purpose is to generate BrickStore's database. It is not usable for anything else.

### Ubuntu 22.04
If double-clicking the downloaded package brings up the archive manager instead of the software center, you need to fix Ubuntu first: https://itsfoss.com/cant-install-deb-file-ubuntu/

Of course you can always install via the command line as well: `sudo apt install ./Downloads/Ubuntu-22.04-brickstore_<VERSION>_amd64.deb`

### Mint 21
You can install the Ubuntu 22.04 package, but the graphical installer currently has a bug and is not able to parse the dependencies correctly: *Error: Dependency is not satisfiable: qt6-base-abi*.
If that's the case, the simplest solution is to open a terminal and install from there: `sudo apt install ./Downloads/Ubuntu-22.04-brickstore_<VERSION>_amd64.deb`

### Arch
Arch has a well maintained package, available directly via AUR: https://aur.archlinux.org/packages/brickstore

### Generic AppImage
An AppImage installation is provided, that is completely distribution independent.
You can read more about AppImages here: https://appimage.org/

### Compile from sources
If you cannot use one of the pre-compiled packages, you can however easily build the software yourself using the classic Unix command: `./configure && cmake --build .`.
For more information about this, [see the corresponding Wiki page](https://github.com/rgriebl/brickstore/wiki/Building-from-Source).

***

## Android
Currently in testing: https://play.google.com/store/apps/details?id=de.brickforge.brickstore

ARMv7 and ARM64 packages are also available for side-loading.

***

## iOS
Currently in public Testflight testing: https://testflight.apple.com/join/qIu2smfl

The IPA package cannot be installed directly, unless you have a jail-broken device.
