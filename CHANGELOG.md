# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

## [2021.4.1] - 2021-04-21
### Added
- All item image views now have `Copy` and `Save` action in their context menus.
- Documents can be sorted on multiple columns now: clicking the column header sets and toggles the
  primary sort key, while shift-clicking sets and toggles the additional sort keys. You can sort by
  as many additional columns as you like.

### Please Note
BrickLink's servers are very unstable right now (April 2021). This version of BrickStore tries to
work around some of the problems, but the price guide download interface is still broken on
BrickLink's side and may lead to invalid price guide data in BrickStore.

See https://github.com/rgriebl/brickstore/issues/335 and https://www.bricklink.com/message.asp?ID=1266428
for more information.

The problem has been reported to BrickLink, but no feedback was received yet.

## [2021.3.3] - 2021-03-25
### Added
- Printing a document will now print exactly what you see on the screen, using a preview window.
  The old print script (as well as any customized print scripts) are still available via the 
  `Extras` menu.
- Multiple orders with differing currencies can now be combined on import.

### Improvements
- The marker column is now saved and restored.
- In addition to the in-place duplication, there is now also a `Paste silent` functionality which
  works just like `Paste`, but never asks about overwriting or consolidating - it just appends to
  the document.


## [2021.3.2] - 2021-03-22
### Added
- Experimental support for a marker column that can be used for the multi-order picking: multiple
  orders can be imported into the same document now. Markers are not saved at the moment.
- All main window keyboard shortcuts are now configurable in the Settings dialog.

### Improvements
- When loading files with outdated item or color references, BrickStore will now mark the file as
  modified after updating them.
- BrickLink Studio models should now import without any missing parts or mismatched colors.
- Added a quick way to duplicate selected lots: `Edit` > `Duplicate` or `Ctrl+D`.
- The Windows x64 release will now automatically send anonymized crash reports to
  https://sentry.io This might be extended to the other platforms if it proves to be useful.


## [2021.3.1] - 2021-03-05
### Added
- *Import LDraw Model* can now import BrickLink Studio models as well.
- Editing a field while multiple rows are selected will change the value in all selected rows.
  You can also navigate within a selection with `Ctrl+cursor keys` (`Cmd` on macOS), so you don't
  have to use your mouse at all.
- *Copy remarks from document* got extended to *Copy fields from document* and can now copy and
  merge any field in multiple ways from one document to another (e.g. merging in prices from another
  document using weighted averages).
- The context menu is really context sensitive now and will only show specific field commands for
  the clicked column.
- BrickStore will now restore all the saved documents in your last session on startup (this can be
  disabled in the settings dialog).
- The separate *Difference Mode* is gone. Instead it is always active and works on all fields. Just
  like with errors, you can choose to show or hide the difference markers via the `View` menu. This
  makes it possible to *Mass-Update* all possible fields in your BrickLink store from within
  BrickStore.
- The status bar moved to the top of the document and got more interactive: clicking on `Errors` or 
  `Differences` will jump to the next one (also available on the keyboard: `F5` or `F6`).
- Experimental support for distribution independent Linux *AppImage* builds.

### Improvements
- Sorting and filtering are now independent operations again: the filter edit has a specific 
  re-apply filter button now and clicking on the last sorted column header will re-sort the list.
- All import dialogs are non-modal now to allow multiple imports easily.
- High-DPI support has been enhanced.
- Multi-threaded price guide loading to (partially) compensate for the slow I/O on Windows.
- Faster document loading and saving.
- Undo/redo actions are now named exactly after the action that triggered them.
- The update check can now download and start the new installer for you.
- All numbers should now show with localized thousand's separators.
- Set-to-price-guide will now block the current document until it is finished. This is to prevent
  odd behavior if you played around with undo/redo too much while the download was happening in the
  background, plus you can undo the whole operation in a single click on *Undo* now.
- Clicking on the logo/progress circle in the top, right corner gives you the option to cancel all
  current downloads (pictures and price guides).
- Updating the BrickLink order list is a lot faster now.
- There is now an option to set a default for part-out operations within documents.
- All open and save file dialogs remember the last used directory during a session.
- Renovated the price increase/decrease dialog.
- The modify item and color dialogs remember their last state and geometry. You can reset their
  size and position to be centered on the currently active item by double-clicking the title bar.


## [2021.2.2] - 2021-02-17
This is a quick-fix release for 2021.2.1: it adds the missing German translation and fixes a bug
where the language would sometimes revert to English.


## [2021.2.1] - 2021-02-17
### Improvements
- Complete overhaul of BrickLink cart and order importing.
- Sorting and filtering are undoable operations now just like in Excel. Also both are really 
  one-time actions now: adding or removing items will not dynamically re-apply the sorting and
  filtering. There's reapply button to do it manually when you want to have the list resorted.

### Added
- BrickLink Mass **Update** works on all supported fields now. The document you want to export has
  to be in **Difference Mode**, but this is activated automatically when importing your store
  inventory.
- Clicking the **Error** label (or pressing `F6`) will jump to the next field that has erroneous 
  input.
- The filter in the add-item and import-set dialogs gained even more functionality: have a look at
  the tool-tip label to learn about it.
- Right-clicking on Minifigs gives you the option to quickly and easily filter for similar body
  parts, head gear, etc.
- Added support for finding parts using part-color-codes (element numbers) in the add-item dialog.

### Fixed
A lot.


## [2021.2.0] - 2021-02-07
### Improvements
- Operations on a lot of rows at once have been sped up by several orders of magnitude.
- Picture downloads are now done completely in a background thread, keeping the UI more responsive.

