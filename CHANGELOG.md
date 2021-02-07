# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

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


[Unreleased]: https://github.com/rgriebl/brickstore/compare/v2021.2.0...HEAD
[2021.2.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.2.0
[2021.1.7]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.7
[2021.1.6]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.6
[2021.1.5]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.5
[2021.1.4]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.4
[2021.1.3]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.3
[2021.1.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.2
[2021.1.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.1
[2021.1.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.0
