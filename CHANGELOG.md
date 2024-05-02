# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]


## [2024.5.2] - 2024-05-02

Bugfix release for a few annoying problems with 5.1:
- BrickLink store and order imports work again now.
- Camera activation handling is fixed.
- Android: opening documents from other applications did not work consistently.


## [2024.5.1] - 2024-05-01

This is a quite big release for the mobile iOS and Android version. There are too many fixes and new features to mention them all, but here are the most important ones:
- Adding items is now at least possible via a Brickognize item scan. (I'm still undecided on how to port the hugely complex add-item dialog to the mobile UI)
- Recent files can now be pinned and cleared (long press the entry) and they should survive app updates.
- You can now open BSX files directly from the system's "Files" application.

Other noteworthy changes:
- Item sorting had to be fixed again. It's also twice as fast now.
- The BrickLink login session is now extended as long as BrickStore is running, which should cut down on the number of "New Login" mails.
- Windows 10 versions older than 1809 are not compatible anymore, because they do not support modern, secure HTTPS connections.
- BrickStore's database is now re-generated and served via Microsoft's GitHub cloud infrastructure. This removes the dependency on the health of my personal servers, while also ensuring a consistent download speed worldwide.


## [2024.4.1] - 2024-04-05

**Important**: Price guide updates in all prior 2024.1/.2/.3 versions are broken as of April 4th.

> These BrickStore versions do not send the required API key to the BrickLink servers in all cases and BrickLink started blocking these requests on April 4th (without giving me a heads-up, of course).

Other noteworthy fixes:
- Sorting was sometimes a little off, when it came to comparing strings ending in numbers (e.g. *Plate 1 x 4* vs. *Plate 1 x 3*).
- The mobile UI got another huge update, especially the item info dialog.


## [2024.3.1] - 2024-03-26

Important for macOS users: this release works around two bugs in Apple's build tools:
- The standard macOS version will now work again on macOS 11.
- The crash on Wanted List download does not occur anymore.

Other changes:
- Support for alternate BrickLink item ids was added to the info displays, the item tooltips and the Add Item dialog filter. Also a new column showing these ids is available for the main document views: it's not visible by default, but it can be shown via the column headers' right-click menu.
- BrickStore will now try harder to resolve unknown items when loading old documents:
  - Items changing type from or to `Set` are not named correctly in BrickLink's change-log. BrickStore is trying to work around that now.
  - If the change-log doesn't yield a result, BrickStore now tries to find an item with the same description.
  - If that still fails and the item id has upper-case characters, a case-insensitive search for the item-id is performed.


## [2024.2.1] - 2024-02-14

This is an important bug-fix release, if you are using **Brickognize.com**: older versions of BrickStore can flood their servers with pointless, duplicate requests.

Other noteworthy fixes:
- Camera access in Linux AppImages and Snaps is restored.
- A few long-standing, but infrequent crashes on database updates and shutdown have finally been fixed.
- Importing LDraw or Studio models with unknown item ids would merge lots, regardless of color.


## [2024.1.4] - 2024-01-21

Fix for Webcams only showing black on some machines.


## [2024.1.3] - 2024-01-20

A few bug fixes, especially for macOS users with accessibility features enabled on their system.


## [2024.1.2] - 2024-01-11

- The Webcam view for Brickognize has been rewritten and should now be a lot faster and more stable on Windows and macOS. Plus the window can now also be "pinned" to keep it open.
- In addition to the main toolbar, all other icons in the UI can now be freely resized in the same way as the font size.
- LEGO's PCCs (part-color-codes) are now displayed in the item info views and they can also be used directly in the add-item dialog's filter field.
- Not having selected a valid color is now shown with a chrome checkerboard pattern on 3D models (before, the part was confusingly shown as light gray).
- Speckle and Glitter colors on 3D models are now looking a lot more natural.
- The 3D render settings are now directly accessible from the main menu: the two most important settings are *Anti-Aliasing*, which users with decent GPUs should always increase to "Very high" for nicer looks and *Lines* / *Line width* where you can adjust how realistic or "cartoony" you want the rendering to look.

**Please note**: 2023.11.2 introduced a bug where the cache and app-data directories got an extra sub-directory "BrickStore" appended. This change was reverted now. You are only affected by this bug, if you are using custom **extensions**: if that is the case, then please move your extension files up one level again, where they were before 2023.11.2.


## [2023.11.2] - 2023-11-14

Password related improvements:
- No more "Login from a new device" emails from BrickLink every time you restart BrickStore.
  This will not get rid of the these emails completely though, but at least reduce them to around 1 per day, until someone figures out how to extend the login session.
- Your BrickLink password is now saved in the encrypted password store of the operating system:
  - Windows: Credential Manager
  - macOS and iOS: Keychain
  - Linux: GNOME Keyring or KDE Wallet (via libsecret, if available)

  These password stores can give out the password to other (malicious) applications running in the
  same user session (at least macOS and Linux can be configured to notify you about that), so the
  password is also heavily obfuscated to prevent accidental leaks.


## [2023.11.1] - 2023-11-06

Fixes:
- Improved handling of missing price-guide data: you can now choose to set to zero (old behavior), don't change the existing price or even add a red marker to the lots in question.
- New, unencrypted BrickLink Studio files can now be opened the same way as older, encrypted ones.
- Better handling of all the BrickLink XML parsing and generation bugs that will probably never be fixed:
  - Orders with an '&' character in any of the fields should import correctly now.
  - Store inventory downloads, uploads and updates are now able to handle HTML tags in the comments and remarks fields.
- macOS: added a *legacy* build for older macOS versions. (10.14 and 10.15)
- Linux: online/offline detection was sometimes not working correctly.


## [2023.8.1] - 2023-08-12

This is an overall bug-fix release.
Most importantly for macOS though: 2023.5.1 would crash a lot if "Accessibility" options (e.g. voice-over) were enabled in the system preferences.


## [2023.5.1] - 2023-05-29

No new features, but a bit of bug fixing: most importantly, all webcams should now work on macOS.

This release is now also distributed as a "Snap" for Ubuntu Linux.


## [2023.4.4] - 2023-04-18

Fixes:
- Using a webcam to identify parts could crash on macOS, depending on the webcam driver.
- Opening a document in a new window could lead to the app locking up on Windows machines.
- The Android version had a packaging issue, and was missing the SSL libraries required for HTTPS downloads.

Also, the mobile iOS and Android ports are now using the new Material-3 design consistently.


## [2023.4.3] - 2023-04-12

Another bug fix release:
- Fixed a crash introduced by the database optimizations that can occur at any time during normal usage.
- Fixed erratic popup placement on secondary screens on macOS.

There's also one new feature:
- Tier prices can now be set relative to a lot's base price.


## [2023.4.2] - 2023-04-10

Quick bug fix release:
- The `Add` button in the add-item dialog stayed disabled for any item without a color (e.g. Minifigs).


## [2023.4.1] - 2023-04-09

BrickStore has a new website at https://www.brickstore.dev, which is also maintained as part of the GitHub  repository. This should make the somewhat hidden content in the GitHub Wiki pages a lot more visible.

New features:
- [Brickognize](https://brickcognize.com) is now directly integrated into the add-item dialog: just hook up a webcam (or connect to your phone's camera) and click the new "camera" icon next to the filter.
  - In order to make Brickcognize's "detect anything" mode more useful, the item type selector now also supports an "Any" type.
- Colors, categories and recent files can now be _pinned_: once pinned, these items will always stay at the top of their respective lists.
  - List items can be pinned via the pin icon on the right-hand side, while the recent files on the home page can be pinned via a context menu.

Fixes:
- The column sizes on the "consolidate individually" dialog page were not mirrored correctly from the current document.

Optimizations:
- The BrickLink catalog database loads at least twice as fast at startup and uses 50% less memory at runtime.
- Initializing the "appears-in" view for popular items (e.g. Plate 1 x 1) could take a few seconds.


## [2023.3.2] - 2023-03-22

This is just a quick bug fix release:
- Importing orders is completely broken in 2023.3.1
- Using the color-lock feature in the add-item dialog could mess up both the history navigation and the go-to-item functionality of the relationship view.
- Double-clicking an item relationship will now jump to that item, instead of trying to part it out. This makes browsing the catalog in the add-item dialog much more convenient with all the new item relationships. The part-out functionality is still available via the context menu.
- In addition to browser back and forward keys on a keyboard, mice with forward and back buttons are now also supported for history navigation in the add-item dialog.


## [2023.3.1] - 2023-03-21

This release comes with a lot of bug fixes, the most important ones being:
- Loading documents containing items that had their ids renamed more than once during their lifetime could potentially send BrickStore into an infinite loop, trying to resolve the id changes.
- The order import mechanism was implemented on the assumption that the order date never changes. It does however update with each new batch addition, leading to duplicate order entries.

As for new features:
- The BrickStore database does now incorporate the relationship data (e.g. _Similar Parts with Different Molds_) from BrickLink. This data is available in the UI in the same way as the _appears in_ and _consists of_ data.
- The _appears in_ and _consists of_ lists will default to "any color", if no color is selected in the add-item dialog.
- In case BrickStore doesn't startup correctly anymore, you can now hold down `SHIFT` while launching and BrickStore will start in a clean state by re-downloading the latests database and not restoring any documents.


## [2023.2.1] - 2023-02-24

The big new feature of this release is the improved price-guide, which now uses BrickLink's revamped AffilateV1 API:
- Price-guide data is now downloaded in batches of 500 instead of one-by-one.
- You can choose to have those prices include VAT or not - even as viewed from a specific territory.
- As this method needs a private API key, BrickStore will revert to the old mechanism if you compile the app yourself.

While at it, the local caching mechanism for price-guide data and pictures was changed completely:
- All pictures are now converted to WEBP (lossy/80%) after downloading, which compresses to around 10-20% of the original PNG size without obvious visual artifacts.
- Both the price-guide data and pictures are not written to individual files in the cache folder anymore. Instead two Sqlite databases will be used. This results in faster lookups (especially on mobile platforms) and less space wasted in the file-system.

Other new features:
- In addition to "appears-in" and "consists-of", the inventory view now also gained a "can-build" relationship.
- The consolidate items dialog has been reimplemented and uses a similiar workflow as the *Copy values from document* command now.
  - It also allows you to choose what to do with the source lots after the merge: BrickStore can either delete them as it always did, but it can now also be instructed to keep them with their quantity set to `0`. This is especially useful when consolidating your store inventory, because you can Mass-Update those 0-quantity lots afterwards to easily remove them from your store.
- LDraw and BrickLink item ids are not always matching (most common with decorated or composite parts). In order to make the 3D view usable for those items as well, BrickStore's copy of the LDraw library includes an item id mapping now:
  - Item id mappings added as comments in the official LDraw library as well as in Studio's version are taken into account.
  - In addition, manually curated mapping files are applied as well (see [here](https://github.com/rgriebl/brickstore/tree/main/ldraw)).
  - Currently only the *Wheel & Tire Assembly* category has been manually mapped.
  - Anybody is welcome to help with additional mappings!

Other fixes:
- Sorting and filtering large documents on macOS and iOS is now just as fast as on the other platforms.
- The macOS installation is now digitally signed, so there's no need to unblock BrickStore after installing new versions anymore.
- The Linux AppImage is now compatible with modern distros that only ship with OpenSSL version 3.


## [2022.11.2] - 2022-11-28

New features:
- BrickStore now has a full Swedish translation (thank you Johan!)
- Added more part-out options: you can now also part out sets-in-set as well as Minifigs in one go.
- When parting out from within a document, the *status* is now inherited by the new lots.

Quite a few fixes, most importantly:
- Rendering some 3D parts with an excessive amount of lines could crash due to an out-of-memory in the 3D pipeline (just the studs on a 48x48 baseplate consist of more than 300,000 lines).
- Fixed some glitches in the Add Item dialog's new browse history.
- The `Reload User Scripts` command would sometimes not reload scripts.
- Column widths in all the import dialogs are now saved and restored.

Changes to the scripting interface:
- The current JavaScript engine handles scoped enumerations correctly now, but both the documentation and the example scripts have been using the unscoped values, which are just *undefined* nowadays. Please adapt your scripts accordingly (e.g. `BrickLink.Used` needs to be `BrickLink.Condition.Used` now). See also [the documentation here](https://rgriebl.github.io/brickstore/extensions/).


## [2022.11.1] - 2022-11-08

New features:
- loading 3D models is now done in a background thread, making the UI more responsive.
- switching between documents via `Ctrl+Tab` (or `Alt+Tab` on macOS) now shows a list of documents.
- the "appears-in" list views will now also show the "consists-of" relationship of the selected item.
- also in these lists, the context menu was extended to directly go to an item in the Add Items dialog.
- the Add Items dialog gained a browsing history, much like a web browser: this is accessible via the new back, forward and menu buttons in the lower left corner.

Some fixes, the most important being:
- duplicating more than one lot would crash.
- importing an LDraw/Studio file would not correctly import sub-models.
- black-listing very old NVIDIA GPUs for the 3D renderer did not always work.
- the checked/on state of buttons was very hard to see in dark themes.

Also, the Add Items dialog was simplified a bit by removing the text-only item view: this was a relevant optimization 15 years ago, but doesn't add value anymore nowadays.


## [2022.10.2] - 2022-10-14

New features:
- added an optional column for single item weights (the existing *Weight* column was renamed to *Total Weight*).
- changing the row height in document views is now done browser-like: either via `Ctrl/Cmd+MouseWheel`, a zoom gesture or the `View` menu (instead of changing the "Item image size" in the settings dialog).
- the column spacing in document views can be modified, helping with readability.
- in the Add Item dialog, invalid values for (tier) quantities and prices that would prevent you from clicking the `Add` button are now marked in red.

Again, more bug fixes:
- fixed another crash on Windows when restarting BrickStore after running in a maximized window.
- fixed the 3D part view becoming unrepsonsive on macOS.
- fixed the item tooltips sometimes not showing an image on the first try.
- fixed a few inconsistencies with filters on numeric columns.


## [2022.10.1] - 2022-10-05

More bug fixes. This time:
- fixed Sentry crash reports on macOS being next to useless due to a bug in the build setup.
- fixed printing related crashes on Windows and macOS.
- fixed most mouse hover effects not working correctly.
- fixed a few annoyances when navigating documents with the keyboard (Home/End key, sudden jumps).
- fixed cache related crashes when updating the BrickStore database or LDraw library.
- fixed a possible data loss situation when un-splitting a view containing an unsaved document.
- fixed a crash on Windows when restarting BrickStore after running in a maximized window.


## [2022.9.3] - 2022-09-21

Another bug-fix release for 2022.9.1 and 2022.9.2. Most importantly:
- in order to prevent crashes from broken graphics drivers, the new 3D renderer is now disabled on very old GPUs (the list comes from recorded crash reports and will likely be extended in the future).
- reduced the 3D anti-aliasing level to "High" in order to not stress old GPUs too much (you can change that back to "Very high" in the render settings dialog).
- loading documents containing items with invalid or missing color information no longer crashes.


## [2022.9.2] - 2022-09-15

This is strictly a bug-fix release for a few annoying issues found in the 2022.9.1 release:
- upgrades on Windows didn't finish correctly if BrickStore was running.
- the build server produced a broken macOS package that wouldn't start.
- popup-menus and dialogs sometimes got misplaced in multi-monitor setups.


## [2022.9.1] - 2022-09-13

This is a big modernization of both the libraries BrickStore is built upon and its build-system to
keep BrickStore maintainable and also compatible with modern operating systems.

This comes with a lot of advantages, but also disadvantages for a few users.
The disadvantages:
- Windows 7, 8.1 and macOS 10.13 are no longer supported, as they have been discontinued by their respective vendors. Support for these platforms will not come back!
- 32bit Windows (x86) is no longer offered as pre-built package to download. Qt (the library BrickStore is built upon) doesn't offer an installer for this platform anymore, because there are too few users left using it. Getting BrickStore running on a 32bit Windows 10 is technically possible, but you are on your own here, having to build Qt 6.2.4 plus BrickStore yourself.

The advantages:
- Better integration in current desktop systems, like Windows 11 and macOS 12.
- The macOS package is now a Universal Binary which supports both Intel and ARM (M1) natively.
- Added support for Windows' "Tablet" mode as well as automatic detection of dark themes.
- Windows on ARM is now a supported platform, but it needs a separate installer: there is no concept like Apple's "Universal Binaries" on the Windows platform.
- Ubuntu 22.04 LTS is shipping up-to-date Qt libraries again, so there's a native package for that.
- 3D part rendering has once again taken a big step forward to more realistic rendering with better lighting and support for different part materials (e.g. reflections on chrome parts), as well as texturing for glitter, satin and speckle parts.
- In order to support the upcoming macOS 13 as well as Windows on ARM, the 3D renderer now always uses the platform's native render interface (Direct3D on Windows, Metal on macOS/iOS, Vulkan or OpenGL on Linux/Android) directly.

Other notable changes:
- The mobile UIs for Android and iOS have taken a huge step forward, but they still are not feature complete.
- The IPA download for iOS is only installable on jailbroken devices - a version for the official App Store is currently being tested, but needs a bit more work before Apple will accept it.
- The LDraw parts library is now mirrored to brickforge.de, which should really speed up these downloads.

Please read the [installation instructions](https://github.com/rgriebl/brickstore/wiki/Installation-Instructions).


## [2022.4.1] - 2022-04-02
This update is **mandatory** for users of 2022.3.1:

Ever since LDraw released their March update, the affected BrickStore version re-downloads the 60 MB LDraw parts library every time it is started.
This not only affects your internet connection, but also puts an unnecessary load on LDraw's servers.

Please update as soon as possible!


## [2022.3.1] - 2022-03-01
This update is mostly about improvements to the handling of LDraw 3D part models:
- BrickStore will now automatically download and update its own copy of the LDraw parts library.
- Transparent parts are rendered correctly now.
- If available, high resolution parts are loaded and studs are rendered with a LEGO logo on top.
- Made the mouse controls for rotating parts much more intuitive (think of the part as being enclosed in a transparent sphere, that you can grab and roll around).
- On Windows, the rendering is now done via Direct3D instead of OpenGL. This should hopefully get rid of some crashes with those ever broken Intel graphics drivers.

## Fixed
- Fixed a rendering issues with buttons on macOS, introduced in 2022.2.1
- Removed workarounds for various bugs in BrickLink's order metadata, as they are now fixed on BrickLink's side (this also means that older BrickStore versions might now display wrong order totals).


## [2022.2.2] - 2022-02-09
This .2 release fixes two bugs:
- Fixed a crash when adding and also consolidating lots.
- If you had saved a custom *User Standard* column layout, the layout's sort order was not applied to newly created documents.


## [2022.2.1] - 2022-02-09
This release is a bit of a mixed bag: a lot of crucial fixes, but also a bunch of new features.

### Added
- Wanted lists can now be imported via the UI, just like shopping carts and orders.
- The production span of an item is now calculated from the sets it is contained in and the years those sets have been released in. This is currently accessible via tooltips and the `Year` column in the document view.
- Parting out from the *Appears-In* list will now ask the user for more details.
- Parting out from a document item will also ask the user, but only if the inventory contains extras, alternates or counterparts.
- Added a tag to the info dock and the tooltips to clarify which item-type the item belongs to (a set, its instructions and original box all share the same id and name)

### Fixed
- BrickLink's inventories do not correctly handle alternates on extra items: these are not marked as *extra*. BrickStore will now detect this and will mark any alternates as *extra* as well.
- Difference-mode base values were not restored when opening BSX files.
- Closing the main window with a modified document in a split view would crash on exit.
- Fixed a long-standing crash when consolidating currently filtered out lots.
- The picture and price-guide cache was not reset correctly after a manual database update and this could lead to crashes.
- Rendering 3D LDraw models on Windows should now also work even if you have bad OpenGL drivers (or none at all).
- Instructions no longer share the image with their corresponding set entry.


## [2022.1.3] - 2022-01-31
This is another bug-fix release.

### Fixed
- There were still a few crashes when closing message dialogs.
- The filter fields behaved erratically when you saved favorite filters before.
- Importing orders after updating the database produced broken item and color references.
- Loading the cached order data is now done in a background thread to improve startup performance.


## [2022.1.2] - 2022-01-25
This is a bug-fix release.

### Fixed
- Documents could not be closed on macOS.
- Some people experienced crashes when closing message dialogs.
- Closing documents that had been split off into floating windows could lead to crashes.
- Filtering by `<` or `>` on the new date based columns failed.
- Downloading the store contents could stop working after some time.


## [2022.1.1] - 2022-01-21

### Added
- Replaced the document selector tabs with a drop-down list, which scales a lot better with multiple open documents.
- The info "docks" are now really dockable and can be rearranged (and even stacked) to your liking by dragging them around. Also added two new docks: a list of recent documents and currently open documents.
- Detailed information about orders is now available in the order import dialog (right-click on any order) and also by clicking the order information button in the document toolbar after importing.
- Windows can now be split horizontally and vertically (as often as you like) to make working on multiple documents in parallel easier.
- Filters can now be edited both via UI controls and in text form: just click the dots menu on the right of the filter controls and switch to "text only mode". Copying and pasting filters in UI mode is also available via this menu.
- The JavaScript scripting API has been heavily extended and documented. The printing part is more or less stable by now, the UI extension part still needs some testing. [The documentation](https://rgriebl.github.io/brickstore/extensions/) is auto generated from the code.
- Thanks to Sergio, we now have a complete Spanish translation.

### Fixed
- Printing only the selected items also does work now using the default print preview mechanism.
- Version 2021.10 broke the update check mechanism, so the automatic check in these versions will not report that a newer update is available. The affected users will be notified via the Announcements mechanism instead, but they will have to install the update manually.

This release also adds a rudimentary mobile port for Android tablets, but the UI part is far from being finished. Most of the functionality of the desktop version is there, but not accessible yet.
An improved UI and iPadOS support is planned for the upcoming releases.


## [2021.10.2] - 2021-10-07
This .2 release additionally fixes two bugs introduced in the 2021.10.1 release:
- Fixed the status column not sorting correctly.
- Fixed the Windows installer not shipping the newly required `zlib1.dll`.

### Added
- This release brings back the classic filter mechanism from the old BrickStore versions, but it also adds a lot of convenience functionality:
  - The fields to filter on are limited to the currently visible columns.
  - If a field has a limited set of possible values (e.g., colors, status, condition), these values can be auto-completed while typing or just picked from a drop-down list.
  - Even complex filters can be (de)activated by a single click on the filter icon.
- Added support for simple calculations on all numeric document fields. Columns such as price or quantity now allow you to enter either an absolute numeric value (e.g. `42`) or a calculation term (e.g. `=+42` to add 42 to all selected values).

  The syntax is `=<operation><value>` with `<operation>` being one of `+-*/` and `<value>` being a valid integer or floating-point number (depending on the column's data type).

### Improvements
- Fixed a few problems with failing authentications against the BrickLink servers.

### Technical Changes
- To simplify the code base, the minimum supported Qt and C++ version were raised to 5.15 and 20 respectively.
- BrickStore can be built against Qt 6.2 now, but a few small features are still missing.


## [2021.10.1] - 2021-10-07
### Added
- This release brings back the classic filter mechanism from the old BrickStore versions, but it also adds a lot of convenience functionality:
  - The fields to filter on are limited to the currently visible columns.
  - If a field has a limited set of possible values (e.g., colors, status, condition), these values can be auto-completed while typing or just picked from a drop-down list.
  - Even complex filters can be (de)activated by a single click on the filter icon.
- Added support for simple calculations on all numeric document fields. Columns such as price or quantity now allow you to enter either an absolute numeric value (e.g. `42`) or a calculation term (e.g. `=+42` to add 42 to all selected values).

  The syntax is `=<operation><value>` with `<operation>` being one of `+-*/` and `<value>` being a valid integer or floating-point number (depending on the column's data type).

### Improvements
- Fixed a few problems with failing authentications against the BrickLink servers.

### Technical Changes
- To simplify the code base, the minimum supported Qt and C++ version were raised to 5.15 and 20 respectively.
- BrickStore can be built against Qt 6.2 now, but a few small features are still missing.


## [2021.6.1] - 2021-06-02
### Added
- Added support for light and dark themes for all platforms. This uses the native themeing on macOS, while it uses a custom style and color palette for the *Light* and *Dark* themes on Windows and Linux.

### Improvements
- Adjusted the picture cache RAM consumption to avoid crashes on 32bit Windows because the  BrickStore process is consuming too much memory. You should however use the 64bit version if possible: the larger item images used nowadays fill up the cache quite quickly and force the 32bit version to constantly reload images from disk.
- Parting out variable colored items (e.g. `973c00`) works correctly now.


## [2021.5.2] - 2021-05-25
### Added
- The Toolbar is now fully customizable via the `Settings` dialog.

### Improvements
- Enabled most secondary keyboard shortcuts (e.g. `Ctrl+W` to close documents on Windows). See the `Keyboard` page in the `Settings` dialog for a full list.

### Fixed
- `Reset difference mode base values` is now only acting on the selected items again.
- Fixed a few macOS printing bugs: the scaling was off, the wrong colors where used on dark themes and the default paper size was not detected correctly.


## [2021.5.1] - 2021-05-18
### Added
- The list of items in the `Add Item` dialog can now also be filtered by known-colors by clicking the new *lock* icon in the top, right of the color selector.
- Added an announcement mechanism to be able to announce important changes or problem solutions to all users.
- Extended the `Help` menu with easy access to the project's resources on GitHub.

### Improvements
- Optimized database loading and access and improved the known-colors coverage.

### Please Note
BrickLink's server are still unstable, but this version adds yet more workarounds to better deal with this situation.


## [2021.4.1] - 2021-04-21
### Added
- All item image views now have `Copy` and `Save` action in their context menus.
- Documents can be sorted on multiple columns now: clicking the column header sets and toggles the primary sort key, while shift-clicking sets and toggles the additional sort keys. You can sort by as many additional columns as you like.

### Please Note
BrickLink's servers are very unstable right now (April 2021). This version of BrickStore tries to work around some of the problems, but the price guide download interface is still broken on BrickLink's side and may lead to invalid price guide data in BrickStore.

See https://github.com/rgriebl/brickstore/issues/335 and https://www.bricklink.com/message.asp?ID=1266428 for more information.

The problem has been reported to BrickLink, but no feedback was received yet.

## [2021.3.3] - 2021-03-25
### Added
- Printing a document will now print exactly what you see on the screen, using a preview window. The old print script (as well as any customized print scripts) are still available via the  `Extras` menu.
- Multiple orders with differing currencies can now be combined on import.

### Improvements
- The marker column is now saved and restored.
- In addition to the in-place duplication, there is now also a `Paste silent` functionality which works just like `Paste`, but never asks about overwriting or consolidating - it just appends to the document.


## [2021.3.2] - 2021-03-22
### Added
- Experimental support for a marker column that can be used for the multi-order picking: multiple orders can be imported into the same document now. Markers are not saved at the moment.
- All main window keyboard shortcuts are now configurable in the Settings dialog.

### Improvements
- When loading files with outdated item or color references, BrickStore will now mark the file as modified after updating them.
- BrickLink Studio models should now import without any missing parts or mismatched colors.
- Added a quick way to duplicate selected lots: `Edit` > `Duplicate` or `Ctrl+D`.
- The Windows x64 release will now automatically send anonymized crash reports to https://sentry.io This might be extended to the other platforms if it proves to be useful.


## [2021.3.1] - 2021-03-05
### Added
- *Import LDraw Model* can now import BrickLink Studio models as well.
- Editing a field while multiple rows are selected will change the value in all selected rows. You can also navigate within a selection with `Ctrl+cursor keys` (`Cmd` on macOS), so you don't have to use your mouse at all.
- *Copy remarks from document* got extended to *Copy fields from document* and can now copy and merge any field in multiple ways from one document to another (e.g. merging in prices from another document using weighted averages).
- The context menu is really context sensitive now and will only show specific field commands for the clicked column.
- BrickStore will now restore all the saved documents in your last session on startup (this can be disabled in the settings dialog).
- The separate *Difference Mode* is gone. Instead it is always active and works on all fields. Just like with errors, you can choose to show or hide the difference markers via the `View` menu. This makes it possible to *Mass-Update* all possible fields in your BrickLink store from within BrickStore.
- The status bar moved to the top of the document and got more interactive: clicking on `Errors` or  `Differences` will jump to the next one (also available on the keyboard: `F5` or `F6`).
- Experimental support for distribution independent Linux *AppImage* builds.

### Improvements
- Sorting and filtering are now independent operations again: the filter edit has a specific re-apply filter button now and clicking on the last sorted column header will re-sort the list.
- All import dialogs are non-modal now to allow multiple imports easily.
- High-DPI support has been enhanced.
- Multi-threaded price guide loading to (partially) compensate for the slow I/O on Windows.
- Faster document loading and saving.
- Undo/redo actions are now named exactly after the action that triggered them.
- The update check can now download and start the new installer for you.
- All numbers should now show with localized thousand's separators.
- Set-to-price-guide will now block the current document until it is finished. This is to prevent odd behavior if you played around with undo/redo too much while the download was happening in the background, plus you can undo the whole operation in a single click on *Undo* now.
- Clicking on the logo/progress circle in the top, right corner gives you the option to cancel all current downloads (pictures and price guides).
- Updating the BrickLink order list is a lot faster now.
- There is now an option to set a default for part-out operations within documents.
- All open and save file dialogs remember the last used directory during a session.
- Renovated the price increase/decrease dialog.
- The modify item and color dialogs remember their last state and geometry. You can reset their size and position to be centered on the currently active item by double-clicking the title bar.


## [2021.2.2] - 2021-02-17
This is a quick-fix release for 2021.2.1: it adds the missing German translation and fixes a bug where the language would sometimes revert to English.


## [2021.2.1] - 2021-02-17
### Improvements
- Complete overhaul of BrickLink cart and order importing.
- Sorting and filtering are undoable operations now just like in Excel. Also both are really one-time actions now: adding or removing items will not dynamically re-apply the sorting and filtering. There's reapply button to do it manually when you want to have the list resorted.

### Added
- BrickLink Mass **Update** works on all supported fields now. The document you want to export has to be in **Difference Mode**, but this is activated automatically when importing your store inventory.
- Clicking the **Error** label (or pressing `F6`) will jump to the next field that has erroneous input.
- The filter in the add-item and import-set dialogs gained even more functionality: have a look at the tool-tip label to learn about it.
- Right-clicking on Minifigs gives you the option to quickly and easily filter for similar body parts, head gear, etc.
- Added support for finding parts using part-color-codes (element numbers) in the add-item dialog.

### Fixed
A lot.


## [2021.2.0] - 2021-02-07
### Improvements
- Operations on a lot of rows at once have been sped up by several orders of magnitude.
- Picture downloads are now done completely in a background thread, keeping the UI more responsive.

### Added
- New, modern icons bases on KDE's breeze theme. These icons can now adapt to dark desktop themes on macOS and Linux.
- The filter in the add-item and import-set dialogs gained the ability to handle **exclude** words: e.g. `brick 1 x 2 -pattern` or `slope -33 -45`.
- You can save your favorite filters now by pressing `Return` in an filter edit field. Recall them by pressing `Cursor Down` or by clicking the filter icon on the left. The `X` in pop-up list lets you delete unwanted filters.

### Fixed
A lot.

Please read the [installation instructions](https://github.com/rgriebl/brickstore/wiki/Installation-Instructions).


## [2021.1.7] - 2021-01-27
The **Defaults** page in the settings dialog was completely removed. Instead, the affected dialogs will now remember their last settings, even when exiting and restarting BrickStore.

### Added
- Basic support for BrickLink's *My Cost* field was added.
- Auto-saving is finally activated: BrickStore saves any changes you make to temporary files every minute. If it crashes, it will ask on restart if you want to restore any unsaved changes.
- Minifigs have a weight and a year-of-release now.
- Adding or importing items is faster now by simply double-clicking the item (or color).
- Satin colors got their own color category.
- New shortcuts for consolidating items and quickly jumping to BrickLink web-pages.

### Fixed
- Price guide data includes VAT now, so it is a lot closer to what you would see on BrickLink's web pages. It's still not perfect, but if you are interested in the details why this is so hard: #80
- Buyer/Collector mode was not restored on restarts.
- Reimplemented item consolidation, which was actually semi-broken for the last 15 years.
- All column layouts should be restored correctly now.
- Sub-conditions on sets can now be set in the add-item window and will show up correctly in the document's item list.
- Filtering for an empty field is possible by specifying it as `""` (two quotes).


## [2021.1.6] - 2021-01-22
Fixed a crash when closing the settings dialog.


## [2021.1.5] - 2021-01-21
A lot of bug-fixing all over the place, too much to mention all in here. 

### Added
- Switched from using the classic 80x60 small images to BrickLink's newer *normal* format. This should make all item images nice to look at, even on Hi-Dpi screens.
- This change made it possible to have zoom controls in all catalog item views (e.g. the add-item and import-inventory dialogs). Use the `+`/`-` buttons, `Control` + scroll wheel or a touchpad gesture to zoom seamlessly between 50% and 500%.
- The add-item dialog is now more customizable with splitters between elements and these customizations are saved and restored.
- The LDraw detection has been improved, but even if does not work, you can now also specify your LDraw directory in the settings.
- The database update is now honoring the expiration time from the settings dialog and will update automatically on startup, if the database is past its expiry date.
- BrickStore will also check for newer versions on startup automatically.


## [2021.1.4] - 2021-01-15
Performance improvements across the board, especially in the table rendering.

### Added
- First implementation of a generic JavaScript extension interface, not public yet. As a side effect, the old JavaScript printing was replaced with the new one. Any old printing scripts for BrickStore 1.x are not compatible anymore, but porting will be simple once the new API is stable and documented.
- The new column layout load/save mechanism got its missing management dialog.

### Fixed
- BrickStore can now be properly restarted by its installer on Windows.
- Problems with "TLS initialization" on Windows should hopefully be fixed.
- Fixed a crash that occured when setting prices to price guide in two documents in parallel.
- Difference mode is now a per-document setting and survives saving and re-loading the document.
- Order imports now cope with changes to the BrickLink interface. The `Any` order type had to be removed for the time being though.
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
- "Natural language" like filtering for the main document view (the tool-tip on the input field tells the whole story).
- Item and color id changes are now resolved automatically using BrickLink's change log.
- Modernized UI with Hi-Dpi support, still on all of Windows, macOS and Linux.
- Single-app mode: opening an BrickStore document from the Explorer/Finder will now just open a new tab in an already running BrickStore process.
- The font and icon sizes can be scaled between 50% and 200%.
- Undo/redo actions are now presented in a list with human readable labels.

### Removed
- Custom UIs in printing scripts do not work anymore, because the JavaScript runtime is totally different. Even the normal printing scripts have to be adapted slightly, but in the end, I will have to replace the current mechanism once again for future proof Qt 6 based solution next year.
- Per-document column configurations and the "Collector" as well as the "Simple" view mode are not (re)implemented yet.


[Unreleased]: https://github.com/rgriebl/brickstore/compare/v2024.5.2...HEAD
[2024.5.2]: https://github.com/rgriebl/brickstore/releases/tag/v2024.5.2
[2024.5.1]: https://github.com/rgriebl/brickstore/releases/tag/v2024.5.1
[2024.4.1]: https://github.com/rgriebl/brickstore/releases/tag/v2024.4.1
[2024.3.1]: https://github.com/rgriebl/brickstore/releases/tag/v2024.3.1
[2024.2.1]: https://github.com/rgriebl/brickstore/releases/tag/v2024.2.1
[2024.1.4]: https://github.com/rgriebl/brickstore/releases/tag/v2024.1.4
[2024.1.3]: https://github.com/rgriebl/brickstore/releases/tag/v2024.1.3
[2024.1.2]: https://github.com/rgriebl/brickstore/releases/tag/v2024.1.2
[2023.11.2]: https://github.com/rgriebl/brickstore/releases/tag/v2023.11.2
[2023.11.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.11.1
[2023.8.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.8.1
[2023.5.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.5.1
[2023.4.4]: https://github.com/rgriebl/brickstore/releases/tag/v2023.4.4
[2023.4.3]: https://github.com/rgriebl/brickstore/releases/tag/v2023.4.3
[2023.4.2]: https://github.com/rgriebl/brickstore/releases/tag/v2023.4.2
[2023.4.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.4.1
[2023.3.2]: https://github.com/rgriebl/brickstore/releases/tag/v2023.3.2
[2023.3.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.3.1
[2023.2.1]: https://github.com/rgriebl/brickstore/releases/tag/v2023.2.1
[2022.11.2]: https://github.com/rgriebl/brickstore/releases/tag/v2022.11.2
[2022.11.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.11.1
[2022.10.2]: https://github.com/rgriebl/brickstore/releases/tag/v2022.10.2
[2022.10.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.10.1
[2022.9.3]: https://github.com/rgriebl/brickstore/releases/tag/v2022.9.3
[2022.9.2]: https://github.com/rgriebl/brickstore/releases/tag/v2022.9.2
[2022.9.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.9.1
[2022.4.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.4.1
[2022.3.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.3.1
[2022.2.2]: https://github.com/rgriebl/brickstore/releases/tag/v2022.2.2
[2022.2.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.2.1
[2022.1.3]: https://github.com/rgriebl/brickstore/releases/tag/v2022.1.3
[2022.1.2]: https://github.com/rgriebl/brickstore/releases/tag/v2022.1.2
[2022.1.1]: https://github.com/rgriebl/brickstore/releases/tag/v2022.1.1
[2021.10.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.10.2
[2021.10.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.10.1
[2021.6.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.6.1
[2021.5.2]: https://github.com/rgriebl/brickstore/releases/tag/v2021.5.2
[2021.5.1]: https://github.com/rgriebl/brickstore/releases/tag/v2021.5.1
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
