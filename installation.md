---
layout: page
title: Installation Instructions
---

## Windows
Only Windows 10 and 11, and only 64 bit installations are supported.

There are 2 ways to install BrickStore:

1. Via the Microsoft Store: just search for BrickStore and click install.
This installation will then be updated automatically by Windows.

2. Via the Installer you download yourself: choose the one simply named
**Windows** regardless of your current platform (the x64 or ARM64 ones are
legacy).

Start the installer, then when Windows tells you that it "protected your
PC", click `More info` to reveal the `Run anyway` button.  Click that button
and from there on out, it's a standard installation process.

If there is no `Run anyway` button available, then your Windows installation
is configured to not allow unsigned installations to run. Your only option
is then to install from the Microsoft Store.

If you get an error message saying `Error 5: Access is denied`, then please
start the installer as Administrator (in Explorer, right-click the file and
select `Run as administrator`).  This shouldn't happen, but then again we
are talking about Windows...

***

## macOS
Open the DMG image, drag the BrickStore icon to the `Applications` folder
and eject the BrickStore image again, as you would on any other software
installation. Then start BrickStore from your computer's `Applications`
folder.

**Please note:** the standard version will only install on macOS 13 or newer.
10.14, 10.15, 11 and 12 are still supported by the *legacy* build though, but
this might have problems with some web cams not working correctly.

***

## Linux

Please note that the Debian *Backend* package is a command-line only utility and its only purpose is to generate BrickStore's database. It is not usable for anything else.

### Flatpak
Flatpaks for both Intel and ARM machines are available for download. Getting BrickStore registered on https://flathub.org/ is planned for a future release.
The downloaded file should be installable straight from your file manager, or on the commandline via `flatpak install ~/Downloads/BrickStore-<VERSION>-x86_64.flatpak`

### Ubuntu Snap
BrickStore for Ubuntu is primarily distributed as a Snap via Ubuntu's Snapcraft store: https://snapcraft.io/brickstore
for both Intel and ARM processors. You should be able to install and update it straight from the Ubuntu Software Center.

### AppImage
An AppImage installation is provided for Intel processors, that is completely distribution independent.
You can read more about AppImages here: https://appimage.org/

### Ubuntu 24.04 and 26.04
Traditional DEB packages for Ubuntu 24.04 and 26.04 (and their derivatives) are also available. You can eihter install these using your distro's graphical package managager or via the command line: `sudo apt install ./Downloads/Ubuntu-24.04-brickstore_<VERSION>_amd64.deb`

### Arch
Arch has a well maintained package, available directly via AUR: https://aur.archlinux.org/packages/brickstore

### Compile from sources
If you cannot use one of the pre-compiled packages, you can however easily build the software yourself using the classic Unix command: `./configure && cmake --build .`.
For more information about this, [see the corresponding Wiki page](https://github.com/rgriebl/brickstore/wiki/Building-from-Source).

***

## Android
Can be installed directly from the Gogle Play Store: https://play.google.com/store/apps/details?id=de.brickforge.brickstore

ARMv7 and ARM64 packages are also available for side-loading on GitHub.

***

## iOS
BrickStore is now available directly via Apple's App Store: https://apps.apple.com/de/app/brickstore/id1626933201

Test builds between releases are available via Apple's Testflight program: https://testflight.apple.com/join/qIu2smfl


