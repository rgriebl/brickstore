These JavaScript extensions can be used to add custom behavior to
BrickStore.

Only these scripts are bundled in an official BrickStore release:

  * classic-print-script.bs.qml

All others are provided by users to be shared with others. In order to
install them on your system, first download them by clicking on the script
you want to download, then on the script's page, right-click `Raw` on the
top-right of the scripts source code and choose `Save Link as...` (or your
browser's equivalent).
After downloading, you need to copy the script to where BrickStore can pick
it up:

  * On Windows, the directory is `%APPDATA%\BrickStore\extensions` *(copy)*
    In Explorer click the folder icon or the empty space in the address bar. *(paste)*
  * On macOS, the directory is `~/Library/Application Support/BrickStore/extensions` *(copy)*
    In Finder, choose `Go` > `Go to Folder`. *(paste)*
  * On Linux, the directory is `~/.local/share/BrickStore/extensions` *(copy)*
    In Files or Dolphin, press `Ctrl+L`. *(paste)*

BrickStore will now pick up all the scripts it finds in these directories
when starting up. You can also dynamically reload the scripts while
it is running via `Extras` > `Reload user scripts`.

If your script doesn't show up in the `Extras` (or context) menu, then most
likely it has JavaScript syntax errors. Enable the `Error log` via the
`View` > `View info docks` menu.

> Please note that all script files have to have the file extensions `.bs.qml`