### Added
- New, modern icons bases on KDE's breeze theme. These icons can now adapt to dark desktop themes
  on macOS and Linux.
- The filter in the add-item and import-set dialogs gained the ability to handle **exclude** words:
  e.g. `brick 1 x 2 -pattern` or `slope -33 -45`.
- You can save your favorite filters now by pressing `Return` in an filter edit field. Recall them
  by pressing `Cursor Down` or by clicking the filter icon on the left. The `X` in pop-up list
  lets you delete unwanted filters.

### Fixed
A lot.

Please read the [installation instructions](https://github.com/rgriebl/brickstore/wiki/Installation-Instructions).


## [2021.1.7] - 2021-01-27
The **Defaults** page in the settings dialog was completely removed. Instead, the affected dialogs
will now remember their last settings, even when exiting and restarting BrickStore.

### Added
- Basic support for BrickLink's *My Cost* field was added.
- Auto-saving is finally activated: BrickStore saves any changes you make to temporary files every
  minute. If it crashes, it will ask on restart if you want to restore any unsaved changes.
- Minifigs have a weight and a year-of-release now.
- Adding or importing items is faster now by simply double-clicking the item (or color).
- Satin colors got their own color category.
- New shortcuts for consolidating items and quickly jumping to BrickLink web-pages.

### Fixed
- Price guide data includes VAT now, so it is a lot closer to what you would see on BrickLink's
  web pages. It's still not perfect, but if you are interested in the details why this is so hard: 
  #80
- Buyer/Collector mode was not restored on restarts.
- Reimplemented item consolidation, which was actually semi-broken for the last 15 years.
- All column layouts should be restored correctly now.
- Sub-conditions on sets can now be set in the add-item window and will show up correctly in the 
  document's item list.
- Filtering for an empty field is possible by specifying it as `""` (two quotes).


## [2021.1.6] - 2021-01-22
Fixed a crash when closing the settings dialog.


## [2021.1.5] - 2021-01-21
A lot of bug-fixing all over the place, too much to mention all in here. 

### Added
- Switched from using the classic 80x60 small images to BrickLink's newer *normal* format. This
  should make all item images nice to look at, even on Hi-Dpi screens.
- This change made it possible to have zoom controls in all catalog item views (e.g. the add-item
  and import-inventory dialogs). Use the `+`/`-` buttons, `Control` + scroll wheel or a touchpad
  gesture to zoom seamlessly between 50% and 500%.
- The add-item dialog is now more customizable with splitters between elements and these
  customizations are saved and restored.
- The LDraw detection has been improved, but even if does not work, you can now also specify your
  LDraw directory in the settings.
- The database update is now honoring the expiration time from the settings dialog and will update
  automatically on startup, if the database is past its expiry date.
- BrickStore will also check for newer versions on startup automatically.


## [2021.1.4] - 2021-01-15
Performance improvements across the board, especially in the table rendering.

### Added
- First implementation of a generic JavaScript extension interface, not public yet.
  As a side effect, the old JavaScript printing was replaced with the new one. Any old printing
  scripts for BrickStore 1.x are not compatible anymore, but porting will be simple once the new
  API is stable and documented.
- The new column layout load/save mechanism got its missing management dialog.

### Fixed
- BrickStore can now be properly restarted by its installer on Windows.
- Problems with "TLS initialization" on Windows should hopefully be fixed.
- Fixed a crash that occured when setting prices to price guide in two documents in parallel.
- Difference mode is now a per-document setting and survives saving and re-loading the document.
- Order imports now cope with changes to the BrickLink interface. The `Any` order type had to be
  removed for the time being though.
- A lot of small fixes all over the place.


## [2021.1.3] - 2021-01-08
### Added
- The Buyer/Collector mode is working again.
- Column layouts are now saved to documents again.
- Column layouts can also be named, so you can easily apply them to other documents.
- Right-clicking on a cell now gives you the option to create a filter from this cell's contents.

### Fixed
- All of the reported rounding and decimal places related errors should be fixed now.
- Better selection visibility in the image-only item browser view.


## [2021.1.2] - 2021-01-06
### Added
- First try at *Known Colors* support using the Parts/Codes catalog download. This is not perfect, but a nice start.
- Big backend refactoring: switched from CSV to XML downloads to deal with the ever increasing number of broken non-ASCII descriptions.
- Re-colored the app icon to make it less confusing for people still using BrickStock (even though that orange was mine to begin with and I still like it...)

### Fixed
- Filter responsivness and speed have improved.
- The minimum window size should now also fit smaller screens.
- Various small fixes and updates throughout the UI.
- Fixed macOS dark mode glitches.

## [2021.1.1] - 2021-01-03
This deals with the most annoying bugs in the first release.
### Fixed
- Images in tool-tips should no longer overlap the text.
- Pressing 'Return' anywhere in the add item dialog now adds the current item.
- The Unix ./configure build script was broken.
- The Windows installer now comes with an English translation.
- Order imports by date range should work again.
- Implemented checking for new releases, now that there are releases available.
- Fixed the weird sorting behavior when adding new items to a sorted document.


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


[Unreleased]: https://github.com/rgriebl/brickstore/compare/v2021.4.1...HEAD
[2021.4.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.4.1
[2021.3.3]: https://github.com/rgriebl/brickstore/releases/tag/v2021.3.3
[2021.3.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.3.2
[2021.3.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.3.1
[2021.2.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.2.2
[2021.2.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.2.1
[2021.2.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.2.0
[2021.1.7]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.7
[2021.1.6]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.6
[2021.1.5]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.5
[2021.1.4]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.4
[2021.1.3]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.3
[2021.1.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.2
[2021.1.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.1
[2021.1.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.0
