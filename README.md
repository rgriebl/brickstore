<img src="https://raw.githubusercontent.com/rgriebl/brickstore/master/assets/brickstore.png" align="right"
     alt="BrickStore Logo" width="192" height="192">

![](https://github.com/rgriebl/brickstore/workflows/QMake%20Build%20Matrix/badge.svg)
![DL badge](https://img.shields.io/github/downloads/rgriebl/brickstore/latest/total?label=Downloads)
![Custom badge](https://img.shields.io/endpoint?url=https%3A%2F%2Fbrickforge.de%2Fbrickstore-data%2Fdatabase-last-update.py)

## Download

Everything is hosted on GitHub (_the_ hosting provider for open-source projects):
* Releases are built automatically and can be download here:<br><br>
  [![GitHub release (latest by date)](https://img.shields.io/github/v/release/rgriebl/brickstore?label=Download%20BrickStore&color=469BC3&style=for-the-badge)](https://github.com/rgriebl/brickstore/releases/latest)

* There is a project Wiki with [installation instructions](https://github.com/rgriebl/brickstore/wiki/Installation-Instructions).

* Bugs and feature request [can also be reported there](https://github.com/rgriebl/brickstore/issues).

* [Development builds](https://github.com/rgriebl/brickstore/actions) are only accessible when
logged into GitHub (this is a GitHub limitation).

## About

BrickStore is a BrickLink offline management tool. It is **multi-platform** (Windows, macOS and
Linux), **multilingual** (currently English, German and French), **fast** and **stable**.

Some things you can do with BrickStore much more efficiently than with any web based interface:

* Browse and search the BrickLink catalog using a live, as-you-type filter. It is using all the
  cores in your machine to be as fast as possible.
* Easily create XML files for Mass-**Upload** and Mass-**Update** either by parting out sets or by
  adding individual parts (or both).
* Download and view any order by order number.
* Download and view your whole store inventory. An easy way to use this for repricing, is to use the
  BrickLink Mass-Upload functionality.
* Price your items based on the latest price guide information.
* Create XML data for the BrickLink inventory upload.
* If you load files which contain items with obsolete item ids you can fix them using the BrickLink
  catalog change-log.
* Calculate differences between two documents: you can easily check, if it is possible to build a
  set given your current inventory.
* Set matching: given a list of items, BrickStore can tell you which sets you could build from them.
* Unlimited undo/redo support.
* Customized printing via JavaScript based page templates.

## Documentation

BrickStore doesn't come with a complete documentation, but everything has tool-tips: just hover
over an element and you'll get a short description.
There's also a [page with tutorials](https://github.com/rgriebl/brickstore/wiki/Tutorials).

## Requirements

BrickStore works on every **Windows** 32-bit and 64-bit operating system (7, 8.1 and 10),
**macOS** (10.12 or newer) and **Linux** (packages are available for Ubuntu 20.04). If you want to
(or need to) compile BrickStore yourself, then the only requirement is, that the Qt library you are
building against has to be at least version 5.12).

## License

BrickStore is copyrighted &copy;2004-2021 by Robert Griebl, licensed under the
[GNU General Public License (GPL) version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).
All data from [www.bricklink.com](https://www.bricklink.com) is owned by BrickLink. Both BrickLink
and LEGO are trademarks of the LEGO group, which does not sponsor, authorize or endorse this
software. All other trademarks recognized.

Only made possible by [Dan Jezek's](https://www.danjezek.com/) support

## 3rd-Party Components

This projects contains 3rd-party components under different licenses:
* src/minizip: see LICENSE.ZLIB
* src/lzma: see LICENSE.LGPLv21
* assets/icons: see COPYING-ICONS and COPYING-FLAGS
