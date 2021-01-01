# Changelog
All notable changes to this project will be documented in this file.


## [2021.1.0] - 2021-01-01
This is nearly a complete rewrite over many years, porting from Qt 3 to Qt 5. BrickLink also
changed a lot over the years, so a few things aren't supported right now, because I need to find
new ways to get access to the required data.

These are a few of the highlights, but I probably forgot a lot of things ;-)

### Added
- LDraw support with 3D parts rendering.
- Lightning fast multi-threaded filtering and sorting.
- "Natural language" like filtering for the main document view (the tool-tip on the input field
  tells the whole story).
- Item and color id changes are now resolved automatically using BrickLink's change log.
- Modernized UI with Hi-Dpi support, still on all of Windows, macOS and Linux.
- Single-app mode: opening an BrickStore document from the Explorer/Finder will now just open a new
  tab in an already running BrickStore process.
- The font and icon sizes can be scaled between 50% and 200%.
- Undo/redo actions are now presented in a list with human readable labels.

### Removed
- Custom UIs in printing scripts do not work anymore, because the JavaScript runtime is totally
  different. Even the normal printing scripts have to be adapted slightly, but in the end, I will
  have to replace the current mechanism once again for future proof Qt 6 based solution next year.
- Per-document column configurations and the "Collector" as well as the "Simple" view mode
  are not (re)implemented yet.



## [1.1.15] - 2008-09-12

- The 'Item appears in' list now also works when selecting multiple items in the document (how a
  list of sets, each containing all of the selected items).
- The LDraw import doesn't crash on broken (recursive) MPDs, as well as files referencing models
  with spaces in the name, anymore.
- Filtering in the document window is now possible using an arbitrary number of criterias at the
  same time.


## [1.1.14] - 2008-03-08
- Bug fix: wrong prices were set, when using 'Set to PriceGuide' on used parts.


## [1.1.13] - 2008-02-05
- Added a complete slovene translation (thanks to Saso - miskox).
- Fixed a GDI crash on XP when opening more than 32 documents.
- Fixed shopping cart import for items with trailing spaces in the descriptions.
- Reworked all document import keyboard shortcuts.


## [1.1.12] - 2008-01-19
- Various bugfixes.
- Improved alternate part and added counterpart support.


## [1.1.11] - 2008-01-16
- Import multiple orders at once.
- Added a visual marker for alternate parts.
- Support for order ids >999999.
- Full buyer/seller address when importing orders (for use in print templates).


## [1.1.10] - 2007-07-16
- Adapted the price guide parser to the new HTML layout of the page.


## [1.1.9] - 2007-02-18
- Win9x: the BL password was not saved correctly.
- WinXP/Vista: fixed a DLL loading problem which prevented the application from starting.
- Fixed loading documents with active difference mode.
- Adapted the Shopping Cart import to the new HTML layout of the page.


## [1.1.6] - 2006-10-25
- The database was not loaded correctly on big-endian machines (Mac/PowerPC).
- Fixed two possible crashes in Offline Mode.
- A lot of small improvements in the GUI and the import filters.


## [1.1.5] - 2006-06-21
- Not really a change per-se, but the 1.1.4 Windows installer package was broken.
- The Price Guide now shows 3 decimal places (and there is a new command to round prices back to 2
  decimal places)


## [1.1.4] - 2006-06-18
- Added the a BrickLink Shopping Cart importer
- Some bug fixes (Printing crashes, Windows 9x database loading)


## [1.1.3] - 2006-06-03
- Complete new Order import interface
- Remarks and Condition are exported to Wanted-Lists now
- Many bug fixes (Mass-Update missing Lot-IDs, column layout save/restore, Mac PPC key problem)

## [1.1.2] - 2006-02-19
This a rewrite of large parts of the program - some of the highlights:
- All modifications are undoable (no limits on the undo steps)
- Database updates are not fetched directly from the BrickLink server anymore - instead I provide a
  pre-processed database on my own server. This has some advantages:
  - Smaller size: The download is only ~1MB
  - Inventories: ALL set inventories are included in this database
  - Appears-In-Sets: The information which part appears in which set is also included
  - Up to date: The database is re-generated daily (at 4am. CET)
- Print templates are now JavaScript programs and can easily be customized (still needs to
  documented)
- A new file format (BSX) which also stores the state of the user interface (column layout, sort
  order, ...)
