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
