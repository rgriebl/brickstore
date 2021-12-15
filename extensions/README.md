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


## Porting from old BrickStore/BrickStock scripts

The old BrickStore scripts were based on a JavaScript 4 interpreter, but this JS version was 
abandoned by the industry (see [Wikipedia](https://en.wikipedia.org/wiki/ECMAScript#4th_Edition_(abandoned)))

The new scripts are running in a state-of-art JavaScript 7 environment as provided by web browsers
(see [QML JavaScript environment](https://doc.qt.io/qt-5/qtqml-javascript-hostenvironment.html))

Sadly JS4 did a few things differently than today's JS7, so you have to adapt your scripts: some
of these required changes are just simple search-and-replace jobs, while others require you to
rewrite your code a bit.

### The simple ones:
 * Replace `var` with `let` for variable initialization
 * Replace `CReportPage` with `Page`
 * Replace `Utility.localDateString(d)` with `Qt.formatDate(d)`

### Intermediate:
 * Replace the `load()` function with a new file header. Just copy the one from
   [classic-print-script.bs.qml](https://github.com/rgriebl/brickstore/blob/main/extensions/classic-print-script.bs.qml)
   (from file start up to `function printJob`) and change the `name`, `author` and `version` fields
   in the `Script` object.
   You can also set the text of the menu entry via the `text` property of `PrintingScriptAction`.

 * Don't forget to add a `}` add the end of the file afterwards to balance the scope: the old
   scripts only had global JS functions, but all functions are members of the `Script` object
   nowadays.

 * Rename the existing `function printDoc()` function to `function printJob(job, doc, lots)`.
   See below on how to use the `job`, `doc` and `lots` parameters.

 * Replace `Document` with `doc`.
   `Document` was a global object, but `doc` is now just a local function parameter to `printJob()`,
   so make sure to forward `doc` to sub-routines as needed.

 * Replace `Job` with `job`.
   `Job` was a global object, but `job` is now just a local function parameter to `printJob()`, so
   make sure to forward `job` to sub-routines as needed.

 * A lot (or row in the document) was wrongly called *item* in the old API, but this was just too
   confusing when full catalog access to the JS API, so it had to be renamed to *lot*.
   So, in order to iterate over all lots in a document, you now have to access `doc.lots` instead
   of `Document.items`.

 * If you want to support printing just the selected lots, do not iterate over `doc.lots`, but use
   the `lots` parameter given to the `printJob` function.

 * Replace `Money.toLocalString(m, showCurrencyCode)` with `BrickStore.toCurrencyString(m, currencyCode)`.
   The document's currencyCode is not set implicitly anymore, but needs to be retrieved via 
   `doc.currencyCode` in the main `printJob()` function. Keep in mind to forward either `job` or 
   the currency code to sub-routines if needed there.


### Complex:
Font and Color objects changed a lot! The new JS engine is using the built-in Qt types for fonts and
colors and these can be a bit weird, when compared to the old objects:
 * Both cannot be created via the `new` operator anymore
   * For fonts, you have to use the factory function `Qt.font()`:
     see the [Qt documentation for Qt.font](https://doc.qt.io/qt-5/qml-qtqml-qt.html#font-method)
     and the [QML font documentation](https://doc.qt.io/qt-5/qml-font.html) for the available
     font properties.
   * For colors, see the [QML color documentation](https://doc.qt.io/qt-5/qml-color.html)

 * accessing the `page.font` and `page.color` properties now returns a reference to the current
   value, not a copy. This means that you cannot save the current value at the start of the function
   and reset the property to that value again when the function ends, because the "saved" value
   changes everytime you change the actual property.
   If you relied on this behavior (the default print script did), you have to change your approach
   here to always set the colors and fonts before drawing in a sub-routine.

Have a look at the [classic-print-script.bs.qml](https://github.com/rgriebl/brickstore/blob/main/extensions/classic-print-script.bs.qml)
to get an idea how to work with fonts and colors in the new JS engine.
