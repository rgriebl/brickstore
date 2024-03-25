// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

import Mobile


// Menu doesn't resize itself to the maximum of its items' implictWidth
// from https://martin.rpdev.net/2018/03/13/qt-quick-controls-2-automatically-set-the-width-of-menus.html

Menu {
    implicitWidth: {
        let result = 0
        for (let i = 0; i < count; ++i) {
            let item = itemAt(i)
            if (!item.visible)
                continue
            let w = item.implicitWidth
            let lpad = item.leftPadding ?? 0
            let rpad = item.rightPadding ?? 0
            result = Math.max(w + lpad + rpad, result)
        }
        return result
    }
}
