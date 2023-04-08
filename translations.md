---
layout: page
title: Translating
---
Currently maintained translations:
- English
- German
- Spanish
- Swedish

See also this file for a list of maintainers: <https://github.com/rgriebl/brickstore/blob/master/translations/translations.json>

Here's how you can translate BrickStore into another language:

## Get the sources

Check if a (partial) translation already exists for the language you are aiming for here: <https://github.com/rgriebl/brickstore/blob/master/translations>
If it doesn't exist yet, then please open an issue on GitHub, so that I can add an empty translation as a starting point.

You then need to either clone the BrickStore git repository or download it as a ZIP file: [Download link](https://github.com/rgriebl/brickstore/archive/refs/heads/master.zip) and unpack it.

## Get the translation tool, called Qt Linguist

If you are using Windows, you can download a standalone version of Linguist from here: <https://github.com/thurask/Qt-Linguist/releases/download/20211010/linguist_6.2.0.zip>. You only need the `linguist.exe` file from the downloaded ZIP!

Linux users can just install it: for Debian/Ubuntu the package is called `qttools5-dev-tools`

Qt Linguist is quite powerful tool, but also comes with a [complete manual](https://doc.qt.io/qt-5/qtlinguist-index.html).
Please make sure to read through the `Translators` section.

BrickStore makes heavy use of plural form disambiguation, so often times a single translation is not enough. In addition to the Qt Linguist manual, there's more background information to be found in the Qt 5 documentation [here](https://doc.qt.io/qt-5/i18n-source-translation.html#handling-plurals) and [there](https://doc.qt.io/qt-5/i18n-plural-rules.html).

## Translate!

Start the `Linguist` tool, select `File` > `Open`, go to the directory where you cloned the BrickStore repository to (or where you unpacked the downloaded ZIP file), go to the `translations` sub-directory and then finally open the `brickstore_XX.ts` file of the language you want to edit.

You can now translate all the missing parts (as indicated by a `?` icon). The quickest way is to enter a translation and then press `Ctrl+Return`: this way you mark the current translation as "done" and also move on to the next untranslated entry.
If you want to cycle through all the untranslated entries, then `Ctrl+J` / `Ctrl+K` is a quick and easy shortcut.

## Testing your changes locally

From the Qt Linguist tool, generate a binary representation (`brickstore_XX.qm`) of your translation work via `File` > `Release As...`.
Now (re)start BrickStore with the additional command line option `--load-translation path/to/brickstore_XX.qm`

BrickStore will now use your specified `qm` file, with English as a fallback for missing translations. Please note, that switching languages in the Settings dialog will not have any effect as long as you start BrickStore with the `--load-translation` command line switch.

## Get your translations into BrickStore

If you know your way around GitHub, then the easiest thing would be open a pull-request with your changes on the `ts` file. If you just downloaded the ZIP file in the first place, then please open a new issue and attach your modified `ts` file. I will then merge it into the next release.

## Maintaining the translation over time

It is important to keep your local copy (or checkout) of the BrickStore source code up-to-date. Otherwise the powerful `Sources and Forms` preview in linguist will become less and less useful over time, as it cannot match the translations to dialogs or the source code anymore.
So *before* you do any translation work, always do an update first:

* The easiest way to keep the sources up-to-date is using git: a quick `git pull` and you have all the latest changes. If you sent me an translation file via e-mail, you have to undo your local changes via `git checkout .` first though.
* If you are not familiar with git, then you can of course re-download the source code as a ZIP file (see [Get the sources](#get-the-sources) above for a link), delete the old source code folder and unzip the download to a new folder.

Only *then* should you open the updated `brickstore_XX.ts` file in linguist and start translating.
