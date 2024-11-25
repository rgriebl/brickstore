---
layout: page
title: Installation Instructions
---

## Windows
Only Windows 10 and 11, and only 64 bit installations are supported.  As
Windows doesn't have a concept of "universal apps", there are two
installers: one for Intel/AMD (**x64**) and one for ARM (**arm64**) CPUs.

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

Please note: the standard version will only install on macOS 11 or newer.
Both 10.14 and 10.15 are still supported by the *legacy* build though, but
this might have problems with some web cams not working correctly.

***

## Linux

Please note that the Debian *Backend* package is a command-line only utility and its only purpose is to generate BrickStore's database. It is not usable for anything else.

### Flatpak
Starting with version 2024.11.1 a [Flatpak package](https://flatpak.org/) is available for download. Getting BrickStore registered on https://flathub.org/ is planned for a future release.
The downloaded file should be installable straight from your file manager, or on the commandline via `flatpak install ~/Downloads/BrickStore-<VERSION>-x86_64.flatpak`

### Ubuntu Snap
BrickStore for Ubuntu is primarily distributed as a Snap via Ubuntu's Snapcraft store: https://snapcraft.io/brickstore
You should be able to install and update it straight from the Ubuntu Software Center.

### AppImage
An AppImage installation is provided, that is completely distribution independent.
You can read more about AppImages here: https://appimage.org/

### Ubuntu 24.04
A traditional DEB package for Ubuntu 24.04 (and its derivatives) is also available. You can eihter install it using your distro's graphical package managager or via the command line: `sudo apt install ./Downloads/Ubuntu-24.04-brickstore_<VERSION>_amd64.deb`

### Arch
Arch has a well maintained package, available directly via AUR: https://aur.archlinux.org/packages/brickstore

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
