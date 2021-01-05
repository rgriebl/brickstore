# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- First try at *Known Colors* support using the Parts/Codes catalog download. This is not perfect, but a nice start.
- Big backend refactoring: switched from CSV to XML downloads to deal with the ever increasing number of broken non-ASCII descriptions.
- Re-colored the app icon to make it less confusing for people still using BrickStock (even though that orange was mine to begin with and I still like it...)

### Fixed
- Filter responsivness and speed have improved.
- The minimum window size should now also fit smaller screens.
- Various small fixes and updates throughout the UI.

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


[Unreleased]: https://github.com/rgriebl/brickstore/compare/v2021.1.1...HEAD
[2021.1.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.1
[2021.1.0]: https://github.com/rgriebl/brickstore/releases/tag/v2021.1.0
