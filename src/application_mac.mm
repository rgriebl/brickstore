#import <Cocoa/Cocoa.h>
#include "config.h"

bool hasMacThemes()
{
    if (__builtin_available(macOS 10.14, *))
        return true;
    else
        return false;
}

Config::UiTheme currentMacTheme()
{
    Config::UiTheme theme = Config::UiTheme::SystemDefault;

    if (__builtin_available(macOS 10.14, *)) {
        auto appearance = [NSApp.effectiveAppearance
                bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
        if ([appearance isEqualToString:NSAppearanceNameDarkAqua])
            theme = Config::UiTheme::Dark;
        else if ([appearance isEqualToString:NSAppearanceNameAqua])
            theme = Config::UiTheme::Light;
    }
    return theme;
}

void setCurrentMacTheme(Config::UiTheme theme)
{
    if (__builtin_available(macOS 10.14, *)) {
       NSAppearance *appearance = nil;
       switch (theme) {
       case Config::UiTheme::Dark:
           appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
           break;
       case Config::UiTheme::Light:
           appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
           break;
       default:
           break;
       }

       [NSApp setAppearance:appearance];
    }
}

void removeUnneededMacMenuItems()
{
    // from https://forum.qt.io/topic/60623/qt-5-4-2-os-x-10-11-el-capitan-how-to-remove-the-enter-full-screen-menu-item

    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSDisabledDictationMenuItem"];
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSDisabledCharacterPaletteMenuItem"];

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER
    if ([NSWindow respondsToSelector:@selector(allowsAutomaticWindowTabbing)])
        NSWindow.allowsAutomaticWindowTabbing = NO;
#endif
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];
}
