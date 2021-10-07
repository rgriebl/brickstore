import QtQuick
import QtQuick.Controls

// Menu doesn't resize itself to the maximum of its items' implictWidth
// from https://martin.rpdev.net/2018/03/13/qt-quick-controls-2-automatically-set-the-width-of-menus.html

Menu {
    width: {
        let result = 0
        let padding = 0
        for (let i = 0; i < count; ++i) {
            let item = itemAt(i)
            result = Math.max(item.implicitWidth, result)
            padding = Math.max(item.padding, padding)
        }
        return result + padding * 2
    }
}