- Add a 'Find item-number' button in all item selection dialogs for faster searching
- The UI now supports a tabbed or MDI interface (like Firefox).
- Most text-only lists have tooltips showing an image of the part
- Added a check to prevent opening the same document twice
- Ctrl+Mouse-Wheel on the 'Add Item' dialog changes the opacity of the window (WinXP and Mac OS X)
- The 'Insert' key toggles the 'Add Item' dialog's visibility without loosing any selection
- Nearly all fields can be modified when multiple items are selected now
- Added a currently 85% complete dutch translation (thanks to Eric van Horssen - horzel)

## [1.0.125] - 2006-02-19
- Added a complete french translation (thanks to Sylvain Perez - 1001bricks)
- Fixed a few small bugs.

## [1.0.124] - 2005-11-29
- Added Check for Updates: Tells you if a newer version is available and/or if your current version
  has some serious bugs.
- Bug fix: Many settings were not set to the correct default values on a clean install (column
  infos, infobar look, update intervals).
- Windows 2000: Trying to open a file dialog when a document with more than ~1500 lots is shown
  would crash the program (Microsoft bug).

## [1.0.122] - 2005-11-26
- Added a few new features:
  - Divide quantity (the analog of multiply quantity)
  - Set condition for many items at once
  - Direct part number editing
- Some bugfixes (most notably the $0 price guide bug in 1.0.120)

## [1.0.120] - 2005-11-23
- All previous versions compiled against the Qt 3.3.5 library silently create empty files on
  save/export.
MacOS X versions 116 and 119, as well as all Linux versions linked against Qt 3.3.5 are affected.

## [1.0.119] - 2005-11-22
- Many internal changes - considered as BETA to test some new features.
- The LDraw importer has been enhanced to fully support MPDs
- Buyer/Collector View Mode: A new option, that hides all seller specific stuff, like tier prices,
  stockroom options, etc.
- Difference View Mode: Activates four additional columns in the document for price and quantity
  (original value and difference to current for both).
- Subtract Items: You can subtract one document (e.g. a set inventory) from another document
  (e.g. your inventory). If you activate the Difference Mode, you can easily see which parts are
  missing to build this specific set.
- Mass-Update XML support for BrickLink sellers: (only quantity and price for now)
- Fixed the can't download my store inventory bug.

## [1.0.116] - 2005-11-18
- Added support for weight (metric or imperial) and year of release.
- The default document directory is configurable now.
- Added a LDR importer with full submodel support (MPD import is not working yet).
- Similar items can be consolidated manually or automatically when pasting.
- All composite items can be directly parted out from the main list
- Many bugfixes and enhancements.

## [1.0.108] - 2005-08-22
- Memory consumption and startup time were heavily optimized.
- Added support for a BrickLink supplied list of available inventories.

## [1.0.105] - 2005-08-18
- Bug-Fix release: All previous version sometimes crashed if you had multiple files open and quit
  the program. (The crash occured just after everything was saved, so no data was ever lost.)

## [1.0.103] - 2005-08-17
- You can change the item id (just double-click the item-id, -name or -picture).
- Made it easier to enter prices: the '.' and ',' keys are automatically mapped to the current
  decimal point in use.
- Added a new Error Detection feature: the program marks all input fields that would generate an
  Mass-Upload error. (This can of course be switched off anytime.)
- Fixed a boat-load of smaller bugs in the inventory import code - especially the peeron import hack
  (which works amazingly smooth :) )
- The Add Item dialog now also supports direct input of tiered pricing (% or $) and comments (and
  it's still useable on an 800x600 screen!)
- You can navigate the main list like a Excel-table with the keyboard now: edit a field by pressing
  Return/Enter (currently the same as a double-clicking on the selected field).
- Many other improvements, fixes and optimizations.

## [1.0.94] - 2005-08-07
- The memory consumed by images was cut back by 75%.
- HTTP Proxy support was added.
- The print report generator was heavily extended: It is e.g. possible to directly print an invoice
  (with custom layout) in EUR from an downloaded order - see print-templates/invoice-example.xml.
- Remarks for multiple items can now be set with a single command.
- Changed all open/save dialogs to default to the standard Documents folder.
- Support was added for retained, stockroomed and reserved items.
- Changed/Fixed some aspects of the user interface.


[Unreleased]: https://github.com/rgriebl/brickstore/compare/v2021.1.0...HEAD
[2021.1.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.0
